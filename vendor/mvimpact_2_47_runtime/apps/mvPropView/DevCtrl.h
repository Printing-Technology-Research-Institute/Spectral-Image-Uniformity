//-----------------------------------------------------------------------------
#ifndef DevCtrlH
#define DevCtrlH DevCtrlH
//-----------------------------------------------------------------------------
#include "DevData.h"
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include "PropTree.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/string.h>
#include <wx/thread.h>
#include <apps/Common/wxIncludeEpilogue.h>

class wxPropertyGridPage;

typedef std::map<mvIMPACT::acquire::Device*, DeviceData*> DevToTreeMap;

//-----------------------------------------------------------------------------
enum TActiveIssue
//-----------------------------------------------------------------------------
{
    aiTimeout,
    aiDeviceRegistersInvalid
};
//-----------------------------------------------------------------------------
struct ActiveIssue
//-----------------------------------------------------------------------------
{
    TActiveIssue warning_;
    wxString message_;
    wxColour messageTextColour_;
    wxString propertyToSelect_;
    explicit ActiveIssue( TActiveIssue warning, const wxString& message, const wxColour messageTextColour, const wxString& propertyToSelect = wxEmptyString ) : warning_( warning ), message_( message ), messageTextColour_( messageTextColour ), propertyToSelect_( propertyToSelect ) {}
};

//-----------------------------------------------------------------------------
inline bool operator==( const ActiveIssue& lhs, const ActiveIssue& rhs )
//-----------------------------------------------------------------------------
{
    return ( lhs.warning_ == rhs.warning_ ) &&
           ( lhs.message_ == rhs.message_ );
}

