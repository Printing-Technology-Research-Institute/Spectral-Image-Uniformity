//-----------------------------------------------------------------------------
#ifndef ValuesFromUserDlgH
#define ValuesFromUserDlgH ValuesFromUserDlgH
//-----------------------------------------------------------------------------
#include "DevData.h"
#include <map>
#include <vector>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>

#include <apps/Common/wxIncludePrologue.h>
#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/imaglist.h>
#include <wx/spinctrl.h>
#include <wx/treectrl.h>
#include <apps/Common/wxIncludeEpilogue.h>

class wxCheckListBox;
class wxSpinCtrl;
class ImageCanvas;
class PropData;
class PropGridFrameBase;
class PropViewFrame;

typedef std::map<wxString, wxString> StringToStringMap;

std::string BinaryDataFromString( const std::string& value );
std::string BinaryDataToString( const std::string& value );

wxImage PNGToIconImage( const void* pData, const size_t dataSize, bool useSmallIcons );

wxDECLARE_EVENT( latestPackageDownloaded, wxCommandEvent );

//-----------------------------------------------------------------------------
template<class _Tx, typename _Ty>
wxString ReadStringDict( _Tx prop, wxArrayString& choices )
//-----------------------------------------------------------------------------
{
    wxString currentValue;
    if( prop.isValid() )
    {
        currentValue = ConvertedString( prop.readS() );
        std::vector<std::pair<std::string, _Ty> > dict;
        prop.getTranslationDict( dict );
        const typename std::vector<std::pair<std::string, _Ty> >::size_type cnt = dict.size();
        for( typename std::vector<std::pair<std::string, _Ty> >::size_type i = 0; i < cnt; i++ )
        {
            choices.Add( ConvertedString( dict[i].first ) );
        }
    }
    return currentValue;
}

void WriteFile( const void* pBuf, size_t bufSize, const wxString& pathName, wxTextCtrl* pTextCtrl );

//-----------------------------------------------------------------------------
class HEXStringValidator : public wxTextValidator
//-----------------------------------------------------------------------------
{
public:
    HEXStringValidator( wxString* valPtr = NULL );
};

//-----------------------------------------------------------------------------
class OkAndCancelDlg : public wxDialog
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()
protected:
    wxButton*                   pBtnApply_;
    wxButton*                   pBtnCancel_;
    wxButton*                   pBtnOk_;

    virtual void OnBtnApply( wxCommandEvent& ) {}
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        EndModal( wxID_CANCEL );
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        EndModal( wxID_OK );
    }
    void AddButtons( wxWindow* pWindow, wxSizer* pSizer, bool boCreateApplyButton = false );
    void FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer );
    //-----------------------------------------------------------------------------
    enum TWidgetIDs
    //-----------------------------------------------------------------------------
    {
        widBtnOk = 1,
        widBtnCancel = 2,
        widBtnApply = 3,
        widFirst = 100
    };
public:
    explicit OkAndCancelDlg( wxWindow* pParent, wxWindowID id, const wxString& title, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxDEFAULT_DIALOG_STYLE, const wxString& name = wxT( "OkAndCancelDlg" ) );
};

//-----------------------------------------------------------------------------
struct ValueData
//-----------------------------------------------------------------------------
{
    wxString caption_;
    explicit ValueData( const wxString& caption ) : caption_( caption ) {}
    virtual ~ValueData() {}
};

//-----------------------------------------------------------------------------
struct ValueRangeData : public ValueData
//-----------------------------------------------------------------------------
{
    int min_;
    int max_;
    int inc_;
    int def_;
    explicit ValueRangeData( const wxString& caption, int min, int max, int inc, int def ) : ValueData( caption ), min_( min ), max_( max ), inc_( inc ), def_( def ) {}
};

//-----------------------------------------------------------------------------
struct ValueChoiceData : public ValueData
//-----------------------------------------------------------------------------
{
    wxArrayString choices_;
    explicit ValueChoiceData( const wxString& caption, const wxArrayString& choices ) : ValueData( caption ), choices_( choices ) {}
};

//-----------------------------------------------------------------------------
class ValuesFromUserDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    std::vector<wxControl*>     ctrls_;
    std::vector<int>            userInputData_;
