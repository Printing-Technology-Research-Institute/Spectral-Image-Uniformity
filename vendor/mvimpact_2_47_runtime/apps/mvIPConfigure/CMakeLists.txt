set(TARGET mvIPConfigure)

if(WIN32)
    set(PLATFORM_SPECIFIC_HEADERS
        resource.h
    )

    set(RESOURCE_FILES_AUTO_GENERATED
        error_icon.xpm
        ok_icon.xpm
    )
    set_source_files_properties(${RESOURCE_FILES_AUTO_GENERATED} PROPERTIES GENERATED TRUE)
    source_group("Resource Files" FILES ${RESOURCE_FILES_AUTO_GENERATED})

    set(RESOURCE_FILES
        ${RESOURCE_FILES_AUTO_GENERATED}
        ${TARGET}.ico
        ${TARGET}.icon.xpm
        ${TARGET}.rc
    )
    source_group("Resource Files" FILES ${RESOURCE_FILES})
endif()

set(SOURCES
    ../Common/CommonGUIFunctions.cpp
    AssignIPDlg.cpp
    CustomValidators.cpp
    DeviceListCtrl.cpp
    IPConfigureFrame.cpp
    SetupUnicastDlg.cpp
)

set(HEADERS
    ../Common/CommonGUIFunctions.h
    AssignIPDlg.h
    CustomValidators.h
    DeviceListCtrl.h
    IPConfigureFrame.h
    TLILibImports.h
    SetupUnicastDlg.h
    ${PLATFORM_SPECIFIC_HEADERS}
)

add_executable(${TARGET}
    ${HEADERS}
    ${SOURCES}
    ${RESOURCE_FILES}
)
set_target_properties(${TARGET} PROPERTIES FOLDER "Applications(GUI)/mvIPConfigure")
if(MVIMPACT_ACQUIRE_BUILD)
    add_dependencies(${TARGET} mvIPConfigure_IconsToHeader mvIPConfigure_SplashScreenToHeader)
endif()

include(../../CMake/wxWidgets.includes.cmake)
include(../../CMake/wxWidgets.compile.cmake)

target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDeviceManager)
if(WIN32)
    target_link_libraries(${TARGET} PRIVATE comctl32.lib rpcrt4.lib Ws2_32.lib)
endif()
include(../../CMake/wxWidgets.link.cmake)