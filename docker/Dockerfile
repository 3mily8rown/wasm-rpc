FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y time

RUN apt update && \
    apt install -y llvm-16 libllvm16 clang-16 lldb-16 lld-16 \
                   libzstd1 libzstd-dev libedit2 libedit-dev \
                   curl ca-certificates && \
    ln -s /usr/bin/llvm-config-16 /usr/bin/llvm-config


RUN apt update && \
    apt install -y git build-essential cmake clang && \
    git clone https://github.com/bytecodealliance/wasm-micro-runtime.git && \
    cd wasm-micro-runtime/product-mini/platforms/linux && \
    make wamrc && \
    cp build/wamrc /usr/local/bin/

ENV LLVM_DIR=/usr/lib/llvm-16/lib/cmake/llvm
ENV LD_LIBRARY_PATH="/usr/lib/llvm-16/lib:${LD_LIBRARY_PATH}"

WORKDIR /app