public:
    explicit ValuesFromUserDlg( wxWindow* pParent, const wxString& title, const std::vector<ValueData*>& inputData );
    const std::vector<wxControl*>& GetUserInputControls( void ) const
    {
        return ctrls_;
    }
};

//-----------------------------------------------------------------------------
class SettingHierarchyDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    void PopulateTreeCtrl( wxTreeCtrl* pTreeCtrl, wxTreeItemId currentItem, const wxString& currentItemName, const StringToStringMap& settingRelationships );
public:
    explicit SettingHierarchyDlg( wxWindow* pParent, const wxString& title, const StringToStringMap& settingRelationships );
};

//-----------------------------------------------------------------------------
class DetailedRequestInformationDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()

    //-----------------------------------------------------------------------------
    enum TWidgetIDs_DetailedRequestInformation
    //-----------------------------------------------------------------------------
    {
        widSCRequestSelector = widFirst
    };

    mvIMPACT::acquire::FunctionInterface* pFI_;
    wxTreeCtrl* pTreeCtrl_;
    wxSpinCtrl* pSCRequestSelector_;

    void OnSCRequestSelectorChanged( wxSpinEvent& )
    {
        SelectRequest( pSCRequestSelector_->GetValue() );
    }
    void OnSCRequestSelectorTextChanged( wxCommandEvent& )
    {
        SelectRequest( pSCRequestSelector_->GetValue() );
    }
    void PopulateTreeCtrl( wxTreeCtrl* pTreeCtrl, wxTreeItemId parent, Component itComponent );
    void PopulateTreeCtrl( wxTreeCtrl* pTreeCtrl, const int requestNr );
    void SelectRequest( const int requestNr );
public:
    explicit DetailedRequestInformationDlg( wxWindow* pParent, const wxString& title, mvIMPACT::acquire::FunctionInterface* pFI );
};

