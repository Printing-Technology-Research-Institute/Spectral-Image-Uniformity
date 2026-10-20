set(TARGET ContinuousCaptureAllFormats)

mv_add_example_depending_on_cpp_standard(${TARGET} win32)

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})