set(TARGET CaptureToUserMemory_C)

add_executable(${TARGET} "")
set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C")

target_sources(${TARGET} PRIVATE
    ../Common/exampleHelper_C.h
    ../Common/exampleHelper_C.c
    CaptureToUserMemory.c
)

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})
if(CMAKE_COMPILE_FOR_ANDROID AND (ANDROID_NATIVE_API_LEVEL GREATER_EQUAL 26))
    # this is a little weird, but when not specifying this library and using clang to build for ANDROID API LEVEL 26 or greater you will end up with undefined references in C applications
    target_link_libraries(${TARGET} PRIVATE supc++)
endif()