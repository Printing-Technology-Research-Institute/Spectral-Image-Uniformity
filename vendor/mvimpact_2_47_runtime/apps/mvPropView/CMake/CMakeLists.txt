set(TARGET wxPropView.Objects)

if(WIN32)
    set(PLATFORM_SPECIFIC_HEADERS
        ../../Common/win32/ProxyResolver.h
        ../resource.h
    )
    set(RESOURCE_FILES
        ../wxPropView.ico
        ../mvPropView.rc
    )
    source_group("Resource Files" FILES ${RESOURCE_FILES})
    set(PLATFORM_SPECIFIC_SOURCES
        ../../Common/win32/ProxyResolver.cpp
        ../../Common/win32/ProxyResolverContext.cpp
    )
elseif(UNIX)
    set(PLATFORM_SPECIFIC_SOURCES
        ../../Common/linux/ProxyResolverContext.cpp
    )
endif()

set(HEADERS
    ../../Common/CommonGUIFunctions.h
    ../../Common/wxAbstraction.h
    ../CaptureThread.h
    ../DataConversion.h
    ../DataGrid.h
    ../DevCtrl.h
    ../DevData.h
    ../DrawingCanvas.h
    ../GlobalDataStorage.h
    ../HistogramCanvas.h
    ../HistogramCanvasPixel.h
    ../HistogramCanvasSpatialNoise.h
    ../HistogramCanvasTemporalNoise.h
    ${CMAKE_CURRENT_LIST_DIR}/../icons.h
    ../ImageAnalysisPlotControls.h
    ../ImageCanvas.h
    ../LineProfileCanvas.h
    ../LineProfileCanvasHorizontal.h
    ../LineProfileCanvasVertical.h
    ../PlotCanvas.h
    ../PlotCanvasImageAnalysis.h
    ../PlotCanvasInfo.h
    ../PlotCanvasIntensity.h
    ../PropData.h
    ../PropGridFrameBase.h
    ../PropTree.h
    ../PropViewCallback.h
    ../PropViewFrame.h
    ../../Common/ProxyResolverContext.h
    ../resource.h
    ../spinctld.h
    ../SpinEditorDouble.h
    ../thingdef.h
    ../ValuesFromUserDlg.h
    ../VectorScopeCanvas.h
    ../WizardAOI.h
    ../WizardIcons.h
    ../WizardLensControl.h
    ../WizardLUTControl.h
    ../WizardMultiAOI.h
    ../WizardMultiCoreAcquisition.h
    ../WizardQuickSetup.h
    ../WizardSaturation.h
    ../WizardSequencerControl.h
    ${PLATFORM_SPECIFIC_HEADERS}
)
set_source_files_properties(../icons.h PROPERTIES GENERATED TRUE)

set(SOURCES
    ../../Common/CommonGUIFunctions.cpp
    ../CaptureThread.cpp
    ../DataConversion.cpp
    ../DataGrid.cpp
    ../DevCtrl.cpp
    ../DevData.cpp
    ../DrawingCanvas.cpp
    ../GlobalDataStorage.cpp
    ../HistogramCanvas.cpp
    ../HistogramCanvasPixel.cpp
    ../HistogramCanvasSpatialNoise.cpp
    ../HistogramCanvasTemporalNoise.cpp
    ../ImageAnalysisPlotControls.cpp
    ../ImageCanvasCommon.cpp
    ../LineProfileCanvas.cpp
    ../LineProfileCanvasHorizontal.cpp
    ../LineProfileCanvasVertical.cpp
    ../PlotCanvas.cpp
    ../PlotCanvasImageAnalysis.cpp
    ../PlotCanvasInfo.cpp
    ../PlotCanvasIntensity.cpp
    ../PropData.cpp
    ../PropGridFrameBase.cpp
    ../PropTree.cpp
    ../PropViewCallback.cpp
    ../PropViewFrame.cpp
    ../spinctld.cpp
    ../SpinEditorDouble.cpp
    ../ValuesFromUserDlg.cpp
    ../VectorScopeCanvas.cpp
    ../WizardAOI.cpp
    ../WizardLensControl.cpp
    ../WizardLUTControl.cpp
    ../WizardMultiAOI.cpp
    ../WizardMultiCoreAcquisition.cpp
    ../WizardQuickSetup.cpp
    ../WizardSaturation.cpp
    ../WizardSequencerControl.cpp
    ${PLATFORM_SPECIFIC_SOURCES}
)

add_library(${TARGET} OBJECT
    ${HEADERS}
    ${SOURCES}
    ${RESOURCE_FILES}
)
set_target_properties(${TARGET} PROPERTIES FOLDER "Applications(GUI)/wxPropView")
set_target_properties(${TARGET} PROPERTIES PROJECT_LABEL "wxPropView(Objects)")
add_dependencies(${TARGET} mvIMPACT_Acquire::mvDeviceManager)
if(MVIMPACT_ACQUIRE_BUILD)
    add_dependencies(${TARGET} wxPropView_IconsToHeader wxPropView_SplashScreenToHeader mvIMPACT_Acquire_GenICam_Interface)
endif()

target_include_directories(${TARGET} PRIVATE ${mvIMPACT_Acquire_INCLUDE_DIRS})
include(../../../CMake/wxWidgets.includes.cmake)
if(WIN32)
    target_compile_definitions(${TARGET} PRIVATE BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL)
endif()
target_compile_definitions(${TARGET} PRIVATE BUILD_WITH_UPDATE_DIALOG_SUPPORT)
include(../../../CMake/openMP.compile.cmake)
include(../../../CMake/wxWidgets.compile.cmake)