//-----------------------------------------------------------------------------
class DriverInformationDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()

    typedef std::map<std::string, std::string> IgnoredInterfacesInfo;

    //-----------------------------------------------------------------------------
    enum TListItemStates
    //-----------------------------------------------------------------------------
    {
        lisUndefined = 0,
        lisChecked = 1,
        lisUnchecked = 2
    };

    //-----------------------------------------------------------------------------
    enum TWidgetIDs_DriverInformationDlg
    //-----------------------------------------------------------------------------
    {
        widTCtrl = widFirst,
        widTClose
    };

    //-----------------------------------------------------------------------------
    class MyTreeItemData : public wxTreeItemData
    //-----------------------------------------------------------------------------
    {
    public:
        //-----------------------------------------------------------------------------
        enum TElementType
        //-----------------------------------------------------------------------------
        {
            etProducer,
            etTechnology,
            etInterface
        };
        explicit MyTreeItemData( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule, TElementType type ) : wxTreeItemData(),
            pSystemModule_( pSystemModule ), type_( type ), propertyToValueMap_() {}
        mvIMPACT::acquire::GenICam::SystemModule* pSystemModule_;
        TElementType type_;
        std::map<std::string, std::string> propertyToValueMap_;
    };

    std::vector<std::string>                                technologiesToHandleSpeciallyInOurProducers_;
    mvIMPACT::acquire::GenICam::GenTLDriverConfigurator*    pGenTLDriverConfigurator_;
    std::vector<mvIMPACT::acquire::GenICam::SystemModule*>  systemModules_;
    wxTreeCtrl*                                             pTreeCtrl_;
    wxImageList*                                            pIconList_;
    IgnoredInterfacesInfo                                   ignoredInterfacesInfo_;
    IgnoredInterfacesInfo                                   ignoredInterfacesInfoOriginal_;
    TInterfaceEnumerationBehaviour                          masterEnumerationBehaviourOriginal_;
    wxString                                                newestMVIAVersionAvailable_;
    wxString                                                newestMVIAVersionInstalled_;

    wxTreeItemId AddComponentListToList( wxTreeItemId parent, mvIMPACT::acquire::ComponentLocator locator, const char* pName );
    wxTreeItemId AddKnownTechnologyTreeItem( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule, wxTreeItemId producerRootItem, wxTreeItemId treeItem, std::map<wxString, wxTreeItemId>& interfaceTypeMap, const wxString& technology, const std::string& technologyShort );
    void AddStringPropToList( wxTreeItemId parent, mvIMPACT::acquire::ComponentLocator locator, const char* pName );
    static void AppendStringFromProperty( wxString& s, Property& p );
    static void AppendStringFromString( wxString& s, const std::string& toAppend );
    void ApplyGenTLDriverConfigurationChanges( wxTreeItemId treeItem );
    static wxString BuildInterfaceLabel( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule, mvIMPACT::acquire::GenICam::InterfaceModule& im, const std::string& interfaceID );
    wxTreeItemId CreateAndConfigureInterfaceTreeItem( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule, mvIMPACT::acquire::GenICam::InterfaceModule& im, const std::string& interfaceID, wxTreeItemId interfaceTypeTreeItem );
    std::map<wxString, std::map<wxString, bool> > EnumerateGenICamDevices( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule, unsigned int interfaceIndex, const DeviceManager& devMgr );
    std::string GetProducerFileName( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule ) const;
    void PopulateTreeCtrl( mvIMPACT::acquire::Component itDrivers, const mvIMPACT::acquire::DeviceManager& devMgr );
    void ReadIgnoredInterfacesInfoAndEnableAllInterfaces( void );
    void UpdateIgnoredInterfacesSettings( IgnoredInterfacesInfo& interfacesInfo );
    void SetProducerUndefinedStateIfConflictingInterfaceStates( const wxTreeItemId& itemId );
    void SetTechnologyUndefinedStateIfConflictingInterfaceStates( const wxTreeItemId& itemId );
    void OnItemStateClick( wxTreeEvent& event );
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        UpdateIgnoredInterfacesSettings( ignoredInterfacesInfoOriginal_ );
        EndModal( wxID_CANCEL );
    }
    virtual void OnClose( wxCloseEvent& )
    {
        UpdateIgnoredInterfacesSettings( ignoredInterfacesInfoOriginal_ );
        EndModal( wxID_CANCEL );
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        UpdateIgnoredInterfacesSettings( ignoredInterfacesInfo_ );
        ApplyGenTLDriverConfigurationChanges( pTreeCtrl_->GetRootItem() );
        EndModal( wxID_OK );
    }
    static std::string TreeItemLabelToTechnologyString( const wxString& label );
public:
    explicit DriverInformationDlg( wxWindow* pParent, const wxString& title, mvIMPACT::acquire::Component itDrivers,
                                   const mvIMPACT::acquire::DeviceManager& devMgr, const wxString& newestMVIAVersionAvailable, const wxString& newestMVIAVersionInstalled );
    ~DriverInformationDlg();
};

#ifdef BUILD_WITH_UPDATE_DIALOG_SUPPORT
//-----------------------------------------------------------------------------
class UpdatesInformationDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()

    wxWindow* pParent_;
    const StringToStringMap olderDriverVersions_;
    const wxString currentVersion_;
    const wxString newestVersion_;
    const wxString dateReleased_;
    const wxString whatsNew_;
    wxString packageToDownload_;
    wxString fileName_;
    wxCheckBox* pCBAutoCheckForUpdatesWeekly_;
    wxPlatformInfo platformInfo_;
    std::map<int, wxString> packageIDs_;
    std::map<wxString, wxString> packageNames_;

    virtual void OnBtnApply( wxCommandEvent& );
    virtual void OnClose( wxCloseEvent& )
    {
        EndModal( wxID_CANCEL );
    }
    void DownloadPackageThread( void );
public:
    explicit UpdatesInformationDlg( wxWindow* pParent, const wxString& title, const StringToStringMap& olderDriverVersions,
                                    const wxString& currentVersion, const wxString& newestVersion, const wxString& dateReleased, const wxString& whatsNew, const bool boCurrentAutoUpdateCheckState );
    bool GetAutoCheckForUpdatesWeeklyState( void ) const
    {
        return pCBAutoCheckForUpdatesWeekly_->GetValue();
    }
    wxString GetDownloadedPackageName( void );
    wxString ParseDriverToPackageName( const wxString& driverName );
};
#endif //BUILD_WITH_UPDATE_DIALOG_SUPPORT

