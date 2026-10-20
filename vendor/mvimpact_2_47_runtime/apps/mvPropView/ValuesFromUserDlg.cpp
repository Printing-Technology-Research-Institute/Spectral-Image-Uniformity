//-----------------------------------------------------------------------------
#include <apps/Common/CommonGUIFunctions.h>
#include <apps/Common/exampleHelper.h>
#ifdef BUILD_WITH_UPDATE_DIALOG_SUPPORT
#   include <apps/Common/ProxyResolverContext.h>
#endif //BUILD_WITH_UPDATE_DIALOG_SUPPORT
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "GlobalDataStorage.h"
#include "icons.h"
#include "PropData.h"
#include "PropViewFrame.h"
#include "ValuesFromUserDlg.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/arrstr.h>
#include <wx/checklst.h>
#include <wx/file.h>
#include <wx/listbox.h>
#include <wx/progdlg.h>
#include <wx/sstream.h>
#ifdef BUILD_WITH_UPDATE_DIALOG_SUPPORT
#   include <wx/url.h>
#endif //BUILD_WITH_UPDATE_DIALOG_SUPPORT
#include <wx/wfstream.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;
using namespace mvIMPACT::acquire::GenICam;

wxDEFINE_EVENT( latestPackageDownloaded, wxCommandEvent );

//-----------------------------------------------------------------------------
string BinaryDataFromString( const string& value )
//-----------------------------------------------------------------------------
{
    const string::size_type valueLength = value.size();
    const string::size_type bufSize = ( valueLength + 1 ) / 2;
    string binaryValue;
    binaryValue.resize( bufSize );
    for( string::size_type i = 0; i < bufSize; i++ )
    {
        string token( value.substr( i * 2, ( ( ( i * 2 ) + 2 ) > valueLength ) ? 1 : 2 ) );
        unsigned int tmp;
#if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
        sscanf_s( token.c_str(), "%x", &tmp );
#else
        sscanf( token.c_str(), "%x", &tmp );
#endif // #if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
        binaryValue[i] = static_cast<string::value_type>( tmp );
    }
    return binaryValue;
}

//-----------------------------------------------------------------------------
string BinaryDataToString( const string& value )
//-----------------------------------------------------------------------------
{
    const string::size_type len = value.size();
    string result;
    result.resize( len * 2 );
    const size_t BUF_SIZE = 3;
    char buf[BUF_SIZE];
    for( string::size_type i = 0; i < len; i++ )
    {
#if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
        sprintf_s( buf, 3, "%02x", static_cast<unsigned char>( value[i] ) );
#else
        sprintf( buf, "%02x", static_cast<unsigned char>( value[i] ) );
#endif // #if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
        result[i * 2]   = buf[0];
        result[i * 2 + 1] = buf[1];
    }
    return result;
}

//-----------------------------------------------------------------------------
wxImage PNGToIconImage( const void* pData, const size_t dataSize, bool useSmallIcons )
//-----------------------------------------------------------------------------
{
    wxImage image( wxBitmap::NewFromPNGData( pData, dataSize ).ConvertToImage() );
    return useSmallIcons ? image.Scale( 16, 16 ) : image.Scale( 32, 32 );
}

//-----------------------------------------------------------------------------
void WriteFile( const void* pBuf, size_t bufSize, const wxString& pathName, wxTextCtrl* pTextCtrl )
//-----------------------------------------------------------------------------
{
    wxFile file( pathName.c_str(), wxFile::write );
    if( !file.IsOpened() )
    {
        WriteToTextCtrl( pTextCtrl, wxString::Format( wxT( "Storing of %s failed. Could not open file.\n" ), pathName.c_str() ), wxTextAttr( *wxRED ) );
        return;
    }
    size_t bytesWritten = file.Write( pBuf, bufSize );
    if( bytesWritten < bufSize )
    {
        WriteToTextCtrl( pTextCtrl, wxString::Format( wxT( "Storing of %s failed. Could not write all data.\n" ), pathName.c_str() ), wxTextAttr( *wxRED ) );
        return;
    }
    WriteToTextCtrl( pTextCtrl, wxString::Format( wxT( "Storing of %s was successful.\n" ), pathName.c_str() ) );
}

//=============================================================================
//================= Implementation CollectDeviceInformationThread =============
//=============================================================================
//------------------------------------------------------------------------------
class CollectDeviceInformationThread : public wxThread
//------------------------------------------------------------------------------
{
    const mvIMPACT::acquire::DeviceManager& devMgr_;
    mvIMPACT::acquire::GenICam::InterfaceModule im_;
    map<wxString, bool> deviceInformation_;
protected:
    void* Entry( void )
    {
        try
        {
            if( im_.deviceID.isValid() && im_.deviceSelector.isValid() )
            {
                if( im_.deviceID.readS().length() > 0 ) // even if there is NO device, the deviceSelector will contain one 'empty' entry...
                {
                    const int64_type deviceCount = im_.deviceSelector.getMaxValue() + 1;
                    for( int64_type i = 0; i < deviceCount; i++ )
                    {
                        im_.deviceSelector.write( i );
                        const string serial( im_.deviceSerialNumber.isValid() ? im_.deviceSerialNumber.readS() : im_.deviceID.readS() );
                        const Device* const pDev = devMgr_.getDeviceBySerial( serial );
                        const bool boInUse = pDev && pDev->isInUse();
                        deviceInformation_.insert( make_pair( wxString::Format( wxT( "%s(%s)%s" ), ConvertedString( serial ).c_str(), ConvertedString( im_.deviceModelName.isValid() ? im_.deviceModelName.readS() : "UNKNOWN MODEL" ).c_str(), boInUse ? wxT( " - IN USE!" ) : wxT( "" ) ), boInUse ) );
                    }
                }
            }
        }
        catch( const ImpactAcquireException& ) {}
        return 0;
    }
public:
    explicit CollectDeviceInformationThread( const mvIMPACT::acquire::DeviceManager& devMgr, mvIMPACT::acquire::GenICam::SystemModule& systemModule, unsigned int interfaceIndex ) : wxThread( wxTHREAD_JOINABLE ),
        devMgr_( devMgr ), im_( systemModule, interfaceIndex ), deviceInformation_() {}
    const pair<wxString, map<wxString, bool> > GetResults( void ) const
    {
        return make_pair( ConvertedString( im_.interfaceID.read() ), deviceInformation_ );
    }
};

//=============================================================================
//================= Implementation HEXStringValidator =========================
//=============================================================================
//-----------------------------------------------------------------------------
HEXStringValidator::HEXStringValidator( wxString* valPtr /* = NULL */ ) : wxTextValidator( wxFILTER_INCLUDE_CHAR_LIST, valPtr )
//-----------------------------------------------------------------------------
{
    wxArrayString strings;
    strings.push_back( wxT( "a" ) );
    strings.push_back( wxT( "b" ) );
    strings.push_back( wxT( "c" ) );
    strings.push_back( wxT( "d" ) );
    strings.push_back( wxT( "e" ) );
    strings.push_back( wxT( "f" ) );
    strings.push_back( wxT( "A" ) );
    strings.push_back( wxT( "B" ) );
    strings.push_back( wxT( "C" ) );
    strings.push_back( wxT( "D" ) );
    strings.push_back( wxT( "E" ) );
    strings.push_back( wxT( "F" ) );
    for( unsigned int i = 0; i <= 9; i++ )
    {
        wxString s;
        s << i;
        strings.push_back( s );
    }
    SetIncludes( strings );
}

//=============================================================================
//============== Implementation OkAndCancelDlg ================================
//=============================================================================
BEGIN_EVENT_TABLE( OkAndCancelDlg, wxDialog )
    EVT_BUTTON( widBtnOk, OkAndCancelDlg::OnBtnOk )
    EVT_BUTTON( widBtnCancel, OkAndCancelDlg::OnBtnCancel )
    EVT_BUTTON( widBtnApply, OkAndCancelDlg::OnBtnApply )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
OkAndCancelDlg::OkAndCancelDlg( wxWindow* pParent, wxWindowID id, const wxString& title, const wxPoint& pos /*= wxDefaultPosition*/,
                                const wxSize& size /*= wxDefaultSize*/, long style /*= wxDEFAULT_DIALOG_STYLE*/, const wxString& name /* = "OkAndCancelDlg" */ )
    : wxDialog( pParent, id, title, pos, size, style, name ), pBtnApply_( 0 ), pBtnCancel_( 0 ), pBtnOk_( 0 )
//-----------------------------------------------------------------------------
{

}

//-----------------------------------------------------------------------------
void OkAndCancelDlg::AddButtons( wxWindow* pWindow, wxSizer* pSizer, bool boCreateApplyButton /* = false */ )
//-----------------------------------------------------------------------------
{
    // lower line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonSizer->AddStretchSpacer( 100 );
    pBtnOk_ = new wxButton( pWindow, widBtnOk, wxT( "&Ok" ) );
    pButtonSizer->Add( pBtnOk_, wxSizerFlags().Border( wxALL, 7 ) );
    pBtnCancel_ = new wxButton( pWindow, widBtnCancel, wxT( "&Cancel" ) );
    pButtonSizer->Add( pBtnCancel_, wxSizerFlags().Border( wxALL, 7 ) );
    if( boCreateApplyButton )
    {
        pBtnApply_ = new wxButton( pWindow, widBtnApply, wxT( "&Apply" ) );
        pButtonSizer->Add( pBtnApply_, wxSizerFlags().Border( wxALL, 7 ) );
    }
    pSizer->AddSpacer( 10 );
    pSizer->Add( pButtonSizer, wxSizerFlags().Expand() );
}

//-----------------------------------------------------------------------------
void OkAndCancelDlg::FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    pWindow->SetSizer( pSizer );
    pSizer->SetSizeHints( this );
    SetClientSize( pSizer->GetMinSize() );
    SetSizeHints( GetSize() );
}

//=============================================================================
//============== Implementation ValuesFromUserDlg =============================
//=============================================================================
//-----------------------------------------------------------------------------
ValuesFromUserDlg::ValuesFromUserDlg( wxWindow* pParent, const wxString& title, const vector<ValueData*>& inputData )
    : OkAndCancelDlg( pParent, wxID_ANY, title )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pGridSizer                      | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );

    wxPanel* pPanel = new wxPanel( this );

    const int colCount = static_cast<int>( inputData.size() );
    userInputData_.resize( colCount );
    ctrls_.resize( colCount );
    wxFlexGridSizer* pGridSizer = new wxFlexGridSizer( 2 );
    for( int i = 0; i < colCount; i++ )
    {
        pGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxString( wxT( " " ) ) + inputData[i]->caption_ ) );
        wxControl* p = 0;
        if( dynamic_cast<ValueRangeData*>( inputData[i] ) )
        {
            ValueRangeData* pData = dynamic_cast<ValueRangeData*>( inputData[i] );
            p = new wxSpinCtrl( pPanel, widFirst + i, wxString::Format( wxT( "%d" ), pData->def_ ), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, pData->min_, pData->max_, pData->def_ );
        }
        else if( dynamic_cast<ValueChoiceData*>( inputData[i] ) )
        {
            ValueChoiceData* pData = dynamic_cast<ValueChoiceData*>( inputData[i] );
            p = new wxComboBox( pPanel, widFirst + i, pData->choices_[0], wxDefaultPosition, wxDefaultSize, pData->choices_, wxCB_DROPDOWN | wxCB_READONLY );
        }
        else
        {
            wxASSERT( !"Invalid data type detected!" );
        }
        pGridSizer->Add( p, wxSizerFlags().Expand() );
        ctrls_[i] = p;
    }
    pTopDownSizer->Add( pGridSizer );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//=============================================================================
//============== Implementation SettingHierarchyDlg ===========================
//=============================================================================
//-----------------------------------------------------------------------------
SettingHierarchyDlg::SettingHierarchyDlg( wxWindow* pParent, const wxString& title, const StringToStringMap& settingRelationships )
    : OkAndCancelDlg( pParent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pTreeCtrl                       | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );

    wxPanel* pPanel = new wxPanel( this );

    wxTreeCtrl* pTreeCtrl = new wxTreeCtrl( pPanel, wxID_ANY );
    wxTreeItemId rootId = pTreeCtrl->AddRoot( wxT( "Base" ) );
    PopulateTreeCtrl( pTreeCtrl, rootId, wxString( wxT( "Base" ) ), settingRelationships );
    pTreeCtrl->ExpandAll();
    pTopDownSizer->Add( pTreeCtrl, wxSizerFlags( 3 ).Expand() );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//-----------------------------------------------------------------------------
void SettingHierarchyDlg::PopulateTreeCtrl( wxTreeCtrl* pTreeCtrl, wxTreeItemId currentItem, const wxString& currentItemName, const StringToStringMap& settingRelationships )
//-----------------------------------------------------------------------------
{
    StringToStringMap::const_iterator it = settingRelationships.begin();
    StringToStringMap::const_iterator itEND = settingRelationships.end();
    while( it != itEND )
    {
        if( it->second == currentItemName )
        {
            wxTreeItemId newItem = pTreeCtrl->AppendItem( currentItem, it->first );
            PopulateTreeCtrl( pTreeCtrl, newItem, it->first, settingRelationships );
        }
        ++it;
    }
}

//=============================================================================
//============== Implementation DetailedRequestInformationDlg =================
//=============================================================================

BEGIN_EVENT_TABLE( DetailedRequestInformationDlg, OkAndCancelDlg )
    EVT_SPINCTRL( widSCRequestSelector, DetailedRequestInformationDlg::OnSCRequestSelectorChanged )
#ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL // Unfortunately on Linux wxWidgets 2.6.x - ??? handling these messages will cause problems, while on Windows not doing so will not always update the GUI as desired :-(
    EVT_TEXT( widSCRequestSelector, DetailedRequestInformationDlg::OnSCRequestSelectorTextChanged )
#endif // #ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
DetailedRequestInformationDlg::DetailedRequestInformationDlg( wxWindow* pParent, const wxString& title, mvIMPACT::acquire::FunctionInterface* pFI )
    : OkAndCancelDlg( pParent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
      pFI_( pFI ), pTreeCtrl_( 0 )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pLeftRightSizer                 | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | property display                | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    wxPanel* pPanel = new wxPanel( this );

    wxBoxSizer* pLeftRightSizer = new wxBoxSizer( wxHORIZONTAL );
    pSCRequestSelector_ = new wxSpinCtrl( pPanel, widSCRequestSelector, wxT( "0" ), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, pFI_->requestCount() - 1, 1 );
    pSCRequestSelector_->SetToolTip( wxT( "Can be used to quickly move within the requests currently allocated" ) );
    pLeftRightSizer->AddSpacer( 10 );
    pLeftRightSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "Request Selector: " ) ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pLeftRightSizer->AddSpacer( 5 );
    pLeftRightSizer->Add( pSCRequestSelector_, wxSizerFlags().Expand() );

    pTreeCtrl_ = new wxTreeCtrl( pPanel, wxID_ANY );
    PopulateTreeCtrl( pTreeCtrl_, 0 );
    pTreeCtrl_->ExpandAll();
    pTopDownSizer->AddSpacer( 5 );
    pTopDownSizer->Add( pLeftRightSizer, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 5 );
    pTopDownSizer->Add( pTreeCtrl_, wxSizerFlags( 3 ).Expand() );
    pTopDownSizer->AddSpacer( 10 );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 400, 700 );
}

//-----------------------------------------------------------------------------
void DetailedRequestInformationDlg::PopulateTreeCtrl( wxTreeCtrl* pTreeCtrl, wxTreeItemId parent, Component itComponent )
//-----------------------------------------------------------------------------
{
    while( itComponent.isValid() )
    {
        if( itComponent.isList() )
        {
            wxTreeItemId newList = pTreeCtrl->AppendItem( parent, wxString::Format( wxT( "%s" ), ConvertedString( itComponent.name() ).c_str() ) );
            PopulateTreeCtrl( pTreeCtrl, newList, itComponent.firstChild() );
        }
        else if( itComponent.isProp() )
        {
            Property prop( itComponent );
            wxString data = wxString::Format( wxT( "%s%s: " ), ConvertedString( itComponent.name() ).c_str(),
                                              itComponent.isVisible() ? wxT( "" ) : wxT( " (currently invisible)" ) );
            const unsigned int valCount = prop.valCount();
            if( valCount > 1 )
            {
                data.Append( wxT( "[ " ) );
                for( unsigned int i = 0; i < valCount; i++ )
                {
                    data.Append( ConvertedString( prop.readS( static_cast<int>( i ) ) ).c_str() );
                    if( i < valCount - 1 )
                    {
                        data.Append( wxT( ", " ) );
                    }
                }
                data.Append( wxT( " ]" ) );
            }
            else
            {
                data.Append( ConvertedString( prop.readS() ).c_str() );
            }
            pTreeCtrl->AppendItem( parent, data );
        }
        else
        {
            pTreeCtrl->AppendItem( parent, wxString::Format( wxT( "%s" ), ConvertedString( itComponent.name() ).c_str() ) );
        }
        itComponent = itComponent.nextSibling();
    }
}

//-----------------------------------------------------------------------------
void DetailedRequestInformationDlg::PopulateTreeCtrl( wxTreeCtrl* pTreeCtrl, const int requestNr )
//-----------------------------------------------------------------------------
{
    Component itComponent( pFI_->getRequest( requestNr )->getComponentLocator().hObj() );
    wxTreeItemId rootId = pTreeCtrl->AddRoot( ConvertedString( itComponent.name() ) );
    itComponent = itComponent.firstChild();
    PopulateTreeCtrl( pTreeCtrl, rootId, itComponent );
}

//-----------------------------------------------------------------------------
void DetailedRequestInformationDlg::SelectRequest( const int requestNr )
//-----------------------------------------------------------------------------
{
    pTreeCtrl_->DeleteAllItems();
    PopulateTreeCtrl( pTreeCtrl_, requestNr );
    pTreeCtrl_->ExpandAll();
}

//=============================================================================
//============== Implementation DriverInformationDlg ==========================
//=============================================================================
BEGIN_EVENT_TABLE( DriverInformationDlg, OkAndCancelDlg )
    EVT_TREE_STATE_IMAGE_CLICK( widTCtrl, DriverInformationDlg::OnItemStateClick )
    EVT_CLOSE( DriverInformationDlg::OnClose )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
DriverInformationDlg::DriverInformationDlg( wxWindow* pParent, const wxString& title, Component itDrivers,
        const DeviceManager& devMgr, const wxString& newestMVIAVersionAvailable, const wxString& newestMVIAVersionInstalled )
    : OkAndCancelDlg( pParent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
      technologiesToHandleSpeciallyInOurProducers_(), pGenTLDriverConfigurator_( 0 ), systemModules_(), pTreeCtrl_( 0 ), pIconList_( new wxImageList( 16, 16, true, 3 ) ), ignoredInterfacesInfo_(),
      ignoredInterfacesInfoOriginal_(), masterEnumerationBehaviourOriginal_( iebNotConfigured ), newestMVIAVersionAvailable_( newestMVIAVersionAvailable ), newestMVIAVersionInstalled_( newestMVIAVersionInstalled )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pTreeCtrl                       | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBusyCursor busyCursorScope;
    // The next command yields control to pending messages in the windowing system. It is necessary in order
    // to get a busy cursor in Linux systems while waiting for the dialog window to appear
    wxYield();
    technologiesToHandleSpeciallyInOurProducers_.push_back( "GEV" );
    technologiesToHandleSpeciallyInOurProducers_.push_back( "U3V" );
    technologiesToHandleSpeciallyInOurProducers_.push_back( "PCI" );
    const int64_type systemModuleCount = SystemModule::getSystemModuleCount();
    for( int64_type i = 0; i < systemModuleCount; i++ )
    {
        try
        {
            systemModules_.push_back( new mvIMPACT::acquire::GenICam::SystemModule( i ) );
        }
        catch( const ImpactAcquireException& ) {}
    }
    try
    {
        pGenTLDriverConfigurator_ = new mvIMPACT::acquire::GenICam::GenTLDriverConfigurator();
    }
    catch( const ImpactAcquireException& ) {}

    {
        const wxImage checkboxUndefined( wxBitmap::NewFromPNGData( checkbox_undefined_png, sizeof( checkbox_undefined_png ) ).ConvertToImage() );
        pIconList_->Add( checkboxUndefined.Scale( 16, 16 ) );
        const wxImage checkboxChecked( wxBitmap::NewFromPNGData( checkbox_checked_png, sizeof( checkbox_checked_png ) ).ConvertToImage() );
        pIconList_->Add( checkboxChecked.Scale( 16, 16 ) );
        const wxImage checkboxUnchecked( wxBitmap::NewFromPNGData( checkbox_unchecked_png, sizeof( checkbox_unchecked_png ) ).ConvertToImage() );
        pIconList_->Add( checkboxUnchecked.Scale( 16, 16 ) );
    }

    ReadIgnoredInterfacesInfoAndEnableAllInterfaces();
    UpdateDeviceListWithProgressMessage( this, devMgr );
    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    wxPanel* pPanel = new wxPanel( this );
    pTreeCtrl_ = new wxTreeCtrl( pPanel, widTCtrl );
    pTreeCtrl_->SetStateImageList( pIconList_ );
    PopulateTreeCtrl( itDrivers, devMgr );
    // The next line is a workaround to prevent the buttons getting off-screen
    // in Linux systems, when the number of interfaces is high.
    //pTreeCtrl_->SetMaxSize( wxSize( 600, 700 ) );
    pTreeCtrl_->ExpandAll();
    pTopDownSizer->Add( pTreeCtrl_, wxSizerFlags( 1 ).Expand() );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 700, 800 );
    Center();
}

