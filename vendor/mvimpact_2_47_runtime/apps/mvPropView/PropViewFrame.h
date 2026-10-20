//-----------------------------------------------------------------------------
#ifndef PropViewFrameH
#define PropViewFrameH PropViewFrameH
//-----------------------------------------------------------------------------
#include "DevCtrl.h"
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include "HistogramCanvas.h"
#include "ImageAnalysisPlotControls.h"
#include "PlotCanvasInfo.h"
#include "PropGridFrameBase.h"
#include "PropViewCallback.h"
#include <set>
#include "ValuesFromUserDlg.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <apps/Common/wxIncludeEpilogue.h>

#ifdef _OPENMP
#   include <omp.h>
#endif // #ifdef _OPENMP

class DevicePropertyHandler;
class MethodExecutionThread;
class MonitorDisplay;
class WizardAOI;
class WizardColorCorrection;
class SetupRecording;
class WizardLensControl;
class WizardLUTControl;
class WizardMultiAOI;
class WizardMultiCoreAcquisition;
class WizardQuickSetup;

class wxNotebook;
class wxRichToolTip;
class wxSlider;
class wxSplitterEvent;
class wxSplitterWindow;
class wxSpinCtrl;

#define PLOT_WINDOW_ID_RANGE 0x100
#define BUILD_PLOT_WINDOW_START_ID(PLOT) wid##PLOT = ( iap##PLOT + 1 ) * PLOT_WINDOW_ID_RANGE

typedef std::vector<ImageCanvas*> DisplayWindowContainer;

