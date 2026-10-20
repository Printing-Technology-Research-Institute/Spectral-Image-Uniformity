//-----------------------------------------------------------------------------
#ifndef DevDataH
#define DevDataH DevDataH
//-----------------------------------------------------------------------------
#include <apps/Common/wxIncluder.h>
#include <map>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <vector>

class CaptureThread;
class EventThread;
class ImageCanvas;
class PropTree;

wxDECLARE_EVENT( imageReadyEvent, wxCommandEvent );
wxDECLARE_EVENT( imageSkippedEvent, wxCommandEvent );
wxDECLARE_EVENT( toggleDisplayArea, wxCommandEvent );
wxDECLARE_EVENT( liveModeAborted, wxCommandEvent );
wxDECLARE_EVENT( sequenceReadyEvent, wxCommandEvent );
wxDECLARE_EVENT( imageInfoEvent, wxCommandEvent );
wxDECLARE_EVENT( refreshAOIControls, wxCommandEvent );
wxDECLARE_EVENT( refreshCurrentPixelData, wxCommandEvent );
wxDECLARE_EVENT( imageCanvasSelected, wxCommandEvent );
wxDECLARE_EVENT( imageCanvasFullScreen, wxCommandEvent );
wxDECLARE_EVENT( monitorImageClosed, wxCommandEvent );
wxDECLARE_EVENT( featureChangedCallbackReceived, wxCommandEvent );
wxDECLARE_EVENT( imageTimeoutEvent, wxCommandEvent );
wxDECLARE_EVENT( latestChangesDownloaded, wxCommandEvent );
wxDECLARE_EVENT( endFullScreenMode, wxCommandEvent );
wxDECLARE_EVENT( deviceRegistersValidChanged, wxCommandEvent );
wxDECLARE_EVENT( saveImage, wxCommandEvent );
wxDECLARE_EVENT( snapshotButtonHit, wxCommandEvent );
wxDECLARE_EVENT( methodExecutionDone, wxCommandEvent );

//-----------------------------------------------------------------------------
enum TImageDataRetrievalMode
//-----------------------------------------------------------------------------
{
    idrmDefault,
    idrmRecordedSequence
};

//-----------------------------------------------------------------------------
enum TCaptureSettingUsageMode
//-----------------------------------------------------------------------------
{
    csumManual,
    csumAutomatic
};

//-----------------------------------------------------------------------------
enum TWizardIDs
//-----------------------------------------------------------------------------
{
    wAOI,
    wColorCorrection,
    wFileAccessControl,
    wLensControl,
    wLUTControl,
    wMultiAOI,
    wMultiCoreAcquisition,
    wNone,
    wPseudoWizardWhiteBalance,
    wQuickSetup,
    wSequencerControl
};

typedef std::map<TWizardIDs, std::set<HOBJ> > WizardFeatureMap;
typedef std::vector<PropTree*> PropTreeVector;
typedef std::map<int64_type, long> SequencerSetToDisplayMap;
typedef std::map<HOBJ, std::vector<ImageCanvas*>::size_type> SettingToDisplayDict;

//-----------------------------------------------------------------------------
class DeviceRegistersValidCallback : public mvIMPACT::acquire::ComponentCallback
//-----------------------------------------------------------------------------
{
public:
    explicit DeviceRegistersValidCallback( void* pUserData = 0 ) : mvIMPACT::acquire::ComponentCallback( pUserData ) {}
    virtual void execute( mvIMPACT::acquire::Component&, void* pUserData )
    {
#ifndef __clang_analyzer__ // The static analyzer assumes this event memory to be lost :-(
        wxWindow* pParentWindow = reinterpret_cast<wxWindow*>( pUserData );
        wxQueueEvent( pParentWindow->GetEventHandler(), new wxCommandEvent( deviceRegistersValidChanged, pParentWindow->GetId() ) );
#endif // #ifndef __clang_analyzer__
    }
};

