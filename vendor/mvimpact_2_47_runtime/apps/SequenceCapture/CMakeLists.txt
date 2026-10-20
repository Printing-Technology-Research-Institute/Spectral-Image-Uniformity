set(TARGET SequenceCapture)

mv_add_example_depending_on_cpp_standard(${TARGET} win32)

target_sources(${TARGET} PRIVATE
    ../Common/aviexception.h
    ../Common/avihelper.h
    ../Common/avihelper.cpp
    ../Common/aviwrapper.h
    ../Common/aviwrapper.cpp
)

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})
if(WIN32)
    target_link_libraries(${TARGET} PRIVATE ${MVIMPACT_ACQUIRE_VFW32_LIB})
endif()