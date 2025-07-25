/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */



// Plan
//
// We need to do some work. The thread needs to accept buffer message through a msgq.
// The send thread needs to send something, wait for response (recv thread), and then resend something.
// 
//
// Calling on nano should be like this.
// allocate cmd, call, ret, arg
// Then we can do:
// Serialize(buffer, MACRO(run_motor(1, 0.1);))
//
// And then send a packet over the IP stack



// Now, on the microcontroller
// recv waits to constantly recieve. When it does, puts the msg onto the recieved msgq and wakes up the thread that's waiting on the recieved msgq 
//
// That thread then deserializes the message, dispatches it
//
// allocate cmd, call, ret, arg
// call = {cmd, arg, ret}
// Deserialize(buffer, call)
// Dispatch(call)
//
// Then it begins to send the data back
//
// Serialize(buffer, call)
//
// send over IP stack -> we can send through the msgq



#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_pkt_sock_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <errno.h>

#include <zephyr/misc/lorem_ipsum.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_mgmt.h>
#include "../include/rpc.h"
#include "../include/packet.h"
#include "../include/helpers.h"


#define STACK_SIZE 2056
#if defined(CONFIG_NET_TC_THREAD_COOPERATIVE)
#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#else
#define THREAD_PRIORITY K_PRIO_PREEMPT(8)
#endif
#define RECV_BUFFER_SIZE BUF_SIZE
#define WAIT_TIME CONFIG_NET_SAMPLE_SEND_WAIT_TIME

#define FLOOD (CONFIG_NET_SAMPLE_SEND_WAIT_TIME ? 0 : 1)

static struct k_sem quit_lock;

struct packet_data {
  int send_sock;
  int recv_sock;
  char recv_buffer[RECV_BUFFER_SIZE];
};


static struct packet_data sock_packet;
static bool finish;
static K_SEM_DEFINE(iface_up, 0, 1);

static void recv_packet(void);
static void send_packet(void);

K_THREAD_DEFINE(receiver_thread_id, STACK_SIZE,
                recv_packet, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, -1);
K_THREAD_DEFINE(sender_thread_id, STACK_SIZE,
                send_packet, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, -1);


K_THREAD_DEFINE(dispatcher_thread_id, STACK_SIZE,
                command_dispatcher, NULL, NULL, NULL,
                THREAD_PRIORITY, 0, -1);



// TODO: Make something to protect this
K_MSGQ_DEFINE(recv_msgq, sizeof(struct packet_data*), 10 , 1);
K_MSGQ_DEFINE(send_msgq, sizeof(char*), 10 , 1);


void PrintSerialized(const char* buffer) {

  LOG_DBG("%d %f %f %f %f %f %f %d\n", *(Command*)buffer, *(float*)(buffer+32),*(float*)(buffer+64), *(float*)(buffer+96), *(float*)(buffer+128), *(float*)(buffer+164), *(float*)(buffer+192), *(int*)(buffer+224));

}


static void quit(void)
{
  k_sem_give(&quit_lock);
}

static int start_socket(int *sock)
{
  struct sockaddr_ll dst = { 0 };
  int ret;

  *sock = socket(AF_PACKET,
                 IS_ENABLED(CONFIG_NET_SAMPLE_ENABLE_PACKET_DGRAM) ?
                 SOCK_DGRAM : SOCK_RAW,
                 htons(ETH_P_ALL));
  if (*sock < 0) {
    LOG_ERR("Failed to create %s socket : %d",
            IS_ENABLED(CONFIG_NET_SAMPLE_ENABLE_PACKET_DGRAM) ?
            "DGRAM" : "RAW",
            errno);
    return -errno;
  }

  dst.sll_ifindex = net_if_get_by_iface(net_if_get_default());
  dst.sll_family = AF_PACKET;

  ret = bind(*sock, (const struct sockaddr *)&dst,
             sizeof(struct sockaddr_ll));
  if (ret < 0) {
    LOG_ERR("Failed to bind packet socket : %d", errno);
    return -errno;
  }

  return 0;
}