//-----------------------------------------------------------------------------
class PropViewFrame : public PropGridFrameBase
//-----------------------------------------------------------------------------
{
public:
    explicit                            PropViewFrame( const wxString& title, const wxPoint& pos, const wxSize& size, int argc, wxChar** argv );
    virtual                            ~PropViewFrame();
    void                                ClearQuickSetupWizardPointer( void )
    {
        m_pQuickSetupDlgCurrent = 0;
    }
    void                                OnBeforeMultiAOIDialogDestruction( void )
    {
        m_pMultiAOIDlg = 0;
    }
    void                                OnBeforeAOIDialogDestruction( void )
    {
        m_pAOIDlg = 0;
    }
    bool                                DetailedInfosOnCallback( void ) const
    {
        return m_pOptionsDlg->GetMiscellaneousConfiguration()->IsChecked( OptionsDlg::mDisplayDetailedInformationOnCallbacks );
    }
    void                                DeviceRegistersValidChanged( void );
    void                                EndFullScreenMode( void );
    bool                                EnsureAcquisitionState( bool boAcquire );
    bool                                GetAutoShowQSWOnUseDevice( void )
    {
        return m_pOptionsDlg->GetShowQuickSetupOnDeviceOpenCheckBox()->IsChecked();
    }
    DisplayWindowContainer::size_type   GetDisplayCount( void ) const
    {
        return m_DisplayAreas.size();
    }
    bool                                HasEnforcedQSWBehavior( void )
    {
        return m_QuickSetupWizardEnforce != qswiDefaultBehaviour;
    }
    bool                                LoadActiveDeviceFromFile( const wxString& path );
    void                                RemoveCaptureSettingFromStack( Device* pDev, FunctionInterface* pFI, bool boRestoreSetting );
    void                                RestoreSelectorStateAfterQuickSetupWizard( void );
    void                                RestoreGUIStateAfterQuickSetupWizard( bool boFirstRun );
    void                                RestoreGUIStateAfterWizard( bool boConfigureToolBars, bool boRestoreDisplayState );
    void                                SaveCaptureSettingOnStack( Device* pDev, FunctionInterface* pFI );
    void                                SetAutoShowQSWOnUseDevice( bool boShow )
    {
        m_pOptionsDlg->GetShowQuickSetupOnDeviceOpenCheckBox()->SetValue( boShow );
    }
    bool                                SetCurrentImage( const wxFileName& fileName, ImageCanvas* pImageCanvas );
    void                                SetDisplayWindowCount( unsigned int xCount, unsigned int yCount );
    void                                WriteErrorMessage( const wxString& msg )
    {
        WriteLogMessage( msg, m_errorStyle );
    }
    void                                WriteLogMessage( const wxString& msg, const wxTextAttr& style = wxTextAttr( *wxBLACK ) );
protected:
    virtual void AppendCustomPropGridExecutionErrorMessage( wxString& msg ) const;
    virtual void DoExecuteMethod( wxPGProperty* pProp, MethodObject* pMethod );
    virtual bool FeatureChangedCallbacksSupported( void ) const
    {
        return true;
    }
    virtual bool FeatureHasChangedCallback( mvIMPACT::acquire::Component c ) const
    {
        return m_pPropViewCallback->isComponentRegistered( c );
    }
    virtual bool FeatureSupportsWizard( mvIMPACT::acquire::Component ) const
    {
        return m_currentWizard != wNone;
    }
    virtual bool FeatureVersusTimePlotSupported( void ) const
    {
        return true;
    }
    virtual EDisplayFlags GetDisplayFlags( void ) const
    {
        return m_pDevPropHandler ? m_pDevPropHandler->GetDisplayFlags() : dfNone;
    }
    wxString GetDesktopPath( void );
    // event handlers (these functions should _not_ be virtual)
    void OnAction_DefaultAcquisitionStartStopBehaviour_Changed( wxCommandEvent& e );
    void OnAction_DefaultDeviceInterface_Changed( wxCommandEvent& e );
    void OnAction_DisplayConnectedDevicesOnly( wxCommandEvent& )
    {
        UpdateDeviceComboBox();
    }
    void OnConfigureImageDisplayCount( wxCommandEvent& e );
    void OnConfigureImagesPerDisplayCount( wxCommandEvent& e );
    void OnActivateDisplay( wxCommandEvent& e );
    void OnBtnAbort( wxCommandEvent& )
    {
        Abort();
    }
    void OnBtnUnlock( wxCommandEvent& e );
    void OnBtnAcquire( wxCommandEvent& )
    {
        if( !m_boShutdownInProgress )
        {
            Acquire();
        }
    }
    void OnBtnRecord( wxCommandEvent& e );
    void OnBtnSelectPropertyConnectedWithCurrentIssue( wxCommandEvent& e );
    void OnIncDecRecordDisplay( wxCommandEvent& e );
    void OnUseDevice( wxCommandEvent& )
    {
        ToggleCurrentDevice();
    }
    void OnAnalysisPlotCBAOIFullMode( wxCommandEvent& e )
    {
        ConfigureAnalysisPlotAOIFullMode( GetAnalysisControlIndex( e.GetId() ), e.IsChecked() );
    }
    void OnAnalysisPlotCBProcessBayerParity( wxCommandEvent& e );
    void OnCapture_DefaultImageProcessingMode_Changed( wxCommandEvent& e );
#ifdef _OPENMP
    void OnCapture_SetImageProcessingThreadCount( wxCommandEvent& e );
#endif // #ifdef _OPENMP
    void OnCBEnableInfoPlot( wxCommandEvent& );
    void OnCBInfoPlotDifferences( wxCommandEvent& );
    void OnCBInfoPlotAutoScale( wxCommandEvent& );
    void OnCBEnableFeatureValueVsTimePlot( wxCommandEvent& );
    void OnCBFeatureValueVsTimePlotDifferences( wxCommandEvent& );
    void OnCBFeatureValueVsTimePlotAutoScale( wxCommandEvent& );
    void OnClose( wxCloseEvent& e );
    void OnContinuousRecording( wxCommandEvent& e );
    void OnDetailedRequestInformation( wxCommandEvent& e );
    void OnDevComboTextChanged( wxCommandEvent& )
    {
        UpdateDeviceFromComboBox();
    }
    void OnDeviceRegistersValidChanged( wxCommandEvent& )
    {
        DeviceRegistersValidChanged();
    }
    void OnEndFullScreenMode( wxCommandEvent& )
    {
        EndFullScreenMode();
    }
    void OnExportActiveDevice( wxCommandEvent& e );
    void OnAction_Exit( wxCommandEvent& )
    {
        Close( true );
    }
    void OnAction_InterfaceConfigurationAndDriverInformation( wxCommandEvent& )
    {
        ShowInterfaceConfigurationAndDriverInformationDialogue();
    }
    void OnAnalysisPlotAOIhChanged( wxSpinEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, -1, -1, e.GetPosition() );
    }
    void OnAnalysisPlotAOIhTextChanged( wxCommandEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, -1, -1, e.GetInt() );
    }
    void OnAnalysisPlotAOIwChanged( wxSpinEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, -1, e.GetPosition() );
    }
    void OnAnalysisPlotAOIwTextChanged( wxCommandEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, -1, e.GetInt() );
    }
    void OnAnalysisPlotAOIxChanged( wxSpinEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, e.GetPosition() );
    }
    void OnAnalysisPlotAOIxTextChanged( wxCommandEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, e.GetInt() );
    }
    void OnAnalysisPlotAOIyChanged( wxSpinEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, e.GetPosition() );
    }
    void OnAnalysisPlotAOIyTextChanged( wxCommandEvent& e )
    {
        UpdateAnalysisPlotAOIParameter( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, e.GetInt() );
    }
    void OnAnalysisPlot_PlotSelectionChanged( wxCommandEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetPlotSelection( e.GetString() );
    }
    void OnAnalysisPlotHistoryDepthChanged( wxSpinEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetHistoryDepth( e.GetPosition() );
    }
    void OnAnalysisPlotHistoryDepthTextChanged( wxCommandEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetHistoryDepth( e.GetInt() );
    }
    void OnAnalysisPlotDrawStartChanged( wxSpinEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetDrawStartPercent( e.GetPosition() );
    }
    void OnAnalysisPlotDrawStartTextChanged( wxCommandEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetDrawStartPercent( e.GetInt() );
    }
    void OnAnalysisPlotDrawWindowChanged( wxSpinEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetDrawWindowWidthPercent( e.GetPosition() );
    }
    void OnAnalysisPlotDrawWindowTextChanged( wxCommandEvent& e )
    {
        m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas->SetDrawWindowWidthPercent( e.GetInt() );
    }
    void OnAnalysisPlotDrawStepWidthChanged( wxSpinEvent& e )
    {
        dynamic_cast<HistogramCanvas*>( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas )->SetDrawStepWidth( e.GetPosition() );
    }
    void OnAnalysisPlotDrawStepWidthTextChanged( wxCommandEvent& e )
    {
        dynamic_cast<HistogramCanvas*>( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas )->SetDrawStepWidth( e.GetInt() );
    }
    void OnAnalysisPlotGridXStepsChanged( wxSpinEvent& e )
    {
        UpdateAnalysisPlotGridSteps( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, e.GetPosition(), -1 );
    }
    void OnAnalysisPlotGridXStepsTextChanged( wxCommandEvent& e )
    {
        UpdateAnalysisPlotGridSteps( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, e.GetInt(), -1 );
    }
    void OnAnalysisPlotGridYStepsChanged( wxSpinEvent& e )
    {
        UpdateAnalysisPlotGridSteps( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, e.GetPosition() );
    }
    void OnAnalysisPlotGridYStepsTextChanged( wxCommandEvent& e )
    {
        UpdateAnalysisPlotGridSteps( m_ImageAnalysisPlots[GetAnalysisControlIndex( e.GetId() )].m_pPlotCanvas, -1, e.GetInt() );
    }
    void OnAnalysisPlotUpdateSpeedChanged( wxSpinEvent& )
    {
        SetupUpdateFrequencies( true );
    }
    void OnAnalysisPlotUpdateSpeedTextChanged( wxCommandEvent& )
    {
        SetupUpdateFrequencies( true );
    }
    void OnBufferPartIndexChanged( wxSpinEvent& e )
    {
        SetBufferPartIndexToDisplay( static_cast<unsigned int>( e.GetPosition() ) );
    }
    void OnBufferPartIndexTextChanged( wxCommandEvent& e )
    {
        SetBufferPartIndexToDisplay( static_cast<unsigned int>( e.GetInt() ) );
    }
    void OnFeatureChangedCallbackReceived( wxCommandEvent& e )
    {
        WriteLogMessage( e.GetString() );
    }
    void OnHelp_About( wxCommandEvent& e );
    void OnHelp_CheckForUpdatesNow( wxCommandEvent& e );
    void OnHelp_FindFeature( wxCommandEvent& e );
    void OnHelp_OnlineDocumentation( wxCommandEvent& )
    {
        ::wxLaunchDefaultBrowser( wxT( "http://www.matrix-vision.com/manuals/" ) );
    }
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    bool CheckedGetMVIADataDir( wxString& mvDataDirectoryPath );
    void OnHelp_EmailLogFilesZip( wxCommandEvent& e );
    void OnHelp_OpenLogFilesFolder( wxCommandEvent& e );
    void OnHelp_SaveLogFilesAsZip( wxCommandEvent& e );
