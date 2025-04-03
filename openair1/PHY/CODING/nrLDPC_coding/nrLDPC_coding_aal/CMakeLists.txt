##########################################################

# LDPC offload library - ORAN-AAL
##########################################################

add_boolean_option(ENABLE_LDPC_AAL OFF "Build support for LDPC Offload using O-RAN AAL" OFF)

if (ENABLE_LDPC_AAL)

  pkg_check_modules(LIBDPDK REQUIRED libdpdk>=20.11)
  message(STATUS "libdpdk found in: ${LIBDPDK_LIBRARY_DIRS}")
  message(STATUS "libdpdk include path: ${LIBDPDK_INCLUDE_DIRS}")
  message(STATUS "libdpdk version: ${LIBDPDK_VERSION}")

  add_library(ldpc_aal MODULE nrLDPC_coding_aal.c)

  set_target_properties(ldpc_aal PROPERTIES COMPILE_FLAGS "-DALLOW_EXPERIMENTAL_API")
  set_target_properties(ldpc_aal PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
  target_link_libraries(ldpc_aal PRIVATE ldpc_gen_HEADERS ${LIBDPDK_LDFLAGS})
  
  add_dependencies(nr-softmodem ldpc_aal)
  add_dependencies(nr-uesoftmodem ldpc_aal)
  add_dependencies(nr_ulsim ldpc_aal)
  add_dependencies(nr_ulschsim ldpc_aal)
  add_dependencies(nr_dlsim ldpc_aal)
  add_dependencies(nr_dlschsim ldpc_aal)

endif()
