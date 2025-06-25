cmake_minimum_required(VERSION 3.15)
project(wasm_mod LANGUAGES C CXX)

# ---------- variables supplied by the super-build
foreach(var PROTO_DIR SCRIPT_DIR WAMRC_EXECUTABLE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "${var} not set")
    endif()
endforeach()

# 1) Bring in your already-built rpc_lib (which exports rpc_lib::rpc_lib)
#    Either by installed config or via add_subdirectory:
# Tell CMake to search this prefix for Config files
list(APPEND CMAKE_PREFIX_PATH "/home/eb/fyp/wasi-install")
# list(APPEND CMAKE_PREFIX_PATH "/path/to/where/you/installed/rpc_lib")
# 1) First, load nanopb so its imported targets exist
find_package(nanopb CONFIG REQUIRED)

# 2) Now load rpc_lib, which PUBLIC‐links nanopb
find_package(rpc_lib CONFIG REQUIRED)


# # 2) Bring in nanopb (so you can compile message.pb.c)
# find_package(nanopb REQUIRED)   # or reuse rpc_lib’s FetchContent invocation

# Python venv for pyhton ----------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(VENV "${CMAKE_BINARY_DIR}/nanopb_venv")
add_custom_command(
    OUTPUT  "${VENV}/.done"
    COMMAND "${Python3_EXECUTABLE}" -m venv "${VENV}"
    COMMAND "${VENV}/bin/pip" install --upgrade pip protobuf grpcio-tools
    COMMAND "${CMAKE_COMMAND}" -E touch "${VENV}/.done")
add_custom_target(nano_venv ALL DEPENDS "${VENV}/.done")


# client WASM module -----------------------------------------------
# 5) Client WASM module
add_executable(client_app src/client_app.cpp)
target_link_libraries(client_app
  PRIVATE
    rpc_lib::rpc_lib     # your RPC plumbing + nanopb runtime
)
target_include_directories(client_app PRIVATE
  "${CMAKE_SOURCE_DIR}/include"  # if your rpc_lib headers were installed here
  "${CMAKE_PREFIX_PATH}/include/rpc_lib"
)

target_link_options(client_app PRIVATE
    "-Wl,--export-all" "-Wl,--no-gc-sections" "-Wl,--allow-undefined")
target_compile_options(client_app
  PRIVATE
    --target=wasm32-wasi
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

set_target_properties(client_app PROPERTIES SUFFIX ".wasm")
install(TARGETS client_app DESTINATION wasm)

# client WASM module 2-----------------------------------------------
# 5) Client WASM module
add_executable(client_app2 src/client_app2.cpp)
target_link_libraries(client_app2
  PRIVATE
    rpc_lib::rpc_lib     # your RPC plumbing + nanopb runtime
)
target_include_directories(client_app2 PRIVATE
  "${CMAKE_SOURCE_DIR}/include"  # if your rpc_lib headers were installed here
  "${CMAKE_PREFIX_PATH}/include/rpc_lib"
)

target_link_options(client_app2 PRIVATE
    "-Wl,--export-all" "-Wl,--no-gc-sections" "-Wl,--allow-undefined")
target_compile_options(client_app2
  PRIVATE
    --target=wasm32-wasi
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

set_target_properties(client_app2 PROPERTIES SUFFIX ".wasm")
install(TARGETS client_app2 DESTINATION wasm)

# server WASM module ------------------------------------------
# 6) Server WASM module (same pattern)
add_executable(server_app src/server_app.cpp)
target_link_libraries(server_app
  PRIVATE
    rpc_lib::rpc_lib
)
target_include_directories(server_app PRIVATE
  "${CMAKE_SOURCE_DIR}/include"  # if your rpc_lib headers were installed here
  "${CMAKE_PREFIX_PATH}/include/rpc_lib"
)
target_link_options(server_app PRIVATE
  "-Wl,--export-all" "-Wl,--no-gc-sections" "-Wl,--allow-undefined")
target_compile_options(server_app
  PRIVATE
    --target=wasm32-wasi
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
)

set_target_properties(server_app PROPERTIES SUFFIX ".wasm")
install(TARGETS server_app DESTINATION wasm)


# 7) Generate import headers for WAMR
# ensure install‐tree exists
file(MAKE_DIRECTORY "${IMPORTS_DIR}")

# paths to the built .wasm files
set(CLIENT_WASM  "$<TARGET_FILE:client_app>")
set(CLIENT_WASM2  "$<TARGET_FILE:client_app2>")
set(SERVER_WASM  "$<TARGET_FILE:server_app>")

# target header paths
set(CLIENT_HDR  "${IMPORTS_DIR}/client_app_imports.h")
set(SERVER_HDR  "${IMPORTS_DIR}/server_app_imports.h")

# run import script once both .wasm exist
add_custom_command(
  OUTPUT  ${CLIENT_HDR} ${SERVER_HDR}
  COMMAND "${Python3_EXECUTABLE}"
          "${SCRIPT_DIR}/generate_wasm_imports.py"
            --module client_app    --native rpc/native_impl.h
            "${CLIENT_WASM}"       "${CLIENT_HDR}"
  COMMAND "${Python3_EXECUTABLE}"
          "${SCRIPT_DIR}/generate_wasm_imports.py"
            --module server_app    --native rpc/native_impl.h
            "${SERVER_WASM}"       "${SERVER_HDR}"
  DEPENDS client_app server_app
  COMMENT "⟳ Generating WAMR native import‐stub headers"
  VERBATIM
)

# make it part of the normal build
add_custom_target(generate_import_headers ALL
  DEPENDS ${CLIENT_HDR} ${SERVER_HDR}
)

# AOT output paths
set(CLIENT_AOT "${CMAKE_BINARY_DIR}/client_app.aot")
set(CLIENT2_AOT "${CMAKE_BINARY_DIR}/client_app2.aot")
set(SERVER_AOT "${CMAKE_BINARY_DIR}/server_app.aot")

# Compile .wasm to .aot
add_custom_command(
  OUTPUT ${CLIENT_AOT}
  COMMAND ${WAMRC_EXECUTABLE} -o ${CLIENT_AOT} $<TARGET_FILE:client_app>
  DEPENDS client_app
  COMMENT "Compiling client_app.wasm to AOT"
)

add_custom_command(
  OUTPUT ${CLIENT2_AOT}
  COMMAND ${WAMRC_EXECUTABLE} -o ${CLIENT2_AOT} $<TARGET_FILE:client_app2>
  DEPENDS client_app2
  COMMENT "Compiling client_app2.wasm to AOT"
)

add_custom_command(
  OUTPUT ${SERVER_AOT}
  COMMAND ${WAMRC_EXECUTABLE} -o ${SERVER_AOT} $<TARGET_FILE:server_app>
  DEPENDS server_app
  COMMENT "Compiling server_app.wasm to AOT"
)

# Group into an "aot_modules" target so it's built automatically
add_custom_target(aot_modules ALL
  DEPENDS ${CLIENT_AOT} ${CLIENT2_AOT} ${SERVER_AOT}
)

# Install the AOT files as well
install(FILES ${CLIENT_AOT} ${CLIENT2_AOT} ${SERVER_AOT} DESTINATION wasm)