#endif //#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    void OnImageTimeoutEvent( wxCommandEvent& e );
    void OnImageCanvasSelected( wxCommandEvent& e )
    {
        SelectImageCanvas( e.GetInt() );
    }
    void OnImageCanvasFullScreen( wxCommandEvent& e );
    void OnImageInfo( wxCommandEvent& e );
    void OnImageReady( wxCommandEvent& e );
    void OnImageSkipped( wxCommandEvent& e );
    void OnInfoPlot_PlotSelectionComboTextChanged( wxCommandEvent& e );
    void OnInfoPlotUpdateHistoryDepthChanged( wxSpinEvent& e )
    {
        m_pInfoPlotArea->SetHistoryDepth( e.GetPosition() );
    }
    void OnInfoPlotUpdateHistoryDepthTextChanged( wxCommandEvent& e )
    {
        m_pInfoPlotArea->SetHistoryDepth( e.GetInt() );
    }
    void OnInfoPlotUpdateSpeedChanged( wxSpinEvent& e )
    {
        m_pInfoPlotArea->SetUpdateFrequency( e.GetPosition() );
    }
    void OnInfoPlotUpdateSpeedTextChanged( wxCommandEvent& e )
    {
        m_pInfoPlotArea->SetUpdateFrequency( e.GetInt() );
    }
    void OnFeatureValueVsTimePlot_PlotSelectionComboTextChanged( wxCommandEvent& e );
    void OnFeatureValueVsTimePlotUpdateHistoryDepthChanged( wxSpinEvent& e )
    {
        m_pFeatureValueVsTimePlotArea->SetHistoryDepth( e.GetPosition() );
    }
    void OnFeatureValueVsTimePlotUpdateHistoryDepthTextChanged( wxCommandEvent& e )
    {
        m_pFeatureValueVsTimePlotArea->SetHistoryDepth( e.GetInt() );
    }
    void OnFeatureValueVsTimePlotUpdateSpeedChanged( wxSpinEvent& e )
    {
        m_pFeatureValueVsTimePlotArea->SetUpdateFrequency( e.GetPosition() );
    }
    void OnFeatureValueVsTimePlotUpdateSpeedTextChanged( wxCommandEvent& e )
    {
        m_pFeatureValueVsTimePlotArea->SetUpdateFrequency( e.GetInt() );
    }
    void OnLatestChangesDownloaded( wxCommandEvent& );
    void OnLiveModeAborted( wxCommandEvent& e );
    void OnLoadActiveDevice( wxCommandEvent& e );
    void OnLoadActiveDeviceFromFile( wxCommandEvent& e );
    void OnLoadCurrentProduct( wxCommandEvent& e );
    void OnLoadFromDefault( wxCommandEvent& e );
    void OnLoadImage( wxCommandEvent& e );
    void OnManageSettings( wxCommandEvent& e );
    void OnMethodExecutionDone( wxCommandEvent& e );
    void OnMonitorImageClosed( wxCommandEvent& )
    {
        ConfigureMonitorImage( false );
    }
    void OnMouseEntersStatusBar( wxMouseEvent& )
    {
        if( ShowStatusBarTooltip() && !m_statusBarTooltipTimer.IsRunning() )
        {
            m_statusBarTooltipTimer.StartOnce( 1000 );
        }
    }
    void OnMouseLeavesStatusBar( wxMouseEvent& )
    {
        StopTimer( m_statusBarTooltipTimer );
    }
    void OnMouseMovesWithinStatusBar( wxMouseEvent& e )
    {
        m_mousePositionOnStatusBar = e.GetPosition();
    }
    void OnNotebookPageChanged( wxNotebookEvent& e );
    void OnNBDisplayMethodPageChanged( wxNotebookEvent& e );
    virtual void OnPopUpPropReadFromFile( wxCommandEvent& e );
    virtual void OnPopUpPropWriteToFile( wxCommandEvent& e );
    virtual void OnPopUpPropAttachCallback( wxCommandEvent& e );
    virtual void OnPopUpPropDetachCallback( wxCommandEvent& e );
    virtual void OnPopUpPropPlotInFeatureValueVsTimePlot( wxCommandEvent& e );
    virtual void OnPopUpWizard( wxCommandEvent& e );
    virtual void OnPropertyChangedCustom( wxPropertyGridEvent& )
    {
        m_pDevPropHandler->SetActiveDevice( GetSelectedDeviceSerial() );
    }
    virtual void OnPropertyGridTimer( void );
    virtual void OnPropertyRightClickedCustom( wxPropertyGridEvent& )
    {
        m_pDevPropHandler->SetActiveDevice( GetSelectedDeviceSerial() );
    }
    virtual void OnPropertySelectedCustom( wxPropertyGridEvent& e )
    {
        UpdateWizardStatus( e.GetProperty() );
    }
    void OnRefreshAOIControls( wxCommandEvent& e );
    void OnRefreshCurrentPixelData( wxCommandEvent& e );
    void OnSaveActiveDevice( wxCommandEvent& e );
    void OnSaveCurrentProduct( wxCommandEvent& e );
    void OnSaveImage( wxCommandEvent& e );
    void OnSaveImageSequenceToFiles( wxCommandEvent& e );
    void OnSaveImageSequenceToStream( wxCommandEvent& e );
    void OnSaveSnapshot( wxCommandEvent& e );
    void OnSaveToDefault( wxCommandEvent& e );
    void OnSequenceReady( wxCommandEvent& e );
    void OnSettings_Analysis_ShowControls( wxCommandEvent& e );
    void OnSettings_Analysis_SynchronizeAOIs( wxCommandEvent& e );
    void OnSettings_PropGrid_Show( wxCommandEvent& e );
    void OnSettings_PropGrid_UpdateViewMode( wxCommandEvent& )
    {
        UpdatePropGridViewMode();
    }
    void OnSettings_PropGrid_ViewModeChanged( wxCommandEvent& e );
    void OnCapture_CaptureSettings_CreateCaptureSetting( wxCommandEvent& e );
    void OnCapture_CaptureSettings_CaptureSettingHierarchy( wxCommandEvent& e );
    void OnCapture_CaptureSettings_AssignToDisplays( wxCommandEvent& e );
    void OnCapture_CaptureSettings_UsageMode_Changed( wxCommandEvent& e );
    virtual void OnTimerCustom( wxTimerEvent& );
    void OnWizard_Open( wxCommandEvent& );
    void OnWizards_AOI( wxCommandEvent& )
    {
        Wizard_AOI();
    }
    void OnWizards_ColorCorrection( wxCommandEvent& )
    {
        Wizard_ColorCorrection();
    }
    void OnWizards_FileAccessControl_UploadFile( wxCommandEvent& )
    {
        Wizard_FileAccessControl( true );
    }
    void OnWizards_FileAccessControl_DownloadFile( wxCommandEvent& )
    {
        Wizard_FileAccessControl( false );
    }
    void OnWizards_LensControl( wxCommandEvent& )
    {
        Wizard_LensControl();
    }
    void OnWizards_LUTControl( wxCommandEvent& )
    {
        Wizard_LUTControl();
    }
    void OnWizards_MultiAOI( wxCommandEvent& )
    {
        Wizard_MultiAOI();
    }
    void OnWizards_MultiCoreAcquisition( wxCommandEvent& )
    {
        Wizard_MultiCoreAcquisition();
    }
    void OnWizards_QuickSetup( wxCommandEvent& )
    {
        Wizard_QuickSetup( false );
    }
    void OnWizards_SequencerControl( wxCommandEvent& )
    {
        Wizard_SequencerControl();
    }
    void OnSettings_Options( wxCommandEvent& e );
    void OnSettings_SetUpdateFrequency( wxCommandEvent& e );
    void OnSetupCaptureQueueDepth( wxCommandEvent& e );
    void OnSetupHardDiskRecording( wxCommandEvent& )
    {
        SettingsSetupRecording( rmHardDisk );
    }
    void OnSetupSnapshotRecording( wxCommandEvent& )
    {
        SettingsSetupRecording( rmSnapshot );
    }
    void OnSetupRecordSequenceSize( wxCommandEvent& e );
    void OnCapture_VideoStreamRecording_Setup( wxCommandEvent& e );
    void OnCapture_VideoStreamRecording_Start( wxCommandEvent& e );
    void OnCapture_VideoStreamRecording_Pause( wxCommandEvent& e );
    void OnCapture_VideoStreamRecording_Stop( wxCommandEvent& e );
    void OnShowIncompleteFrames( wxCommandEvent& e );
    void OnShowMonitorDisplay( wxCommandEvent& e );
    void OnSize( wxSizeEvent& e );
    void OnSLRecordDisplay( wxScrollEvent& e )
    {
        UpdateRecordedImage( static_cast<size_t>( e.GetPosition() ) );
    }
    void OnSplitterSashPosChanged( wxSplitterEvent& e );
    void OnToggleDisplayWindowSize( wxCommandEvent& e );
    void OnToggleFullScreenMode( wxCommandEvent& e )
    {
        ShowFullScreen( e.IsChecked() );
    }
    void OnUpdateDeviceList( wxCommandEvent& )
    {
        UpdateDeviceList();
    }
    void OnUserExperienceComboTextChanged( wxCommandEvent& )
    {
        UpdateUserExperience( m_pUserExperienceCombo->GetValue() );
    }
    void SetupDlgControls( void );
    virtual bool ShowPropGridMethodExecutionErrors( void ) const
    {
        return m_pOptionsDlg->GetMiscellaneousConfiguration()->IsChecked( OptionsDlg::mDisplayMethodExecutionErrors );
    }
    virtual bool ShowFeatureChangeTimeConsumption( void ) const
    {
        return m_pOptionsDlg->GetMiscellaneousConfiguration()->IsChecked( OptionsDlg::mDisplayFeatureChangeTimeConsumption );
    }
    virtual bool ShowStatusBarTooltip( void ) const
    {
        return m_pOptionsDlg->GetMiscellaneousConfiguration()->IsChecked( OptionsDlg::mDisplayStatusBarTooltip );
    }
    virtual bool WizardAOISupported( void ) const
    {
        return true;
    }