static int recv_packet_socket(struct packet_data *packet)
{
  int ret = 0;
  int received;

  LOG_INF("Waiting for packets ...");

  do {
    if (finish) {
      ret = -1;
      break;
    }

    received = recv(packet->recv_sock, packet->recv_buffer,
                    sizeof(packet->recv_buffer), 0);

    if (received < 0) {
      if (errno == EAGAIN) {
        continue;
      }

      LOG_ERR("RAW : recv error %d", errno);
      ret = -errno;
      break;
    }

    // LOG_DBG("Received %d bytes", received);

    // LOG_DBG("Packet is at address: %p", packet);
    // PrintSerialized(packet->recv_buffer);
    while (k_msgq_put(&recv_msgq, &packet, K_NO_WAIT) != 0) { 
      k_msgq_purge(&recv_msgq);
      // LOG_INF("put it in processing msgq");
    }
  } while (true);

  return ret;
}


void Dispatcher(Call* call) {
  LOG_INF("Entering dispatcher");

  switch(call->function_enum) {
    case read_sensor:
      //TODO: extern void read_sensor()
      //
      // Assign its output to ret and then reserialize to the other end .
      break;

    case run_motor:   //currently just running it at a certain speed
      call->ret->err = run_motor_fn(call->args->arg1,call->args->arg2);
      LOG_DBG("Hit case run motor");
      break;

    case pid_to_position:
      // TODO: 
      break;

    case finger_pos:
      // TODO:
      break;
    default:
      LOG_DBG("Hit case default");
      break;

  }

}


static void command_dispatcher() {
  struct packet_data* packet = 0x0;
  LOG_DBG("Started recieve thread");


  while (1) {
    k_msgq_get(&recv_msgq, &packet, K_FOREVER);
    LOG_DBG("Packet is at address: %p", packet);
    LOG_DBG("Recieved message (subthread): %s", packet->recv_buffer);

    //Allocate Call 
    Call* call_mcu = malloc(sizeof(Call));
    char* buffer_mcu = malloc(sizeof(char[BUF_SIZE]));

    LOG_DBG("Creating MCU buffer from packet");

    memcpy(buffer_mcu, packet->recv_buffer, sizeof(packet->recv_buffer));


    Args* args_mcu = malloc(sizeof(Args));
    Ret* ret_mcu = malloc(sizeof(Ret));
    call_mcu->args = args_mcu;
    call_mcu->ret = ret_mcu;

    // PrintSerialized(buffer_mcu);

    // LOG_DBG("Sent to deser");
    // LOG_DBG("buffer_mcu: %p call_mcu: %p", (void*)buffer_mcu, (void*)call_mcu);
    // LOG_DBG("call_mcu->function_enum (buffer): %d", *(Command*)buffer_mcu);
    // LOG_DBG("call_mcu->args: %p", (void*)call_mcu->args);
    // LOG_DBG("call_mcu->args->arg1: %f", call_mcu->args->arg1);
    // LOG_DBG("call_mcu->args->arg2: %f", call_mcu->args->arg2);
    // LOG_DBG("call_mcu->args->arg3: %f", call_mcu->args->arg3);
    // LOG_DBG("call_mcu->ret: %p", (void*)call_mcu->ret);
    // LOG_DBG("call_mcu->ret->ret1: %f", call_mcu->ret->arg1);
    // LOG_DBG("call_mcu->ret->ret2: %f", call_mcu->ret->arg2);
    // LOG_DBG("call_mcu->ret->ret3: %f", call_mcu->ret->arg3);
    // LOG_DBG("call_mcu->ret->err: %d", call_mcu->ret->err);







    // for (int i = 0; i < (BUF_SIZE); ++i) {
    //   printk("%02X ", (unsigned char)buffer_mcu[i]);
    // }
    // printk("\n");







    Deserialize(buffer_mcu, call_mcu);

    if (call_mcu->function_enum < last)  {
      LOG_DBG("Sent to dispatch");
      Dispatcher(call_mcu);
      fflush(stdout);
      LOG_DBG("Reserializing");
      Serialize(call_mcu, buffer_mcu);

      PrintSerialized(buffer_mcu);

      while (k_msgq_put(&send_msgq, &buffer_mcu, K_NO_WAIT) != 0) {
        k_msgq_purge(&send_msgq);
        LOG_DBG("put it in send msgq");
      }
    } else {
      free(buffer_mcu);
    }

    free(args_mcu);
    free(ret_mcu);
    free(call_mcu);

  }
}




