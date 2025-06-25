cmake_minimum_required(VERSION 3.8)
project(MultiHostRPC C CXX)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_DEBUG "-g")


# --------------------------------------------------
# Shared source files
# --------------------------------------------------
file(GLOB_RECURSE RPC_SOURCES "src/rpc/*.cpp")
file(GLOB_RECURSE WAMR_SOURCES "src/wamr/*.cpp")
file(GLOB_RECURSE WASM_SOURCES "src/wasm/*.cpp")
file(GLOB_RECURSE MESSAGE_SOURCES "src/ring_buffer_rpc/*.cpp")

set(SHARED_SOURCES
  ${RPC_SOURCES}
  ${MESSAGE_SOURCES}
  ${WAMR_SOURCES}
  ${WASM_SOURCES}
)

# --------------------------------------------------
# Add host executables
# --------------------------------------------------

set(HOST_NAMES client_host server_host rpc_host)

foreach(HOST ${HOST_NAMES})
  add_executable(${HOST} src/hosts/${HOST}.cpp ${SHARED_SOURCES})

  target_include_directories(${HOST} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ring_buffer_rpc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rpc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wamr"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/wasm"
    "${wamr_ext_SOURCE_DIR}/core/iwasm/include"
    "${IMPORTS_DIR}"
  )

  target_link_libraries(${HOST}
    wamrlib
    pthread
    m
    dl
  )

  # to ensure generated imports.h is available (can remove if do extern "C" in wasm files)
  add_dependencies(${HOST}  wasm)

  install(TARGETS ${HOST}
    RUNTIME DESTINATION bin
  )
endforeach()

# --------------------------------------------------
# Add WAMR support
# --------------------------------------------------

set(WAMR_BUILD_PLATFORM "linux")
set(WAMR_BUILD_TARGET X86_64)
set(WAMR_BUILD_SPEC_TEST 0)
set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_AOT 1)
set(WAMR_BUILD_JIT 0)
set(WAMR_BUILD_LAZY_JIT 0)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
set(WAMR_BUILD_LIB_PTHREAD 0)
set(WAMR_BUILD_REF_TYPES 1)

include(FetchContent)
FetchContent_Declare(
    wamr_ext
    GIT_REPOSITORY "https://github.com/bytecodealliance/wasm-micro-runtime"
    GIT_TAG "4e50d2191ca8f177ad03a9d80eebc44b59a932db"
)
FetchContent_MakeAvailable(wamr_ext)

FetchContent_GetProperties(wamr_ext SOURCE_DIR WAMR_ROOT_DIR)
message(STATUS WAMR_ROOT_DIR ${WAMR_ROOT_DIR})

include (${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
add_library(wamrlib "${WAMR_RUNTIME_LIB_SOURCE}")
target_include_directories(wamrlib PUBLIC ${PLATFORM_SHARED_DIR})
