# Build script for the RPC library using CMake (native target)
# Run this from the rpc_lib directory
#!/bin/bash
set -e  # Exit immediately if a command exits with a non-zero status

rm -rf build      

mkdir build && cd build

cmake \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=../native-install \
  ..

cmake --build . --target install
