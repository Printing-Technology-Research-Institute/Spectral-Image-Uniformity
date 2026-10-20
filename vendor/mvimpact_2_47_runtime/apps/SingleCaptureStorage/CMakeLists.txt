set(TARGET SingleCaptureStorage)

add_executable(${TARGET} ${TARGET}.cpp)
set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C++")

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})