typedef std::map<wxString, PropData*> NameToFeatureMap;

//-----------------------------------------------------------------------------
class FindFeatureDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()
    PropGridFrameBase* pParent_;
    const NameToFeatureMap& nameToFeatureMap_;
    wxTextCtrl* pTCFeatureName_;
    wxCheckBox* pCBMatchCase_;
    wxListBox* pLBFeatureList_;
    const bool boDisplayInvisibleComponents_;
    //-----------------------------------------------------------------------------
    enum TWidgetIDs_FindFeatures
    //-----------------------------------------------------------------------------
    {
        widTCFeatureName = widFirst,
        widLBFeatureList,
        widCBMatchCase
    };
    void BuildFeatureList( wxArrayString& features, const bool boMatchCase = false, const wxString& pattern = wxEmptyString ) const;
    void OnFeatureListDblClick( wxCommandEvent& e );
    void OnFeatureListSelect( wxCommandEvent& e )
    {
        SelectFeatureInPropertyGrid( e.GetSelection() );
    }
    void OnFeatureNameTextChanged( wxCommandEvent& )
    {
        UpdateFeatureList();
    }
    void OnMatchCaseChanged( wxCommandEvent& )
    {
        UpdateFeatureList();
    }
    void SelectFeatureInPropertyGrid( const wxString& selection );
    void SelectFeatureInPropertyGrid( const int selection );
    void UpdateFeatureList( void );
public:
    explicit FindFeatureDlg( PropGridFrameBase* pParent, const NameToFeatureMap& nameToFeatureMap, const bool boMatchCaseActive, const bool boDisplayInvisibleComponents );
    bool GetMatchCase( void ) const
    {
        return pCBMatchCase_->GetValue();
    }
};

//-----------------------------------------------------------------------------
class DetailedFeatureInfoDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    wxTextCtrl* pLogWindow_;
    wxTextAttr fixedPitchStyle_;
    wxTextAttr fixedPitchStyleBold_;
    void AddFeatureInfo( const wxString& infoName, const wxString& info );
    void AddFeatureInfoFromException( const wxString& infoName, const ImpactAcquireException& e );
public:
    explicit DetailedFeatureInfoDlg( wxWindow* pParent, mvIMPACT::acquire::Component comp );
};

//-----------------------------------------------------------------------------
class BinaryDataDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()
    HEXStringValidator HEXStringValidator_;
    wxTextCtrl* pTCBinaryData_;
    wxTextCtrl* pTCAsciiData_;
    size_t lastDataLength_;
    wxTextAttr fixedPitchStyle_;
    //-----------------------------------------------------------------------------
    enum TWidgetIDs_FindFeatures
    //-----------------------------------------------------------------------------
    {
        widTCBinaryData = widFirst,
        widTCAsciiData
    };
    void OnBinaryDataTextChanged( wxCommandEvent& );
    void ReformatBinaryData( void );
    static void RemoveSeparatorChars( wxString& data );
    void UpdateAsciiData( void );
public:
    explicit BinaryDataDlg( wxWindow* pParent, const wxString& featureName, const wxString& value );
    static size_t FormatData( const wxString& data, wxString& formattedData, const int lineLength, const int fieldLength );
    wxString GetBinaryData( void ) const;
};

//-----------------------------------------------------------------------------
class AssignSettingsToDisplaysDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    std::vector<wxControl*> ctrls_;
public:
    explicit AssignSettingsToDisplaysDlg( wxWindow* pParent, const wxString& title, const std::vector<std::pair<std::string, int> >& settings, const SettingToDisplayDict& settingToDisplayDict, size_t displayCount );
    const std::vector<wxControl*>& GetUserInputControls( void ) const
    {
        return ctrls_;
    }
};

//-----------------------------------------------------------------------------
class RawImageImportDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    PropGridFrameBase* pParent_;
    wxComboBox* pCBPixelFormat_;
    wxComboBox* pCBBayerParity_;
    wxSpinCtrl* pSCWidth_;
    wxSpinCtrl* pSCHeight_;