//-----------------------------------------------------------------------------
DriverInformationDlg::~DriverInformationDlg()
//-----------------------------------------------------------------------------
{
    pTreeCtrl_->SetStateImageList( 0 );
    delete pIconList_;
    for_each( systemModules_.begin(), systemModules_.end(), ptr_fun( DeleteElement<SystemModule*> ) );
}

//-----------------------------------------------------------------------------
wxTreeItemId DriverInformationDlg::AddComponentListToList( wxTreeItemId parent, mvIMPACT::acquire::ComponentLocator locator, const char* pName )
//-----------------------------------------------------------------------------
{
    ComponentList list;
    locator.bindComponent( list, string( pName ) );
    return list.isValid() ? pTreeCtrl_->AppendItem( parent, ConvertedString( list.name() ) ) : wxTreeItemId();
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::AddStringPropToList( wxTreeItemId parent, ComponentLocator locator, const char* pName )
//-----------------------------------------------------------------------------
{
    PropertyS prop;
    locator.bindComponent( prop, string( pName ) );
    if( prop.isValid() )
    {
        const wxString propName = ConvertedString( prop.name() );
        if( propName == wxString( "Version" ) )
        {
            const wxString propValue = ConvertedString( prop.read().substr( 0, prop.read().find_last_of( "." ) ) );
            if( VersionFromString( propValue ) < VersionFromString( newestMVIAVersionInstalled_ ) )
            {
                const wxTreeItemId treeItem = pTreeCtrl_->AppendItem( parent, wxString::Format( wxT( "%s: %s (It is recommended to update to at least version %s!!!)" ), propName.c_str(), propValue.c_str(), newestMVIAVersionInstalled_.c_str() ) );
                pTreeCtrl_->SetItemTextColour( treeItem, acDarkBluePastel );
                pTreeCtrl_->SetItemBackgroundColour( treeItem, acBluePastel );
                pTreeCtrl_->SetItemTextColour( parent, acDarkBluePastel );
            }
            else if( VersionFromString( propValue ) < VersionFromString( newestMVIAVersionAvailable_ ) )
            {
                const wxTreeItemId treeItem = pTreeCtrl_->AppendItem( parent, wxString::Format( wxT( "%s: %s (Version %s is now available!!!)" ), propName.c_str(), propValue.c_str(), newestMVIAVersionAvailable_.c_str() ) );
                pTreeCtrl_->SetItemTextColour( treeItem, acDarkBluePastel );
                pTreeCtrl_->SetItemTextColour( parent, acDarkBluePastel );
            }
            else
            {
                pTreeCtrl_->AppendItem( parent, wxString::Format( wxT( "%s: %s" ), propName.c_str(), propValue.c_str() ) );
            }
        }
        else
        {
            const wxString propValue = ConvertedString( prop.read() );
            pTreeCtrl_->AppendItem( parent, wxString::Format( wxT( "%s: %s" ), propName.c_str(), propValue.c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::AppendStringFromProperty( wxString& s, Property& p )
//-----------------------------------------------------------------------------
{
    if( p.isValid() )
    {
        AppendStringFromString( s, p.readS() );
    }
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::AppendStringFromString( wxString& s, const string& toAppend )
//-----------------------------------------------------------------------------
{
    if( s.IsEmpty() )
    {
        s = ConvertedString( toAppend );
    }
    else
    {
        s.Append( wxString::Format( wxT( "(%s)" ), ConvertedString( toAppend ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::ApplyGenTLDriverConfigurationChanges( wxTreeItemId treeItem )
//-----------------------------------------------------------------------------
{
    while( treeItem.IsOk() )
    {
        MyTreeItemData* pItemData = reinterpret_cast<MyTreeItemData*>( pTreeCtrl_->GetItemData( treeItem ) );
        if( pItemData &&
            ( pItemData->type_ == MyTreeItemData::etProducer ) &&
            !pItemData->propertyToValueMap_.empty() )
        {
            const string producerFileName( GetProducerFileName( pItemData->pSystemModule_ ) );
            if( !producerFileName.empty() )
            {
                try
                {
                    if( pGenTLDriverConfigurator_ )
                    {
                        GenTLProducerConfiguration producerConfiguration( pGenTLDriverConfigurator_->hasProducerConfiguration( producerFileName ) ? pGenTLDriverConfigurator_->getProducerConfiguration( producerFileName ) : pGenTLDriverConfigurator_->createProducerConfiguration( producerFileName ) );
                        const map<string, PropertyIInterfaceEnumerationBehaviour> interfaceConfigurations( producerConfiguration.getInterfaceEnumerationBehaviours() );
                        const map<string, PropertyIInterfaceEnumerationBehaviour>::const_iterator itInterfaceConfigurationEND = interfaceConfigurations.end();
                        map<string, string>::const_iterator it = pItemData->propertyToValueMap_.begin();
                        const map<string, string>::const_iterator itEND = pItemData->propertyToValueMap_.end();
                        while( it != itEND )
                        {
                            if( it->first == "EnumerationEnable" )
                            {
                                producerConfiguration.enumerationEnable.write( ( it->second == "0" ) ? bFalse : bTrue );
                            }
                            else
                            {
                                const map<string, PropertyIInterfaceEnumerationBehaviour>::const_iterator itInterfaceConfiguration = interfaceConfigurations.find( it->first );
                                if( itInterfaceConfiguration == itInterfaceConfigurationEND )
                                {
                                    producerConfiguration.createProducerInterfaceConfigurationEntry( it->first ).writeS( it->second );
                                }
                                else
                                {
                                    itInterfaceConfiguration->second.writeS( it->second );
                                }
                            }
                            ++it;
                        }
                    }
                }
                catch( const ImpactAcquireException& ) {}
            }
        }
        if( pTreeCtrl_->HasChildren( treeItem ) )
        {
            wxTreeItemIdValue cookie;
            ApplyGenTLDriverConfigurationChanges( pTreeCtrl_->GetFirstChild( treeItem, cookie ) );
        }
        treeItem = pTreeCtrl_->GetNextSibling( treeItem );
    }
}

//-----------------------------------------------------------------------------
wxString DriverInformationDlg::BuildInterfaceLabel( SystemModule* pSystemModule, InterfaceModule& im, const string& interfaceID )
//-----------------------------------------------------------------------------
{
    wxString interfaceDisplayName;
    if( im.interfaceDisplayName.isValid() )
    {
        interfaceDisplayName = wxString::Format( wxT( "(%s)" ), ConvertedString( im.interfaceDisplayName.readS() ).c_str() );
    }
    else if( pSystemModule->interfaceDisplayName.isValid() )
    {
        interfaceDisplayName = wxString::Format( wxT( "(%s)" ), ConvertedString( pSystemModule->interfaceDisplayName.readS() ).c_str() );
    }
    return wxString::Format( wxT( "%s%s" ), ConvertedString( interfaceID ).c_str(), interfaceDisplayName.c_str() );
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::SetProducerUndefinedStateIfConflictingInterfaceStates( const wxTreeItemId& itemId )
//-----------------------------------------------------------------------------
{
    if( itemId.IsOk() )
    {
        wxTreeItemIdValue cookie;
        wxTreeItemId interfaceTypeId = pTreeCtrl_->GetFirstChild( itemId, cookie );
        bool boAtLeastOneEnumerate = false;
        bool boAtLeastOneIgnore = false;
        while( interfaceTypeId.IsOk() )
        {
            wxTreeItemId interfaceId = pTreeCtrl_->GetFirstChild( interfaceTypeId, cookie );
            while( interfaceId.IsOk() )
            {
                if( pTreeCtrl_->GetItemState( interfaceId ) == lisChecked )
                {
                    boAtLeastOneEnumerate = true;
                }
                else
                {
                    boAtLeastOneIgnore = true;
                }

                if( boAtLeastOneEnumerate && boAtLeastOneIgnore )
                {
                    pTreeCtrl_->SetItemState( itemId, lisUndefined );
                    return;
                }
                interfaceId = pTreeCtrl_->GetNextChild( interfaceTypeId, cookie );
            }
            interfaceTypeId = pTreeCtrl_->GetNextChild( itemId, cookie );
        }
    }
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::SetTechnologyUndefinedStateIfConflictingInterfaceStates( const wxTreeItemId& itemId )
//-----------------------------------------------------------------------------
{
    if( itemId.IsOk() &&
        ( pTreeCtrl_->GetChildrenCount( itemId, false ) > 1 ) )
    {
        bool boAtLeastOneEnumerate = false;
        bool boAtLeastOneIgnore = false;
        wxTreeItemIdValue cookie;
        wxTreeItemId childId = pTreeCtrl_->GetFirstChild( itemId, cookie );
        while( childId.IsOk() )
        {
            if( pTreeCtrl_->GetItemState( childId ) == lisChecked )
            {
                boAtLeastOneEnumerate = true;
            }
            else
            {
                boAtLeastOneIgnore = true;
            }

            if( boAtLeastOneEnumerate && boAtLeastOneIgnore )
            {
                pTreeCtrl_->SetItemState( itemId, lisUndefined );
                return;
            }
            childId = pTreeCtrl_->GetNextChild( itemId, cookie );
        }
    }
}

//-----------------------------------------------------------------------------
map<wxString, map<wxString, bool> > DriverInformationDlg::EnumerateGenICamDevices( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule, unsigned int interfaceCount, const DeviceManager& devMgr )
//-----------------------------------------------------------------------------
{
    static const int MAX_TIME_MS = 10000;
    const string producerFileName( GetProducerFileName( pSystemModule ) );
    wxProgressDialog progressDialog( wxT( "Obtaining Device Information" ),
                                     wxString::Format( wxT( "Obtaining device information from %u interface%s\n\nGenTL Producer name: %s\nGenTL Producer vendor: %s...\n\nThis dialog will disappear automatically once this operation completes!" ),
                                             interfaceCount,
                                             ( ( interfaceCount > 1 ) ? wxT( "s" ) : wxT( "" ) ),
                                             ConvertedString( producerFileName.empty() ? "Could not be obtained" : producerFileName ).c_str(),
                                             ConvertedString( pSystemModule->TLVendorName.isValid() ? pSystemModule->TLVendorName.readS() : "Could not be obtained" ).c_str() ),
                                     MAX_TIME_MS, // range
                                     this,
                                     wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME );

    // create a thread for each interface
    vector<CollectDeviceInformationThread*> threads;
    for( unsigned int i = 0; i < interfaceCount; i++ )
    {
        threads.push_back( new CollectDeviceInformationThread( devMgr, *pSystemModule, i ) );
        threads.back()->Run();
    }

    // if at least one thread is still running keep waiting
    for( unsigned int i = 0; i < interfaceCount; i++ )
    {
        if( threads[i]->IsRunning() )
        {
            wxMilliSleep( 100 );
            progressDialog.Pulse();
            i = 0;
        }
    }

    // collect the data and clean up
    map<wxString, map<wxString, bool> > results;
    for( unsigned int i = 0; i < interfaceCount; i++ )
    {
        threads[i]->Wait();
        results.insert( threads[i]->GetResults() );
        delete threads[i];
    }
    progressDialog.Update( MAX_TIME_MS );
    return results;
}

//-----------------------------------------------------------------------------
string DriverInformationDlg::GetProducerFileName( mvIMPACT::acquire::GenICam::SystemModule* pSystemModule ) const
//-----------------------------------------------------------------------------
{
    string fileName;
    if( pSystemModule->TLFileName.isValid() )
    {
        fileName = pSystemModule->TLFileName.readS();
    }
    if( fileName.empty() && pSystemModule->TLPath.isValid() )
    {
        fileName = pSystemModule->TLPath.readS();
        const string::size_type pos = fileName.find_last_of( "/\\" );
        if( ( pos != string::npos ) && ( ( pos + 1 ) < fileName.length() ) )
        {
            fileName = fileName.substr( pos + 1 );
        }
    }
    return fileName;
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::OnItemStateClick( wxTreeEvent& event )
//-----------------------------------------------------------------------------
{
    const wxTreeItemId itemId = event.GetItem();
    MyTreeItemData* pItemData = reinterpret_cast<MyTreeItemData*>( pTreeCtrl_->GetItemData( itemId ) );
    if( !pItemData )
    {
        return;
    }

    const TListItemStates state = ( pTreeCtrl_->GetItemState( itemId ) == lisChecked ) ? lisUnchecked : lisChecked;
    wxTreeItemIdValue cookie;
    SystemModule* pSystemModule = pItemData->pSystemModule_;
    if( pSystemModule->mvInterfaceTechnologyToIgnoreSelector.isValid() &&
        pSystemModule->mvInterfaceTechnologyToIgnoreEnable.isValid() &&
        pSystemModule->mvDeviceUpdateListBehaviour.isValid() )
    {
        // This is a MATRIX VISION producer with integrated interface/technology configuration properties
        pTreeCtrl_->SetItemState( itemId, state );
        if( pTreeCtrl_->GetItemText( itemId ) == wxT( "GigEVision" ) ||
            pTreeCtrl_->GetItemText( itemId ) == wxT( "USB3Vision" ) ||
            pTreeCtrl_->GetItemText( itemId ) == wxT( "PCI/PCIe" ) )
        {
            // recursively apply the same state to all children(interfaces of the same type)
            wxTreeItemId childId = pTreeCtrl_->GetFirstChild( itemId, cookie );
            while( childId.IsOk() )
            {
                pTreeCtrl_->SetItemState( childId, state );
                childId = pTreeCtrl_->GetNextChild( itemId, cookie );
            }

            ignoredInterfacesInfo_[TreeItemLabelToTechnologyString( pTreeCtrl_->GetItemText( itemId ) )] = ( pTreeCtrl_->GetItemState( itemId ) == lisUnchecked ) ? string( "1" ) : string( "0" );
            // recursively update the settings of all children(interfaces of the same type)
            childId = pTreeCtrl_->GetFirstChild( itemId, cookie );
            const IgnoredInterfacesInfo::iterator itEND = ignoredInterfacesInfo_.end();
            while( childId.IsOk() )
            {
                const string itemText( pTreeCtrl_->GetItemText( childId ) );
                IgnoredInterfacesInfo::iterator it = ignoredInterfacesInfo_.begin();
                while( it != itEND )
                {
                    if( itemText.find( it->first ) == 0 )
                    {
                        it->second = string( "NotConfigured" );
                        break;
                    }
                    ++it;
                }
                childId = pTreeCtrl_->GetNextChild( itemId, cookie );
            }
        }
        else
        {
            // First set the state of the parent to the same state of the current child that did cause this function to be called...
            const wxTreeItemId parentId = pTreeCtrl_->GetItemParent( itemId );
            const TListItemStates previousParentState = static_cast<TListItemStates>( pTreeCtrl_->GetItemState( parentId ) );
            pTreeCtrl_->SetItemState( parentId, state );
            // ... then check if all children of the current parent have the same state. If there is at least one with a different state
            // set the parent state to 'Undefined'
            wxTreeItemId childId = pTreeCtrl_->GetFirstChild( parentId, cookie );
            bool boParentBecameUndefined = false;
            while( childId.IsOk() )
            {
                if( pTreeCtrl_->GetItemState( childId ) != state )
                {
                    pTreeCtrl_->SetItemState( parentId, lisUndefined );
                    boParentBecameUndefined = previousParentState != lisUndefined;
                    break;
                }
                childId = pTreeCtrl_->GetNextChild( parentId, cookie );
            }

            // Since configuring a child can modify the parent state too, we have to update the ignoredInterfacesInfo_ to
            // reflect this change, before we do anything else.
            const bool boParentIsUnchecked = ( pTreeCtrl_->GetItemState( parentId ) == lisUnchecked );
            const string technology( TreeItemLabelToTechnologyString( pTreeCtrl_->GetItemText( parentId ) ) );
            if( !technology.empty() )
            {
                ignoredInterfacesInfo_[technology] = boParentIsUnchecked ? string( "1" ) : string( "0" );
            }

            if( boParentBecameUndefined )
            {
                // we need to update ALL children of the current technology now!
                childId = pTreeCtrl_->GetFirstChild( parentId, cookie );
                const IgnoredInterfacesInfo::iterator itEND = ignoredInterfacesInfo_.end();
                while( childId.IsOk() )
                {
                    const string itemText( pTreeCtrl_->GetItemText( childId ) );
                    IgnoredInterfacesInfo::iterator it = ignoredInterfacesInfo_.begin();
                    while( it != itEND )
                    {
                        if( itemText.find( it->first ) == 0 )
                        {
                            if( boParentIsUnchecked && ( pTreeCtrl_->GetItemState( childId ) == lisChecked ) )
                            {
                                it->second = string( "ForceEnumerate" );
                            }
                            else if( !boParentIsUnchecked && ( pTreeCtrl_->GetItemState( childId ) == lisUnchecked ) )
                            {
                                it->second = string( "ForceIgnore" );
                            }
                            else
                            {
                                it->second = string( "NotConfigured" );
                            }
                            break;
                        }
                        ++it;
                    }
                    childId = pTreeCtrl_->GetNextChild( itemId, cookie );
                }
            }
            else
            {
                // just THIS tree item causes a change in the ignoredInterfacesInfo_ list
                const string itemText( pTreeCtrl_->GetItemText( itemId ) );
                IgnoredInterfacesInfo::iterator it = ignoredInterfacesInfo_.begin();
                const IgnoredInterfacesInfo::iterator itEND = ignoredInterfacesInfo_.end();
                while( it != itEND )
                {
                    if( itemText.find( it->first ) == 0 )
                    {
                        if( pTreeCtrl_->GetItemState( itemId ) == lisUnchecked )
                        {
                            it->second = boParentIsUnchecked ? string( "NotConfigured" ) : string( "ForceIgnore" );
                        }
                        else if( pTreeCtrl_->GetItemState( itemId ) == lisChecked )
                        {
                            it->second = boParentIsUnchecked ? string( "ForceEnumerate" ) : string( "NotConfigured" );
                        }
                        break;
                    }
                    ++it;
                }
            }
        }
    }
    else if( pItemData->type_ == MyTreeItemData::etProducer )
    {
        // This is a GenTL producer without integrated interface/technology configuration support and
        // the user has clicked on the root node, thus will access the 'EnumerationEnable' property
        // for this producer.
        pTreeCtrl_->SetItemState( itemId, state );
        // recursively apply the same state to all interfaces
        wxTreeItemId interfaceTypeId = pTreeCtrl_->GetFirstChild( itemId, cookie );
        while( interfaceTypeId.IsOk() )
        {
            wxTreeItemId interfaceId = pTreeCtrl_->GetFirstChild( interfaceTypeId, cookie );
            while( interfaceId.IsOk() )
            {
                pTreeCtrl_->SetItemState( interfaceId, state );
                interfaceId = pTreeCtrl_->GetNextChild( interfaceTypeId, cookie );
            }
            interfaceTypeId = pTreeCtrl_->GetNextChild( itemId, cookie );
        }
        pItemData->propertyToValueMap_.clear();
        pItemData->propertyToValueMap_.insert( make_pair( "EnumerationEnable", state == lisChecked ? "1" : "0" ) );
    }
    else if( pItemData->type_ == MyTreeItemData::etInterface )
    {
        // This is a GenTL producer without integrated interface/technology configuration support and
        // the user has clicked an interface node, thus will access the 'InterfaceEnumeartionBehaviour' property
        // for this interface of the top-level producer.
        pTreeCtrl_->SetItemState( itemId, state );
        // set the state of the parent (producer) to undefined or to item state, depending on the states of the other siblings.
        wxTreeItemId parentId = pTreeCtrl_->GetItemParent( itemId );
        parentId = pTreeCtrl_->GetItemParent( parentId );
        pTreeCtrl_->SetItemState( parentId, state );
        wxTreeItemId interfaceTypeId = pTreeCtrl_->GetFirstChild( parentId, cookie );
        bool boContinueProcessing = true;
        while( interfaceTypeId.IsOk() && boContinueProcessing )
        {
            wxTreeItemId interfaceId = pTreeCtrl_->GetFirstChild( interfaceTypeId, cookie );
            while( interfaceId.IsOk() )
            {
                if( pTreeCtrl_->GetItemState( interfaceId ) != state )
                {
                    pTreeCtrl_->SetItemState( parentId, lisUndefined );
                    boContinueProcessing = false;
                    break;
                }
                interfaceId = pTreeCtrl_->GetNextChild( interfaceTypeId, cookie );
            }
            interfaceTypeId = pTreeCtrl_->GetNextChild( parentId, cookie );
        }

        MyTreeItemData* pParentItemData = reinterpret_cast<MyTreeItemData*>( pTreeCtrl_->GetItemData( parentId ) );
        if( pParentItemData )
        {
            // as later when actually applying the new behaviour (if the user ends the dialog using the 'Ok' button)
            // we will just evaluate the tree items stating they represent a 'producer' we collect all the information
            // in these tree items only!
            const std::map<std::string, std::string>::const_iterator it = pItemData->propertyToValueMap_.find( "InterfaceID" );
            if( it != pItemData->propertyToValueMap_.end() )
            {
                pParentItemData->propertyToValueMap_[it->second] = ( state == lisUnchecked ) ? "ForceIgnore" : "ForceEnumerate";
            }
        }
    }
}

//-----------------------------------------------------------------------------
wxTreeItemId DriverInformationDlg::AddKnownTechnologyTreeItem( SystemModule* pSystemModule, wxTreeItemId producerRootItem, wxTreeItemId treeItem, map<wxString, wxTreeItemId>& interfaceTypeMap, const wxString& technology, const string& technologyShort )
//-----------------------------------------------------------------------------
{
    if( !treeItem.IsOk() )
    {
        treeItem = pTreeCtrl_->AppendItem( producerRootItem, technology, -1, -1, new MyTreeItemData( pSystemModule, MyTreeItemData::etTechnology ) );
        interfaceTypeMap.insert( make_pair( technology, treeItem ) );
        pTreeCtrl_->SetItemBold( treeItem );
        if( pSystemModule->mvInterfaceTechnologyToIgnoreSelector.isValid() &&
            supportsEnumStringValue( pSystemModule->mvInterfaceTechnologyToIgnoreSelector, technologyShort ) )
        {
            pSystemModule->mvInterfaceTechnologyToIgnoreSelector.writeS( technologyShort );
            pTreeCtrl_->SetItemState( treeItem, ( ignoredInterfacesInfo_[technologyShort] == string( "1" ) ) ? lisUnchecked : lisChecked );
        }
    }
    return treeItem;
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::PopulateTreeCtrl( Component itDrivers, const DeviceManager& devMgr )
//-----------------------------------------------------------------------------
{
    const wxTreeItemId rootId = pTreeCtrl_->AddRoot( ConvertedString( itDrivers.name() ) );
    itDrivers = itDrivers.firstChild();
    while( itDrivers.isValid() )
    {
        // If the driver is not installed then do not even list it!
        ComponentLocator locator( itDrivers.hObj() );
        if( locator.findComponent( "Version" ) != INVALID_ID )
        {
            ComponentList clDriver( itDrivers.hObj() );
            wxTreeItemId newItem;
            if( clDriver.contentDescriptor().empty() )
            {
                newItem = pTreeCtrl_->AppendItem( rootId, wxEmptyString );
            }
            else
            {
                newItem = pTreeCtrl_->AppendItem( rootId, wxString::Format( wxT( " %s" ), ConvertedString( itDrivers.name() ).c_str() ) );
                pTreeCtrl_->SetItemBold( newItem );
            }
            AddStringPropToList( newItem, locator, "FullPath" );
            AddStringPropToList( newItem, locator, "Version" );

            if( ( clDriver.name() == string( "mvGenTLConsumer" ) ) && !systemModules_.empty() )
            {
                const vector<mvIMPACT::acquire::GenICam::SystemModule*>::size_type systemModuleCount = systemModules_.size();
                for( vector<mvIMPACT::acquire::GenICam::SystemModule*>::size_type systemModuleIndex = 0; systemModuleIndex < systemModuleCount; systemModuleIndex++ )
                {
                    SystemModule* pSystemModule = systemModules_[systemModuleIndex];
                    wxString producerInfo;
                    const string producerFileName( GetProducerFileName( pSystemModule ) );
                    AppendStringFromProperty( producerInfo, pSystemModule->TLDisplayName );
                    AppendStringFromProperty( producerInfo, pSystemModule->TLVendorName );
                    AppendStringFromString( producerInfo, producerFileName );
                    wxTreeItemId producerRootItem = pTreeCtrl_->AppendItem( newItem, producerInfo, -1, -1, new MyTreeItemData( pSystemModule, MyTreeItemData::etProducer ) );
                    if( pGenTLDriverConfigurator_ && !producerFileName.empty() )
                    {
                        if( pGenTLDriverConfigurator_->hasProducerConfiguration( producerFileName ) )
                        {
                            GenTLProducerConfiguration producerConfiguration( pGenTLDriverConfigurator_->getProducerConfiguration( producerFileName ) );
                            pTreeCtrl_->SetItemState( producerRootItem, ( producerConfiguration.enumerationEnable.read() == bTrue ) ? lisChecked : lisUnchecked );
                        }
                        else if( !pSystemModule->mvInterfaceTechnologyToIgnoreEnable.isValid() )
                        {
                            // only set this for top-level items if the producer does NOT offer the internal 'technology/interface ignore' interface
                            pTreeCtrl_->SetItemState( producerRootItem, lisChecked );
                        }
                    }

                    wxTreeItemId interfaceTypeGEV;
                    wxTreeItemId interfaceTypeU3V;
                    wxTreeItemId interfaceTypePCI;
                    if( pSystemModule->interfaceSelector.isValid() )
                    {
                        map<wxString, wxTreeItemId> interfaceTypeMap;
                        const int64_type interfaceCount = pSystemModule->getInterfaceModuleCount();
                        for( int64_type i = 0; i < interfaceCount; i++ )
                        {
                            InterfaceModule im( *pSystemModule, i );
                            wxTreeItemId interfaceTreeItem;
                            const string interfaceType( im.interfaceType.isValid() ? im.interfaceType.readS() : "UNKNOWN-INTERFACE-TYPE" );
                            if( ( interfaceType == string( "GEV" ) ) ||
                                ( interfaceType == string( "GigEVision" ) ) )
                            {
                                interfaceTypeGEV = AddKnownTechnologyTreeItem( pSystemModule, producerRootItem, interfaceTypeGEV, interfaceTypeMap, wxT( "GigEVision" ), "GEV" );
                            }
                            else if( ( interfaceType == string( "U3V" ) ) ||
                                     ( interfaceType == string( "USB3Vision" ) ) )
                            {
                                interfaceTypeU3V = AddKnownTechnologyTreeItem( pSystemModule, producerRootItem, interfaceTypeU3V, interfaceTypeMap, wxT( "USB3Vision" ), "U3V" );
                            }
                            else if( interfaceType == string( "PCI" ) )
                            {
                                interfaceTypePCI = AddKnownTechnologyTreeItem( pSystemModule, producerRootItem, interfaceTypePCI, interfaceTypeMap, wxT( "PCI/PCIe" ), "PCI" );
                            }
                            else if( interfaceTypeMap.find( interfaceType ) == interfaceTypeMap.end() )
                            {
                                wxTreeItemId interfaceTypeTreeItem = pTreeCtrl_->AppendItem( producerRootItem, interfaceType, -1, -1, new MyTreeItemData( pSystemModule, MyTreeItemData::etTechnology ) );
                                interfaceTypeMap.insert( make_pair( interfaceType, interfaceTypeTreeItem ) );
                                pTreeCtrl_->SetItemBold( interfaceTypeTreeItem );
                            }
                        }

                        map<wxString, map<wxString, bool> > deviceInformation = EnumerateGenICamDevices( pSystemModule, static_cast<unsigned int>( interfaceCount ), devMgr );
                        for( unsigned int i = 0; i < interfaceCount; i++ )
                        {
                            InterfaceModule im( *pSystemModule, i );
                            wxTreeItemId interfaceTreeItem;
                            const string interfaceID( im.interfaceID.isValid() ? im.interfaceID.readS() : ( pSystemModule->interfaceID.isValid() ? pSystemModule->interfaceID.readS() : "" ) );
                            if( interfaceID.empty() )
                            {
                                continue;
                            }
                            const string interfaceType( im.interfaceType.isValid() ? im.interfaceType.readS() : "UNKNOWN-INTERFACE-TYPE" );
                            if( ( interfaceType == string( "GEV" ) ) ||
                                ( interfaceType == string( "GigEVision" ) ) )
                            {
                                interfaceTreeItem = CreateAndConfigureInterfaceTreeItem( pSystemModule, im, interfaceID, interfaceTypeGEV );
                            }
                            else if( ( interfaceType == string( "U3V" ) ) ||
                                     ( interfaceType == string( "USB3Vision" ) ) )
                            {
                                interfaceTreeItem = CreateAndConfigureInterfaceTreeItem( pSystemModule, im, interfaceID, interfaceTypeU3V );
                            }
                            else if( interfaceType == string( "PCI" ) )
                            {
                                interfaceTreeItem = CreateAndConfigureInterfaceTreeItem( pSystemModule, im, interfaceID, interfaceTypePCI );
                            }
                            else
                            {
                                const map<wxString, wxTreeItemId>::const_iterator itInterfaceType = interfaceTypeMap.find( ConvertedString( interfaceType ) );
                                interfaceTreeItem = CreateAndConfigureInterfaceTreeItem( pSystemModule, im, interfaceID, ( itInterfaceType == interfaceTypeMap.end() ) ? producerRootItem : itInterfaceType->second );
                            }
                            const map<wxString, map<wxString, bool> >::const_iterator itDeviceInformationForInterface = deviceInformation.find( interfaceID );
                            if( ( itDeviceInformationForInterface != deviceInformation.end() ) && !itDeviceInformationForInterface->second.empty() )
                            {
                                map<wxString, bool>::const_iterator itDevices = itDeviceInformationForInterface->second.begin();
                                const map<wxString, bool>::const_iterator itDevicesEND = itDeviceInformationForInterface->second.end();
                                while( itDevices != itDevicesEND )
                                {
                                    wxTreeItemId deviceTreeItem = pTreeCtrl_->AppendItem( interfaceTreeItem, itDevices->first );
                                    if( itDevices->second )
                                    {
                                        pTreeCtrl_->SetItemTextColour( deviceTreeItem, acDarkGrey );
                                    }
                                    ++itDevices;
                                }
                            }
                        }
                        if( pSystemModule->mvInterfaceTechnologyToIgnoreEnable.isValid() )
                        {
                            SetTechnologyUndefinedStateIfConflictingInterfaceStates( interfaceTypeGEV );
                            SetTechnologyUndefinedStateIfConflictingInterfaceStates( interfaceTypeU3V );
                            SetTechnologyUndefinedStateIfConflictingInterfaceStates( interfaceTypePCI );
                        }
                        else
                        {
                            SetProducerUndefinedStateIfConflictingInterfaceStates( producerRootItem );
                        }
                    }
                }
            }
            else
            {
                const wxTreeItemId devices = AddComponentListToList( newItem, locator, "Devices" );
                if( devices.IsOk() )
                {
                    Component itDev( locator.findComponent( "Devices" ) );
                    itDev = itDev.firstChild();
                    while( itDev.isValid() )
                    {
                        const PropertyS serial( itDev );
                        const Device* const pDev = devMgr.getDeviceBySerial( serial.read() );
                        const bool boInUse = pDev && pDev->isInUse();
                        const wxTreeItemId deviceTreeItem = pTreeCtrl_->AppendItem( devices, wxString::Format( wxT( "%s(%s)%s" ), ConvertedString( serial.read() ).c_str(), ConvertedString( pDev ? pDev->product.read() : "UNBOUND POINTER!" ).c_str(), boInUse ? wxT( " - IN USE!" ) : wxT( "" ) ) );
                        if( boInUse )
                        {
                            pTreeCtrl_->SetItemTextColour( deviceTreeItem, acDarkGrey );
                        }
                        itDev = itDev.nextSibling();
                    }
                }
            }
        }
        itDrivers = itDrivers.nextSibling();
    }
}

//-----------------------------------------------------------------------------
wxTreeItemId DriverInformationDlg::CreateAndConfigureInterfaceTreeItem( SystemModule* pSystemModule, InterfaceModule& im, const string& interfaceID, wxTreeItemId interfaceTypeTreeItem )
//-----------------------------------------------------------------------------
{
    // try to get a human readable name for this interface
    const wxString interfaceLabel( BuildInterfaceLabel( pSystemModule, im, interfaceID ) );
    const string interfaceLabelANSI( interfaceLabel.mb_str() );
    const IgnoredInterfacesInfo::const_iterator it = ignoredInterfacesInfo_.find( interfaceLabelANSI );
    const IgnoredInterfacesInfo::const_iterator itEND = ignoredInterfacesInfo_.end();
    MyTreeItemData* pTreeItemData = new MyTreeItemData( pSystemModule, MyTreeItemData::etInterface );
    pTreeItemData->propertyToValueMap_.insert( make_pair( "InterfaceID", interfaceID ) );
    wxTreeItemId interfaceTreeItem = pTreeCtrl_->AppendItem( interfaceTypeTreeItem, interfaceLabel, -1, -1, pTreeItemData );

    const string producerFileName( GetProducerFileName( pSystemModule ) );
    if( it != itEND )
    {
        int s = pTreeCtrl_->GetItemState( interfaceTypeTreeItem );
        pTreeCtrl_->SetItemState( interfaceTreeItem, ( it->second == string( "NotConfigured" ) ) ? s : ( it->second == string( "ForceIgnore" ) ) ? lisUnchecked : lisChecked );
    }
    else if( pGenTLDriverConfigurator_ && !producerFileName.empty() )
    {
        if( pGenTLDriverConfigurator_->hasProducerConfiguration( producerFileName ) )
        {
            GenTLProducerConfiguration producerConfiguration( pGenTLDriverConfigurator_->getProducerConfiguration( producerFileName ) );
            const map<string, PropertyIInterfaceEnumerationBehaviour> interfaceEnumerationBehaviour( producerConfiguration.getInterfaceEnumerationBehaviours() );
            const map<string, PropertyIInterfaceEnumerationBehaviour>::const_iterator itInterfaceEnumerationBehaviour = interfaceEnumerationBehaviour.find( interfaceID );
            if( itInterfaceEnumerationBehaviour == interfaceEnumerationBehaviour.end() )
            {
                pTreeCtrl_->SetItemState( interfaceTreeItem, ( producerConfiguration.enumerationEnable.read() == bTrue ) ? lisChecked : lisUnchecked );
            }
            else
            {
                switch( itInterfaceEnumerationBehaviour->second.read() )
                {
                case iebNotConfigured:
                    pTreeCtrl_->SetItemState( interfaceTreeItem, ( producerConfiguration.enumerationEnable.read() == bTrue ) ? lisChecked : lisUnchecked );
                    break;
                case iebForceEnumerate:
                    pTreeCtrl_->SetItemState( interfaceTreeItem, lisChecked );
                    break;
                case iebForceIgnore:
                    pTreeCtrl_->SetItemState( interfaceTreeItem, lisUnchecked );
                    break;
                }
            }
        }
        else
        {
            pTreeCtrl_->SetItemState( interfaceTreeItem, lisChecked );
        }
    }
    return interfaceTreeItem;
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::ReadIgnoredInterfacesInfoAndEnableAllInterfaces( void )
//-----------------------------------------------------------------------------
{
    const vector<string>::size_type technologyCount = technologiesToHandleSpeciallyInOurProducers_.size();
    const vector<mvIMPACT::acquire::GenICam::SystemModule*>::size_type systemModuleCount = systemModules_.size();
    for( vector<mvIMPACT::acquire::GenICam::SystemModule*>::size_type systemModuleIndex = 0; systemModuleIndex < systemModuleCount; systemModuleIndex++ )
    {
        SystemModule* pSystemModule = systemModules_[systemModuleIndex];
        if( pSystemModule->interfaceID.isValid() &&
            pSystemModule->interfaceSelector.isValid() &&
            pSystemModule->interfaceUpdateList.isValid() &&
            pSystemModule->mvDeviceUpdateListBehaviour.isValid() )
        {
            pSystemModule->interfaceUpdateList.call();
            for( vector<string>::size_type technologyIndex = 0; technologyIndex < technologyCount; technologyIndex++ )
            {
                const string technology( technologiesToHandleSpeciallyInOurProducers_[technologyIndex] );
                string value( "0" );
                if( pSystemModule->mvInterfaceTechnologyToIgnoreSelector.isValid() && pSystemModule->mvInterfaceTechnologyToIgnoreEnable.isValid() &&
                    supportsEnumStringValue( pSystemModule->mvInterfaceTechnologyToIgnoreSelector, technology ) )
                {
                    pSystemModule->mvInterfaceTechnologyToIgnoreSelector.writeS( technology );
                    value = ( pSystemModule->mvInterfaceTechnologyToIgnoreEnable.read() == bTrue ) ? "1" : "0";
                    ignoredInterfacesInfo_.insert( make_pair( technology, value ) );
                    ignoredInterfacesInfoOriginal_.insert( make_pair( technology, value ) );
                    pSystemModule->mvInterfaceTechnologyToIgnoreEnable.write( bFalse );
                }
            }
            const int64_type interfaceCount = pSystemModule->interfaceSelector.getMaxValue() + 1;
            for( int64_type i = 0; i < interfaceCount; i++ )
            {
                pSystemModule->interfaceSelector.write( i );
                const string value( pSystemModule->mvDeviceUpdateListBehaviour.isValid() ? pSystemModule->mvDeviceUpdateListBehaviour.readS() : "NotConfigured" );
                InterfaceModule im( *pSystemModule, i );
                const string key( BuildInterfaceLabel( pSystemModule, im, pSystemModule->interfaceID.readS() ) );
                ignoredInterfacesInfo_.insert( make_pair( key, value ) );
                ignoredInterfacesInfoOriginal_.insert( make_pair( key, value ) );
                if( pSystemModule->mvDeviceUpdateListBehaviour.isValid() )
                {
                    pSystemModule->mvDeviceUpdateListBehaviour.writeS( "NotConfigured" );
                }
            }
        }
    }
    if( pGenTLDriverConfigurator_ )
    {
        masterEnumerationBehaviourOriginal_ = pGenTLDriverConfigurator_->masterEnumerationBehaviour.read();
        pGenTLDriverConfigurator_->masterEnumerationBehaviour.write( iebForceEnumerate );
    }
}

//-----------------------------------------------------------------------------
string DriverInformationDlg::TreeItemLabelToTechnologyString( const wxString& label )
//-----------------------------------------------------------------------------
{
    if( label == wxT( "GigEVision" ) )
    {
        return "GEV";
    }
    else if( label == wxT( "PCI/PCIe" ) )
    {
        return "PCI";
    }
    else if( label == wxT( "USB3Vision" ) )
    {
        return "U3V";
    }
    return "";
}

//-----------------------------------------------------------------------------
void DriverInformationDlg::UpdateIgnoredInterfacesSettings( IgnoredInterfacesInfo& interfacesInfo )
//-----------------------------------------------------------------------------
{
    const vector<string>::size_type technologyCount = technologiesToHandleSpeciallyInOurProducers_.size();
    const vector<mvIMPACT::acquire::GenICam::SystemModule*>::size_type systemModuleCount = systemModules_.size();
    for( vector<mvIMPACT::acquire::GenICam::SystemModule*>::size_type systemModuleIndex = 0; systemModuleIndex < systemModuleCount; systemModuleIndex++ )
    {
        SystemModule* pSystemModule = systemModules_[systemModuleIndex];
        if( pSystemModule->interfaceSelector.isValid() &&
            pSystemModule->interfaceID.isValid() &&
            pSystemModule->interfaceType.isValid() &&
            pSystemModule->mvDeviceUpdateListBehaviour.isValid() )
        {
            for( vector<string>::size_type technologyIndex = 0; technologyIndex < technologyCount; technologyIndex++ )
            {
                const string technology( technologiesToHandleSpeciallyInOurProducers_[technologyIndex] );
                if( pSystemModule &&
                    pSystemModule->mvInterfaceTechnologyToIgnoreSelector.isValid() &&
                    pSystemModule->mvInterfaceTechnologyToIgnoreEnable.isValid() &&
                    supportsEnumStringValue( pSystemModule->mvInterfaceTechnologyToIgnoreSelector, technology ) )
                {
                    pSystemModule->mvInterfaceTechnologyToIgnoreSelector.writeS( technology );
                    if( interfacesInfo[technology] == "1" )
                    {
                        pSystemModule->mvInterfaceTechnologyToIgnoreEnable.write( bTrue );
                    }
                }
            }

            const IgnoredInterfacesInfo::const_iterator itGEV = interfacesInfo.find( "GEV" );
            const IgnoredInterfacesInfo::const_iterator itU3V = interfacesInfo.find( "U3V" );
            const IgnoredInterfacesInfo::const_iterator itPCI = interfacesInfo.find( "PCI" );
            const IgnoredInterfacesInfo::const_iterator itEND = interfacesInfo.end();

            const int64_type interfaceCount = pSystemModule->getInterfaceModuleCount();
            for( int64_type i = 0; i < interfaceCount; i++ )
            {
                pSystemModule->interfaceSelector.write( i );
                InterfaceModule im( *pSystemModule, i );
                const string interfaceLabelANSI( BuildInterfaceLabel( pSystemModule, im, pSystemModule->interfaceID.readS() ).mb_str() );
                const IgnoredInterfacesInfo::const_iterator it = interfacesInfo.find( interfaceLabelANSI );
                if( it == itEND )
                {
                    continue;
                }
                if( it->second == "NotConfigured" )
                {
                    pSystemModule->mvDeviceUpdateListBehaviour.writeS( "NotConfigured" );
                }
                else if( it->second == "ForceEnumerate" )
                {
                    const string interfaceType( pSystemModule->interfaceType.readS() );
                    if( ( ( interfaceType == "GEV" ) && ( itGEV != itEND ) && ( itGEV->second == "1" ) ) ||
                        ( ( interfaceType == "U3V" ) && ( itU3V != itEND ) && ( itU3V->second == "1" ) ) ||
                        ( ( interfaceType == "PCI" ) && ( itPCI != itEND ) && ( itPCI->second == "1" ) ) )
                    {
                        pSystemModule->mvDeviceUpdateListBehaviour.writeS( "ForceEnumerate" );
                    }
                    else
                    {
                        pSystemModule->mvDeviceUpdateListBehaviour.writeS( "NotConfigured" );
                    }
                }
                else if( it->second == "ForceIgnore" )
                {
                    const string interfaceType( pSystemModule->interfaceType.readS() );
                    if( ( ( interfaceType == "GEV" ) && ( itGEV != itEND ) && ( itGEV->second == "0" ) ) ||
                        ( ( interfaceType == "U3V" ) && ( itU3V != itEND ) && ( itU3V->second == "0" ) ) ||
                        ( ( interfaceType == "PCI" ) && ( itPCI != itEND ) && ( itPCI->second == "0" ) ) )
                    {
                        pSystemModule->mvDeviceUpdateListBehaviour.writeS( "ForceIgnore" );
                    }
                    else
                    {
                        pSystemModule->mvDeviceUpdateListBehaviour.writeS( "NotConfigured" );
                    }
                }
            }
        }
    }
    if( pGenTLDriverConfigurator_ )
    {
        pGenTLDriverConfigurator_->masterEnumerationBehaviour.write( masterEnumerationBehaviourOriginal_ );
    }
}

#ifdef BUILD_WITH_UPDATE_DIALOG_SUPPORT
//=============================================================================
//=========== Implementation Contact MATRIX VISION Server Thread===============
//=============================================================================
//------------------------------------------------------------------------------
class DownloadMVSoftwarePackageThread : public wxThread
//------------------------------------------------------------------------------
{
    wxString urlString_;
    wxEvtHandler* pEventHandler_;
    int parentWindowID_;
    unsigned int connectionTimeout_s_;
    bool aborted_;
    wxString fileName_;
protected:
    void* Entry()
    {
        wxURL url( urlString_ );
        url.GetProtocol().SetDefaultTimeout( connectionTimeout_s_ );
        wxPlatformInfo platformInfo;
        if( ( platformInfo.GetOperatingSystemId() & wxOS_WINDOWS ) != 0 )
        {
            ProxyResolverContext proxyResolverContext( wxT( "wxPropView" ), wxT( "http://www.matrix-vision.de" ) );
            if( proxyResolverContext.GetProxy( 0 ).length() > 0 )
            {
                url.SetProxy( wxString( proxyResolverContext.GetProxy( 0 ).c_str() ) + wxT( ":" ) + wxString::Format( wxT( "%u" ), proxyResolverContext.GetProxyPort( 0 ) ) );
            }
        }
        if( url.GetError() == wxURL_NOERR )
        {
            wxInputStream* pInStream = url.GetInputStream();
            if( pInStream )
            {
                if( pInStream->IsOk() )
                {
                    wxString path;
                    static const wxString DOWNLOADS_DIR( wxT( "USERPROFILE" ) );
                    ::wxGetEnv( DOWNLOADS_DIR, &path );
                    AppendPathSeparatorIfNeeded( path );

                    fileName_ = path.Append( wxT( "Downloads/" ) ).Append( urlString_.AfterLast( wxT( '/' ) ) );

                    if( !wxDirExists( fileName_.BeforeLast( wxT( '/' ) ) ) )
                    {
                        fileName_ = wxT( "DownloadDirectoryNotFound" );
                    }
                    else if( !wxFileExists( fileName_ ) )
                    {
                        wxFileOutputStream localFile( fileName_ );
                        auto_array_ptr<unsigned char> pBuf( 1024 );
                        while( pInStream->CanRead() && !pInStream->Eof() )
                        {
                            pInStream->Read( pBuf.get(), pBuf.parCnt() );
                            localFile.WriteAll( pBuf.get(), pInStream->LastRead() );
                            if( aborted_ )
                            {
                                localFile.Close();
                                if( wxFileExists( fileName_ ) )
                                {
                                    ::wxRemoveFile( fileName_ );
                                    fileName_ = wxT( "NoFile" );
                                }
                                break;
                            }
                        }
                    }
                }
                delete pInStream;
            }
        }
        else
        {
            // An error occurred while trying to contact the MATRIX VISION server...
            wxMessageBox( wxString::Format( wxT( "Error:\nDestination directory not found!" ) ), wxT( "Error while downloading the new software package" ), wxOK | wxICON_EXCLAMATION );
        }
        if( pEventHandler_ && ( GetKind() == wxTHREAD_DETACHED ) )
        {
            wxQueueEvent( pEventHandler_, new wxCommandEvent( latestPackageDownloaded, parentWindowID_ ) );
        }
        return 0;
    }
public:
    explicit DownloadMVSoftwarePackageThread( const wxString& urlString, wxThreadKind kind, unsigned int connectionTimeout_s = 5 ) : wxThread( kind ),
        urlString_( urlString ), pEventHandler_( 0 ), parentWindowID_( wxID_ANY ), connectionTimeout_s_( connectionTimeout_s ), aborted_( false ), fileName_( wxT( "" ) ) {}
    void AttachEventHandler( wxEvtHandler* pEventHandler, int parentWindowID )
    {
        pEventHandler_ = pEventHandler;
        parentWindowID_ = parentWindowID;
    }
    void Abort( void )
    {
        aborted_ = true;
    }
    bool GetAborted( void ) const
    {
        return aborted_;
    }
    wxString GetDownloadedFileName( void ) const
    {
        return fileName_;
    }
};

//=============================================================================
//=================== Implementation UpdatesInformationDlg ====================
//=============================================================================
BEGIN_EVENT_TABLE( UpdatesInformationDlg, OkAndCancelDlg )
    EVT_BUTTON( widFirst + 0, UpdatesInformationDlg::OnBtnApply )
    EVT_BUTTON( widFirst + 1, UpdatesInformationDlg::OnBtnApply )
    EVT_BUTTON( widFirst + 2, UpdatesInformationDlg::OnBtnApply )
    EVT_BUTTON( widFirst + 3, UpdatesInformationDlg::OnBtnApply )
    EVT_BUTTON( widFirst + 4, UpdatesInformationDlg::OnBtnApply )
    EVT_BUTTON( widFirst + 5, UpdatesInformationDlg::OnBtnApply )
    EVT_CLOSE( UpdatesInformationDlg::OnClose )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
UpdatesInformationDlg::UpdatesInformationDlg( wxWindow* pParent, const wxString& title, const StringToStringMap& olderDriverVersions,
        const wxString& currentVersion, const wxString& newestVersion, const wxString& dateReleased, const wxString& whatsNew, const bool boCurrentAutoUpdateCheckState ) :
    OkAndCancelDlg( pParent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize ), pParent_( pParent ), olderDriverVersions_( olderDriverVersions ),
    currentVersion_( currentVersion ), newestVersion_( newestVersion ), dateReleased_( dateReleased ), whatsNew_( whatsNew ), packageToDownload_( ), fileName_( wxT( "" ) ), platformInfo_(), packageIDs_()
//-----------------------------------------------------------------------------
{
    /*
    |-------------------------------------|
    | pTopDownSizer                       |
    |                spacer               |
    | |---------------------------------| |
    | |          Release Info           | |
    | |---------------------------------| |
    |                spacer               |
    | |---------------------------------| |
    | |            WhatsNew             | |
    | |---------------------------------| |
    |-------------------------------------|
    */

    packageNames_.insert( make_pair( wxT( "mvBlueFOX" ), wxT( "mvBlueFOX" ) ) );
    packageNames_.insert( make_pair( wxT( "mvSIGMAfg" ), wxT( "mvDELTA_mvSIGMA" ) ) );
    packageNames_.insert( make_pair( wxT( "mvGenTLConsumer" ), wxT( "mvGenTL_Acquire" ) ) );
    packageNames_.insert( make_pair( wxT( "mvHYPERIONfg" ), wxT( "mvHYPERION" ) ) );
    packageNames_.insert( make_pair( wxT( "mvTITANfg" ), wxT( "mvTITAN_mvGAMMA" ) ) );
    packageNames_.insert( make_pair( wxT( "mvVirtualDevice" ), wxT( "mvVirtualDevice" ) ) );

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 20 );
    wxPanel* pPanel = new wxPanel( this );

    wxFont tmpFont = pPanel->GetFont();
    tmpFont.SetPointSize( pPanel->GetFont().GetPointSize() + 1 );

    wxStaticText* pVersionText = new wxStaticText( pPanel, wxID_ANY, ( VersionFromString( currentVersion_ ) < VersionFromString( newestVersion_ ) ) ? wxT( "A newer mvIMPACT Acquire version is available:" ) : wxT( "The newest mvIMPACT Acquire version is already installed on your system:" ) );
    pVersionText->SetFont( tmpFont );

    wxStaticText* pNewVersion = new wxStaticText( pPanel, wxID_ANY, ( VersionFromString( newestVersion_ ) >= VersionFromString( currentVersion_ ) ) ? newestVersion_ : currentVersion_ );
    wxStaticText* pDateText = new wxStaticText( pPanel, wxID_ANY, wxT( "Release date:" ) );
    wxStaticText* pDate = new wxStaticText( pPanel, wxID_ANY, ( VersionFromString( newestVersion_ ) >= VersionFromString( currentVersion_ ) ) ? dateReleased_ : wxString( wxT( "Not yet released" ) ) );
    wxStaticText* pWhatsNewText = new wxStaticText( pPanel, wxID_ANY, wxT( "What's new:" ) );

    wxTextCtrl* pWhatsNew = new wxTextCtrl( pPanel, wxID_ANY, whatsNew_, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY | wxTE_DONTWRAP );
    wxFont* fixedWidthFont = new wxFont( 8, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    pWhatsNew->SetFont( *fixedWidthFont );
    pWhatsNew->ShowPosition( 0 );
    pTopDownSizer->Add( pVersionText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
    pTopDownSizer->Add( pNewVersion, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
    if( VersionFromString( currentVersion_ ) < VersionFromString( newestVersion_ ) )
    {
        pNewVersion->SetForegroundColour( *wxBLUE );
        wxStaticText* pOldVersion = new wxStaticText( pPanel, wxID_ANY, wxString::Format( wxT( "(current version is %s)" ), currentVersion_.c_str() ) );
        pTopDownSizer->Add( pOldVersion, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
    }

    if( olderDriverVersions.empty() )
    {
        pTopDownSizer->AddSpacer( 25 );
        pTopDownSizer->Add( pDateText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
        pTopDownSizer->Add( pDate, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
        pTopDownSizer->AddSpacer( 25 );
    }
    else
    {
        wxStaticText* pDriversNotUpToDateText = new wxStaticText( pPanel, wxID_ANY, wxString::Format( wxT( "%s, following driver%s not up to date:" ),
                VersionFromString( currentVersion_ ) < VersionFromString( newestVersion_ ) ? "Furthermore" : "However",
                olderDriverVersions.size() == 1 ? wxT( " is" ) : wxT( "s are" ) ) );
        wxStaticText* pDriversUpdateRecommendationText = new wxStaticText( pPanel, wxID_ANY, wxT( "It is recommended that all installed drivers are of the same version!" ) );
        pDriversNotUpToDateText->SetFont( tmpFont );
        pDriversUpdateRecommendationText->SetFont( tmpFont );

        pTopDownSizer->AddSpacer( 10 );
        pTopDownSizer->Add( pDateText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
        pTopDownSizer->Add( pDate, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
        pTopDownSizer->AddSpacer( 25 );
        pTopDownSizer->Add( pDriversNotUpToDateText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
        pTopDownSizer->AddSpacer( 10 );
        StringToStringMap::const_iterator it = olderDriverVersions_.begin();
        StringToStringMap::const_iterator itEND = olderDriverVersions_.end();
        int cnt = 0;

        for( ; it != itEND; it++ )
        {
            wxGridSizer* pDriverPackageSizer = new wxGridSizer( wxHORIZONTAL );
            wxStaticText* pOldDriverText = new wxStaticText( pPanel, wxID_ANY, wxString::Format( wxT( "%s(Version %s)" ), ( it->first ).mb_str(), ( it->second ).mb_str() ) );
            pOldDriverText->SetForegroundColour( acDarkBluePastel );
            pDriverPackageSizer->AddSpacer( 25 );
            pDriverPackageSizer->Add( pOldDriverText, wxSizerFlags().Align( wxALIGN_LEFT ).Border( wxALL, 5 ) );

            if ( platformInfo_.GetOperatingSystemId() == wxOS_WINDOWS_NT )
            {
                packageIDs_.insert( make_pair( widFirst + cnt, ( it->first ).mb_str() ) );
                wxButton* pBtnDownload = new wxButton( pPanel, widFirst + cnt, wxT( "Update" ) );
                pBtnDownload->SetToolTip( ParseDriverToPackageName( ( it->first ).mb_str() ) );
                pDriverPackageSizer->Add( pBtnDownload, wxSizerFlags().Align( wxALIGN_RIGHT ) ) ;
                cnt++;
            }

            pTopDownSizer->Add( pDriverPackageSizer, wxSizerFlags().Expand() );
            pTopDownSizer->AddSpacer( 5 );
        }
        pTopDownSizer->AddSpacer( 10 );
        pTopDownSizer->Add( pDriversUpdateRecommendationText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
        pTopDownSizer->AddSpacer( 10 );
        if( !olderDriverVersions_.empty() && ( platformInfo_.GetOperatingSystemId() == wxOS_WINDOWS_NT ) )
        {
            wxStaticText* pDriversUpdateProcessInformationText = new wxStaticText( pPanel, wxID_ANY, wxT( "Once an 'Update'-Button has been pressed, the corresponding package will be downloaded to the\ncurrent user's 'Downloads' directory. When the download has finished you can select to start the\ninstallation automatically or do it manually at a later point in time." ) );
            pDriversUpdateProcessInformationText->SetFont( tmpFont );
            pTopDownSizer->AddSpacer( 10 );
            pTopDownSizer->Add( pDriversUpdateProcessInformationText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
            pTopDownSizer->AddSpacer( 25 );
        }
    }

    pTopDownSizer->Add( pWhatsNewText, wxSizerFlags().Align( wxALIGN_CENTRE_HORIZONTAL ) );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pWhatsNew, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 10 );

    pDateText->SetFont( tmpFont );
    pWhatsNewText->SetFont( tmpFont );
    tmpFont.SetWeight( wxFONTWEIGHT_BOLD );
    pDate->SetFont( tmpFont );
    tmpFont.SetPointSize( tmpFont.GetPointSize() + 1 );
    pNewVersion->SetFont( tmpFont );

    // lower line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pCBAutoCheckForUpdatesWeekly_ = new wxCheckBox( pPanel, wxID_ANY, wxT( "&Auto-Check For Updates Every Week..." ) );
    pCBAutoCheckForUpdatesWeekly_->SetValue( boCurrentAutoUpdateCheckState );
    pButtonSizer->Add( pCBAutoCheckForUpdatesWeekly_, wxSizerFlags().Border( wxALL, 7 ) );
    pButtonSizer->AddStretchSpacer( 100 );
    if( ( !olderDriverVersions_.empty() ) ||
        ( VersionFromString( currentVersion_ ) < VersionFromString( newestVersion_ ) ) )
    {
        pBtnOk_ = new wxButton( pPanel, widBtnOk, wxT( "&Go To Download Page" ) );
        pButtonSizer->Add( pBtnOk_, wxSizerFlags().Border( wxALL, 7 ) );
    }
    pBtnCancel_ = new wxButton( pPanel, widBtnCancel, wxT( "&Ok" ) );
    pButtonSizer->Add( pBtnCancel_, wxSizerFlags().Border( wxALL, 7 ) );
    pTopDownSizer->Add( pButtonSizer, wxSizerFlags().Expand() );

    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 640, -1 );
    Center();
}

//------------------------------------------------------------------------------
void UpdatesInformationDlg::OnBtnApply( wxCommandEvent& e )
//------------------------------------------------------------------------------
{
    fileName_ = wxEmptyString;
    map<int, wxString>::const_iterator it = packageIDs_.find( e.GetId() );

    if( it != packageIDs_.end() )
    {
        packageToDownload_ = ParseDriverToPackageName( it->second );
    }

    if( packageToDownload_ != wxEmptyString )
    {
        wxString packageURL = wxT( "http://static.matrix-vision.com/mvIMPACT_Acquire/" );
        packageURL = packageURL.Append( newestVersion_ ).Append( wxString( wxT( "/" ) ) ).Append( packageToDownload_ );

        if( packageURL != wxEmptyString )
        {
            // This has to be called in order to be able to initialize sockets outside of
            // the main thread ( http://www.litwindow.com/Knowhow/wxSocket/wxsocket.html )
            wxSocketBase::Initialize();

            int maxUpdateTime_ms = ( packageToDownload_.Contains( wxT( "mvGenTL_Acquire" ) ) ) ? 180000 : 90000;

            static const int s_UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS = 1000;

            DownloadMVSoftwarePackageThread* pDownloadSoftwarePackageThread = new DownloadMVSoftwarePackageThread( packageURL, wxTHREAD_JOINABLE, maxUpdateTime_ms );
            pDownloadSoftwarePackageThread->AttachEventHandler( GetEventHandler(), GetId() );
            pDownloadSoftwarePackageThread->Run();

            wxBusyCursor busyCursorScope;

            wxProgressDialog dlg( wxT( "Downloading Latest Driver Package" ),
                                  wxT( "Contacting MATRIX VISION Servers..." ),
                                  static_cast<int>( maxUpdateTime_ms ), // range
                                  this, // parent
                                  wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME | wxPD_CAN_ABORT | wxPD_SMOOTH );

            int progress = 0;
            while( pDownloadSoftwarePackageThread->IsRunning() )
            {
                if( !dlg.WasCancelled() )
                {
                    wxMilliSleep( s_UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS );
                    dlg.Update(  progress, wxT( "Downloading software package... " ) );
                    progress = progress + s_UPDATE_THREAD_PROGRESS_INTERVAL_MILLISECONDS;
                }
                else if( !pDownloadSoftwarePackageThread->GetAborted() )
                {
                    pDownloadSoftwarePackageThread->Abort();
                }

                if( fileName_ == wxT( "" ) )
                {
                    fileName_ = pDownloadSoftwarePackageThread->GetDownloadedFileName();
                }
            }
            pDownloadSoftwarePackageThread->Wait();
            if( !dlg.WasCancelled() )
            {
                if( wxMessageBox( wxString::Format( wxT( "Would you like to start the update now?\n\nwxPropView will close itself during the process. Please make sure all other MATRIX VISION tools are closed before proceeding.\n\nTo abort the update process, please select 'No'." ) ), wxT( "Update Information" ), wxYES_NO | wxICON_INFORMATION ) == wxYES )
                {
                    EndModal( wxID_SETUP );
                }
            }
        }
        else if( packageToDownload_ == wxEmptyString )
        {
            wxMessageBox( wxString::Format( wxT( "Error\n\nThere went something wrong." ) ), wxT( "Update Failed" ),  wxOK | wxICON_ERROR );
        }
        else if( fileName_ == wxT( "NoFile" ) )
        {
            wxMessageBox( wxString::Format( wxT( "Error\n\nThere was an error downloading the update. Please try again later." ) ), wxT( "Update Failed" ), wxOK | wxICON_EXCLAMATION );
        }
        else if( fileName_ == wxT( "DownloadDirectoryNotFound" ) )
        {
            wxMessageBox( wxString::Format( wxT( "Error\n\nCan not proceed. The destination directory has not been found on the system." ) ), wxT( "Update Failed" ), wxOK | wxICON_ERROR );
        }
    }
}

//------------------------------------------------------------------------------
wxString UpdatesInformationDlg::GetDownloadedPackageName( void )
//------------------------------------------------------------------------------
{
    return fileName_;
}

//------------------------------------------------------------------------------
wxString UpdatesInformationDlg::ParseDriverToPackageName( const wxString& driverName )
//------------------------------------------------------------------------------
{
    const map<wxString, wxString>::const_iterator it = packageNames_.find( driverName );
    wxString packageName = ( it == packageNames_.end() ) ? wxString( wxEmptyString ) : it->second;
    if( packageName != wxEmptyString )
    {
        const wxString packageExtension = packageName.Contains( wxT( "mvGenTL_Acquire" ) ) ? wxString( wxT( ".exe" ) ) : wxString( wxT( ".msi" ) );
        packageName = packageName.Append( wxString( ( platformInfo_.GetArchitecture() == wxARCH_64 ) ? wxT( "-x86_64" ) : wxT( "-x86" ) ) ).Append( wxString( wxT( "-" ) ) ).Append( newestVersion_ ).Append( packageExtension );
    }
    return packageName;
}
#endif // BUILD_WITH_UPDATE_DIALOG_SUPPORT

//=============================================================================
//============== Implementation FindFeatureDlg ================================
//=============================================================================
BEGIN_EVENT_TABLE( FindFeatureDlg, OkAndCancelDlg )
    EVT_CHECKBOX( widCBMatchCase, FindFeatureDlg::OnMatchCaseChanged )
    EVT_LISTBOX( widLBFeatureList, FindFeatureDlg::OnFeatureListSelect )
    EVT_LISTBOX_DCLICK( widLBFeatureList, FindFeatureDlg::OnFeatureListDblClick )
    EVT_TEXT( widTCFeatureName, FindFeatureDlg::OnFeatureNameTextChanged )
END_EVENT_TABLE()
//-----------------------------------------------------------------------------
FindFeatureDlg::FindFeatureDlg( PropGridFrameBase* pParent, const NameToFeatureMap& nameToFeatureMap, const bool boMatchCaseActive, const bool boDisplayInvisibleComponents ) :
    OkAndCancelDlg( pParent, wxID_ANY, wxT( "Find Feature" ), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
    pParent_( pParent ), nameToFeatureMap_( nameToFeatureMap ), boDisplayInvisibleComponents_( boDisplayInvisibleComponents )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        |                message              |
        |                spacer               |
        | |---------------------------------| |
        | | pLBFeatureList_                 | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxPanel* pPanel = new wxPanel( this );

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Type the name of feature you are looking for." ) ) );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " " ) ) );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " A Double-click on a list item will automatically select the feature and close this dialog. " ) ) );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " " ) ) );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Use '|' to combine multiple search strings(whitespace characters will be interpreted as part of the " ) ) );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " tokens to search for). " ) ) );
    pTopDownSizer->AddSpacer( 10 );
    pTCFeatureName_ = new wxTextCtrl( pPanel, widTCFeatureName );
    pTopDownSizer->Add( pTCFeatureName_, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 10 );
    pCBMatchCase_ = new wxCheckBox( pPanel, widCBMatchCase, wxT( "Match &case" ) );
    pCBMatchCase_->SetValue( boMatchCaseActive );
    pTopDownSizer->Add( pCBMatchCase_ );
    pTopDownSizer->AddSpacer( 10 );
    wxArrayString features;
    BuildFeatureList( features );
    pLBFeatureList_ = new wxListBox( pPanel, widLBFeatureList, wxDefaultPosition, wxDefaultSize, features );
    pTopDownSizer->Add( pLBFeatureList_, wxSizerFlags( 6 ).Expand() );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 200, 250 );
}

//-----------------------------------------------------------------------------
void FindFeatureDlg::BuildFeatureList( wxArrayString& features, const bool boMatchCase /* = false */, const wxString& pattern /* = wxEmptyString */ ) const
//-----------------------------------------------------------------------------
{
    NameToFeatureMap::const_iterator it = nameToFeatureMap_.begin();
    const NameToFeatureMap::const_iterator itEND = nameToFeatureMap_.end();
    wxArrayString searchTokens = wxSplit( pattern, wxT( '|' ) );
    wxArrayString::size_type searchTokenCount = searchTokens.size();
    while( it != itEND )
    {
        bool boAddToList = pattern.IsEmpty();
        if( !boAddToList )
        {
            const wxString candidate( boMatchCase ? it->first : it->first.Lower() );
            for( wxArrayString::size_type i = 0; i < searchTokenCount; i++ )
            {
                if( candidate.Find( searchTokens[i].c_str() ) != wxNOT_FOUND )
                {
                    boAddToList = true;
                    break;
                }
            }
        }

        if( boAddToList )
        {
            Component c( it->second->GetComponent() );
            if( c.visibility() != cvInvisible )
            {
                const size_t index = features.Add( it->first );
                if( GlobalDataStorage::Instance()->GetComponentVisibility() < c.visibility() )
                {
                    features[index].Append( wxString::Format( wxT( " (%s level)" ), ConvertedString( c.visibilityAsString() ).c_str() ) );
                }
                if( !c.isVisible() )
                {
                    features[index].Append( wxT( " (currently invisible)" ) );
                }
            }
        }
        ++it;
    }
}

//-----------------------------------------------------------------------------
void FindFeatureDlg::OnFeatureListDblClick( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    SelectFeatureInPropertyGrid( e.GetSelection() );
    Close();
}

//-----------------------------------------------------------------------------
void FindFeatureDlg::SelectFeatureInPropertyGrid( const wxString& selection )
//-----------------------------------------------------------------------------
{
    NameToFeatureMap::const_iterator it = nameToFeatureMap_.find( selection );
    if( it != nameToFeatureMap_.end() )
    {
        pParent_->SelectPropertyInPropertyGrid( it->second );
    }
}

//-----------------------------------------------------------------------------
void FindFeatureDlg::SelectFeatureInPropertyGrid( const int selection )
//-----------------------------------------------------------------------------
{
    SelectFeatureInPropertyGrid( pLBFeatureList_->GetString( selection ) );
}

//-----------------------------------------------------------------------------
void FindFeatureDlg::UpdateFeatureList( void )
//-----------------------------------------------------------------------------
{
    wxString pattern( pCBMatchCase_->GetValue() ? pTCFeatureName_->GetValue() : pTCFeatureName_->GetValue().Lower() );
    while( !pattern.IsEmpty() && ( pattern.Last() == wxT( '|' ) ) )
    {
        pattern.RemoveLast();
    }
    pLBFeatureList_->Clear();
    wxArrayString features;
    BuildFeatureList( features, pCBMatchCase_->GetValue(), pattern );
    if( !features.empty() )
    {
        pLBFeatureList_->InsertItems( features, 0 );
    }
}

//=============================================================================
//============== Implementation DetailedFeatureInfoDlg ========================
//=============================================================================
//-----------------------------------------------------------------------------
DetailedFeatureInfoDlg::DetailedFeatureInfoDlg( wxWindow* pParent, Component comp ) :
    OkAndCancelDlg( pParent, wxID_ANY, wxT( "Detailed Feature Info" ), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
    pLogWindow_( 0 )
//-----------------------------------------------------------------------------
{
    fixedPitchStyle_.SetFont( wxFont( 10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL ) );
    fixedPitchStyle_.SetTextColour( *wxBLUE );
    wxFont font( 10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD );
    //font.SetUnderlined( true );
    fixedPitchStyleBold_.SetFont( font );
    /*
        |-------------------------------------|
        | pFlexGridSizer                      |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */
    wxPanel* pPanel = new wxPanel( this );
    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pLogWindow_ = new wxTextCtrl( pPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );
    AddFeatureInfo( wxT( "Component Name: " ), ConvertedString( comp.name() ) );
    AddFeatureInfo( wxT( "Component Display Name: " ), ConvertedString( comp.displayName() ) );
    AddFeatureInfo( wxT( "Component Description: " ), ConvertedString( comp.docString() ) );
    AddFeatureInfo( wxT( "Component Type: " ), ConvertedString( comp.typeAsString() ) );
    AddFeatureInfo( wxT( "Component Visibility: " ), ConvertedString( comp.visibilityAsString() ) );
    AddFeatureInfo( wxT( "Component Representation: " ), ConvertedString( comp.representationAsString() ) );
    AddFeatureInfo( wxT( "Component Flags: " ), ConvertedString( comp.flagsAsString() ) );
    AddFeatureInfo( wxT( "Component Default State: " ), comp.isDefault() ? wxT( "Yes" ) : wxT( "No" ) );
    {
        vector<mvIMPACT::acquire::Component> selectingFeatures;
        comp.selectingFeatures( selectingFeatures );
        ostringstream oss;
        if( selectingFeatures.empty() )
        {
            oss << "NO OTHER FEATURE";
        }
        else
        {
            PropData::AppendSelectorInfo( oss, selectingFeatures );
        }
        AddFeatureInfo( wxT( "Component Is Selected By: " ), ConvertedString( oss.str() ) );
    }
    {
        vector<mvIMPACT::acquire::Component> selectedFeatures;
        comp.selectedFeatures( selectedFeatures );
        ostringstream oss;
        if( selectedFeatures.empty() )
        {
            oss << "NO OTHER FEATURE";
        }
        else
        {
            PropData::AppendSelectorInfo( oss, selectedFeatures );
        }
        AddFeatureInfo( wxT( "Component Selects: " ), ConvertedString( oss.str() ) );
    }
    AddFeatureInfo( wxT( "Component Handle(HOBJ): " ), wxString::Format( wxT( "0x%08x" ), comp.hObj() ) );
    AddFeatureInfo( wxT( "Component Changed Counter: " ), wxString::Format( wxT( "%d" ), comp.changedCounter() ) );
    AddFeatureInfo( wxT( "Component Changed Counter(Attributes): " ), wxString::Format( wxT( "%d" ), comp.changedCounterAttr() ) );
    const TComponentType type( comp.type() );
    if( type == ctList )
    {
        ComponentList list( comp );
        AddFeatureInfo( wxT( "List Content Descriptor: " ), ConvertedString( list.contentDescriptor() ) );
        AddFeatureInfo( wxT( "List Size: " ), wxString::Format( wxT( "%d" ), list.size() ) );
    }
    else if( type == ctMeth )
    {
        Method meth( comp );
        AddFeatureInfo( wxT( "Method Parameter List: " ), ConvertedString( meth.paramList() ) );
        AddFeatureInfo( wxT( "Method Signature: " ), MethodObject::BuildFriendlyName( meth.hObj() ) );
    }
    else if( type & ctProp )
    {
        Property prop( comp );
        AddFeatureInfo( wxT( "Property String Format String: " ), ConvertedString( prop.stringFormatString() ) );
        const int valCnt = static_cast<int>( prop.valCount() );
        AddFeatureInfo( wxT( "Property Value Count: " ), wxString::Format( wxT( "%d" ), valCnt ) );
        AddFeatureInfo( wxT( "Property Value Count(max): " ), wxString::Format( wxT( "%u" ), prop.maxValCount() ) );
        if( ( prop.type() == ctPropString ) && ( prop.flags() & cfContainsBinaryData ) )
        {
            // The 'prop.type()' check is only needed because some drivers with versions < 1.12.33
            // did incorrectly specify the 'cfContainsBinaryData' flag even though the data type was not 'ctPropString'...
            for( int i = 0; i < valCnt; i++ )
            {
                PropertyS propS( comp );
                AddFeatureInfo( wxString::Format( wxT( "Property Value[%d]: " ), i ), wxEmptyString );
                wxString binaryDataFormatted;
                try
                {
                    wxString binaryDataRAW( ConvertedString( BinaryDataToString( propS.readBinary( i ) ) ) );
                    BinaryDataDlg::FormatData( binaryDataRAW, binaryDataFormatted, 64, 8 );
                    binaryDataFormatted.RemoveLast(); // Remove the last '\n' as this will be added by 'AddFeatureInfo' as well!
                    AddFeatureInfo( wxEmptyString, binaryDataFormatted );
                }
                catch( const ImpactAcquireException& e )
                {
                    AddFeatureInfoFromException( wxEmptyString, e );
                }
                AddFeatureInfo( wxString::Format( wxT( "Property Value Binary Buffer Size[%d]: " ), i ), wxString::Format( wxT( "%u" ), propS.binaryDataBufferSize( i ) ) );
            }
        }
        else
        {
            for( int i = 0; i < valCnt; i++ )
            {
                const wxString infoValue = wxString::Format( wxT( "Property Value[%d]: " ), i );
                try
                {
                    AddFeatureInfo( infoValue, ConvertedString( prop.readS( i, string( ( prop.flags() & cfAllowValueCombinations ) ? "\"%s\" " : "" ) ) ) );
                }
                catch( const ImpactAcquireException& e )
                {
                    AddFeatureInfoFromException( infoValue, e );
                }
            }
        }
        try
        {
            AddFeatureInfo( wxT( "Property Value(min): " ), ConvertedString( prop.hasMinValue() ? prop.readS( plMinValue ) : string( "NOT DEFINED" ) ) );
        }
        catch( const ImpactAcquireException& e )
        {
            AddFeatureInfoFromException( wxT( "Property Value(min): " ), e );
        }
        try
        {
            AddFeatureInfo( wxT( "Property Value(max): " ), ConvertedString( prop.hasMaxValue() ? prop.readS( plMaxValue ) : string( "NOT DEFINED" ) ) );
        }
        catch( const ImpactAcquireException& e )
        {
            AddFeatureInfoFromException( wxT( "Property Value(max): " ), e );
        }
        try
        {
            AddFeatureInfo( wxT( "Property Value(inc): " ), ConvertedString( prop.hasStepWidth() ? prop.readS( plStepWidth ) : string( "NOT DEFINED" ) ) );
        }
        catch( const ImpactAcquireException& e )
        {
            AddFeatureInfoFromException( wxT( "Property Value(inc): " ), e );
        }
        if( ( type == ctPropString ) && ( prop.flags() & cfContainsBinaryData ) )
        {
            PropertyS propS( comp );
            AddFeatureInfo( wxT( "Property Value Binary Buffer Size(max): " ), wxString::Format( wxT( "%u" ), propS.binaryDataBufferMaxSize() ) );
        }

        if( prop.hasDict() )
        {
            const wxString formatString = ConvertedString( prop.stringFormatString() );
            try
            {
                if( type == ctPropInt )
                {
                    vector<pair<string, int> > dict;
                    PropertyI( prop ).getTranslationDict( dict );
                    const vector<pair<string, int> >::size_type vSize = dict.size();
                    for( vector<pair<string, int> >::size_type i = 0; i < vSize; i++ )
                    {
                        const wxString value = wxString::Format( formatString.c_str(), dict[i].second );
                        AddFeatureInfo( wxString::Format( wxT( "Property Translation Dictionary Entry[%u]: " ), static_cast<unsigned int>( i ) ), wxString::Format( wxT( "%s(%s)" ), ConvertedString( dict[i].first ).c_str(), value.c_str() ) );
                    }
                }
                else if( type == ctPropInt64 )
                {
                    vector<pair<string, int64_type> > dict;
                    PropertyI64( prop ).getTranslationDict( dict );
                    const vector<pair<string, int64_type> >::size_type vSize = dict.size();
                    for( vector<pair<string, int64_type> >::size_type i = 0; i < vSize; i++ )
                    {
                        const wxString value = wxString::Format( formatString.c_str(), dict[i].second );
                        AddFeatureInfo( wxString::Format( wxT( "Property Translation Dictionary Entry[%u]: " ), static_cast<unsigned int>( i ) ), wxString::Format( wxT( "%s(%s)" ), ConvertedString( dict[i].first ).c_str(), value.c_str() ) );
                    }
                }
                else if( type == ctPropFloat )
                {
                    vector<pair<string, double> > dict;
                    PropertyF( prop ).getTranslationDict( dict );
                    const vector<pair<string, double> >::size_type vSize = dict.size();
                    for( vector<pair<string, double> >::size_type i = 0; i < vSize; i++ )
                    {
                        const wxString value = wxString::Format( formatString.c_str(), dict[i].second );
                        AddFeatureInfo( wxString::Format( wxT( "Property Translation Dictionary Entry[%u]: " ), static_cast<unsigned int>( i ) ), wxString::Format( wxT( "%s(%s)" ), ConvertedString( dict[i].first ).c_str(), value.c_str() ) );
                    }
                }
            }
            catch( const ImpactAcquireException& e )
            {
                AddFeatureInfoFromException( wxT( "Property Translation Dictionary: " ), e );
            }
        }
    }
    pTopDownSizer->Add( pLogWindow_, wxSizerFlags( 6 ).Expand() );
    pLogWindow_->ScrollLines( -( 256 * 256 ) ); // make sure the text control always shows the beginning of the help text
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 700, 700 );
}

//-----------------------------------------------------------------------------
void DetailedFeatureInfoDlg::AddFeatureInfo( const wxString& infoName, const wxString& info )
//-----------------------------------------------------------------------------
{
    WriteToTextCtrl( pLogWindow_, infoName, fixedPitchStyleBold_ );
    if( !infoName.IsEmpty() && ( infoName.Last() != wxChar( wxT( ' ' ) ) ) )
    {
        WriteToTextCtrl( pLogWindow_, wxT( " " ), fixedPitchStyle_ );
    }
    WriteToTextCtrl( pLogWindow_, info, fixedPitchStyle_ );
    WriteToTextCtrl( pLogWindow_, wxT( "\n" ) );
}

//-----------------------------------------------------------------------------
void DetailedFeatureInfoDlg::AddFeatureInfoFromException( const wxString& infoName, const ImpactAcquireException& e )
//-----------------------------------------------------------------------------
{
    AddFeatureInfo( infoName, wxString::Format( wxT( "There was a problem reading this value: %s(%d(%s))" ), ConvertedString( e.getErrorString() ).c_str(), e.getErrorCode(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
}

//=============================================================================
//============== Implementation BinaryDataDlg =================================
//=============================================================================
BEGIN_EVENT_TABLE( BinaryDataDlg, OkAndCancelDlg )
    EVT_TEXT( widTCBinaryData, BinaryDataDlg::OnBinaryDataTextChanged )
END_EVENT_TABLE()
//-----------------------------------------------------------------------------
BinaryDataDlg::BinaryDataDlg( wxWindow* pParent, const wxString& featureName, const wxString& value ) :
    OkAndCancelDlg( pParent, wxID_ANY, wxString::Format( wxT( "Binary Data Editor(%s)" ), featureName.c_str() ), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
    pTCBinaryData_( 0 ), pTCAsciiData_( 0 )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pLeftRightSizer                 | |
        | |---------------------------------| |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxPanel* pPanel = new wxPanel( this );

    wxFont fixedPitchFont( 10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    fixedPitchStyle_.SetFont( fixedPitchFont );

    pTCBinaryData_ = new wxTextCtrl( pPanel, widTCBinaryData, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH2, HEXStringValidator_ );
    pTCBinaryData_->SetDefaultStyle( fixedPitchStyle_ );
    pTCAsciiData_ = new wxTextCtrl( pPanel, widTCAsciiData, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH2 | wxTE_READONLY );
    pTCAsciiData_->SetDefaultStyle( fixedPitchStyle_ );
    WriteToTextCtrl( pTCBinaryData_, value, fixedPitchStyle_ );
    ReformatBinaryData();

    wxBoxSizer* pLeftRightSizer = new wxBoxSizer( wxHORIZONTAL );
    pLeftRightSizer->Add( pTCBinaryData_, wxSizerFlags( 2 ).Expand() );
    pLeftRightSizer->Add( pTCAsciiData_, wxSizerFlags( 1 ).Expand() );

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pLeftRightSizer, wxSizerFlags( 6 ).Expand() );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 560, 200 );
}

//-----------------------------------------------------------------------------
size_t BinaryDataDlg::FormatData( const wxString& data, wxString& formattedData, const int lineLength, const int fieldLength )
//-----------------------------------------------------------------------------
{
    const size_t len = data.Length();
    for( size_t i = 0; i < len; i++ )
    {
        formattedData.Append( data[i] );
        if( i > 0 )
        {
            if( ( ( i + 1 ) % lineLength ) == 0 )
            {
                formattedData.Append( wxT( "\n" ) );
            }
            else if( ( ( i + 1 ) % fieldLength ) == 0 )
            {
                formattedData.Append( wxT( " " ) );
            }
        }
    }
    return formattedData.Length();
}

//-----------------------------------------------------------------------------
wxString BinaryDataDlg::GetBinaryData( void ) const
//-----------------------------------------------------------------------------
{
    wxString data( pTCBinaryData_->GetValue() );
    RemoveSeparatorChars( data );
    return data;
}

//-----------------------------------------------------------------------------
void BinaryDataDlg::OnBinaryDataTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    ReformatBinaryData();
    UpdateAsciiData();
}

//-----------------------------------------------------------------------------
void BinaryDataDlg::ReformatBinaryData( void )
//-----------------------------------------------------------------------------
{
    wxString data( pTCBinaryData_->GetValue() );
    long x = 0;
    long y = 0;
    pTCBinaryData_->PositionToXY( pTCBinaryData_->GetInsertionPoint(), &x, &y );

    const size_t pos = static_cast<size_t>( ( y * 36 ) + x );
    if( ( ( x == 9 ) || ( x == 18 ) || ( x == 27 ) ) &&
        ( ( data.Length() > pos ) && ( data[pos] == wxT( ' ' ) ) ) )
    {
        ++x;
    }
    RemoveSeparatorChars( data );
    wxString formattedData;
    FormatData( data, formattedData, 32, 8 );
#if wxCHECK_VERSION(2, 7, 1)
    pTCBinaryData_->ChangeValue( formattedData ); // this function will NOT generate a wxEVT_COMMAND_TEXT_UPDATED event.
#else
    pTCBinaryData_->SetValue( formattedData ); // this function will generate a wxEVT_COMMAND_TEXT_UPDATED event and has been declared deprecated.
#endif // #if wxCHECK_VERSION(2, 7, 1)
    pTCBinaryData_->SetStyle( 0, pTCBinaryData_->GetLastPosition(), fixedPitchStyle_ );
    if( x == 36 )
    {
        if( lastDataLength_ > formattedData.Length() )
        {

        }
        else if( lastDataLength_ < formattedData.Length() )
        {
            x = 1;
            ++y;
        }
    }
    lastDataLength_ = formattedData.Length();
    pTCBinaryData_->SetInsertionPoint( pTCBinaryData_->XYToPosition( x, y ) );
}

//-----------------------------------------------------------------------------
void BinaryDataDlg::RemoveSeparatorChars( wxString& data )
//-----------------------------------------------------------------------------
{
    data.Replace( wxT( " " ), wxT( "" ) );
    data.Replace( wxT( "\n" ), wxT( "" ) );
    data.Replace( wxT( "\r" ), wxT( "" ) );
}

//-----------------------------------------------------------------------------
void BinaryDataDlg::UpdateAsciiData( void )
//-----------------------------------------------------------------------------
{
    string binaryDataRawANSI( GetBinaryData().mb_str() );
    string binaryData( BinaryDataFromString( binaryDataRawANSI ) );
    const string::size_type len = binaryData.size();
    for( string::size_type i = 0; i < len; i++ )
    {
        if( static_cast<unsigned char>( binaryData[i] ) < 32 ) // 32 is the whitespace
        {
            binaryData[i] = '.';
        }
    }
    wxString formattedData;
    FormatData( ConvertedString( binaryData ), formattedData, 16, 4 );
#if wxCHECK_VERSION(2, 7, 1)
    pTCAsciiData_->ChangeValue( formattedData ); // this function will NOT generate a wxEVT_COMMAND_TEXT_UPDATED event.
#else
    pTCAsciiData_->SetValue( formattedData ); // this function will generate a wxEVT_COMMAND_TEXT_UPDATED event and has been declared deprecated.
#endif // #if wxCHECK_VERSION(2, 7, 1)
    pTCAsciiData_->SetStyle( 0, pTCAsciiData_->GetLastPosition(), fixedPitchStyle_ );
}

//=============================================================================
//============== Implementation AssignSettingsToDisplaysDlg ===================
//=============================================================================
//-----------------------------------------------------------------------------
AssignSettingsToDisplaysDlg::AssignSettingsToDisplaysDlg( wxWindow* pParent, const wxString& title,
        const vector<pair<string, int> >& settings, const SettingToDisplayDict& settingToDisplayDict, size_t displayCount )
    : OkAndCancelDlg( pParent, wxID_ANY, title )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        |                message              |
        |                spacer               |
        | |---------------------------------| |
        | | pGridSizer                      | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    wxPanel* pPanel = new wxPanel( this, wxID_ANY, wxPoint( 5, 5 ) );
    pTopDownSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Whenever a request is returned by\n the driver it will be drawn onto the\n display the used capture setting\n has been assigned to." ) ) );
    pTopDownSizer->AddSpacer( 10 );
    const int colCount = static_cast<int>( settings.size() );
    ctrls_.resize( colCount );
    wxFlexGridSizer* pGridSizer = new wxFlexGridSizer( 2 );
    wxArrayString choices;
    for( size_t i = 0; i < displayCount; i++ )
    {
        choices.Add( wxString::Format( wxT( "Display %u" ), static_cast<unsigned int>( i ) ) );
    }
    for( int i = 0; i < colCount; i++ )
    {
        pGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxString( wxT( " " ) ) + ConvertedString( settings[i].first ) ) );
        SettingToDisplayDict::const_iterator it = settingToDisplayDict.find( settings[i].second );
        wxArrayString::size_type selection = 0;
        if( settingToDisplayDict.empty() )
        {
            selection = ( displayCount > 0 ) ? i % displayCount : 0;
        }
        else
        {
            selection = ( it != settingToDisplayDict.end() ) ? static_cast<wxArrayString::size_type>( it->second ) : 0;
        }
        ctrls_[i] = new wxComboBox( pPanel, widFirst + i, choices[selection], wxDefaultPosition, wxDefaultSize, choices, wxCB_DROPDOWN | wxCB_READONLY );
        pGridSizer->Add( ctrls_[i], wxSizerFlags().Expand() );
    }
    pTopDownSizer->Add( pGridSizer );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//=============================================================================
//============== Implementation RawImageImportDlg =============================
//=============================================================================
//-----------------------------------------------------------------------------
RawImageImportDlg::RawImageImportDlg( PropGridFrameBase* pParent, const wxString& title, const wxFileName& fileName )
    : OkAndCancelDlg( pParent, wxID_ANY, title ), pParent_( pParent )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pGridSizer                      | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );

    wxPanel* pPanel = new wxPanel( this );
    wxFlexGridSizer* pGridSizer = new wxFlexGridSizer( 2 );
    wxArrayString pixelFormatChoices;
    pixelFormatChoices.Add( wxT( "Mono8" ) );
    pixelFormatChoices.Add( wxT( "Mono10" ) );
    pixelFormatChoices.Add( wxT( "Mono12" ) );
    pixelFormatChoices.Add( wxT( "Mono12Packed_V1" ) );
    pixelFormatChoices.Add( wxT( "Mono12Packed_V2" ) );
    pixelFormatChoices.Add( wxT( "Mono14" ) );
    pixelFormatChoices.Add( wxT( "Mono16" ) );
    pixelFormatChoices.Add( wxT( "BGR888Packed" ) );
    pixelFormatChoices.Add( wxT( "BGR101010Packed_V2" ) );
    pixelFormatChoices.Add( wxT( "RGB888Packed" ) );
    pixelFormatChoices.Add( wxT( "RGB101010Packed" ) );
    pixelFormatChoices.Add( wxT( "RGB121212Packed" ) );
    pixelFormatChoices.Add( wxT( "RGB141414Packed" ) );
    pixelFormatChoices.Add( wxT( "RGB161616Packed" ) );
    pixelFormatChoices.Add( wxT( "RGBx888Packed" ) );
    pixelFormatChoices.Add( wxT( "RGB888Planar" ) );
    pixelFormatChoices.Add( wxT( "RGBx888Planar" ) );
    pixelFormatChoices.Add( wxT( "YUV411_UYYVYY_Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV422Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV422_UYVYPacked" ) );
    pixelFormatChoices.Add( wxT( "YUV422_10Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV422_UYVY_10Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV444_UYVPacked" ) );
    pixelFormatChoices.Add( wxT( "YUV444_UYV_10Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV444Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV444_10Packed" ) );
    pixelFormatChoices.Add( wxT( "YUV422Planar" ) );

    wxArrayString bayerParityChoices;
    bayerParityChoices.Add( wxT( "Undefined" ) );
    bayerParityChoices.Add( wxT( "Red-green" ) );
    bayerParityChoices.Add( wxT( "Green-red" ) );
    bayerParityChoices.Add( wxT( "Blue-green" ) );
    bayerParityChoices.Add( wxT( "Green-blue" ) );

    // The file name is expected to look like this:
    // <name(don't care).<width>x<height>.<pixel format>(BayerPattern=<pattern>).raw
    wxString nakedName = fileName.GetName().BeforeFirst( wxT( '.' ) );
    wxString resolution = fileName.GetName().AfterFirst( wxT( '.' ) ).BeforeFirst( wxT( '.' ) );
    wxString width( resolution.BeforeFirst( wxT( 'x' ) ) );
    wxString height( resolution.AfterFirst( wxT( 'x' ) ) );
    wxString format = fileName.GetName().AfterLast( wxT( '.' ) );
    wxString formatNaked = format.BeforeFirst( wxT( '(' ) );
    wxString bayerPattern = format.AfterFirst( wxT( '=' ) ).BeforeFirst( wxT( ')' ) );

    if( nakedName.IsEmpty() || resolution.IsEmpty() || format.IsEmpty() )
    {
        pParent_->WriteLogMessage( wxString::Format( wxT( "Failed to extract file format from %s(%s %sx%s %s).\n" ), fileName.GetFullPath().c_str(), nakedName.c_str(), width.c_str(), height.c_str(), format.c_str() ) );
    }

    const size_t pixelFormatCount = pixelFormatChoices.Count();
    wxString initialPixelFormat = pixelFormatChoices[0];
    for( size_t i = 1; i < pixelFormatCount; i++ )
    {
        if( formatNaked == pixelFormatChoices[i] )
        {
            initialPixelFormat = pixelFormatChoices[i];
            break;
        }
    }

    const size_t bayerParityCount = bayerParityChoices.Count();
    wxString initialbayerParity = bayerParityChoices[0];
    for( size_t i = 1; i < bayerParityCount; i++ )
    {
        if( bayerPattern == bayerParityChoices[i] )
        {
            initialbayerParity = bayerParityChoices[i];
            break;
        }
    }

    pGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Pixel Format:" ) ) );
    pCBPixelFormat_ = new wxComboBox( pPanel, wxID_ANY, initialPixelFormat, wxDefaultPosition, wxDefaultSize, pixelFormatChoices, wxCB_DROPDOWN | wxCB_READONLY );
    pGridSizer->Add( pCBPixelFormat_, wxSizerFlags().Expand() );

    pGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Bayer Parity:" ) ) );
    pCBBayerParity_ = new wxComboBox( pPanel, wxID_ANY, initialbayerParity, wxDefaultPosition, wxDefaultSize, bayerParityChoices, wxCB_DROPDOWN | wxCB_READONLY );
    pGridSizer->Add( pCBBayerParity_, wxSizerFlags().Expand() );

    long w;
    width.ToLong( &w );
    width = wxString::Format( wxT( "%d" ), w ); // get rid of incorrect additional characters (in case someone did name the file 'bla.1024sdfgsdx1024@blub.Mono8.raw' this removes the 'sdfgsd' sequence
    pGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Width:" ) ) );
    pSCWidth_ = new wxSpinCtrl( pPanel, wxID_ANY, width, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1024 * 256, w );
    pGridSizer->Add( pSCWidth_, wxSizerFlags().Expand() );

    long h;
    height.ToLong( &h );
    height = wxString::Format( wxT( "%d" ), h ); // get rid of incorrect additional characters (in case someone did name the file 'bla.1024sdfgsdx1024@blub.Mono8.raw' this removes the '@blub' sequence
    pGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Height:" ) ) );
    pSCHeight_ = new wxSpinCtrl( pPanel, wxID_ANY, height, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1024 * 256, h );
    pGridSizer->Add( pSCHeight_, wxSizerFlags().Expand() );

    pTopDownSizer->Add( pGridSizer );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//-----------------------------------------------------------------------------
wxString RawImageImportDlg::GetFormat( void ) const
//-----------------------------------------------------------------------------
{
    wxString format( pCBPixelFormat_->GetValue() );
    if( pCBBayerParity_->GetValue() != wxT( "Undefined" ) )
    {
        format.Append( wxString::Format( wxT( "(BayerParity: %s)" ), pCBBayerParity_->GetValue().c_str() ) );
    }
    return format;
}

//-----------------------------------------------------------------------------
long RawImageImportDlg::GetWidth( void ) const
//-----------------------------------------------------------------------------
{
    return pSCWidth_->GetValue();
}

//-----------------------------------------------------------------------------
long RawImageImportDlg::GetHeight( void ) const
//-----------------------------------------------------------------------------
{
    return pSCHeight_->GetValue();
}

//=============================================================================
//============== Implementation OptionsDlg ====================================
//=============================================================================
//-----------------------------------------------------------------------------
OptionsDlg::OptionsDlg( PropGridFrameBase* pParent, const map<TWarnings, bool>& initialWarningConfiguration,
                        const map<TAppearance, bool>& initialAppearanceConfiguration,
                        const map<TPropertyGrid, bool>& initialPropertyGridConfiguration,
                        const map<TMiscellaneous, bool>& initialMiscellaneousConfiguration )
    : OkAndCancelDlg( pParent, wxID_ANY, wxT( "Options" ) ), pWarningConfiguration_( 0 ),
      pAppearanceConfiguration_( 0 ), pPropertyGridConfiguration_( 0 ), pMiscellaneousConfiguration_( 0 ),
      initialWarningConfiguration_(), initialAppearanceConfiguration_(), initialPropertyGridConfiguration_(),
      initialMiscellaneousConfiguration_(), boShowQuickSetupOnDeviceOpen_( false )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pAppearanceConfiguration        | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pWarningConfiguration           | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pPropertyGridConfiguration      | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pMiscellaneousConfiguration     | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );

    wxPanel* pPanel = new wxPanel( this );

    wxArrayString choices;
    choices.resize( aMAX );
    choices[aShowLeftToolBar] = wxT( "Show Left Tool Bar" );
    choices[aShowUpperToolBar] = wxT( "Show Upper Tool Bar" );
    choices[aShowStatusBar] =  wxT( "Show Status Bar" );
    CreateCheckListBox( pPanel, pTopDownSizer, &pAppearanceConfiguration_, choices, wxT( "Appearance: " ), initialAppearanceConfiguration );

    choices.resize( wMAX );
    choices[wWarnOnOutdatedFirmware] = wxT( "Warn On Outdated Firmware" );
    choices[wWarnOnDamagedFirmware] = wxT( "Warn On Damaged Firmware" );
    choices[wWarnOnReducedDriverPerformance] = wxT( "Warn On Reduced Driver Performance" );
#if defined(linux) || defined(__linux) || defined(__linux__)
    choices[wWarnOnPotentialNetworkUSBBufferIssues] = wxT( "Warn On Potential Network/USB Buffer Issues" );
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
    choices[wWarnOnPotentialFirewallIssues] = wxT( "Warn On Potential Firewall Issues" );
    CreateCheckListBox( pPanel, pTopDownSizer, &pWarningConfiguration_, choices, wxT( "Warnings: " ), initialWarningConfiguration );

    choices.resize( pgMAX );
    choices[pgDisplayToolTips] = wxT( "Display Tool Tips" );
    choices[pgUseSelectorGrouping] = wxT( "Use Selector Grouping" );
    choices[pgPreferDisplayNames] = wxT( "Prefer Display Names" );
    choices[pgCreateEditorsWithSlider] = wxT( "Create Editors With Slider" );
    choices[pgDisplayPropertyIndicesAsHex] = wxT( "Display Property Indices As Hex" );
    choices[pgSynchronousMethodExecution] = wxT( "Synchronous Method Execution" );
    CreateCheckListBox( pPanel, pTopDownSizer, &pPropertyGridConfiguration_, choices, wxT( "Property Grid: " ), initialPropertyGridConfiguration );

    choices.resize( mMAX );
    choices[mAllowFastSingleFrameAcquisition] = wxT( "Allow Fast Single Frame Acquisition" );
    choices[mDisplayDetailedInformationOnCallbacks] = wxT( "Display Detailed Information On Callbacks" );
    choices[mDisplayMethodExecutionErrors] = wxT( "Display Method Execution Errors" );
    choices[mDisplayFeatureChangeTimeConsumption] = wxT( "Display Feature Change Time Consumption" );
    choices[mDisplayStatusBarTooltip] = wxT( "Display Status Bar Tooltip" );

    wxStaticBoxSizer* pMiscellaneousConfigurationSizer = CreateCheckListBox( pPanel, pTopDownSizer, &pMiscellaneousConfiguration_, choices, wxT( "Miscellaneous: " ), initialMiscellaneousConfiguration );

    pCBShowQuickSetupOnDeviceOpen_ = new wxCheckBox( pPanel, wxID_ANY, wxT( "Show Quick Setup On Device Open" ) );
    pMiscellaneousConfigurationSizer->AddSpacer( 5 );
    pMiscellaneousConfigurationSizer->Add( pCBShowQuickSetupOnDeviceOpen_ );

    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//-----------------------------------------------------------------------------
void OptionsDlg::BackupCurrentState( void )
//-----------------------------------------------------------------------------
{
    StoreCheckListBoxStateToMap( pWarningConfiguration_, initialWarningConfiguration_ );
    StoreCheckListBoxStateToMap( pAppearanceConfiguration_, initialAppearanceConfiguration_ );
    StoreCheckListBoxStateToMap( pPropertyGridConfiguration_, initialPropertyGridConfiguration_ );
    StoreCheckListBoxStateToMap( pMiscellaneousConfiguration_, initialMiscellaneousConfiguration_ );
    boShowQuickSetupOnDeviceOpen_ = pCBShowQuickSetupOnDeviceOpen_->IsChecked();
}

//-----------------------------------------------------------------------------
template<typename _Ty>
wxStaticBoxSizer* OptionsDlg::CreateCheckListBox( wxPanel* pPanel, wxBoxSizer* pTopDownSizer, wxCheckListBox** ppCheckListBox, const wxArrayString& choices, const wxString& title, const map<_Ty, bool>& initialConfiguration )
//-----------------------------------------------------------------------------
{
    *ppCheckListBox = new wxCheckListBox( pPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices );
    RestoreCheckListBoxStateFromMap( *ppCheckListBox, initialConfiguration );
    wxStaticBoxSizer* pConfigurationSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, title );
    pConfigurationSizer->Add( *ppCheckListBox, wxSizerFlags().Expand() );
    pTopDownSizer->Add( pConfigurationSizer, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 10 );
    return pConfigurationSizer;
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void OptionsDlg::RestoreCheckListBoxStateFromMap( wxCheckListBox* pCheckListBox, const map<_Ty, bool>& stateMap )
//-----------------------------------------------------------------------------
{
    typename map<_Ty, bool>::const_iterator it = stateMap.begin();
    const typename map<_Ty, bool>::const_iterator itEND = stateMap.end();
    while( it != itEND )
    {
        if( it->first < static_cast<_Ty>( pCheckListBox->GetCount() ) )
        {
            pCheckListBox->Check( it->first, it->second );
        }
        ++it;
    }
}

//-----------------------------------------------------------------------------
void OptionsDlg::RestorePreviousState( void )
//-----------------------------------------------------------------------------
{
    RestoreCheckListBoxStateFromMap( pWarningConfiguration_, initialWarningConfiguration_ );
    RestoreCheckListBoxStateFromMap( pAppearanceConfiguration_, initialAppearanceConfiguration_ );
    RestoreCheckListBoxStateFromMap( pPropertyGridConfiguration_, initialPropertyGridConfiguration_ );
    RestoreCheckListBoxStateFromMap( pMiscellaneousConfiguration_, initialMiscellaneousConfiguration_ );
    pCBShowQuickSetupOnDeviceOpen_->SetValue( boShowQuickSetupOnDeviceOpen_ );
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void OptionsDlg::StoreCheckListBoxStateToMap( wxCheckListBox* pCheckListBox, map<_Ty, bool>& stateMap )
//-----------------------------------------------------------------------------
{
    stateMap.clear();
    const unsigned int count = pCheckListBox->GetCount();
    for( unsigned int i = 0; i < count; i++ )
    {
        stateMap.insert( make_pair( static_cast<_Ty>( i ), pCheckListBox->IsChecked( i ) ) );
    }
}

//=============================================================================
//============== Implementation WelcomeDlg ====================================
//=============================================================================
//-----------------------------------------------------------------------------
WelcomeToPropViewDialog::WelcomeToPropViewDialog( PropGridFrameBase* pParent ) :
    OkAndCancelDlg( pParent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER )
//-----------------------------------------------------------------------------
{
    /*
        |----------------------------------------|
        | pSTWelcome                             |
        |                spacer                  |
        | pInfoTextSizer                         |
        |                spacer                  |
        | |------------------------------------| |
        | | button OptionShowQSWSizer          | |
        | |------------------------------------| |
        |                spacer                  |
        | |------------------------------------| |
        | | button OptionDontShowQSWSizer      | |
        | |------------------------------------| |
        |                spacer                  |
        | |------------------------------------| |
        | | button OptionNeverShowQSWSizer     | |
        | |-------------------------------- ---| |
        |----------------------------------------|
    */

    wxPanel* pPanel = new wxPanel( this );

    wxFont headlineFont ( pPanel->GetFont() );
    headlineFont.SetWeight( wxFONTWEIGHT_BOLD );
    headlineFont.SetPointSize( 16 );

    const wxString showQSWHeadline( wxT( "Welcome to the Quick Setup wizard" ) );
    const wxString showQSWInformationString( wxT( "The 'Quick Setup' wizard is a configuration dialog that allows to optimize the device and driver settings to get the best possible\nresults regarding acquisition speed and image quality." ) );
    const wxString showQSWOptionOKString( wxT( "Show 'Quick Setup' wizard now (recommended for beginners)." ) );
    const wxString showQSWOptionDontShowString( wxT( "Don't show the 'Quick Setup' wizard now." ) );
    const wxString showQSWOptionNeverShowString( wxT( "Never show the 'Quick Setup' wizard upon device open." ) );

    pBtnOk_ = new wxButton( pPanel, widBtnOk, showQSWOptionOKString, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "btnOK" ) );
    pBtnApply_ = new wxButton( pPanel, widBtnCancel, showQSWOptionDontShowString, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "btnNO" ) );
    pBtnCancel_ = new wxButton( pPanel, widBtnApply, showQSWOptionNeverShowString, wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "btnNEVER" ) );

    pBtnOk_->SetBitmap( wxBitmap::NewFromPNGData( checked_png, sizeof( checked_png ) ) );
    pBtnApply_->SetBitmap( wxBitmap::NewFromPNGData( cancel_png, sizeof( cancel_png ) ) );
    pBtnCancel_->SetBitmap( wxBitmap::NewFromPNGData( unavailable_png, sizeof( unavailable_png ) ) );

    pBtnOk_->SetToolTip( wxT( "Proceeds to 'Quick Setup' wizard." ) );
    pBtnApply_->SetToolTip( wxT( "No settings will be changed, but the 'Quick Setup' wizard won't be disabled completely." ) );
    pBtnCancel_->SetToolTip( wxT( "The 'Quick Setup' wizard can be reenabled by enabling the 'Show Quick Setup On Device Open' option at the settings menu 'Settings -> Options...'" ) );

    wxBoxSizer* pInfoTextSizer = new wxBoxSizer( wxHORIZONTAL );
    pInfoTextSizer->AddSpacer( 12 );
    pInfoTextSizer->Add( new wxStaticText( pPanel, wxID_ANY, showQSWInformationString ), wxSizerFlags( ).Expand() );
    pInfoTextSizer->AddSpacer( 12 );

    wxStaticText* pSTWelcome =  new wxStaticText( pPanel, wxID_ANY, showQSWHeadline );
    pSTWelcome->SetFont( headlineFont );

    // putting it all together
    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 12 );
    pTopDownSizer->Add( pSTWelcome, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 12 );
    pTopDownSizer->Add( pInfoTextSizer, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 12 );
    AddButtonToSizer( pBtnOk_, pTopDownSizer );
    AddButtonToSizer( pBtnApply_, pTopDownSizer );
    AddButtonToSizer( pBtnCancel_, pTopDownSizer );

    wxBoxSizer* pOuterSizer = new wxBoxSizer( wxHORIZONTAL );
    pOuterSizer->AddSpacer( 12 );
    pOuterSizer->Add( pTopDownSizer, wxSizerFlags().Expand() );
    pOuterSizer->AddSpacer( 12 );

    FinalizeDlgCreation( pPanel, pOuterSizer );
}

//=============================================================================
//============== Implementation SetupRecording  ===============================
//=============================================================================
BEGIN_EVENT_TABLE( SetupRecording, OkAndCancelDlg )
    EVT_CLOSE( SetupRecording::OnClose )
    EVT_BUTTON( widBtnDirPicker, SetupRecording::OnBtnDirPicker )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
SetupRecording::SetupRecording( PropViewFrame* pParent, const wxString& title, RecordingParameters& rRecordingParameters )
    : OkAndCancelDlg( pParent, widMainFrame, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
      pParent_( pParent ), rRecordingParameters_( rRecordingParameters ), boAcquisitionStateOnCancel_( false ), boGUICreated_( false )
//-----------------------------------------------------------------------------
{
    CreateGUI();
    GetCurrentRecordingSettings();
}

//-----------------------------------------------------------------------------
void SetupRecording::CreateGUI( void )
//-----------------------------------------------------------------------------
{
    /*
    |-------------------------------------------------------------|
    |pTopDownSizer                                                |
    | |---------------------------------------------------------| |
    | |pGlobalRecordingSettingsSizer                            | |
    | |                                                         | |
    | | pTCSetupHints_                                          | |
    | |                                                         | |
    | |                                                         | |
    | | pCBEnableRecordingMode_                                 | |
    | |                                                         | |
    | | pCBEnableMetaSaving_                                    | |
    | |                                                         | |
    | | |------------------------|----------------------------| | |
    | | |pFlexGridSizer          |                            | | |
    | | |                        |                            | | |
    | | | pBtnDirPicker_         |  pSelectedOutputDirectory_ | | |
    | | |                        |                            | | |
    | | |------------------------|----------------------------| | |
    | | |                        |                            | | |
    | | | pLBChooseOutputFormat_ |  pInfoChooseDirectory      | | |
    | | |                        |                            | | |
    | | |------------------------|----------------------------| | |
    | |---------------------------------------------------------| |
    |                                                 buttons     |
    |-------------------------------------------------------------|
    */

    TRecordMode recMode = rRecordingParameters_.recordMode_;
    wxPanel* pPanel = new wxPanel( this );
    redAndBoldFont_.SetTextColour( *wxRED );
    redAndBoldFont_.SetFontWeight( wxFONTWEIGHT_BOLD );
    greenAndBoldFont_.SetTextColour( *wxGREEN );
    greenAndBoldFont_.SetFontWeight( wxFONTWEIGHT_BOLD );

    wxFont fixedPitchFont( 10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    fixedPitchFont.SetWeight( wxFONTWEIGHT_BOLD );
    fixedPitchFont.SetUnderlined( true );
    wxTextAttr fixedPitchStyle;
    fixedPitchStyle.SetFont( fixedPitchFont );
    pTCSetupHints_ = new wxTextCtrl( pPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );
    if( recMode == rmSnapshot )
    {
        WriteSnapshotInfoMessage( pTCSetupHints_, fixedPitchFont, fixedPitchStyle );
    }
    else if( recMode == rmHardDisk )
    {
        WriteHardDiskInfoMessage( pTCSetupHints_, fixedPitchFont, fixedPitchStyle );
    }
    else
    {
        wxMessageBox( wxString::Format( wxT( "INVALID/UNHANDLED record mode: %d" ), recMode ), wxT( "INTERNAL ERROR" ) );
        WriteToTextCtrl( pTCSetupHints_, wxT( "This dialog got some weird parameters for it's creation.\n Any changes would most likely end up in an uncertain behavior.\n" ), fixedPitchStyle );
    }
    pTCSetupHints_->ScrollLines( -( 256 * 256 ) ); // make sure the text control always shows the beginning of the help text
    pBtnDirPicker_ = new wxButton( pPanel, widBtnDirPicker, wxT( "Select Directory" ) );

    pSelectedOutputDirectory_ = new wxStaticText( pPanel, wxID_ANY, ( rRecordingParameters_.targetDirectory_.IsEmpty() ) ? wxT( "Directory not selected" ) : wxString::Format( wxT( "%s" ), rRecordingParameters_.targetDirectory_ ) );

    pLBChooseOutputFormat_ = new wxListBox( pPanel, widLBChooseOutputFormat );
    pLBChooseOutputFormat_->Append( wxString( wxT( "*.bmp" ) ) );
    pLBChooseOutputFormat_->Append( wxString( wxT( "*.tif" ) ) );
    pLBChooseOutputFormat_->Append( wxString( wxT( "*.png" ) ) );
    pLBChooseOutputFormat_->Append( wxString( wxT( "*.jpg(slowest)" ) ) );
    pLBChooseOutputFormat_->Append( wxString( wxT( "*.raw(fastest)" ) ) );

    pCBEnableMetaSaving_ = new wxCheckBox( pPanel, widCBEnableMetaSaving, wxT( "Enable saving of image meta data" ) );

    pCBEnableRecordingMode_ = new wxCheckBox( pPanel, widCBEnableRecordingMode,
            ( recMode == rmHardDisk ) ? wxT( "Enable hard disk recording" ) : wxT( "Enable snapshot mode" ) );
    wxStaticText* pInfoChooseDirectory = new wxStaticText( pPanel, wxID_ANY, wxT( "Please select the output \nfile format for the images" ) );

    wxFlexGridSizer* pFlexGridSizer = new wxFlexGridSizer( 2, wxSize( 20, 20 ) );
    pFlexGridSizer->Add( pBtnDirPicker_ );
    pFlexGridSizer->Add( pSelectedOutputDirectory_, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pFlexGridSizer->Add( pLBChooseOutputFormat_ );
    pFlexGridSizer->Add( pInfoChooseDirectory, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );

    wxStaticBoxSizer* pGlobalRecordingSettingsSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, ( recMode == rmHardDisk ) ? wxT( "Hard Disk Recording Settings" ) : wxT( "Snapshot Recording Settings: " ) );
    pGlobalRecordingSettingsSizer->AddSpacer( 5 );
    pGlobalRecordingSettingsSizer->Add( pTCSetupHints_, wxSizerFlags( 3 ).Expand() );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pCBEnableRecordingMode_ );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pCBEnableMetaSaving_ );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pFlexGridSizer );

    pTopDownSizer_ = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer_->AddSpacer( 10 );
    pTopDownSizer_->Add( pGlobalRecordingSettingsSizer, wxSizerFlags( 8 ).Expand() );
    pTopDownSizer_->AddSpacer( 10 );
    AddButtons( pPanel, pTopDownSizer_, true );
    pTopDownSizer_->SetMinSize( wxSize( 650, 700 ) );
    if( rRecordingParameters_.recordMode_ == rmSnapshot )
    {
        pTopDownSizer_->SetMinSize( wxSize( 600, 650 ) );
    }
    FinalizeDlgCreation( pPanel, pTopDownSizer_ );

    boGUICreated_ = true;
}

//-----------------------------------------------------------------------------
void SetupRecording::SelectOutputDirectory()
//-----------------------------------------------------------------------------
{
    rRecordingParameters_.targetDirectory_ = ::wxDirSelector( wxT( "Choose a target directory for the images" ) );
    pSelectedOutputDirectory_->SetLabel( wxString::Format( wxT( "%s" ), rRecordingParameters_.targetDirectory_ ) );
}

//-----------------------------------------------------------------------------
void SetupRecording::OnClose( wxCloseEvent& e )
//-----------------------------------------------------------------------------
{
    Hide();
    if( e.CanVeto() )
    {
        e.Veto();
    }
}

//-----------------------------------------------------------------------------
void SetupRecording::ApplyRecordingSettings( void )
//-----------------------------------------------------------------------------
{
    WriteRecordingStatusEnabled();
    WriteOutputFileFormat();
    WriteMetaDataSavingEnabled();
    CheckRecordingDirNotEmpty();
    pParent_->WriteLogMessage( wxString::Format( wxT( "Current recording settings: Mode: %s (" ), rRecordingParameters_.recordMode_ == rmHardDisk ? wxT( "Hard disk" ) : wxT( "Snapshot" ) ) );
    pParent_->WriteLogMessage( rRecordingParameters_.boActive_ ? wxT( "ENABLED" ) : wxT( "DISABLED" ), rRecordingParameters_.boActive_ ? greenAndBoldFont_ : redAndBoldFont_ );

    pParent_->WriteLogMessage( wxString::Format( wxT( "), Format: %s, Directory : %s, metadata : " ),
                               GetStringRepresentationFromImageFileFormat( rRecordingParameters_.fileFormat_ ).c_str(),
                               rRecordingParameters_.targetDirectory_.c_str() ), wxTextAttr( *wxBLACK, wxNullColour, *wxNORMAL_FONT ) );
    pParent_->WriteLogMessage( rRecordingParameters_.boSaveMetaData_ ? wxT( "ENABLED" ) : wxT( "DISABLED" ), rRecordingParameters_.boSaveMetaData_ ? greenAndBoldFont_ : redAndBoldFont_ );
    pParent_->WriteLogMessage( wxT( "\n" ), wxTextAttr( *wxBLACK, wxNullColour, *wxNORMAL_FONT ) );
}
//-----------------------------------------------------------------------------
void SetupRecording::ReadOutputFileFormat( void )
//-----------------------------------------------------------------------------
{
    if( rRecordingParameters_.fileFormat_ != wxNOT_FOUND )
    {
        pLBChooseOutputFormat_->SetSelection( GetImageFileFormatIndexFromFileFormat( rRecordingParameters_.fileFormat_ ) );
    }
}

//-----------------------------------------------------------------------------
void SetupRecording::ReadMetaDataSavingEnabled( void )
//-----------------------------------------------------------------------------
{
    pCBEnableMetaSaving_->SetValue( rRecordingParameters_.boSaveMetaData_ );
}

//-----------------------------------------------------------------------------
void SetupRecording::ReadRecordingStatusEnabled( void )
//-----------------------------------------------------------------------------
{
    pCBEnableRecordingMode_->SetValue( rRecordingParameters_.boActive_ );
}

//-----------------------------------------------------------------------------
void SetupRecording::CheckRecordingDirNotEmpty( void )
//-----------------------------------------------------------------------------
{
    if( rRecordingParameters_.targetDirectory_.IsEmpty() )
    {
        pParent_->WriteErrorMessage( wxString::Format( wxT( "No output directory selected, recording mode canceled.\n" ) ) );
        rRecordingParameters_.boActive_ = false;
        pCBEnableRecordingMode_->SetValue( rRecordingParameters_.boActive_ );
    }
}

//-----------------------------------------------------------------------------
void SetupRecording::WriteOutputFileFormat( void )
//-----------------------------------------------------------------------------
{
    if( pLBChooseOutputFormat_->GetSelection() == wxNOT_FOUND )
    {
        pParent_->WriteLogMessage( wxString::Format( wxT( "Output file format not selected, recording mode canceled." ) ) );
        rRecordingParameters_.boActive_ = false;
        pCBEnableRecordingMode_->SetValue( rRecordingParameters_.boActive_ );
    }
    else
    {
        rRecordingParameters_.fileFormat_ = GetImageFileFormatFromFileFilterIndex( pLBChooseOutputFormat_->GetSelection() );
    }

}
//-----------------------------------------------------------------------------
void SetupRecording::WriteMetaDataSavingEnabled( void )
//-----------------------------------------------------------------------------
{
    rRecordingParameters_.boSaveMetaData_ = pCBEnableMetaSaving_->GetValue();
}

//-----------------------------------------------------------------------------
void SetupRecording::WriteRecordingStatusEnabled( void )
//-----------------------------------------------------------------------------
{
    rRecordingParameters_.boActive_ = pCBEnableRecordingMode_->GetValue();
}

//-----------------------------------------------------------------------------
void SetupRecording::WriteHardDiskInfoMessage( wxTextCtrl* pTCSetupHints, wxFont fixedPitchFont, wxTextAttr fixedPitchStyle )
//-----------------------------------------------------------------------------
{
    WriteToTextCtrl( pTCSetupHints, wxT( "Setup Hints:\n" ), fixedPitchStyle );
    fixedPitchFont.SetWeight( wxFONTWEIGHT_NORMAL );
    fixedPitchFont.SetUnderlined( false );
    fixedPitchStyle.SetFont( fixedPitchFont );
    WriteToTextCtrl( pTCSetupHints, wxT( "The 'Hard Disk Recording' mode will allow you to capture all displayed\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "images into a pre-selected folder on the systems hard disk.\n\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Note that this will most likely NOT store every image captured,\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "but every image that actually is sent to the display only. When a still\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "image is constantly redrawn ( e.g. because of paint events caused by\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "moving around an AOI ) this will also store the image to disk multiple \n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "times, thus this option can NOT be used as a reliable \n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "'capture data to disk' function but as a simple 'try to record\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "what I see as individual images' option only.\n\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "In this setup dialog you should set a destination folder for\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "the images to get stored to. If you choose to save the image meta\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "data as well, these will be stored in a .xml file beside the images.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Make sure, the ChunkMode is active, to get all available data.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "All changes will apply if you press the OK or the Apply button.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Cancel will end the dialog but wont change any settings.\n" ), fixedPitchStyle );
}

//-----------------------------------------------------------------------------
void SetupRecording::WriteSnapshotInfoMessage( wxTextCtrl* pTCSetupHints, wxFont fixedPitchFont, wxTextAttr fixedPitchStyle )
//-----------------------------------------------------------------------------
{
    WriteToTextCtrl( pTCSetupHints, wxT( "Setup Hints:\n" ), fixedPitchStyle );
    fixedPitchFont.SetWeight( wxFONTWEIGHT_NORMAL );
    fixedPitchFont.SetUnderlined( false );
    fixedPitchStyle.SetFont( fixedPitchFont );
    WriteToTextCtrl( pTCSetupHints, wxT( "The 'Snapshot mode' will allow you to capture the current image\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "of the currently selected display window into a pre-selected folder\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "on the systems hard disk every time you hit the space bar.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "When working with multiple display windows you can select the\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "display window you want to take snapshots from by simply\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "clicking on it. Images will be stored with the current frame\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "number in the file name, so each image can only be captured\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "once, but different images will NOT override each other!\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "In this setup dialog you should set a destination folder for\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "the images to get stored to. If you choose to save the image meta\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "data as well, these will be stored in a .xml file beside the images.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Make sure, the ChunkMode is active, to get all available data.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "All changes will apply if you press the OK or the Apply button.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Cancel will end the dialog but wont change any settings.\n" ), fixedPitchStyle );
}

//=============================================================================
//============== Implementation SetupVideoStreamRecording  ====================
//=============================================================================
BEGIN_EVENT_TABLE( SetupVideoStreamRecording, OkAndCancelDlg )
    EVT_CLOSE( SetupVideoStreamRecording::OnClose )
    EVT_CHECKBOX( widCBEnableRecording, SetupVideoStreamRecording::OnCBEnableRecording )
    EVT_BUTTON( widBtnFilePicker, SetupVideoStreamRecording::OnBtnFilePicker )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
SetupVideoStreamRecording::SetupVideoStreamRecording( PropViewFrame* pParent, const wxString& title, VideoStreamRecordingParameters& rVideoStreamRecordingParameters )
    : OkAndCancelDlg( pParent, widVSRMainFrame, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
      pParent_( pParent ), rVideoStreamRecordingParameters_( rVideoStreamRecordingParameters ), boGUICreated_( false )
//-----------------------------------------------------------------------------
{
    CreateGUI();
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::CreateGUI( void )
//-----------------------------------------------------------------------------
{
    /*
    |-------------------------------------------------------------|
    |pTopDownSizer                                                |
    | |---------------------------------------------------------| |
    | |pGlobalRecordingSettingsSizer                            | |
    | |                                                         | |
    | | pTCVSRSetupHints_                                       | |
    | |                                                         | |
    | | pCBEnableRecording_                                     | |
    | |                                                         | |
    | | |-----------------------------------------------------| | |
    | | |pFlexGridDriverSettingsSizer                         | | |
    | | |                                                     | | |
    | | | |-------------------------------------------------| | | |
    | | | |pFlexGridSizer         |                         | | | |
    | | | |                       |                         | | | |
    | | | | pLBChoosePixelFormat_ |  pInfoChoosePixelFormat | | | |
    | | | |                       |                         | | | |
    | | | |-------------------------------------------------| | | |
    | | |-----------------------------------------------------| | |
    | |                                                         | |
    | | |-----------------------------------------------------| | |
    | | |pFlexGridVideoStreamSettingsSizer                    | | |
    | | |                                                     | | |
    | | | |-----------------------|-------------------------| | | |
    | | | |pFlexGridSizer         |                         | | | |
    | | | |                       |                         | | | |
    | | | | pBtnFilePicker_       |  pSelectedOutputFile_   | | | |
    | | | |                       |                         | | | |
    | | | |-----------------------|-------------------------| | | |
    | | | |                       |                         | | | |
    | | | | pLBChooseCodec_       |  pInfoChooseCodec       | | | |
    | | | |                       |                         | | | |
    | | | |-----------------------|-------------------------| | | |
    | | | |                       |                         | | | |
    | | | | pSPChooseCompression_ |  pInfoChooseCompression | | | |
    | | | |                       |                         | | | |
    | | | |-----------------------|-------------------------| | | |
    | | | |                       |                         | | | |
    | | | | pSPChooseBitrate_     |  pInfoChooseBitrate     | | | |
    | | | |                       |                         | | | |
    | | | |-----------------------|-------------------------| | | |
    | | |-----------------------------------------------------| | |
    | |                                                         | |
    | | pCBEnableStopAcquire_                                   | |
    | |                                                         | |
    | | pCBNotOverwriteFile_                                    | |
    | |                                                         | |
    | |---------------------------------------------------------| |
    |                                                 buttons     |
    |-------------------------------------------------------------|
    */

    wxPanel* pPanel = new wxPanel( this );
    wxFont fixedPitchFont( 10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
    fixedPitchFont.SetWeight( wxFONTWEIGHT_BOLD );
    fixedPitchFont.SetUnderlined( true );
    wxTextAttr fixedPitchStyle;
    fixedPitchStyle.SetFont( fixedPitchFont );

    pTCVSRSetupHints_ = new wxTextCtrl( pPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );
    WriteInfoMessage( pTCVSRSetupHints_, fixedPitchFont, fixedPitchStyle );
    pTCVSRSetupHints_->ScrollLines( -( 256 * 256 ) ); // make sure the text control always shows the beginning of the help text

    pCBEnableRecording_ = new wxCheckBox( pPanel, widCBEnableRecording, wxT( "Enable video stream recording mode" ) );

    pBtnFilePicker_ = new wxButton( pPanel, widBtnFilePicker, wxT( "Select an output file" ) );
    pBtnFilePicker_->Enable( false );
    pSelectedOutputFile_ = new wxStaticText( pPanel, wxID_ANY, ( rVideoStreamRecordingParameters_.tempTargetFile_.IsEmpty() ) ? wxT( "No output file selected" ) : wxString::Format( wxT( "%s" ), rVideoStreamRecordingParameters_.targetFile_ ) );

    pLBChoosePixelFormat_ = new wxListBox( pPanel, widLBChoosePixelFormat );
    pLBChoosePixelFormat_->Append( wxString( wxT( "YUV422Packed" ) ) );
    pLBChoosePixelFormat_->Append( wxString( wxT( "YUV422Planar" ) ) );
    pLBChoosePixelFormat_->Enable( false );

    wxStaticText* pInfoChoosePixelFormat = new wxStaticText( pPanel, wxID_ANY, wxT( "FFmpeg supports only a subset of pixel formats available in mvIMPACT Acquire for encoding.\nThose listed here will be generated by the device driver and can be used for video stream encoding.\nPlease select one." ) );

    pLBChooseCodec_ = new wxListBox( pPanel, widLBChooseCodec );
    pLBChooseCodec_->Append( wxString( wxT( "H.264" ) ) );
    pLBChooseCodec_->Append( wxString( wxT( "H.265" ) ) );
    pLBChooseCodec_->Enable( false );

    wxStaticText* pInfoChooseCodec = new wxStaticText( pPanel, wxID_ANY, wxT( "Please select a video codec" ) );

    pSPChooseCompression_ = new wxSpinCtrl( pPanel, widChooseCompression, wxString( wxT( "60" ) ), wxDefaultPosition, wxDefaultSize,
                                            wxSP_ARROW_KEYS, 0, 100, 60 );
    pSPChooseCompression_->Enable( false );

    wxStaticText* pInfoChooseCompression = new wxStaticText( pPanel, wxID_ANY, wxT( "Please select a compression quality (%)" ) );

    pSPChooseBitrate_ = new wxSpinCtrl( pPanel, widChooseBitrate, wxString( wxT( "6000" ) ), wxDefaultPosition, wxDefaultSize,
                                        wxSP_ARROW_KEYS, 500, 50000, 6000 );
    pSPChooseBitrate_->Enable( false );

    wxStaticText* pInfoChooseBitrate = new wxStaticText( pPanel, wxID_ANY, wxT( "Please select a bitrate value (kbps)" ) );

    pCBEnableStopAcquire_ = new wxCheckBox( pPanel, widCBEnableStopAcquire, wxT( "Synchronize acquisition stop with recording stop." ) );
    pCBEnableStopAcquire_->Enable( false );

    pCBNotOverwriteFile_ = new wxCheckBox( pPanel, widCBEnableOverwriteFile, wxT( "Do not overwrite the recorded video stream if the current output file name is the same as the previous one." ) );
    pCBNotOverwriteFile_->Enable( false );

    wxFlexGridSizer* pFlexGridDriverSettingsSizer = new wxFlexGridSizer( 2, wxSize( 20, 20 ) );
    pFlexGridDriverSettingsSizer->Add( pLBChoosePixelFormat_ );
    pFlexGridDriverSettingsSizer->Add( pInfoChoosePixelFormat, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );

    wxStaticBoxSizer* pRecordingDriverSettingsSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Device Driver Setup Parameters" ) );
    pRecordingDriverSettingsSizer->AddSpacer( 5 );
    pRecordingDriverSettingsSizer->Add( pFlexGridDriverSettingsSizer );

    wxFlexGridSizer* pFlexGridVideoStreamSettingsSizer = new wxFlexGridSizer( 2, wxSize( 20, 20 ) );
    pFlexGridVideoStreamSettingsSizer->Add( pBtnFilePicker_ );
    pFlexGridVideoStreamSettingsSizer->Add( pSelectedOutputFile_, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pFlexGridVideoStreamSettingsSizer->Add( pLBChooseCodec_ );
    pFlexGridVideoStreamSettingsSizer->Add( pInfoChooseCodec, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pFlexGridVideoStreamSettingsSizer->Add( pSPChooseCompression_ );
    pFlexGridVideoStreamSettingsSizer->Add( pInfoChooseCompression, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pFlexGridVideoStreamSettingsSizer->Add( pSPChooseBitrate_ );
    pFlexGridVideoStreamSettingsSizer->Add( pInfoChooseBitrate, wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );

    wxStaticBoxSizer* pRecordingVideoStreamSettingsSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Video Stream Setup Parameters" ) );
    pRecordingVideoStreamSettingsSizer->AddSpacer( 5 );
    pRecordingVideoStreamSettingsSizer->Add( pFlexGridVideoStreamSettingsSizer );
    pRecordingVideoStreamSettingsSizer->SetMinSize( wxSize( 671, 20 ) );

    wxStaticBoxSizer* pGlobalRecordingSettingsSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Video Stream Recording Settings" ) );
    pGlobalRecordingSettingsSizer->AddSpacer( 5 );
    pGlobalRecordingSettingsSizer->Add( pTCVSRSetupHints_, wxSizerFlags( 3 ).Expand() );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pCBEnableRecording_ );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pRecordingDriverSettingsSizer );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pRecordingVideoStreamSettingsSizer );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pCBEnableStopAcquire_ );
    pGlobalRecordingSettingsSizer->AddSpacer( 10 );
    pGlobalRecordingSettingsSizer->Add( pCBNotOverwriteFile_ );

    pTopDownSizer_ = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer_->AddSpacer( 10 );
    pTopDownSizer_->Add( pGlobalRecordingSettingsSizer, wxSizerFlags( 8 ).Expand() );
    pTopDownSizer_->AddSpacer( 10 );
    AddButtons( pPanel, pTopDownSizer_, true );
    pTopDownSizer_->SetMinSize( wxSize( 650, 810 ) );

    FinalizeDlgCreation( pPanel, pTopDownSizer_ );

    boGUICreated_ = true;
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::ChangeRecordingMode()
//-----------------------------------------------------------------------------
{
    pLBChoosePixelFormat_->Enable( pCBEnableRecording_->GetValue() );
    pLBChoosePixelFormat_->IsEnabled() ? pLBChoosePixelFormat_->SetSelection( 0 ) : pLBChoosePixelFormat_->Deselect( pLBChoosePixelFormat_->GetSelection() );
    pBtnFilePicker_->Enable( pCBEnableRecording_->GetValue() );
    if( !pCBEnableRecording_->GetValue() )
    {
        DeactivateVideoStreamRecording();
        if( !rVideoStreamRecordingParameters_.tempTargetFile_.IsEmpty() )
        {
            rVideoStreamRecordingParameters_.tempTargetFile_ = wxEmptyString;
            pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "No output file selected\n" ) ) );
        }
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::SelectOutputFile()
//-----------------------------------------------------------------------------
{
    rVideoStreamRecordingParameters_.tempTargetFile_ = wxFileSelector( wxT( "Please create/select a file for recording the video stream" ),
            wxEmptyString, wxEmptyString, wxT( "*.mp4" ), wxT( "H.264 / H.265 files (*.mp4)|*.mp4|MPEG-2 Video files (*.m2v)|*.m2v" ), wxFD_SAVE );
    rVideoStreamRecordingParameters_.tempTargetFile_.IsEmpty() ? pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "No output file selected\n" ) ) ) : pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "%s" ), rVideoStreamRecordingParameters_.tempTargetFile_ ) );
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::ActivateVideoStreamRecording()
//-----------------------------------------------------------------------------
{
    pCBEnableStopAcquire_->Enable( !rVideoStreamRecordingParameters_.tempTargetFile_.IsEmpty() );
    pCBNotOverwriteFile_->Enable( !rVideoStreamRecordingParameters_.tempTargetFile_.IsEmpty() );
    pLBChooseCodec_->Enable( rVideoStreamRecordingParameters_.tempTargetFile_.EndsWith( wxT( ".mp4" ) ) );
    pLBChooseCodec_->IsEnabled() ? pLBChooseCodec_->SetSelection( 0 ) : pLBChooseCodec_->Deselect( pLBChooseCodec_->GetSelection() );
    pSPChooseCompression_->Enable( rVideoStreamRecordingParameters_.tempTargetFile_.EndsWith( wxT( ".mp4" ) ) );
    pSPChooseBitrate_->Enable( rVideoStreamRecordingParameters_.tempTargetFile_.EndsWith( wxT( ".m2v" ) ) );
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::DeactivateVideoStreamRecording()
//-----------------------------------------------------------------------------
{
    if( pLBChooseCodec_->IsEnabled() )
    {
        pLBChooseCodec_->Deselect( pLBChooseCodec_->GetSelection() );
        pLBChooseCodec_->Enable( false );
    }
    pSPChooseCompression_->Enable( false );
    pSPChooseBitrate_->Enable( false );
    pCBEnableStopAcquire_->SetValue( false );
    pCBEnableStopAcquire_->Enable( false );
    pCBNotOverwriteFile_->SetValue( false );
    pCBNotOverwriteFile_->Enable( false );
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::WriteTargetFile()
//-----------------------------------------------------------------------------
{
    rVideoStreamRecordingParameters_.targetFile_ = rVideoStreamRecordingParameters_.tempTargetFile_;
    if( rVideoStreamRecordingParameters_.targetFile_.EndsWith( wxT( ".m2v" ) ) )
    {
        pParent_->WriteLogMessage( wxString::Format( wxT( "\nA '*.m2v' file has a fixed playback frequency of 25Hz. So the original acquisition speed is only reflected correctly when the capture frame rate was 25Hz as well. Otherwise the stream will appear to run faster or slower..." ) ) );
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::WritePixelFormat()
//-----------------------------------------------------------------------------
{
    if( pLBChoosePixelFormat_->IsEnabled() )
    {
        pixelFormatIndex_ = pLBChoosePixelFormat_->GetSelection();
        rVideoStreamRecordingParameters_.pixelFormat_ = GetPixelFormatFromFileFilterIndex( pixelFormatIndex_ );
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::WriteCodec()
//-----------------------------------------------------------------------------
{
    if( pLBChooseCodec_->IsEnabled() )
    {
        codecIndex_ = pLBChooseCodec_->GetSelection();
        rVideoStreamRecordingParameters_.codec_ = GetVideoCodecFromFileFilterIndex( pLBChooseCodec_->GetSelection() );
    }
    else
    {
        rVideoStreamRecordingParameters_.codec_ = vcMPEG2;
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::WriteCompression()
//-----------------------------------------------------------------------------
{
    if( pSPChooseCompression_->IsEnabled() )
    {
        rVideoStreamRecordingParameters_.compression_ = pSPChooseCompression_->GetValue();
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::WriteBitrate()
//-----------------------------------------------------------------------------
{
    if( pSPChooseBitrate_->IsEnabled() )
    {
        rVideoStreamRecordingParameters_.bitrate_ = pSPChooseBitrate_->GetValue();
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::CheckStopAcquire()
//-----------------------------------------------------------------------------
{
    if( pCBEnableStopAcquire_->IsEnabled() )
    {
        rVideoStreamRecordingParameters_.boStopAcquire_ = pCBEnableStopAcquire_->GetValue();
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::CheckOverwriteFile()
//-----------------------------------------------------------------------------
{
    if( pCBNotOverwriteFile_->IsEnabled() )
    {
        rVideoStreamRecordingParameters_.boNotOverwriteFile_ = pCBNotOverwriteFile_->GetValue();
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::ApplyRecordingSettings( void )
//-----------------------------------------------------------------------------
{
    rVideoStreamRecordingParameters_.boEnableRecording_ = pCBEnableRecording_->GetValue();
    if( rVideoStreamRecordingParameters_.boEnableRecording_ )
    {
        if( !rVideoStreamRecordingParameters_.tempTargetFile_.IsEmpty() )
        {
            WriteTargetFile();
            WritePixelFormat();
            WriteCodec();
            WriteCompression();
            WriteBitrate();
            CheckStopAcquire();
            CheckOverwriteFile();
            boLBChoosePixelFormatEnabled_ = pLBChoosePixelFormat_->IsEnabled();
            boLBChooseCodecEnabled_ = pLBChooseCodec_->IsEnabled();
            boSPChooseCompressionEnabled_ = pSPChooseCompression_->IsEnabled();
            boSPChooseBitrateEnabled_ = pSPChooseBitrate_->IsEnabled();
            boCBStopAcquireEnabled_ = pCBEnableStopAcquire_->IsEnabled();
            boCBOverwriteFileEnabled_ = pCBNotOverwriteFile_->IsEnabled();
            rVideoStreamRecordingParameters_.boActive_ = true;
            pParent_->WriteLogMessage( wxString::Format( wxT( "\nThe current settings have been applied.\n" ) ) );
        }
        else
        {
            rVideoStreamRecordingParameters_.boActive_ = false;
            pParent_->WriteErrorMessage( wxString::Format( wxT( "\nNo settings have been applied due to invalid output file.\n" ) ) );
        }
    }
    else
    {
        rVideoStreamRecordingParameters_.boActive_ = false;
        pParent_->WriteLogMessage( wxString::Format( wxT( "\nThe video stream recording mode has been turned off.\n" ) ) );
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::DiscardSettings( void )
//-----------------------------------------------------------------------------
{
    if( rVideoStreamRecordingParameters_.boEnableRecording_ )
    {
        if( rVideoStreamRecordingParameters_.boActive_ )
        {
            pCBEnableRecording_->SetValue( true );
            pCBEnableRecording_->Enable();
            pBtnFilePicker_->Enable();
            pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "%s" ), rVideoStreamRecordingParameters_.targetFile_ ) );
            if( boLBChoosePixelFormatEnabled_ )
            {
                pLBChoosePixelFormat_->Enable();
                pLBChoosePixelFormat_->SetSelection( pixelFormatIndex_ );
            }
            if( boLBChooseCodecEnabled_ )
            {
                pLBChooseCodec_->Enable();
                pLBChooseCodec_->SetSelection( codecIndex_ );
            }
            if( boSPChooseCompressionEnabled_ )
            {
                pSPChooseCompression_->Enable();
                pSPChooseCompression_->SetValue( rVideoStreamRecordingParameters_.compression_ );
            }
            if( boSPChooseBitrateEnabled_ )
            {
                pSPChooseBitrate_->Enable();
                pSPChooseBitrate_->SetValue( rVideoStreamRecordingParameters_.bitrate_ );
            }
            if( boCBStopAcquireEnabled_ )
            {
                pCBEnableStopAcquire_->Enable();
                pCBEnableStopAcquire_->SetValue( rVideoStreamRecordingParameters_.boStopAcquire_ );
            }
            if( boCBOverwriteFileEnabled_ )
            {
                pCBNotOverwriteFile_->Enable();
                pCBNotOverwriteFile_->SetValue( rVideoStreamRecordingParameters_.boNotOverwriteFile_ );
            }
            pParent_->WriteLogMessage( wxString::Format( wxT( "\nChanges to the current settings have been discarded.\n" ) ) );
        }
        else
        {
            pCBEnableRecording_->SetValue( true );
            pBtnFilePicker_->Enable();
            pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "No output file selected\n" ) ) );
            pLBChoosePixelFormat_->Deselect( pLBChoosePixelFormat_->GetSelection() );
            pLBChoosePixelFormat_->Enable( false );
            pLBChooseCodec_->Deselect( pLBChooseCodec_->GetSelection() );
            pLBChooseCodec_->Enable( false );
            pSPChooseCompression_->Enable( false );
            pSPChooseBitrate_->Enable( false );
            pCBEnableStopAcquire_->SetValue( false );
            pCBEnableStopAcquire_->Enable( false );
            pCBNotOverwriteFile_->SetValue( false );
            pCBNotOverwriteFile_->Enable( false );
        }
    }
    else
    {
        pCBEnableRecording_->SetValue( false );
        pBtnFilePicker_->Enable( false );
        pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "No output file selected\n" ) ) );
        pLBChoosePixelFormat_->Deselect( pLBChoosePixelFormat_->GetSelection() );
        pLBChoosePixelFormat_->Enable( false );
        pLBChooseCodec_->Deselect( pLBChooseCodec_->GetSelection() );
        pLBChooseCodec_->Enable( false );
        pSPChooseCompression_->Enable( false );
        pSPChooseBitrate_->Enable( false );
        pCBEnableStopAcquire_->SetValue( false );
        pCBEnableStopAcquire_->Enable( false );
        pCBNotOverwriteFile_->SetValue( false );
        pCBNotOverwriteFile_->Enable( false );
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::EnableSetupDialog( bool boEnableDialog )
//-----------------------------------------------------------------------------
{
    if( !boEnableDialog )
    {
        pCBEnableRecording_->SetValue( rVideoStreamRecordingParameters_.boEnableRecording_ );
        pCBEnableRecording_->Disable();
        pSelectedOutputFile_->SetLabel( wxString::Format( wxT( "%s" ), rVideoStreamRecordingParameters_.targetFile_ ) );
        pBtnFilePicker_->Disable();
        if( boLBChoosePixelFormatEnabled_ )
        {
            pLBChoosePixelFormat_->SetSelection( pixelFormatIndex_ );
            pLBChoosePixelFormat_->Disable();
        }
        if( boLBChooseCodecEnabled_ )
        {
            pLBChooseCodec_->SetSelection( codecIndex_ );
            pLBChooseCodec_->Disable();
        }
        if( boSPChooseCompressionEnabled_ )
        {
            pSPChooseCompression_->SetValue( rVideoStreamRecordingParameters_.compression_ );
            pSPChooseCompression_->Disable();
        }
        if( boSPChooseBitrateEnabled_ )
        {
            pSPChooseBitrate_->SetValue( rVideoStreamRecordingParameters_.bitrate_ );
            pSPChooseBitrate_->Disable();
        }
        if( boCBStopAcquireEnabled_ )
        {
            pCBEnableStopAcquire_->SetValue( rVideoStreamRecordingParameters_.boStopAcquire_ );
            pCBEnableStopAcquire_->Disable();
        }
        if( boCBOverwriteFileEnabled_ )
        {
            pCBNotOverwriteFile_->SetValue( rVideoStreamRecordingParameters_.boNotOverwriteFile_ );
            pCBNotOverwriteFile_->Disable();
        }
        pBtnApply_->Disable();
        pBtnOk_->Disable();
        pBtnCancel_->Disable();
    }
    else
    {
        pCBEnableRecording_->Enable();
        pCBEnableRecording_->SetValue( rVideoStreamRecordingParameters_.boEnableRecording_ );
        pBtnFilePicker_->Enable();
        pLBChoosePixelFormat_->Enable( boLBChoosePixelFormatEnabled_ );
        pLBChooseCodec_->Enable( boLBChooseCodecEnabled_ );
        pSPChooseCompression_->Enable( boSPChooseCompressionEnabled_ );
        pSPChooseBitrate_->Enable( boSPChooseBitrateEnabled_ );
        pCBEnableStopAcquire_->Enable( boCBStopAcquireEnabled_ );
        pCBNotOverwriteFile_->Enable( boCBOverwriteFileEnabled_ );
        pBtnApply_->Enable();
        pBtnOk_->Enable();
        pBtnCancel_->Enable();
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::OnClose( wxCloseEvent& e )
//-----------------------------------------------------------------------------
{
    Hide();
    if( e.CanVeto() )
    {
        e.Veto();
    }
}

//-----------------------------------------------------------------------------
void SetupVideoStreamRecording::WriteInfoMessage( wxTextCtrl* pTCSetupHints, wxFont fixedPitchFont, wxTextAttr fixedPitchStyle )
//-----------------------------------------------------------------------------
{
    WriteToTextCtrl( pTCSetupHints, wxT( "Setup Hints:\n" ), fixedPitchStyle );
    fixedPitchFont.SetWeight( wxFONTWEIGHT_NORMAL );
    fixedPitchFont.SetUnderlined( false );
    fixedPitchStyle.SetFont( fixedPitchFont );
    WriteToTextCtrl( pTCSetupHints, wxT( "The 'Video Stream Recording' allows to capture all displayed images into a compressed video file.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Please set up the 'Video Stream Recording' here. Afterwards, click 'Ok' or 'Apply' to apply the current settings. Note that 'Ok' or 'Apply' is also needed when deactivating the recording mode.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Once the settings have been applied, you can control the recording via 'Start', 'Pause', 'Stop' buttons at the top right tool-bar. Note that the control buttons are deactivated when no settings are applied.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "- 'Start': Starts recording to a video file.\nIt can be chosen in the check-box below whether to overwrite the recorded video stream if the currently selected output file has the same file name is the same as the previous one. If activated, a file selector will pop up to ask for a new output file.\nIf acquisition is not on beforehand, starting recording will automatically turn on the acquisition.\nNote that during recording, the setup is deactivated.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "- 'Pause': Pauses/Resumes recording.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "- 'Stop': Stops recording. It can be chosen in the check-box below whether to stop the acquisition as soon as recording is stopped.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Click 'Apply' to apply the current settings.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Click 'Ok' to apply the current settings and close the setup dialog.\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "\n" ), fixedPitchStyle );
    WriteToTextCtrl( pTCSetupHints, wxT( "Click 'Cancel' to discard changes to the current settings and close the setup dialog'.\n" ), fixedPitchStyle );
}
