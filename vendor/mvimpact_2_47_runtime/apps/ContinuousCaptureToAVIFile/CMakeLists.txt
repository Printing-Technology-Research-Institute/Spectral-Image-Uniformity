set(TARGET ContinuousCaptureToAVIFile)

add_executable(${TARGET} "")
set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C++")

target_sources(${TARGET} PRIVATE
    ../Common/aviexception.h
    ../Common/avihelper.h
    ../Common/avihelper.cpp
    ../Common/aviwrapper.h
    ../Common/aviwrapper.cpp
    ${TARGET}.cpp
)

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})
if(WIN32)
    target_link_libraries(${TARGET} PRIVATE ${MVIMPACT_ACQUIRE_VFW32_LIB})
endif()