private:
    DECLARE_EVENT_TABLE()
    //-----------------------------------------------------------------------------
    enum TImageAnalysisPlot
    //-----------------------------------------------------------------------------
    {
        iapPixelHistogram,
        iapLineProfileHorizontal,
        iapLineProfileVertical,
        iapSpatialNoiseHistogram,
        iapTemporalNoiseHistogram,
        iapIntensityPlot,
        iapVectorScope,
        iapLAST
    };
    //-----------------------------------------------------------------------------
    enum TMenuItem
    //-----------------------------------------------------------------------------
    {
        miAction_Exit = mibLAST,
        miAction_UseDevice,
        miAction_DefaultAcquisitionStartStopBehaviour_Default,
        miAction_DefaultAcquisitionStartStopBehaviour_User,
        miAction_DisplayConnectedDevicesOnly,
        miAction_DefaultDeviceInterface_DeviceSpecific,
        miAction_DefaultDeviceInterface_GenICam,
        miAction_CaptureSettings_Save_ToDefault,
        miAction_CaptureSettings_Save_CurrentProduct,
        miAction_CaptureSettings_Save_ActiveDevice,
        miAction_CaptureSettings_Save_ExportActiveDevice,
        miAction_CaptureSettings_Load_FromDefault,
        miAction_CaptureSettings_Load_CurrentProduct,
        miAction_CaptureSettings_Load_ActiveDevice,
        miAction_CaptureSettings_Load_ActiveDeviceFromFile,
        miAction_CaptureSettings_Manage,
        miAction_InterfaceConfigurationAndDriverInformation,
        miAction_LoadImage,
        miAction_SaveImage,
        miAction_SaveImageSequenceToFiles,
        miAction_SaveImageSequenceToStream,
        miAction_UpdateDeviceList,
        miCapture_Acquire,
        miCapture_Abort,
        miCapture_Unlock,
        miCapture_DefaultImageProcessingMode_ProcessAll,
        miCapture_DefaultImageProcessingMode_ProcessLatestOnly,
#ifdef _OPENMP
        miCapture_SetImageProcessingThreadCount,
#endif // #ifdef _OPENMP
        miCapture_Record,
        miCapture_Forward,
        miCapture_Backward,
        miCapture_Recording_SilentMode,
        miCapture_Recording_Continuous,
        miCapture_Recording_SetupSequenceSize,
        miCapture_VideoStreamRecording_Setup,
        miCapture_VideoStreamRecording_Start,
        miCapture_VideoStreamRecording_Pause,
        miCapture_VideoStreamRecording_Stop,
        miCapture_SetupHardDiskRecording,
        miCapture_SetupSnapshotMode,
        miCapture_SetupCaptureQueueDepth,
        miCapture_DetailedRequestInformation,
        miCapture_CaptureSettings_CreateCaptureSetting,
        miCapture_CaptureSettings_CaptureSettingHierarchy,
        miCapture_CaptureSettings_AssignToDisplays,
        miCapture_CaptureSettings_UsageMode_Manual,
        miCapture_CaptureSettings_UsageMode_Automatic,
        miSettings_PropGrid_Show,
        miSettings_PropGrid_ViewMode_StandardView,
        miSettings_PropGrid_ViewMode_DevelopersView,
        miSettings_Display_ConfigureImageDisplayCount,
        miSettings_Display_ConfigureImagesPerDisplayCount,
        miSettings_Display_Active,
        miSettings_Display_ShowMonitorImage,
        miSettings_Display_ShowIncompleteFrames,
        miSettings_ToggleFullScreenMode,
        miSettings_Options,
        miSettings_SetUpdateFrequency,
        miSettings_Analysis_ShowControls,
        miSettings_Analysis_SynchronizeAOIs,
        miWizard_Open,
        miWizards_AOI,
        miWizards_ColorCorrection,
        miWizards_FileAccessControl_UploadFile,
        miWizards_FileAccessControl_DownloadFile,
        miWizards_LensControl,
        miWizards_LUTControl,
        miWizards_MultiAOI,
        miWizards_MultiCoreAcquisition,
        miWizards_QuickSetup,
        miWizards_SequencerControl,
        miHelp_About,
        miHelp_CheckForUpdatesNow,
        miHelp_OnlineDocumentation,
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
        miHelp_SaveLogFilesAsZip,
        miHelp_OpenLogFilesFolder,
        miHelp_EmailLogFilesZip,
#endif //#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
        miHelp_FindFeature,
        widStatusBar,
        widVerSplitter,
        widDevCombo,
        widHorSplitter,
        widLogHistogramSplitter,
        widMainFrame,
        widInfoPlot_PlotSelectionCombo,
        widCBEnableInfoPlot,
        widSCBufferPartIndex,
        widSCInfoPlotUpdateSpeed,
        widSCInfoPlotHistoryDepth,
        widCBInfoPlotDifferences,
        widCBInfoPlotAutoScale,
        widCBEnableFeatureValueVsTimePlot,
        widSCFeatureValueVsTimePlotUpdateSpeed,
        widSCFeatureValueVsTimePlotHistoryDepth,
        widCBFeatureValueVsTimePlotDifferences,
        widCBFeatureValueVsTimePlotAutoScale,
        widSLRecordDisplay,
        widNotebook,
        widUserExperienceCombo,
        widBtnSelectPropertyConnectedWithCurrentIssue,

        BUILD_PLOT_WINDOW_START_ID( PixelHistogram ),
        BUILD_PLOT_WINDOW_START_ID( SpatialNoiseHistogram ),
        BUILD_PLOT_WINDOW_START_ID( TemporalNoiseHistogram ),
        BUILD_PLOT_WINDOW_START_ID( LineProfileHorizontal ),
        BUILD_PLOT_WINDOW_START_ID( LineProfileVertical ),
        BUILD_PLOT_WINDOW_START_ID( IntensityPlot ),
        BUILD_PLOT_WINDOW_START_ID( VectorScope )
    };
    //-----------------------------------------------------------------------------
    enum TImageAnalysisPlotIDs
    //-----------------------------------------------------------------------------
    {
        iapidCBAOIFullMode,
        iapidCBProcessBayerParity,
        iapidSCAOIx,
        iapidSCAOIy,
        iapidSCAOIw,
        iapidSCAOIh,
        iapidCoBPlotSelection,
        iapidSCHistoryDepth,
        iapidSCDrawStart_percent,
        iapidSCDrawWindow_percent,
        iapidSCDrawStepWidth,
        iapidSCGridStepsX,
        iapidSCGridStepsY,
        iapidSCUpdateSpeed,
        iapidNBDisplayMethod,
        iapidGrid
    };
    //-----------------------------------------------------------------------------
    enum
    //-----------------------------------------------------------------------------
    {
        DISPLAY_COUNT_MAX = 6,
        DEFAULT_DISPLAY_UPDATE_PERIOD = 20,
        IMAGES_WITHIN_ONE_BUFFER_MAX = 256
    };
    //-----------------------------------------------------------------------------
    enum TStatusBarField
    //-----------------------------------------------------------------------------
    {
        sbfFramesPerSecond,
        sbfBandwidthConsumed,
        sbfFramesDelivered,
        sbfStatistics,
        sbfImageFormat,
        sbfPixelData,
        NO_OF_STATUSBAR_FIELDS
    };
    //-----------------------------------------------------------------------------
    enum TStatusBarFieldLoadedImage
    //-----------------------------------------------------------------------------
    {
        sbflfFilePath,
        sbflfImageFormat,
        sbflfPixelData,
        NO_OF_STATUSBAR_FIELDS_LOADED_IMAGE
    };
    //-----------------------------------------------------------------------------
    enum TQuickSetupWizardInvocation
    //-----------------------------------------------------------------------------
    {
        qswiDefaultBehaviour,
        qswiForceShow,
        qswiForceHide
    };
    //-----------------------------------------------------------------------------
    enum TTimerEventCustom
    //-----------------------------------------------------------------------------
    {
        teUpdateDisplay = teCUSTOM_ID,
        teShowInterfaceConfigurationDialog,
        teShowStatusBarTooltip
    };

    //-----------------------------------------------------------------------------
    struct GUISetup
    //-----------------------------------------------------------------------------
    {
        int             verticalSplitterPosition_;
        int             horizontalSplitterPosition_;
        bool            boDisplayShown_;
        bool            boPropertyGridShown_;
        bool            boLeftToolBarShown_;
        bool            boUpperToolbarShown_;
        bool            boAnalysisTabsShown_;
        bool            boStatusBarShown_;
        bool            boFitToCanvas_;
        bool            boFullAOIMode_;
        int             scrollPositionX_;
        int             scrollPositionY_;
        double          zoomFactor_;
        unsigned int    displayCountX_;
        unsigned int    displayCountY_;
        explicit GUISetup() : verticalSplitterPosition_( -1 ), horizontalSplitterPosition_( -1 ), boDisplayShown_( true ),
            boPropertyGridShown_( true ), boLeftToolBarShown_( true ), boUpperToolbarShown_( true ), boAnalysisTabsShown_( true ),
            boStatusBarShown_( true ), boFitToCanvas_( false ), boFullAOIMode_( false ), scrollPositionX_( -1 ), scrollPositionY_( -1 ), zoomFactor_( 1. ),
            displayCountX_( 1 ), displayCountY_( 1 ) {}
    };

    typedef std::map<std::string, int> ProductFirmwareTable;
    typedef std::map<Property, std::string> SelectorStates;
    typedef std::vector<RequestData> RequestInfoContainer;
    MethodExecutionThread*              m_pMethodExecutionThread;
    ImageAnalysisPlotControls           m_ImageAnalysisPlots[iapLAST];
    bool                                m_boAllowFullDeviceFileAccess;
    bool                                m_boCloseDeviceInProgress;
    bool                                m_boFindFeatureMatchCaseActive;
    bool                                m_boHandleImageTimeoutEvents;
    bool                                m_boDisplayWindowMaximized;
    bool                                m_boSaveMetaDataOnSaveFileDialog;
    bool                                m_boShutdownInProgress;
    bool                                m_boSingleCaptureInProgess;
    bool                                m_boUpdateAOIInProgress;
    bool                                m_boCurrentImageIsFromFile;
    bool                                m_boNewImageDataAvailable;
    bool                                m_boImageCanvasInFullScreenMode;
    bool                                m_boStartInFullScreenMode;
    bool                                m_boFirstTimerFunctionHit;
    bool                                m_boCheckedForUpdates;
    wxTextAttr                          m_errorStyle;
    bool                                m_boSelectedDeviceSupportsMultiFrame;
    bool                                m_boSelectedDeviceSupportsSingleFrame;
    RequestInfoContainer                m_CurrentRequestDataContainer;
    int                                 m_CurrentImageAnalysisControlIndex;
    wxString                            m_CurrentLoadedImageFileName;
    wxTimer                             m_DelayedLaunchTimer;
    unsigned int                        m_DeviceListChangedCounter;
    unsigned int                        m_DeviceCount;
    ProductFirmwareTable                m_ProductFirmwareTable;
    SelectorStates                      m_SelectorStatesMap;
    int                                 m_HorizontalSplitterPos;
    const wxString                      m_NoDevStr;
    const wxString                      m_LaunchInterfaceConfigurationStr;
    const wxString                      m_SingleFrameStr;
    const wxString                      m_MultiFrameStr;
    const wxString                      m_ContinuousStr;
    const wxString                      m_ImageFileFormatFilter;
    static const wxString               s_WhiteBalanceToolTip;
    static const wxString               s_WizardsToolTip;
    wxString                            m_lastAcquisitionMode;
    wxString                            m_newestMVIAVersionAvailable;
    wxString                            m_downloadedFileContent;
    wxDateTime                          m_lastCheckForNewerMVIAVersion;
    wxCheckBox*                         m_pCBEnableInfoPlot;
    wxCheckBox*                         m_pCBInfoPlotDifferences;
    wxCheckBox*                         m_pCBInfoPlotAutoScale;
    wxCheckBox*                         m_pCBEnableFeatureValueVsTimePlot;
    wxCheckBox*                         m_pCBFeatureValueVsTimePlotDifferences;
    wxCheckBox*                         m_pCBFeatureValueVsTimePlotAutoScale;
    wxTextCtrl*                         m_pTCTimestampAsTime;
    wxComboBox*                         m_pDevCombo;
    wxComboBox*                         m_pAcquisitionModeCombo;
    wxSlider*                           m_pRecordDisplaySlider;
    DevicePropertyHandler*              m_pDevPropHandler;
    DisplayWindowContainer              m_DisplayAreas;
    SettingToDisplayDict                m_settingToDisplayDict;
    SequencerSetToDisplayMap            m_sequencerSetToDisplayMap;
    ImageCanvas*                        m_pLastMouseHooverDisplay;
    ImageCanvas*                        m_pCurrentAnalysisDisplay;
    int                                 m_CurrentRequestDataIndex;
    wxCriticalSection                   m_critSect;
    wxPanel*                            m_pDisplayPanel;
    wxGridSizer*                        m_pDisplayGridSizer;
    wxPanel*                            m_pSettingsPanel;
    wxComboBox*                         m_pUserExperienceCombo;
    wxStaticBitmap*                     m_pStaticBitmapWarning;
    wxStaticText*                       m_pSTCurrentIssue;
    wxButton*                           m_pBtnSelectPropertyConnectedWithCurrentIssue;
    static const ActiveIssue            s_RequestTimeoutWarning;
    std::vector<ActiveIssue>            m_currentIssues;
    MonitorDisplay*                     m_pMonitorImage;
    OptionsDlg*                         m_pOptionsDlg;
    bool                                m_boShowWelcomeDlg;
    WizardAOI*                          m_pAOIDlg;
    WizardLUTControl*                   m_pLUTControlDlg;
    WizardColorCorrection*              m_pColorCorrectionDlg;
    WizardLensControl*                  m_pLensControlDlg;
    WizardMultiAOI*                     m_pMultiAOIDlg;
    WizardMultiCoreAcquisition*           m_pMultiCoreAcquisitionDlg;
    WizardQuickSetup*                   m_pQuickSetupDlgCurrent;
    bool                                m_boShowQuickSetupWizardCurrentProcess;
    TQuickSetupWizardInvocation         m_QuickSetupWizardEnforce;
    wxSplitterWindow*                   m_pHorizontalSplitter;
    PlotCanvasInfo*                     m_pInfoPlotArea;
    PlotCanvasFeatureVsTime*            m_pFeatureValueVsTimePlotArea;
    wxComboBox*                         m_pInfoPlotSelectionCombo;
    wxTextCtrl*                         m_pLogWindow;
    wxNotebook*                         m_pLowerRightWindow;
    wxMenuBar*                          m_pMenuBar;
    wxMenuItem*                         m_pMIAction_Use;
    wxMenuItem*                         m_pMIAction_DisplayConnectedDevicesOnly;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Save_ToDefault;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Save_CurrentProduct;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Save_ActiveDevice;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Save_ExportActiveDevice;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Load_FromDefault;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Load_CurrentProduct;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Load_ActiveDevice;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Load_ActiveDeviceFromFile;
    wxMenuItem*                         m_pMIAction_CaptureSettings_Manage;
    wxMenuItem*                         m_pMIAction_InterfaceConfigurationAndDriverInformation;
    wxMenuItem*                         m_pMIAction_LoadImage;
    wxMenuItem*                         m_pMIAction_SaveImage;
    wxMenuItem*                         m_pMIAction_SaveImageSequenceToFiles;
    wxMenuItem*                         m_pMIAction_SaveImageSequenceToStream;
    wxMenuItem*                         m_pMIAction_UpdateDeviceList;
    wxMenuItem*                         m_pMICapture_Acquire;
    wxMenuItem*                         m_pMICapture_Abort;
    wxMenuItem*                         m_pMICapture_Unlock;
