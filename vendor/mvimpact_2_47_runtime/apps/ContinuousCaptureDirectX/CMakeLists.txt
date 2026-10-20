set(TARGET ContinuousCaptureDirectX)

add_executable(${TARGET} "")
set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C++")

target_sources(${TARGET} PRIVATE
    CaptureParameters.h
    CaptureParameters.cpp
    resource.h
    ${TARGET}.cpp)

target_include_directories(${TARGET} PRIVATE ${DXSDK_DIR}/Include)

target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDeviceManager)
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(DXSDK_LIB_DIR ${DXSDK_DIR}/Lib/x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(DXSDK_LIB_DIR ${DXSDK_DIR}/Lib/x86)
    else()
        message(FATAL_ERROR "Help me if you can, I'm feeling down / And I do appreciate you being 'round / Help me get my feet back on the ground / Won't you please, please help me?")
    endif()
    target_link_libraries(${TARGET} PRIVATE ${DXSDK_LIB_DIR}/ddraw.lib ${DXSDK_LIB_DIR}/dxguid.lib)
endif()
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
endif()