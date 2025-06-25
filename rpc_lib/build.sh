# Build script for the RPC library using CMake and WASI SDK
# Pls run from the rpc_lib directory
#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status

rm -rf build      

mkdir build && cd build

cmake \
  -DCMAKE_TOOLCHAIN_FILE=/home/eb/fyp/wasi-sdk-25.0-x86_64-linux/share/cmake/wasi-sdk.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/home/eb/fyp/wasi-install \
  ..

cmake --build . --target install