if(NOT MVIMPACT_ACQUIRE_BUILD)
    cmake_minimum_required(VERSION 3.8)

    # This will allow to create sub-folders e.g. within a Visual Studio solution
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Project definition and configuration
    project(mvIMPACT_Acquire.Examples LANGUAGES C CXX)

    # Policies
    cmake_policy(SET CMP0028 NEW) # Double colon in target name means ALIAS or IMPORTED target

    include(../mvIMPACT_AcquireConfig.cmake)

    # Direct-X support
    if(WIN32)
        if(EXISTS $ENV{DXSDK_DIR})
            set(DXSDK_DIR $ENV{DXSDK_DIR})
        else()
            message(STATUS "The environment variable 'DXSDK_DIR' is not defined. Project files for applications using DirectX will not be generated! To build these files define this environment variable, let it point to the root directory of the DirectX SDK and re-run CMake!")
        endif()
    endif()

    # Qt support
    include(../CMake/qt.detection.cmake)

    # 'Video For Windows' support for the 64-bit Borland compiler works slightly different
    if(BORLAND AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
        find_path(BORLAND_VFW32_LIB_DIR
            NAMES vfw32.a
            PATHS ${linker_Path1} ${linker_Path2} ${linker_Path3}
            DOC "Location of the Borland SDK 64-bit import library of the vfw32.lib"
        )
        if(${BORLAND_VFW32_LIB_DIR} STREQUAL "BORLAND_VFW32_LIB_DIR-NOTFOUND")
            message(STATUS "${BORLAND_VFW32_LIB_DIR} Could not find vfw32.a needed to build example applications using the 'Video For Windows' API with a 64-bit Borland compiler. These example applications will not be available. Locations searched: '${linker_Path1}', '${linker_Path2}', '${linker_Path3}'")
            set(MVIMPACT_ACQUIRE_VFW32_LIB "VFW32_LIB_DIR-NOTFOUND")
        else()
            set(MVIMPACT_ACQUIRE_VFW32_LIB "${BORLAND_VFW32_LIB_DIR}/vfw32.a")
        endif()
    else()
        set(MVIMPACT_ACQUIRE_VFW32_LIB "Vfw32.lib")
    endif()

    # wxWidgets support
    if(EXISTS $ENV{WX_WIDGETS_DIR})
        set(WX_WIDGETS_DIR $ENV{WX_WIDGETS_DIR})
        message(STATUS "Using wxWidgets from environment variable WX_WIDGETS_DIR: '${WX_WIDGETS_DIR}'")
    else()
        include(../CMake/wxWidgets.detection.cmake)
    endif()

    # Output directory overwrites
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    if(WIN32)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    elseif(UNIX)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    endif()

    set(MVIMPACT_ACQUIRE_GENERATE_EXAMPLE_APPLICATIONS ON)

    # Detect which C++ standard version is supported by the used compiler
    include(../CMake/cpp.standard.detection.cmake)
endif()

function(mv_add_example_depending_on_cpp_standard TARGET)
    #message(STATUS "Generating code for '${TARGET}' for c++${CPP_STANDARD_VERSION}")
    if(CPP_STANDARD_VERSION GREATER_EQUAL 11)
        add_executable(${TARGET} ${TARGET}.cpp)
        # The requested version of the c++ standard IS supported, thus we could also use the 'mv_enable_and_enforce_cpp_standard_version' function
        # instead but then the status message would be misleading since the c++ standard is NOT a hard requirement here!
        mv_enable_and_enforce_cpp_standard_version_if_supported(${TARGET} 11)
    else()
        if(ARGC GREATER 1)
            add_executable(${TARGET} ${TARGET}.${ARGV1}.cpp)
        else()
            add_executable(${TARGET} ${TARGET}.legacy.cpp)
        endif()
    endif()
    set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C++${CPP_STANDARD_VERSION}")
endfunction()

# Directories to process
if(MVIMPACT_ACQUIRE_GENERATE_EXAMPLE_APPLICATIONS)
    # 'C' examples
    if(NOT CMAKE_COMPILE_FOR_ANDROID OR (CMAKE_COMPILE_FOR_ANDROID AND (ANDROID_NATIVE_API_LEVEL GREATER_EQUAL 26))) # supported since version 26
        add_subdirectory(Callback_C)
        add_subdirectory(CaptureToUserMemory_C)
        add_subdirectory(ContinuousCapture_C)
        add_subdirectory(ContinuousCaptureGenICam_C)
    endif()
    # 'C++' examples
    add_subdirectory(Callback)
    add_subdirectory(CaptureToUserMemory)
    add_subdirectory(ContinuousCapture)
    add_subdirectory(ContinuousCaptureAllDevices)
    add_subdirectory(ContinuousCaptureMultiPart)
    add_subdirectory(DigitalIOs)
    add_subdirectory(GenericInterfaceLayout)
    add_subdirectory(GenICamCommonSettingsUsage)
    if(CPP_STANDARD_VERSION GREATER_EQUAL 11)
        add_subdirectory(GenICamI2cUsage)
        add_subdirectory(TimestampFeatures)
    endif()
    add_subdirectory(GenICamInterfaceLayout)
    add_subdirectory(GenICamSequencerParameterChangeAtRuntime)
    add_subdirectory(GenICamSequencerUsage)
    add_subdirectory(GenICamSequencerUsageWithPaths)
    add_subdirectory(GenICamSmartFrameRecallUsage)
    add_subdirectory(Properties)
    add_subdirectory(SingleCapture)
    add_subdirectory(SingleCaptureStorage)
    if(WIN32 OR (CPP_STANDARD_VERSION GREATER_EQUAL 11))
        add_subdirectory(CameraDescriptions)
        add_subdirectory(ContinuousCaptureMultipleInputs)
        add_subdirectory(ContinuousCaptureMultipleVideoSignals)
        add_subdirectory(ContinuousCaptureOnlyProcessLatest)
        add_subdirectory(SingleCaptureMasterSlave)
    endif()
    if(WIN32)
        add_subdirectory(ContinuousCaptureAllFormats)
        if(DXSDK_DIR)
            add_subdirectory(ContinuousCaptureDirectX)
        endif()
        if(NOT (${MVIMPACT_ACQUIRE_VFW32_LIB} STREQUAL "VFW32_LIB_DIR-NOTFOUND"))
            add_subdirectory(ContinuousCaptureToAVIFile)
            add_subdirectory(SequenceCapture)
        endif()
    endif()
endif()
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross compilation! Will NOT build Qt or wxWidgets based applications as these packages are most certainly not part of the cross-toolchain. Adjust CMake scripts if needed and you know what you are doing and consider submitting a patch to us!")
else()
    if(Qt5Widgets_FOUND)
        add_subdirectory(ContinuousCaptureQt)
    endif()
    # Only try to build wxWidgets based applications if wxWidgets can be found on the system.
    if(WX_WIDGETS_DIR OR wxWidgets_FOUND)
        if(BORLAND)
            message(STATUS "Borland compiler detected! At least in our experience wxWidgets cannot be digested out of the box by this compiler. Adjust CMake scripts if needed and you know what you are doing and consider submitting a patch to us!")
        else()
            if(MVIMPACT_ACQUIRE_BUILD OR UNIX)
                add_subdirectory(mvDeviceConfigure)
                add_subdirectory(mvPropView/CMake/NoDisplayLib)
                add_subdirectory(mvPropView/CMake/NoDisplayLibNoRawBitmap)
            endif()
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mvIPConfigure)
                add_subdirectory(mvIPConfigure)
            endif()
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/mvPropView)
                add_subdirectory(mvPropView/CMake)
                if(WIN32)
                    add_subdirectory(mvPropView/CMake/Default)
                endif()
            endif()
        endif()
    endif()
endif()
