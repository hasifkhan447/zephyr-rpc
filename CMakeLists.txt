# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sockets_echo_server)
find_library(RPC NAMES rpc HINTS "/home/hak/External/Documents/rpc/rpc-library/build/")

add_subdirectory(./rpc-library)

target_link_libraries(app PRIVATE ${RPC}) 


target_sources(app PRIVATE src/packet.c)
include_directories(./include/)
include(${ZEPHYR_BASE}/samples/net/common/common.cmake)
include_directories(../rpc-library/include/)