public:
    explicit RawImageImportDlg( PropGridFrameBase* pParent, const wxString& title, const wxFileName& fileName );
    wxString GetFormat( void ) const;
    wxString GetPixelFormat( void ) const
    {
        return pCBPixelFormat_->GetValue();
    }
    wxString GetBayerParity( void ) const
    {
        return pCBBayerParity_->GetValue();
    }
    long GetWidth( void ) const;
    long GetHeight( void ) const;
};

//-----------------------------------------------------------------------------
class OptionsDlg : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    wxCheckListBox* pWarningConfiguration_;
    wxCheckListBox* pAppearanceConfiguration_;
    wxCheckListBox* pPropertyGridConfiguration_;
    wxCheckListBox* pMiscellaneousConfiguration_;
    wxCheckBox* pCBShowQuickSetupOnDeviceOpen_;

public:
    //-----------------------------------------------------------------------------
    enum TWarnings
    //-----------------------------------------------------------------------------
    {
        wWarnOnOutdatedFirmware,
        wWarnOnDamagedFirmware,
        wWarnOnReducedDriverPerformance,
#if defined(linux) || defined(__linux) || defined(__linux__)
        wWarnOnPotentialNetworkUSBBufferIssues,
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
        wWarnOnPotentialFirewallIssues,
        wMAX
    };
    //-----------------------------------------------------------------------------
    enum TAppearance
    //-----------------------------------------------------------------------------
    {
        aShowLeftToolBar,
        aShowUpperToolBar,
        aShowStatusBar,
        aMAX
    };
    //-----------------------------------------------------------------------------
    enum TPropertyGrid
    //-----------------------------------------------------------------------------
    {
        pgDisplayToolTips,
        pgUseSelectorGrouping,
        pgPreferDisplayNames,
        pgCreateEditorsWithSlider,
        pgDisplayPropertyIndicesAsHex,
        pgSynchronousMethodExecution,
        pgMAX
    };
    //-----------------------------------------------------------------------------
    enum TMiscellaneous
    //-----------------------------------------------------------------------------
    {
        mAllowFastSingleFrameAcquisition,
        mDisplayDetailedInformationOnCallbacks,
        mDisplayMethodExecutionErrors,
        mDisplayFeatureChangeTimeConsumption,
        mDisplayStatusBarTooltip,
        mMAX
    };
private:
    std::map<TWarnings, bool> initialWarningConfiguration_;
    std::map<TAppearance, bool> initialAppearanceConfiguration_;
    std::map<TPropertyGrid, bool> initialPropertyGridConfiguration_;
    std::map<TMiscellaneous, bool> initialMiscellaneousConfiguration_;
    bool boShowQuickSetupOnDeviceOpen_;

    template<typename _Ty>
    wxStaticBoxSizer* CreateCheckListBox( wxPanel* pPanel, wxBoxSizer* pTopDownSizer, wxCheckListBox** ppCheckListBox, const wxArrayString& choices, const wxString& title, const std::map<_Ty, bool>& initialConfiguration );
    template<typename _Ty>
    void RestoreCheckListBoxStateFromMap( wxCheckListBox* pCheckListBox, const std::map<_Ty, bool>& stateMap );
    template<typename _Ty>
    void StoreCheckListBoxStateToMap( wxCheckListBox* pCheckListBox, std::map<_Ty, bool>& stateMap );
public:
    explicit OptionsDlg( PropGridFrameBase* pParent,
                         const std::map<TWarnings, bool>& initialWarningConfiguration,
                         const std::map<TAppearance, bool>& initialAppearanceConfiguration,
                         const std::map<TPropertyGrid, bool>& initialPropertyGridConfiguration,
                         const std::map<TMiscellaneous, bool>& initialMiscellaneousConfiguration );

    void BackupCurrentState( void );
    wxCheckListBox* GetWarningConfiguration( void ) const
    {
        return pWarningConfiguration_;
    }
    wxCheckListBox* GetAppearanceConfiguration( void ) const
    {
        return pAppearanceConfiguration_;
    }
    wxCheckListBox* GetPropertyGridConfiguration( void ) const
    {
        return pPropertyGridConfiguration_;
    }
    wxCheckListBox* GetMiscellaneousConfiguration( void ) const
    {
        return pMiscellaneousConfiguration_;
    }
    wxCheckBox* GetShowQuickSetupOnDeviceOpenCheckBox( void ) const
    {
        return pCBShowQuickSetupOnDeviceOpen_;
    }
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        EndModal( wxID_CANCEL );
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        EndModal( wxID_OK );
    }
    void RestorePreviousState( void );
};

