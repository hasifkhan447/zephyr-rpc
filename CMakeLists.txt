# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sockets_echo_server)
target_sources(app PRIVATE src/packet.c src/helpers.c)
include_directories(./include/)
include(${ZEPHYR_BASE}/samples/net/common/common.cmake)

zephyr_get_include_directories_for_lang_as_string(       C includes)
zephyr_get_system_include_directories_for_lang_as_string(C system_includes)
zephyr_get_compile_definitions_for_lang_as_string(       C definitions)
zephyr_get_compile_options_for_lang_as_string(           C options)

if (DEFINED CMAKE_C_COMPILER_TARGET)
  set(target_flag "--target=${CMAKE_C_COMPILER_TARGET}")
endif()

set (external_project_cflags
  "${target_flag} ${includes} ${definitions} ${options} ${system_includes}"
)

include(ExternalProject)

set(rpc_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/rpc-library/)
set(rpc_build_dir ${CMAKE_CURRENT_SOURCE_DIR}/rpc-library/)

set(RPC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rpc-library/lib)
set(RPC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rpc-library/include)

if (CMAKE_GENERATOR STREQUAL "Unix Makefiles")
set(submake "$(MAKE)")
else()
set(submake "make")
endif()


ExternalProject_Add(
  rpclib_project
  PREFIX ${rpc_build_dir}
  SOURCE_DIR ${rpc_src_dir}
  BINARY_DIR ${rpc_src_dir}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND 
  ${submake}
  PREFIX=${rpc_build_dir}
  CC=${CMAKE_C_COMPILER}
  AR=${CMAKE_AR}
  CFLAGS=${external_project_cflags}
  INSTALL_COMMAND=""
  BUILD_BYPRODUCTS ${RPC_LIB_DIR}/librpc.a
)

add_library(rpc_lib STATIC IMPORTED GLOBAL)
add_dependencies(
  rpc_lib
  rpc_project
)

set_target_properties(rpc_lib PROPERTIES IMPORTED_LOCATION ${RPC_LIB_DIR}/librpc.a)
set_target_properties(rpc_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${RPC_INCLUDE_DIR})


target_link_libraries(app PUBLIC rpc_lib)








