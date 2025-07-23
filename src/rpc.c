#include "../include/rpc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


//TODO: Is there any way I can make generating this Serialize easier? So I don't have to make the Args and Ret together? Is there any macro I can make?


void PrintSerialized(const char* buffer) {

  printf("%d %f %f %f %f %f %f %d\n", *(Command*)buffer, *(float*)(buffer+32),*(float*)(buffer+64), *(float*)(buffer+96), *(float*)(buffer+128), *(float*)(buffer+164), *(float*)(buffer+192), *(int*)(buffer+224));

}

void Serialize(const Call* call, char* buffer) { // called on one side
  
  memcpy(buffer, &call->function_enum, sizeof(Command));

  memcpy(buffer+32, &call->args->arg1, sizeof(float));
  memcpy(buffer+64, &call->args->arg2, sizeof(float));
  memcpy(buffer+96, &call->args->arg3, sizeof(float));

  if (call->ret != 0x0) {
    memcpy(buffer+128, &call->ret->arg1, sizeof(float));
    memcpy(buffer+160, &call->ret->arg2, sizeof(float));
    memcpy(buffer+192, &call->ret->arg3, sizeof(float));
    memcpy(buffer+224, &call->ret->err, sizeof(float));
  }

}; //This needs to be serialized, then sent, and then freed

void Deserialize(const char* buffer, Call *call) { // called on another side
  call->function_enum = *(Command*)buffer; //typecast through *Command
  
  call->args->arg1 = *(float*)(buffer+32);
  call->args->arg2 = *(float*)(buffer+64);
  call->args->arg3 = *(float*)(buffer+96);

  if (call->ret != 0x0) {
    call->ret->arg1 = *(float*)(buffer+128);
    call->ret->arg2 = *(float*)(buffer+164);
    call->ret->arg3 = *(float*)(buffer+192);

    call->ret->err = *(int*)(buffer+224);
  }
};


void Dispatcher(Call* call) { // Only going to be run on the MCU
  switch(call->function_enum) {
    case read_sensor:
      //TODO: extern void read_sensor()
      //
      // Assign its output to ret and then reserialize to the other end .
      break;

    case run_motor:   //currently just running it at a certain speed
      extern int run_motor_fn(float motor_id, float pwm_frequency);
      call->ret->err = run_motor_fn(call->args->arg1,call->args->arg2);
      break;

    case pid_to_position:
      // TODO: 
      break;

    case finger_pos:
      // TODO:
      break;
  }
}