static void recv_packet(void)
{
  int ret;
  struct timeval timeo_optval = {
    .tv_sec = 1,
    .tv_usec = 0,
  };

  ret = start_socket(&sock_packet.recv_sock);
  if (ret < 0) {
    quit();
    return;
  }

  ret = setsockopt(sock_packet.recv_sock, SOL_SOCKET, SO_RCVTIMEO,
                   &timeo_optval, sizeof(timeo_optval));
  if (ret < 0) {
    quit();
    return;
  }

  while (ret == 0) {
    ret = recv_packet_socket(&sock_packet);
    if (ret < 0) {
      quit();
      return;
    }
  }
}

static int send_packet_socket(struct packet_data* packet)
{
  struct sockaddr_ll dst = { 0 };
  size_t send = 256U;
  int ret;
  char* buffer_mcu;
  dst.sll_ifindex = net_if_get_by_iface(net_if_get_default());

  if (IS_ENABLED(CONFIG_NET_SAMPLE_ENABLE_PACKET_DGRAM)) {
    dst.sll_halen = sizeof(struct net_eth_addr);

    /* FIXME: assume IP data atm */
    dst.sll_protocol = htons(ETH_P_IP);

    ret = net_bytes_from_str(
      dst.sll_addr,
      dst.sll_halen,
      CONFIG_NET_SAMPLE_DESTINATION_ADDR);
    if (ret < 0) {
      LOG_ERR("Invalid MAC address '%s'",
              CONFIG_NET_SAMPLE_DESTINATION_ADDR);
    }
  }

  do {
    if (finish) {
      ret = -1;
      break;
    }

    //TODO: Pull from sending msgq, then give the signal to free memory (this needs to be handled up there, when msgq is read, free)
    k_msgq_get(&send_msgq, &buffer_mcu, K_FOREVER);

    /* Sending dummy data */
    ret = sendto(packet->send_sock, buffer_mcu, send, 0,
                 (const struct sockaddr *)&dst,
                 sizeof(struct sockaddr_ll));
    if (ret < 0) {
      LOG_ERR("Failed to send, errno %d", errno);
      break;
    } else {
      if (!FLOOD) {
        LOG_DBG("Sent %zd bytes", send);
      }
    }

    PrintSerialized(buffer_mcu);

    free(buffer_mcu);

    /* If we have received any data, flush it here in order to
     * not to leak memory in IP stack.
     */
    do {
      static char recv_buffer[RECV_BUFFER_SIZE];

      ret = recv(packet->send_sock, recv_buffer,
                 sizeof(recv_buffer), MSG_DONTWAIT);
    } while (ret > 0);

    if (!FLOOD) {
      k_msleep(WAIT_TIME);
    }
  } while (true);

  return ret;
}

static void send_packet(void)
{
  int ret;

  ret = start_socket(&sock_packet.send_sock);
  if (ret < 0) {
    quit();
    return;
  }

  while (ret == 0) {
    ret = send_packet_socket(&sock_packet);
    if (ret < 0) {
      quit();
      return;
    }
  }
}

static void iface_up_handler(struct net_mgmt_event_callback *cb,
                             uint64_t mgmt_event, struct net_if *iface)
{
  if (mgmt_event == NET_EVENT_IF_UP) {
    k_sem_give(&iface_up);
  }
}

static void wait_for_interface(void)
{
  struct net_if *iface = net_if_get_default();
  struct net_mgmt_event_callback iface_up_cb;

  if (net_if_is_up(iface)) {
    return;
  }

  net_mgmt_init_event_callback(&iface_up_cb, iface_up_handler,
                               NET_EVENT_IF_UP);
  net_mgmt_add_event_callback(&iface_up_cb);

  /* Wait for the interface to come up. */
  k_sem_take(&iface_up, K_FOREVER);

  net_mgmt_del_event_callback(&iface_up_cb);
}




int main(void)
{
  k_sem_init(&quit_lock, 0, K_SEM_MAX_LIMIT);

  wait_for_interface();

  LOG_INF("Packet socket sample is running");

  k_thread_start(receiver_thread_id);
  k_thread_start(sender_thread_id);
  k_thread_start(dispatcher_thread_id);

  k_sem_take(&quit_lock, K_FOREVER);

  LOG_INF("Stopping...");

  finish = true;

  k_thread_join(receiver_thread_id, K_FOREVER);
  k_thread_join(sender_thread_id, K_FOREVER);
  k_thread_join(dispatcher_thread_id, K_FOREVER);

  if (sock_packet.recv_sock >= 0) {
    (void)close(sock_packet.recv_sock);
  }

  if (sock_packet.send_sock >= 0) {
    (void)close(sock_packet.send_sock);
  }
  return 0;
}
