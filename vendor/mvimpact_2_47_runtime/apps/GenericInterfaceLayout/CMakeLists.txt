set(TARGET GenericInterfaceLayout)

mv_add_example_depending_on_cpp_standard(${TARGET})

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})