#ifdef _OPENMP
    wxMenuItem*                         m_pMICapture_SetImageProcessingThreadCount;
#endif // #ifdef _OPENMP
    wxMenuItem*                         m_pMICapture_DefaultImageProcessingMode_ProcessAll;
    wxMenuItem*                         m_pMICapture_DefaultImageProcessingMode_ProcessLatestOnly;
    wxMenuItem*                         m_pMICapture_Record;
    wxMenuItem*                         m_pMICapture_Forward;
    wxMenuItem*                         m_pMICapture_Backward;
    wxMenuItem*                         m_pMICapture_Recording_SlientMode;
    wxMenuItem*                         m_pMICapture_Recording_Continuous;
    wxMenuItem*                         m_pMICapture_Recording_SetupSequenceSize;
    wxMenuItem*                         m_pMICapture_SetupHardDiskRecording;
    wxMenuItem*                         m_pMICapture_SetupSnapshotMode;
    wxMenuItem*                         m_pMICapture_VideoStreamRecording_Setup;
    wxMenuItem*                         m_pMICapture_SelectBufferPartIndex;
    wxMenuItem*                         m_pMICapture_SetupCaptureQueueDepth;
    wxMenuItem*                         m_pMICapture_DetailedRequestInformation;
    wxMenuItem*                         m_pMICapture_CaptureSettings_CreateCaptureSetting;
    wxMenuItem*                         m_pMICapture_CaptureSettings_CaptureSettingHierarchy;
    wxMenuItem*                         m_pMICapture_CaptureSettings_AssignToDisplays;
    wxMenuItem*                         m_pMICapture_CaptureSettings_UsageMode_Manual;
    wxMenuItem*                         m_pMICapture_CaptureSettings_UsageMode_Automatic;
    wxMenuItem*                         m_pMIHelp_AutoCheckForUpdatesWeekly;
    wxMenuItem*                         m_pMISettings_Display_ConfigureImageDisplayCount;
    wxMenuItem*                         m_pMISettings_Display_ConfigureImagesPerDisplayCount;
    wxMenuItem*                         m_pMISettings_Display_Active;
    wxMenuItem*                         m_pMISettings_Display_ShowMonitorImage;
    wxMenuItem*                         m_pMISettings_Display_ShowIncompleteFrames;
    wxMenuItem*                         m_pMISettings_Analysis_ShowControls;
    wxMenuItem*                         m_pMISettings_Analysis_SynchroniseAOIs;
    wxMenuItem*                         m_pMISettings_PropGrid_Show;
    wxMenuItem*                         m_pMISettings_PropGrid_ViewMode_StandardView;
    wxMenuItem*                         m_pMISettings_PropGrid_ViewMode_DevelopersView;
    wxMenuItem*                         m_pMISettings_ToggleFullScreenMode;
    wxMenuItem*                         m_pMIWizards_AOI;
    wxMenuItem*                         m_pMIWizards_ColorCorrection;
    wxMenuItem*                         m_pMIWizards_FileAccessControl_UploadFile;
    wxMenuItem*                         m_pMIWizards_FileAccessControl_DownloadFile;
    wxMenuItem*                         m_pMIWizards_LensControl;
    wxMenuItem*                         m_pMIWizards_LUTControl;
    wxMenuItem*                         m_pMIWizards_MultiAOI;
    wxMenuItem*                         m_pMIWizards_MultiCoreAcquisition;
    wxMenuItem*                         m_pMIWizards_SequencerControl;
    wxMenuItem*                         m_pMIWizards_QuickSetup;
    wxPanel*                            m_pPanel;
    wxSpinCtrl*                         m_pSCBufferPartIndex;
    wxSpinCtrl*                         m_pSCInfoPlotHistoryDepth;
    wxSpinCtrl*                         m_pSCInfoPlotUpdateSpeed;
    wxSpinCtrl*                         m_pSCFeatureValueVsTimePlotHistoryDepth;
    wxSpinCtrl*                         m_pSCFeatureValueVsTimePlotUpdateSpeed;
    wxStatusBar*                        m_pStatusBar;
    wxToolBar*                          m_pUpperToolBar;
    wxToolBar*                          m_pLeftToolBar;
    wxSplitterWindow*                   m_pVerticalSplitter;
    wxWindowDisabler*                   m_pWindowDisabler;
    PropViewCallback*                   m_pPropViewCallback;
    TWizardIDs                          m_currentWizard;
    int                                 m_VerticalSplitterPos;
    DevicePropertyHandler::TViewMode    m_ViewMode;
    wxString                            m_defaultAcquisitionStartStopBehaviour;
    wxString                            m_defaultDeviceInterfaceLayout;
    TImageProcessingMode                m_defaultImageProcessingMode;
    VideoStreamRecordingParameters      m_VideoStreamRecordingParameters;
    RecordingParameters                 m_HardDiskRecordingParameters;
    RecordingParameters                 m_SnapshotParameters;
    SetupVideoStreamRecording*          m_pSetupVideoStreamRecordingDlg;
    SetupRecording*                     m_pSetupSnapshotRecordingDlg;
    SetupRecording*                     m_pSetupHardDiskRecordingDlg;
    GUISetup                            m_GUIBeforeWizard;
    wxTimer                             m_DisplayUpdateTimer;
    long                                m_displayUpdateFrequency;
    const bool                          m_UseSmallIcons;
    unsigned int                        m_displayCountX;
    unsigned int                        m_displayCountY;
    wxTimer                             m_statusBarTooltipTimer;
    wxPoint                             m_mousePositionOnStatusBar;

    // Everything related to warnings displayed above the property grid
    // right of the user experience control.
    bool                                AddIssue( const ActiveIssue& warning );
    bool                                RemoveIssue( const ActiveIssue& warning );
    void                                UpdateCurrentIssue( void );

    void                                Abort( void );
    void                                Acquire( void );
    void                                AssignDefaultSettingToDisplayRelationship( void );
    mvIMPACT::acquire::TBayerMosaicParity BayerParityFromString( const wxString& value );
    static int                          BuildPlotWindowStartID( int index )
    {
        return ( index + 1 ) * PLOT_WINDOW_ID_RANGE;
    }
    template<typename _Ty, typename _Tx>
    size_t                              BuildStringArrayFromPropertyDict( wxArrayString& choices, _Tx prop ) const;
    void                                ChangeActiveDevice( const wxString& newSerial );
    void                                ConfigureGUIForWizard( void );
    void                                CheckForDriverPerformanceIssues( Device* pDev );
    bool                                CheckIfNICMTUIsAboveACertainLimit( Device* pDev, const int minimalMTU, int64_type& gevInterfaceMTU ) const;
