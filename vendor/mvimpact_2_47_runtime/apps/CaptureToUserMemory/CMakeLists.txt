set(TARGET CaptureToUserMemory)

mv_add_example_depending_on_cpp_standard(${TARGET})

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})
# pthread and rt are part of the bionic in Android and don't exist as standalone libraries
if(UNIX AND NOT CMAKE_COMPILE_FOR_ANDROID)
    target_link_libraries(${TARGET} PRIVATE pthread)
endif()