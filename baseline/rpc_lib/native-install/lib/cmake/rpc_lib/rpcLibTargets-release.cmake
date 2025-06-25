#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "rpc_lib::rpc_lib" for configuration "Release"
set_property(TARGET rpc_lib::rpc_lib APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(rpc_lib::rpc_lib PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/librpc_lib.a"
  )

list(APPEND _cmake_import_check_targets rpc_lib::rpc_lib )
list(APPEND _cmake_import_check_files_for_rpc_lib::rpc_lib "${_IMPORT_PREFIX}/lib/librpc_lib.a" )

# Import target "rpc_lib::rpc_primitives" for configuration "Release"
set_property(TARGET rpc_lib::rpc_primitives APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(rpc_lib::rpc_primitives PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/librpc_primitives.a"
  )

list(APPEND _cmake_import_check_targets rpc_lib::rpc_primitives )
list(APPEND _cmake_import_check_files_for_rpc_lib::rpc_primitives "${_IMPORT_PREFIX}/lib/librpc_primitives.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