#if defined(linux) || defined(__linux) || defined(__linux__)
    int                                 CheckForPotentialBufferIssuesGetValueHelper( const wxString& path );
    void                                CheckForPotentialBufferIssues( Device* pDev );
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
    void                                CheckForPotentialFirewallIssues( void );
    void                                CheckForUpdates( void );
    int                                 CheckIfDeviceIsReachable( Device* pDev, bool& boRunningIPConfigureMightHelp );
    void                                ClearDisplayInProgressStates( void );
    void                                CloneAllRequests( bool boClone = true );
    void                                CloneAndUnlockAllRequestsAndFreeRecordedSequence( bool boClone = true );
    void                                CollectSelectors( Component it );
    void                                ConfigureAnalysisPlot( const wxRect* pAOI = 0 );
    void                                ConfigureAnalysisPlotAOIFullMode( int plotIndex, bool boActive );
    void                                ConfigureAOI( ImageAnalysisPlotControls& controls, const wxRect& rect );
    void                                ConfigureAOIControlLimits( ImageAnalysisPlotControls& controls );
    void                                ConfigureLive( void );
    void                                ConfigureMethodExecutionBehaviour( bool boSynchronous );
    void                                ConfigureMonitorImage( bool boVisible );
    void                                ConfigureRequestCount( void );
    void                                ConfigureSplitter( wxSplitterWindow* pSplitter, wxWindow* pWindowToRemove, wxWindow* pWindowToShow, bool boHorizontally = true );
    void                                ConfigureStatusBar( bool boShow );
    void                                ConfigureToolBar( wxToolBar* pToolBar, bool boShow )
    {
        boShow ? pToolBar->Show() : pToolBar->Hide();
    }
    void                                CreateDisplayWindows( void );
    wxScrolledWindow*                   CreateImageAnalysisPlotControls( wxWindow* pParent, int windowIDOffset );
    void                                CreateLeftToolBar( void );
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    void                                CreateLogZipFile( const wxString& logFileDirectoryPath, const wxString& targetFullPath );
#endif //#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    void                                CreateUpperToolBar( void );
    void                                Deinit( void );
    PlotCanvasImageAnalysis*            DeselectAnalysisPlot( void );
    void                                DestroyAdditionalDialogs( void );
    template<typename _Ty>
    void                                DestroyDialog( _Ty** ppDialog );
    bool                                DetectedBootSectionOnBlueCOUGARX( Device* pDev );
    void                                DisplaySettingLoadSaveDeleteErrorMessage( const wxString& msgPrefix, int originalErrorCode );
    void                                ForcedRefreshOfAnalysisPlot( const int analysisPlotIndex ) const;
    int                                 GetAnalysisControlIndex( void ) const;
    int                                 GetAnalysisControlIndex( int windowID ) const
    {
        return ( windowID >> 8 ) - 1;
    }
    int                                 GetDesiredDeviceIndex( wxString& serial ) const;
    mvIMPACT::acquire::Component        GetDriversIterator( void ) const;
    unsigned int                        GetGenTLDeviceCount( void ) const;
    wxString                            GetSelectedDeviceSerial( void ) const
    {
        const wxString s( m_pDevCombo->GetValue() );
        return s.SubString( 0, s.Find( wxString( wxT( " | (" ) ) ) - 1 );
    }
    bool                                IsGenTLDriverInstalled( void ) const;
    int                                 LoadDeviceSetting( FunctionInterface* pFI, const std::string& name, const TStorageFlag flags, const TScope scope );
    void                                OpenDriverWithTimeMeassurement( Device* pDev );
    void                                ParseDownloadedFileContent( const bool boSilent );
    mvIMPACT::acquire::TImageBufferPixelFormat PixelFormatFromString( const wxString& value );
    void                                PostQuickSetupWizardSettings( void );
    void                                ReEnableSingleCapture( void );
    void                                RefreshCurrentPixelData( void );
    void                                RefreshDisplays( bool boEraseBackground );
    void                                RefreshPendingImageQueueDepthForCurrentDevice( void );
    template<class _Ty> void            RegisterAnalysisPlot( int index, wxWindowID id, wxWindowID gridID, const wxColour& AOIColour );
    wxRect                              RestoreConfiguration( const unsigned int displayCount, bool& boMaximized );
    int                                 SaveDeviceSetting( FunctionInterface* pFI, const std::string& name, TStorageFlag flags, const TScope scope );
    void                                SaveGUIStateBeforeWizard( void );
    void                                SaveSelectorStateBeforeQuickSetupWizard( TDeviceInterfaceLayout currentDeviceInterfaceLayout );
    void                                SaveImageMetaData( wxFileName filenameAndPath );
    void                                SaveImage( const wxString& filenameAndPath, TImageFileFormat fileFormat, bool boSaveMetaData );
    void                                SearchAndSelectImageCanvas( ImageCanvas* pImageCanvas );
    void                                SelectImageCanvas( int index );
    void                                SetBufferPartIndexToDisplay( unsigned int index );
    void                                SetCommonToolBarProperties( wxToolBar* pToolBar );
    void                                Settings_ShowQuickSetupOnUse( void )
    {
        m_boShowQuickSetupWizardCurrentProcess = HasEnforcedQSWBehavior() ? true : GetAutoShowQSWOnUseDevice();
    }
    void                                SetupGUIOnFirstRun( void );
    void                                SetupAcquisitionControls( const bool boDevOpen, const bool boDevPresent, const bool boDevLive );
    void                                SetupCaptureSettingsUsageMode( TCaptureSettingUsageMode mode );
    void                                SetupDisplayLogSplitter( bool boForceSplit = false );
    void                                SetupImageProcessingMode( void );
    void                                SetupUpdateFrequencies( bool boCurrentValue );
    void                                SetupVerSplitter( void );
    void                                SettingsSetupRecording( TRecordMode );
    void                                ShowInterfaceConfigurationAndDriverInformationDialogue( void );
    void                                StartDisplayUpdateTimer( void )
    {
        m_DisplayUpdateTimer.Start( 1000 / m_displayUpdateFrequency );
    }
    wxString                            StringFromPixelFormat( const mvIMPACT::acquire::TImageBufferPixelFormat value );
    bool                                UpdateAcquisitionModes( void );
    void                                UpdateAcquisitionStartStopBehaviours( void );
    void                                UpdateAnalysisPlotAOIParameter( PlotCanvasImageAnalysis* pPlotCanvas, int x = -1, int y = -1, int w = -1, int h = -1 );
    void                                UpdateAnalysisPlotGridSteps( PlotCanvasImageAnalysis* pPlotCanvas, int XSteps, int YSteps );
    void                                UpdateData( const TImageDataRetrievalMode mode, const bool boShowInfo, const bool boForceUnlock, const bool boForceReReadImageData = false );
    void                                UpdateDeviceComboBox( void );
    void                                UpdateDeviceFromComboBox( void );
    void                                UpdateDeviceInterfaceLayouts( void );
    void                                UpdateDeviceList( bool boShowProgressDialog = true );
    void                                UpdateFeatureVsTimePlotFeature( void );
    void                                UpdateInfoPlotCombo( void );
    void                                UpdateLUTWizardAfterLoadSetting( const int result );
    void                                UpdatePropGridViewMode( void );
    void                                UpdateRecordedImage( size_t index );
    void                                UpdateSettingTable( void );
    void                                UpdateStatusBar( void );
    void                                UpdateTitle( void );
    void                                UpdateWizardStatus( wxPGProperty* prop );
    void                                UpdateUserControlledImageProcessingEnableProperties( void );
    void                                UpdateUserExperience( const wxString& userExperience );
    bool                                ShowOutdatedFirmwareMessage( Device* pDev, bool boRebootMsg = false );
    bool                                ShowRescueModeFirmwareMessageAndStartDeviceConfigure( Device* pDev );
    void                                ToggleCurrentDevice( void );
    bool                                WeekPassedSinceLastUpdatesCheck( void ) const;
    void                                Wizard_AOI( void );
    void                                Wizard_ColorCorrection( void );
    void                                Wizard_FileAccessControl( bool boUpload );
    void                                Wizard_LensControl( void );
    void                                Wizard_LUTControl( void );
    void                                Wizard_MultiAOI( void );
    void                                Wizard_MultiCoreAcquisition( void );
    void                                Wizard_PseudoHighQualityWhiteBalance( void );
    void                                Wizard_QuickSetup( bool boAutoConfigureOnStart );
    void                                Wizard_SequencerControl( void );
};

#endif // PropViewFrameH
