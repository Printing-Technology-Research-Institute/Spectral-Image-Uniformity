set(TARGET wxPropView)

if(WIN32)
    set(PLATFORM_SPECIFIC_SOURCES
        ../../win32/ImageCanvas.cpp
    )
elseif(UNIX)
    set(PLATFORM_SPECIFIC_SOURCES
        ../../ImageCanvas.cpp
    )
endif()

add_executable(${TARGET}
    $<TARGET_OBJECTS:wxPropView.Objects>
    ../Common.postTarget.cmake
    ${PLATFORM_SPECIFIC_SOURCES}
)

if(WIN32)
    target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDisplay)
endif()
include(../../../../CMake/openMP.compile.cmake)
include(../Common.postTarget.cmake)