//-----------------------------------------------------------------------------
class WelcomeToPropViewDialog : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
protected:
    void AddButtonToSizer( wxButton* pBtn, wxBoxSizer* pTopDownSizer )
    {
        wxBoxSizer* pSizer = new wxBoxSizer( wxHORIZONTAL );
        pSizer->AddSpacer( 12 );
        pSizer->Add( pBtn, wxEXPAND );
        pSizer->AddSpacer( 12 );
        pTopDownSizer->Add( pSizer, wxSizerFlags().Expand() );
        pTopDownSizer->AddSpacer( 12 );
    }
    virtual void OnBtnApply( wxCommandEvent& )
    {
        EndModal( wxID_NO );
    }
public:
    explicit WelcomeToPropViewDialog( PropGridFrameBase* pParent );
};

//-----------------------------------------------------------------------------
enum TRecordMode
//-----------------------------------------------------------------------------
{
    rmHardDisk,
    rmSnapshot
};

//-----------------------------------------------------------------------------
struct RecordingParameters
//-----------------------------------------------------------------------------
{
    bool boActive_;
    bool boSaveMetaData_;
    wxString targetDirectory_;
    TImageFileFormat fileFormat_;
    TRecordMode recordMode_;
    explicit RecordingParameters() : boActive_( false ), boSaveMetaData_( false ), targetDirectory_(), fileFormat_( iffBMP ), recordMode_( rmHardDisk ) {}
};

//-----------------------------------------------------------------------------
struct VideoStreamRecordingParameters
//-----------------------------------------------------------------------------
{
    bool boEnableRecording_;
    bool boActive_;
    bool boStopAcquire_;
    bool boNotOverwriteFile_;
    bool boStartReady_;
    bool boPauseReady_;
    bool boStopReady_;
    wxString targetFile_;
    wxString prevTargetFile_;
    wxString tempTargetFile_;
    TImageDestinationPixelFormat pixelFormat_;
    TVideoCodec codec_;
    unsigned int compression_;
    unsigned int bitrate_;
    explicit VideoStreamRecordingParameters() : boEnableRecording_( false ), boActive_( false ), boStopAcquire_( false ),
        boNotOverwriteFile_( false ), boStartReady_( true ), boPauseReady_( false ), boStopReady_( false ), targetFile_(),
        prevTargetFile_(), tempTargetFile_(), pixelFormat_(), codec_(), compression_(), bitrate_() {}
};

//-----------------------------------------------------------------------------
class SetupRecording : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()

    PropViewFrame* pParent_;
    RecordingParameters& rRecordingParameters_;
    bool boAcquisitionStateOnCancel_;
private:
    //-----------------------------------------------------------------------------
    enum TWidgetIDs_SetupRecording
    //-----------------------------------------------------------------------------
    {
        widMainFrame = widFirst,
        widCBEnableRecordingMode,
        widCBEnableMetaSaving,
        widLBChooseOutputFormat,
        widBtnDirPicker
    };

    wxTextCtrl*                         pTCSetupHints_;
    wxCheckBox*                         pCBEnableRecordingMode_;
    wxCheckBox*                         pCBEnableMetaSaving_;
    wxListBox*                          pLBChooseOutputFormat_;
    wxButton*                           pBtnDirPicker_;
    wxBoxSizer*                         pTopDownSizer_;
    wxStaticText*                       pSelectedOutputDirectory_;
    wxTextAttr                          redAndBoldFont_;
    wxTextAttr                          greenAndBoldFont_;
    bool                                boGUICreated_;

    void CreateGUI( void );
    void ApplyRecordingSettings();
    void GetCurrentRecordingSettings()
    {
        ReadOutputFileFormat();
        ReadMetaDataSavingEnabled();
        ReadRecordingStatusEnabled();
    }
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        Hide();
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        ApplyRecordingSettings();
        Hide();
    }
    virtual void OnBtnApply( wxCommandEvent& )
    {
        ApplyRecordingSettings();
    }
    void OnBtnDirPicker( wxCommandEvent& )
    {
        SelectOutputDirectory();
    }
    void ReadOutputFileFormat();
    void ReadMetaDataSavingEnabled();
    void ReadRecordingStatusEnabled();
    void CheckRecordingDirNotEmpty();
    void WriteOutputFileFormat();
    void WriteMetaDataSavingEnabled();
    void WriteRecordingStatusEnabled();
    void WriteHardDiskInfoMessage( wxTextCtrl*, wxFont, wxTextAttr );
    void WriteSnapshotInfoMessage( wxTextCtrl*, wxFont, wxTextAttr );
    void SelectOutputDirectory();

    void OnClose( wxCloseEvent& e );

