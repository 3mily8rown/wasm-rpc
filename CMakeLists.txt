cmake_minimum_required(VERSION 3.15)
project(super LANGUAGES NONE)
include(ExternalProject)

# TODO: set this to your WASI SDK path
set(WASI_TOOLCHAIN_FILE "/home/eb/fyp/wasi-sdk-25.0-x86_64-linux/share/cmake/wasi-sdk.cmake")
set(WAMRC_EXECUTABLE "/home/eb/fyp/wasm-micro-runtime/wamr-compiler/build/wamrc")


# -----------------------------------------------------------------
# wasm (needs protoc for proto messages)
# -----------------------------------------------------------------
ExternalProject_Add(wasm
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/wasm
  BINARY_DIR ${CMAKE_BINARY_DIR}/wasm-prefix/src/wasm-build
  INSTALL_DIR ${CMAKE_BINARY_DIR}/install

  CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${WASI_TOOLCHAIN_FILE}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DPROTO_DIR=${CMAKE_SOURCE_DIR}/wasm/generated_nano
        -DIMPORTS_DIR=<INSTALL_DIR>/generated
        -DSCRIPT_DIR=${CMAKE_SOURCE_DIR}/host_tools/scripts
        -DWAMRC_EXECUTABLE=${WAMRC_EXECUTABLE}

  UPDATE_COMMAND ""
  BUILD_ALWAYS 1
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel
  INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --prefix <INSTALL_DIR>
)

# -----------------------------------------------------------------
# native (needs imports.h from step 2)
# -----------------------------------------------------------------
set(IMPORTS_DIR "${CMAKE_BINARY_DIR}/install/generated")
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")

set(CMAKE_C_COMPILER /usr/bin/clang)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(THREADS_PREFER_PTHREAD_FLAG ON)
set(CMAKE_THREAD_LIBS_INIT -lpthread)
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_HAVE_PTHREADS_CREATE 1)

add_subdirectory(native)

# ─── run_wasm_host: build-native then invoke the host ───────────────────────
add_custom_target(run_rpc_host
  COMMAND ${CMAKE_BINARY_DIR}/native/rpc_host
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
  VERBATIM
  COMMENT "Running rpc_host"
)
add_custom_target(run_client_host
  COMMAND ${CMAKE_BINARY_DIR}/native/client_host
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
  VERBATIM
  COMMENT "Running client_host"
)
add_custom_target(run_server_host
  COMMAND ${CMAKE_BINARY_DIR}/native/server_host
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  USES_TERMINAL
  VERBATIM
  COMMENT "Running server_host"
)






