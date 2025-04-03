add_subdirectory(MESSAGES)
add_library(xnap xnap_common.c
	    xnap_gNB_task.c
	    xnap_gNB_management_procedures.c)
target_link_libraries(xnap
                        PUBLIC asn1_xnap
                        PRIVATE nr_rrc)
target_include_directories(xnap PUBLIC ${CMAKE_CURRENT_DIR})