public:
    explicit SetupRecording( PropViewFrame* pParent, const wxString& title, RecordingParameters& rRecordingParameters );
};

//-----------------------------------------------------------------------------
class SetupVideoStreamRecording : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()

    PropViewFrame* pParent_;
    VideoStreamRecordingParameters& rVideoStreamRecordingParameters_;
public:
    void EnableSetupDialog( bool boEnableDialog );
private:
    //-----------------------------------------------------------------------------
    enum TWidgetIDs_SetupVideoStreamRecording
    //-----------------------------------------------------------------------------
    {
        widVSRMainFrame = widFirst,
        widCBEnableRecording,
        widBtnFilePicker,
        widLBChoosePixelFormat,
        widLBChooseCodec,
        widChooseCompression,
        widChooseBitrate,
        widCBEnableStopAcquire,
        widCBEnableOverwriteFile
    };

    wxTextCtrl*                         pTCVSRSetupHints_;
    wxCheckBox*                         pCBEnableRecording_;
    wxCheckBox*                         pCBEnableStopAcquire_;
    wxCheckBox*                         pCBNotOverwriteFile_;
    wxListBox*                          pLBChoosePixelFormat_;
    wxListBox*                          pLBChooseCodec_;
    wxButton*                           pBtnFilePicker_;
    wxSpinCtrl*                         pSPChooseCompression_;
    wxSpinCtrl*                         pSPChooseBitrate_;
    wxBoxSizer*                         pTopDownSizer_;
    wxStaticText*                       pSelectedOutputFile_;
    wxTextAttr                          redAndBoldFont_;
    wxTextAttr                          greenAndBoldFont_;
    bool                                boGUICreated_;
    int                                 pixelFormatIndex_;
    int                                 codecIndex_;
    bool                                boLBChoosePixelFormatEnabled_;
    bool                                boLBChooseCodecEnabled_;
    bool                                boSPChooseCompressionEnabled_;
    bool                                boSPChooseBitrateEnabled_;
    bool                                boCBStopAcquireEnabled_;
    bool                                boCBOverwriteFileEnabled_;

    void CreateGUI( void );
    void ApplyRecordingSettings();
    void SelectOutputFile();
    void WriteTargetFile();
    void WritePixelFormat();
    void WriteCodec();
    void WriteCompression();
    void WriteBitrate();
    void CheckStopAcquire();
    void CheckOverwriteFile();
    void ActivateVideoStreamRecording();
    void DeactivateVideoStreamRecording();
    void DiscardSettings();
    void ChangeRecordingMode();
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        Hide();
        DiscardSettings();
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        ApplyRecordingSettings();
        Hide();
    }
    virtual void OnBtnApply( wxCommandEvent& )
    {
        ApplyRecordingSettings();
    }
    void OnCBEnableRecording( wxCommandEvent& )
    {
        ChangeRecordingMode();
    }
    void OnBtnFilePicker( wxCommandEvent& )
    {
        SelectOutputFile();
        ActivateVideoStreamRecording();
    }
    void WriteInfoMessage( wxTextCtrl*, wxFont, wxTextAttr );
    void OnClose( wxCloseEvent& e );

public:
    explicit SetupVideoStreamRecording( PropViewFrame* pParent, const wxString& title, VideoStreamRecordingParameters& rVideoStreamRecordingParameters );
};

#endif // ValuesFromUserDlgH