//-----------------------------------------------------------------------------
class DevicePropertyHandler
//-----------------------------------------------------------------------------
{
public:
    DevicePropertyHandler( wxPropertyGridPage* pPGDevice, bool boDisplayDebugInfo, bool boDisplayFullTree, bool boDisplayInvisibleComponents );
    ~DevicePropertyHandler();
    enum TViewMode
    {
        vmStandard,
        vmDeveloper,
        vmLAST
    };
    void                                        CloseDriver( const wxString& name );
    bool                                        DoesActiveDeviceSupportWizard( TWizardIDs wID ) const;
    mvIMPACT::acquire::Device*                  GetActiveDevice( mvIMPACT::acquire::FunctionInterface** ppFI = 0, const mvIMPACT::acquire::Statistics** ppS = 0, CaptureThread** ppCT = 0 ) const;
    mvIMPACT::acquire::Property                 GetActiveDeviceAcquisitionMode( void ) const;
    mvIMPACT::acquire::PropertyI64              GetActiveDeviceAcquisitionFrameCount( void ) const;
    mvIMPACT::acquire::PropertyI64              GetActiveDeviceAcquisitionMemoryFrameCount( void ) const;
    mvIMPACT::acquire::PropertyIBoolean         GetActiveDeviceDeviceRegistersValid( void ) const;
    void                                        GetActiveDeviceFeatureVsTimePlotInfo( Component& feature, wxString& featureFullPath ) const;
    long                                        GetActiveDeviceImagesPerDisplayCount( void ) const;
    const SequencerSetToDisplayMap              GetActiveDeviceSequencerSetToDisplayMap( void ) const;
    const SettingToDisplayDict                  GetActiveDeviceSettingToDisplayDict( void ) const;
    const WizardFeatureMap                      GetActiveDeviceSupportedWizards( void ) const;
    mvIMPACT::acquire::labs::VideoStream*       GetActiveDeviceVideoStream( void ) const;
    bool                                        GetDeviceData( mvIMPACT::acquire::Device* pDev, mvIMPACT::acquire::FunctionInterface** ppFI = 0, const mvIMPACT::acquire::Statistics** ppS = 0, CaptureThread** ppCT = 0 ) const;
    const mvIMPACT::acquire::DeviceManager&     GetDevMgr( void ) const
    {
        return m_devMgr;
    }
    EDisplayFlags                               GetDisplayFlags( void ) const;
    TViewMode                                   GetViewMode( void ) const
    {
        return m_viewMode;
    }
    static bool                                 ManufacturerStringUsesMVFormat( const wxString& manufacturerSpecificInformation, wxArrayString* pTokens = 0 )
    {
        const wxArrayString tokens = wxSplit( manufacturerSpecificInformation, wxT( '-' ) );
        if( pTokens )
        {
            *pTokens = tokens;
        }
        if( tokens.Count() == 4 )
        {
            if( ( tokens[0].length() == 3 ) &&
                ( tokens[1].length() == 6 ) &&
                ( tokens[2].length() == 6 ) &&
                ( tokens[3].length() == 4 ) )
            {
                return true;
            }
        }
        return false;
    }
    static bool                                 IsBlueCOUGAR_X_XAS( mvIMPACT::acquire::Device* pDev )
    {
        const wxString token = GetProductFromManufacturerInfo( pDev );
        if( token.StartsWith( wxT( "X" ) ) && token.Contains( wxT( "9XAS1" ) ) ) // initial nomenclature
        {
            return true;
        }
        return IsSpecificType( token, "XAS" );
    }
    static bool                                 IsBlueCOUGAR_X( mvIMPACT::acquire::Device* pDev );
    static bool                                 IsBlueCOUGAR_XT( mvIMPACT::acquire::Device* pDev );
    static bool                                 IsBlueFOX3_XAS( mvIMPACT::acquire::Device* pDev )
    {
        const wxString token = GetProductFromManufacturerInfo( pDev );
        if( token.StartsWith( wxT( "2" ) ) && token.Contains( wxT( "9XAS1" ) ) ) // initial nomenclature
        {
            return true;
        }
        return IsSpecificType( token, "XAS" );
    }
    static bool                                 IsBlueNAOS( Device* pDev );
    static bool                                 IsSpecificType( const wxString& manufacturerSpecificInformation, const wxString& typeSuffix )
    {
        wxArrayString tokens;
        if( ManufacturerStringUsesMVFormat( manufacturerSpecificInformation, &tokens ) )
        {
            return tokens[3].find( typeSuffix ) == 0;
        }
        return false;
    }
    wxString                                    OpenDriver( const wxString& name, wxWindow* pParentWindow, unsigned int pendingImageQueueDepth );
    void                                        SetActiveDevice( const wxString& name );
    void                                        SetActiveDeviceFeatureVsTimePlotInfo( Component feature, const wxString& featureFullPath );
    void                                        SetActiveDeviceImagesPerDisplayCount( long imagesPerDisplayCount );
    void                                        SetActiveDeviceSequencerSetToDisplayMap( const SequencerSetToDisplayMap& m );
    void                                        SetActiveDeviceSettingToDisplayDict( const SettingToDisplayDict& m );
    void                                        SetActiveDeviceVideoStream( mvIMPACT::acquire::labs::VideoStream* pVideoStream );
    void                                        SetViewMode( TViewMode mode, bool boUseHexIndices, bool boUseDisplayNames, bool boUseSelectorGrouping );
    void                                        ValidateTrees( bool boForceCleanup = false );
private:
    unsigned int                                m_actDevListChangedCounter;
    unsigned int                                m_actDrvListChangedCounter;
    bool                                        m_boDisplayDebugInfo;
    bool                                        m_boDisplayInvisibleComponents;
    mvIMPACT::acquire::TComponentVisibility     m_componentVisibility;
    unsigned int                                m_fullTreeChangedCounter;
    PropTree*                                   m_pFullTree;
    mvIMPACT::acquire::DeviceManager            m_devMgr;
    DevToTreeMap                                m_devToTreeMap;
    DeviceData*                                 m_pActiveDevData;
    mvIMPACT::acquire::Device*                  m_pActiveDevice;
    wxPropertyGridPage*                         m_pPGDevice;
    mutable wxCriticalSection                   m_critSect;
    TViewMode                                   m_viewMode;
    bool                                        m_boUseHexIndices;
    bool                                        m_boUseDisplayNames;
    bool                                        m_boUseSelectorGrouping;
    static void                                 AddFeatureToSetIfValid( std::set<HOBJ>& s, Component& c );
    void                                        GetInterfaceClasses( const DeviceData* p, mvIMPACT::acquire::FunctionInterface** ppFI = 0, const mvIMPACT::acquire::Statistics** ppS = 0, CaptureThread** ppCT = 0 ) const;
    static wxString                             GetProductFromManufacturerInfo( mvIMPACT::acquire::Device* pDev );
    void                                        ReCreateDriverTrees( void );
    void                                        ReCreateFullTree( void );
    void                                        CheckForWizards( mvIMPACT::acquire::Device* pDev, DeviceData* pDevData );
    void                                        UpdateTree( PropTree* pTree, unsigned int& actChangedCounter, const bool boForceRedraw, const bool boForceCleanup );
    void                                        UpdateTreeMap( void );
    void                                        CreateDriverTrees( DevToTreeMap::iterator& it );
    void                                        DeleteDriverTrees( DevToTreeMap::iterator& it );
    void                                        GetActiveDevicePropTreeExpansionData( wxPropertyGrid* pPropGrid, std::set<wxString>& s, wxString& selectedGridProperty ) const;
    void                                        SetActiveDevicePropTreeExpansionData( wxPropertyGrid* pPropGrid, std::set<wxString>& s, wxString& selectedGridProperty );
    void                                        PrepareTreeArray( void );
    void                                        PrepareTreeArrayCommon( void );

    struct TreeProp
    {
        TreeProp( const std::string& listName, const std::string& displayedName ) : ListName( listName ),
            DisplayName( displayedName ), ChangedCounter( 0 ), ListType( mvIMPACT::acquire::dltUndefined ) {}
        TreeProp( mvIMPACT::acquire::TDeviceListType listType, const std::string& displayedName ) : ListName( "" ),
            DisplayName( displayedName ), ChangedCounter( 0 ), ListType( listType ) {}
        std::string                         ListName;
        std::string                         DisplayName;
        unsigned int                        ChangedCounter;
        mvIMPACT::acquire::TDeviceListType  ListType;
    };
    std::vector<TreeProp> m_vTreeProps;
};

#endif // DevCtrlH
