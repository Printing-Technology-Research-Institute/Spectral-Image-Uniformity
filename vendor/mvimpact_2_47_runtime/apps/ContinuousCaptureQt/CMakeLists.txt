set(TARGET ContinuousCaptureQt)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
SET(CMAKE_AUTORCC ON)

if(WIN32)
    set(PLATFORM_SPECIFIC_SOURCES
        win32/ImageCanvas.cpp
    )
    set(PLATFORM_SPECIFIC_RESOURCES
        ${TARGET}.rc
    )
else()
    set(PLATFORM_SPECIFIC_SOURCES
        ImageCanvas.cpp
    )
endif()

set(RESOURCE_FILES
    bulb-off.png
    bulb-on.png
    ../Common/mv.ico
    ${TARGET}.qrc
    ${PLATFORM_SPECIFIC_RESOURCES}
)
source_group("Resource Files" FILES ${RESOURCE_FILES})

set(QT_DESIGNER_FILES
    ${TARGET}.ui
)
source_group("Qt Designer Files" FILES ${QT_DESIGNER_FILES})

set(SOURCES
    CaptureThread.cpp
    main.cpp
    ContinuousCaptureQt.cpp
    ${PLATFORM_SPECIFIC_SOURCES}
)

set(HEADERS
    CaptureThread.h
    ImageCanvas.h
    ContinuousCaptureQt.h
)

add_executable(${TARGET}
    ${HEADERS}
    ${QT_DESIGNER_FILES}
    ${RESOURCE_FILES}
    ${SOURCES}
)
set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C++")
mv_enable_and_enforce_cpp_standard_version_if_supported(${TARGET} 11)

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:\"mainCRTStartup\" /SUBSYSTEM:WINDOWS")
endif()

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES} Qt5::Widgets)

if(WIN32 AND MVIMPACT_ACQUIRE_BUILD) # todo: QT_DIR is not set if building the examples only!
    add_custom_command(TARGET ${TARGET}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy ${QT_DIR}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/platforms
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