//-----------------------------------------------------------------------------
struct DeviceData
//-----------------------------------------------------------------------------
{
    DeviceData( PropTree* pDrv, PropTree* pDev ) : pDriverTree( pDrv ), pDeviceTree( pDev ), pFuncInterface( 0 ),
        pStatistics( 0 ), pInfo( 0 ), deviceRegistersValid(), pDeviceRegistersValidCallback( 0 ), acquisitionMode(), acquisitionFrameCount(), mvAcquisitionMemoryFrameCount(),
        pCaptureThread( 0 ), boWasLive( false ), lockedRequest( mvIMPACT::acquire::INVALID_ID ),
        featureVsTimePlotPath(), featureVsTimeFeature(), imagesPerDisplayCount( 1 ), expandedGridProperties(), selectedGridProperty(), pVideoStream( 0 ) {}
    PropTree*                               pDriverTree;
    PropTree*                               pDeviceTree;
    PropTreeVector                          vDriverTrees;
    mvIMPACT::acquire::FunctionInterface*   pFuncInterface;
    mvIMPACT::acquire::Statistics*          pStatistics;
    mvIMPACT::acquire::Info*                pInfo;
    mvIMPACT::acquire::PropertyIBoolean     deviceRegistersValid;
    DeviceRegistersValidCallback*           pDeviceRegistersValidCallback;
    mvIMPACT::acquire::Property             acquisitionMode;
    mvIMPACT::acquire::PropertyI64          acquisitionFrameCount;
    mvIMPACT::acquire::PropertyI64          mvAcquisitionMemoryFrameCount;
    CaptureThread*                          pCaptureThread;
    bool                                    boWasLive;
    int                                     lockedRequest;
    WizardFeatureMap                        supportedWizards;
    SequencerSetToDisplayMap                sequencerSetToDisplayMap;
    SettingToDisplayDict                    settingToDisplayDict;
    wxString                                featureVsTimePlotPath;
    mvIMPACT::acquire::Component            featureVsTimeFeature;
    long                                    imagesPerDisplayCount;
    std::set<wxString>                      expandedGridProperties;
    wxString                                selectedGridProperty;
    mvIMPACT::acquire::labs::VideoStream*   pVideoStream;
};

//-----------------------------------------------------------------------------
struct VariableValue
//-----------------------------------------------------------------------------
{
    TComponentType type;
    union
    {
        int intRep;
        int64_type int64Rep;
        double doubleRep;
    } value;
    explicit VariableValue() : type( ctPropInt )
    {
        value.intRep = 0;
    }
};

//-----------------------------------------------------------------------------
struct RequestInfoData
//-----------------------------------------------------------------------------
{
    VariableValue plotValue_;
    int64_type frameNr_;
    int exposeTime_us_;
    double gain_dB_;
    int64_type timeStamp_us_;
    HOBJ settingUsed_;
    TRequestResult requestResult_;
    int64_type chunkSequencerSetActive_;
    explicit RequestInfoData() : plotValue_(), frameNr_( 0 ), exposeTime_us_( 0 ), gain_dB_( 0.0 ), timeStamp_us_( 0LL ),
        settingUsed_( INVALID_ID ), requestResult_( rrOK ), chunkSequencerSetActive_( 0LL ) {}
};

//-----------------------------------------------------------------------------
struct RequestData
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::ImageBufferDesc      image_;
    mvIMPACT::acquire::TBayerMosaicParity   bayerParity_;
    wxString                                pixelFormat_;
    RequestInfoData                         requestInfo_;
    std::vector<wxString>                   requestInfoStrings_;
    int                                     requestNr_;
    unsigned int                            bufferPartIndex_;
    static const wxString                   UNKNOWN_PIXEL_FORMAT_STRING_;
    bool                                    boBufferContainsPackedJpeg_;
    explicit RequestData();
};

void SetRequestDataFromWxImage( RequestData& requestData, const wxImage& image );

#endif // DevDataH
