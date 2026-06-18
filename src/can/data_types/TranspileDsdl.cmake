# Convert DSDL data structures to C headers and include them
function(transpile_dsdl)
  # Most of this comes from https://github.com/OpenCyphal-Garage/demos

  # Folder with DSDL data type definitions
  set(DSDL_DIR "${CMAKE_SOURCE_DIR}/src/can/data_types")

  function(resolve_dsdl_namespace input_path output_var)
    if(IS_DIRECTORY "${input_path}")
      set(${output_var}
          "${input_path}"
          PARENT_SCOPE
          )
      return()
    endif()

    file(READ "${input_path}" linked_path)
    string(STRIP "${linked_path}" linked_path)
    cmake_path(ABSOLUTE_PATH linked_path BASE_DIRECTORY "${DSDL_DIR}" NORMALIZE)
    string(REGEX REPLACE "[/\\]+$" "" linked_path "${linked_path}")
    set(${output_var}
        "${linked_path}"
        PARENT_SCOPE
        )
  endfunction()

  # Output directory for transpiled C headers.
  set(TRANSPILED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/include/transpiled/")

  # Transpile DSDL into C using Nunavut.
  find_package(nnvg REQUIRED)
  create_dsdl_target(
    # Generate the support library for generated C headers, which is "nunavut.h".
    "nunavut_support"
    c
    "${CMAKE_SOURCE_DIR}/src/can/nunavut_c_templates" # Use our own templates, we have adjusted them
    ${TRANSPILED_INCLUDE_DIR}
    ""
    OFF
    little
    "only"
    )
  resolve_dsdl_namespace("${DSDL_DIR}/uavcan" DSDL_UAVCAN_DIR)
  resolve_dsdl_namespace("${DSDL_DIR}/prusa3d" DSDL_PRUSA3D_DIR)
  set(dsdl_root_namespace_dirs # List all DSDL root namespaces to transpile here.
      ${DSDL_UAVCAN_DIR}
      # Do not use reg types: ${DSDL_DIR}/reg
      ${DSDL_PRUSA3D_DIR}
      )
  foreach(ns_dir ${dsdl_root_namespace_dirs})
    get_filename_component(ns ${ns_dir} NAME)
    message(STATUS "DSDL namespace ${ns} at ${ns_dir}")
    create_dsdl_target(
      "dsdl_${ns}" # CMake target name
      c # Target language to transpile into
      "${CMAKE_SOURCE_DIR}/src/can/nunavut_c_templates" # Use our own templates, we have adjusted
      ${TRANSPILED_INCLUDE_DIR} # Destination directory (add it to the includes)
      ${ns_dir} # Source directory
      OFF # Disable variable array capacity override
      little # Endianness of the target platform (alternatives: "big", "any")
      "never" # Support files are generated once in the nunavut_support target (above)
      ${dsdl_root_namespace_dirs} # Look-up DSDL namespaces
      )
    target_link_libraries("dsdl_${ns}" INTERFACE nunavut_support)
  endforeach()
endfunction()
