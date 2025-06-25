cmake_minimum_required(VERSION 3.15)
project(rpc_lib LANGUAGES C CXX)

# 1) Fetch nanopb runtime (no generator)
include(FetchContent)
set(nanopb_BUILD_GENERATOR OFF CACHE BOOL "" FORCE)
set(nanopb_BUILD_RUNTIME   ON  CACHE BOOL "" FORCE)
FetchContent_Declare(
  nanopb
  GIT_REPOSITORY https://github.com/nanopb/nanopb.git
  GIT_TAG        0.4.9
)
FetchContent_MakeAvailable(nanopb)
add_library(nanopb::nanopb ALIAS protobuf-nanopb-static)

# -----------------------------------------------------------------
# protoc
# -----------------------------------------------------------------
set(VENV_DIR "${CMAKE_BINARY_DIR}/nanopb_venv")
set(VENV_PYTHON "${VENV_DIR}/bin/python")
set(VENV_CREATED_MARKER "${VENV_DIR}/.created")

# Only create the venv + install protobuf once
# add_custom_command(
#   OUTPUT "${VENV_CREATED_MARKER}"
#   COMMAND python3 -m venv "${VENV_DIR}"
#   COMMAND "${VENV_PYTHON}" -m pip install --upgrade pip
#   COMMAND "${VENV_PYTHON}" -m pip install protobuf==3.20.3
#   COMMAND ${CMAKE_COMMAND} -E touch "${VENV_CREATED_MARKER}"
#   COMMENT "Creating temporary virtual environment and installing protobuf==3.20.3"
# )
add_custom_command(
  OUTPUT "${VENV_CREATED_MARKER}"
  COMMAND python3 -m venv "${VENV_DIR}"
  COMMAND "${VENV_PYTHON}" -m pip install --upgrade pip
  COMMAND env CC=/usr/bin/clang CXX=/usr/bin/clang++ "${VENV_PYTHON}" -m pip install --no-binary=protobuf protobuf==3.20.3 grpcio-tools
  # Regenerate nanopb_pb2.py inside the correct protobuf runtime
  COMMAND "${VENV_PYTHON}" -m grpc_tools.protoc
          --proto_path="${nanopb_SOURCE_DIR}/generator/proto"
          --python_out="${nanopb_SOURCE_DIR}/generator/proto"
          "${nanopb_SOURCE_DIR}/generator/proto/nanopb.proto"
  COMMAND ${CMAKE_COMMAND} -E touch "${VENV_CREATED_MARKER}"
  COMMENT "Creating venv, installing protobuf from source, and regenerating nanopb_pb2.py"
)



add_custom_target(nano_venv DEPENDS "${VENV_CREATED_MARKER}")

# Path to your .proto files
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")

find_program(SYSTEM_PROTOC
  NAMES protoc
  PATHS "/home/eb/fyp/protobuf-older/protobuf/build"
  NO_DEFAULT_PATH
)
message(STATUS "Using protoc: ${SYSTEM_PROTOC}")

if(NOT SYSTEM_PROTOC)
  message(FATAL_ERROR "protoc not found; please install protobuf-compiler")
endif()

# # Wrapper script so protoc finds the generator in the venv
# set(WRAP "${CMAKE_BINARY_DIR}/protoc-gen-nanopb")
# file(WRITE  "${WRAP}"
#   "#!/usr/bin/env bash\nexec \"${VENV}/bin/python\" \"${nanopb_SOURCE_DIR}/generator/nanopb_generator.py\" --protoc-plugin \"$@\"")
# file(CHMOD "${WRAP}" PERMISSIONS
#      OWNER_READ OWNER_WRITE OWNER_EXECUTE
#      GROUP_READ GROUP_EXECUTE
#      WORLD_READ WORLD_EXECUTE)
set(WRAP "${CMAKE_BINARY_DIR}/protoc-gen-nanopb")
file(WRITE "${WRAP}"
  "#!/usr/bin/env bash\n"
  "echo Using Python: ${VENV_PYTHON} >&2\n"
  "exec \"${VENV_PYTHON}\" \"${nanopb_SOURCE_DIR}/generator/nanopb_generator.py\" --protoc-plugin \"$@\""
)

# file(WRITE "${WRAP}"
#   "#!/usr/bin/env bash\n"
#   "exec \"${VENV_PYTHON}\" \"${nanopb_SOURCE_DIR}/generator/nanopb_generator.py\" --protoc-plugin \"$@\""
# )
file(CHMOD "${WRAP}" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE
  GROUP_READ GROUP_EXECUTE
  WORLD_READ WORLD_EXECUTE)



set(GEN "${CMAKE_BINARY_DIR}/generated_nano")
file(MAKE_DIRECTORY "${GEN}")

# generate rpc_envelope.pb.{c,h} ----------
add_custom_command(
    OUTPUT  "${GEN}/rpc_envelope.pb.c" "${GEN}/rpc_envelope.pb.h"
    WORKING_DIRECTORY "${PROTO_DIR}" 
    COMMAND "${SYSTEM_PROTOC}"
            --plugin=protoc-gen-nanopb=${WRAP}
            --proto_path="${PROTO_DIR}"
            --proto_path="${nanopb_SOURCE_DIR}/generator/proto"
            --nanopb_out="${GEN}"
            "rpc_envelope.proto"
    DEPENDS
        "${PROTO_DIR}/rpc_envelope.proto"
        "${PROTO_DIR}/rpc_envelope.options"
        nano_venv)


add_custom_target(generate_protos
  DEPENDS
    "${GEN}/rpc_envelope.pb.c"
    "${GEN}/rpc_envelope.pb.h"
)

# -------------------------------

# 2) Build the RPC library
# add_library(rpc_lib STATIC
#   # your implementation
#   src/rpc_client.cpp
#   src/rpc_server.cpp

#   # the pregenerated envelope/response stubs
#   src/generated_nano/rpc_envelope.pb.c
# )
add_library(rpc_lib STATIC
  src/rpc_client.cpp
  src/rpc_server.cpp
  "${GEN}/rpc_envelope.pb.c"
)

add_dependencies(rpc_lib generate_protos)

# 3) Public include dirs
target_include_directories(rpc_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    # $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/generated_nano>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated_nano>
    $<INSTALL_INTERFACE:include/rpc_lib>

)

# 4) Link in nanopb runtime
target_link_libraries(rpc_lib PUBLIC nanopb::nanopb)

# 5) Force WASI flags (assuming you pass in your toolchain file at configure)
target_compile_options(rpc_lib
  PUBLIC
    --target=wasm32-wasi
    -fno-exceptions
    -fno-rtti
)
# target_compile_definitions(rpc_lib PUBLIC PB_FIELD_32BIT)

set_target_properties(rpc_lib PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PUBLIC_HEADER "include/rpc_client.h;include/rpc_server.h"
)

# 6) Install/export targets so users can find_package(rpc_lib)
include(GNUInstallDirs)
install(TARGETS rpc_lib
  EXPORT rpcLibTargets
  ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rpc_lib
)

install(
  DIRECTORY "${CMAKE_BINARY_DIR}/generated_nano/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/rpc_lib"
  FILES_MATCHING PATTERN "*.h"
)


install(EXPORT rpcLibTargets
  NAMESPACE rpc_lib::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rpc_lib
)

# Optionally provide a config file so downstream can simply do:
#   find_package(rpc_lib CONFIG REQUIRED)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/rpc_libConfigVersion.cmake"
  VERSION 1.0.0
  COMPATIBILITY AnyNewerVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindRpcLib.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/rpc_libConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rpc_lib
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/rpc_libConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/rpc_libConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rpc_lib
)
