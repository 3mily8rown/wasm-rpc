cmake_minimum_required(VERSION 3.15)
project(NativeRPCExample C CXX)
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rpc_lib/native-install")

find_package(nanopb CONFIG REQUIRED)
find_package(rpc_lib CONFIG REQUIRED)

# ——— Shared RPC primitives (client, server, socket, queue) ———
file(GLOB RPC_PRIMITIVES_SRCS
  ../native/src/rpc/socket_communication.cpp
  ../native/src/rpc/message_queue.cpp
  ../native/src/ring_buffer_rpc/rb_message_queue.cpp
)
add_library(rpc_primitives STATIC ${RPC_PRIMITIVES_SRCS})
target_include_directories(rpc_primitives PUBLIC
  ../native/include
)

# ——— Native client_app ———
add_executable(native_client src/client_app.cpp)
target_link_libraries(native_client PRIVATE rpc_lib::rpc_lib)

# ——— Native server_app ———
add_executable(native_server src/server_app.cpp)
target_link_libraries(native_server PRIVATE rpc_lib::rpc_lib)

