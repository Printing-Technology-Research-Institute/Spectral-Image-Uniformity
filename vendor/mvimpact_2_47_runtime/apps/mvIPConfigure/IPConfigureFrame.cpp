#if defined(linux) || defined(__linux) || defined(__linux__)
#   include <sys/socket.h>
#   include <arpa/inet.h>
#   include <errno.h>
#else
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
#include <algorithm>
#include <apps/Common/wxAbstraction.h>
#include "AssignIPDlg.h"
#include <common/auto_array_ptr.h>
#include <common/function_cast.h>
#include <common/STLHelper.h>
#include "DeviceListCtrl.h"
#include "error_icon.xpm"
#include "IPConfigureFrame.h"
#include <limits>
#include "ok_icon.xpm"
#include "SetupUnicastDlg.h"
#include "SplashScreen/SplashScreen.h"
#include <string>
#include <fstream>
#include "TLILibImports.h"
#include "mvIPConfigure.icon.xpm"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/arrstr.h>
#include <wx/artprov.h>
#include "wx/combobox.h"
#include "wx/config.h"
#include "wx/filename.h"
#include <wx/ffile.h>
#include <wx/splitter.h>
#include <wx/spinctrl.h>
#include <wx/utils.h>
#include <apps/Common/wxIncludeEpilogue.h>

#undef min // otherwise we can't work with the 'numeric_limits' template here as someone might define a macro 'min'
#undef max // otherwise we can't work with the 'numeric_limits' template here as someone might define a macro 'max'

using namespace std;

//=============================================================================
//=================== internal helper functions ===============================
//=============================================================================
#define CHECK_INTERFACE_INDEX \
    const size_t interfaceIndex = static_cast<size_t>(atoi( value.BeforeFirst( wxT(';') ).mb_str() )); \
    if( interfaceIndex >= DetectedDeviceInfo::MAX_INTERFACE_COUNT ) \
    { \
        commandLineErrors.Append( wxString::Format( wxT("Invalid interface index in command line parameter: '%s'. Ignored.\n"), param.c_str() ) ); \
        continue; \
    }

IPConfigureFrame* g_pFrame = 0;

//-----------------------------------------------------------------------------
inline unsigned long GetIPv4AddressFromString( const char* pValue )
//-----------------------------------------------------------------------------
{
    in_addr IPv4Addr;
    inet_pton( AF_INET, pValue, &IPv4Addr );
#if defined(linux) || defined(__linux) || defined(__linux__)
    return IPv4Addr.s_addr;
#else
    return IPv4Addr.S_un.S_addr;
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
}

//=============================================================================
//================= Implementation MyApp ======================================
//=============================================================================
//-----------------------------------------------------------------------------
class MyApp : public wxApp
//-----------------------------------------------------------------------------
{
    void CreateApplicationInstance( void )
    {
        g_pFrame = new IPConfigureFrame( wxString::Format( wxT( "mvIPConfigure - Configuration Tool For Network Related Settings Of GigE Vision(tm) Devices(%s)" ), VERSION_STRING ), wxDefaultPosition, wxDefaultSize, argc, argv );
    }
public:
    virtual bool OnInit()
    {
        wxImage::AddHandler( new wxPNGHandler );

        bool boShowWindow = true;
        for( int i = 1; i < argc; i++ )
        {
            wxString param( wxApp::argv[i].MakeLower() );
            const wxString key( param.BeforeFirst( wxT( '=' ) ) );
            if( key == wxT( "hidden" ) )
            {
                boShowWindow = false;
            }
        }

        if( boShowWindow )
        {
            SplashScreenScope splashScreenScope( wxBitmap::NewFromPNGData( splashScreen_png, sizeof( splashScreen_png ) ), wxBitmap( mvIPConfigure_icon_xpm ) );
            CreateApplicationInstance();
            g_pFrame->Show( true );
            SetTopWindow( g_pFrame );
            //Workaround for the refreshing of the Log wxTextCtrl
            g_pFrame->WriteLogMessage( wxT( "" ) );
        }
        else
        {
            CreateApplicationInstance();
        }
        return true;
    }
};

IMPLEMENT_APP( MyApp )

//=============================================================================
//============== Implementation PerformanceIssuesDlg ==========================
//=============================================================================
BEGIN_EVENT_TABLE( PerformanceIssuesDlg, wxDialog )
    EVT_BUTTON( widBtnOk, PerformanceIssuesDlg::OnBtnOk )
    EVT_BUTTON( widBtnCancel, PerformanceIssuesDlg::OnBtnCancel )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
PerformanceIssuesDlg::PerformanceIssuesDlg( wxWindow* pParent, wxWindowID id, const wxString& title, DetectedDeviceInfo* pDeviceInfo )
    : wxDialog( pParent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
      pBtnCancel_( 0 ), pBtnOk_( 0 ), pDeviceInfo_( pDeviceInfo )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    wxPanel* pPanel = new wxPanel( this );
    pTreeCtrl_ = new wxTreeCtrl( pPanel, wxID_ANY );
    pTopDownSizer->Add( pTreeCtrl_, wxSizerFlags( 3 ).Expand() );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
    SetSize( 800, 300 );
}

//-----------------------------------------------------------------------------
void PerformanceIssuesDlg::AddButtons( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    // lower line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonSizer->AddStretchSpacer( 100 );
    pBtnOk_ = new wxButton( pWindow, widBtnOk, wxT( "&Ok" ) );
    pButtonSizer->Add( pBtnOk_, wxSizerFlags().Border( wxALL, 7 ) );
    pBtnCancel_ = new wxButton( pWindow, widBtnCancel, wxT( "&Cancel" ) );
    pButtonSizer->Add( pBtnCancel_, wxSizerFlags().Border( wxALL, 7 ) );
    pSizer->AddSpacer( 10 );
    pSizer->Add( pButtonSizer, wxSizerFlags().Expand() );
}

//-----------------------------------------------------------------------------
void PerformanceIssuesDlg::FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    pWindow->SetSizer( pSizer );
    pSizer->SetSizeHints( this );
    SetClientSize( pSizer->GetMinSize() );
    SetSizeHints( GetSize() );
}

//-----------------------------------------------------------------------------
wxTreeItemId PerformanceIssuesDlg::AddComponentListToList( wxTreeCtrl* pTreeCtrl, wxTreeItemId parent, mvIMPACT::acquire::ComponentLocator locator, const char* pName )
//-----------------------------------------------------------------------------
{
    ComponentList list;
    locator.bindComponent( list, string( pName ) );
    if( !list.isValid() )
    {
        return wxTreeItemId();
    }
    return pTreeCtrl->AppendItem( parent, ConvertedString( list.name() ) );
}

//-----------------------------------------------------------------------------
void PerformanceIssuesDlg::AddStringPropToList( wxTreeCtrl* pTreeCtrl, wxTreeItemId parent, ComponentLocator locator, const char* pName )
//-----------------------------------------------------------------------------
{
    PropertyS prop;
    locator.bindComponent( prop, string( pName ) );
    if( prop.isValid() )
    {
        pTreeCtrl->AppendItem( parent, wxString::Format( wxT( "%s: %s" ), ConvertedString( prop.name() ).c_str(), ConvertedString( prop.read() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void PerformanceIssuesDlg::ExpandAll( wxTreeCtrl* pTreeCtrl )
//-----------------------------------------------------------------------------
{
    ExpandAllChildren( pTreeCtrl, pTreeCtrl->GetRootItem() );
}

//-----------------------------------------------------------------------------
/// \brief this code is 'stolen' from the wxWidgets 2.8.0 source as this application
/// might be compiled with older versions of wxWidgets not supporting the wxTreeCtrl::ExpandAll function
void PerformanceIssuesDlg::ExpandAllChildren( wxTreeCtrl* pTreeCtrl, const wxTreeItemId& item )
//-----------------------------------------------------------------------------
{
    // expand this item first, this might result in its children being added on
    // the fly
    pTreeCtrl->Expand( item );

    // then (recursively) expand all the children
    wxTreeItemIdValue cookie;
    for( wxTreeItemId idCurr = pTreeCtrl->GetFirstChild( item, cookie ); idCurr.IsOk(); idCurr = pTreeCtrl->GetNextChild( item, cookie ) )
    {
        ExpandAllChildren( pTreeCtrl, idCurr );
    }
}

//-----------------------------------------------------------------------------
void PerformanceIssuesDlg::RefreshDlgControls( void )
//-----------------------------------------------------------------------------
{
    ExpandAll( pTreeCtrl_ );
}

//=============================================================================
//============= Implementation MessageDialogPopUpWithCheckbox =================
//=============================================================================

BEGIN_EVENT_TABLE( MessageDialogPopUpWithCheckbox, wxDialog )
    EVT_BUTTON( widBtnYes, MessageDialogPopUpWithCheckbox::OnBtnYes )
    EVT_BUTTON( widBtnNo, MessageDialogPopUpWithCheckbox::OnBtnNo )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
MessageDialogPopUpWithCheckbox::MessageDialogPopUpWithCheckbox( wxWindow* pParent, wxWindowID id, const wxString& title, const wxString& description, const wxString& checkBoxText, const wxString& yesButtonLabel, const wxString& noButtonLabel, bool boInitialCheckBoxState )
    : wxDialog( pParent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE ),
      pBtnYes_( 0 ), pBtnNo_( 0 ), pCheckBox_( 0 )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    const wxIcon warningIcon = wxArtProvider::GetMessageBoxIcon( wxICON_EXCLAMATION );
    SetIcon( warningIcon );
    SetBackgroundColour( *wxWHITE );
    wxPanel* pPanel = new wxPanel( this );

    wxBoxSizer* pTextSizer = new wxBoxSizer( wxHORIZONTAL );
    pTextSizer->AddSpacer( 20 );
    pTextSizer->Add( new wxStaticBitmap( pPanel, wxID_ANY, warningIcon ) );
    pTextSizer->AddSpacer( 20 );
    pTextSizer->Add( new wxStaticText( pPanel, wxID_ANY, description ) );
    pTextSizer->AddSpacer( 20 );

    pCheckBox_ = new wxCheckBox( pPanel, wxID_ANY, checkBoxText );
    wxBoxSizer* pCheckBoxSizer = new wxBoxSizer( wxHORIZONTAL );
    pCheckBoxSizer->AddSpacer( 10 );
    pCheckBoxSizer->Add( pCheckBox_ );
    pCheckBoxSizer->AddSpacer( 10 );
    pCheckBox_->SetValue( boInitialCheckBoxState );

    pTopDownSizer->Add( pTextSizer, wxSizerFlags( 3 ).Expand() );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pCheckBoxSizer, wxSizerFlags( 1 ).Expand() );
    AddButtons( pPanel, pTopDownSizer, yesButtonLabel, noButtonLabel );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//-----------------------------------------------------------------------------
void MessageDialogPopUpWithCheckbox::AddButtons( wxWindow* pWindow, wxSizer* pSizer, const wxString& yesButtonLabel, const wxString& noButtonLabel )
//-----------------------------------------------------------------------------
{
    // lower line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonSizer->AddStretchSpacer( 100 );
    pBtnYes_ = new wxButton( pWindow, widBtnYes, yesButtonLabel );
    pButtonSizer->Add( pBtnYes_, wxSizerFlags().Border( wxALL, 7 ) );
    pBtnNo_ = new wxButton( pWindow, widBtnNo, noButtonLabel );
    pButtonSizer->Add( pBtnNo_, wxSizerFlags().Border( wxALL, 7 ) );
    pSizer->Add( pButtonSizer, wxSizerFlags().Expand() );
}

//-----------------------------------------------------------------------------
void MessageDialogPopUpWithCheckbox::FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    pWindow->SetSizer( pSizer );
    pSizer->SetSizeHints( this );
    SetClientSize( pSizer->GetMinSize() );
    SetSizeHints( GetSize() );
    Centre();
}

//=============================================================================
//============= Implementation MessageDialogPopUpWithHyperlink ================
//=============================================================================

BEGIN_EVENT_TABLE( MessageDialogPopUpWithHyperlink, wxDialog )
    EVT_BUTTON( widBtnOK, MessageDialogPopUpWithHyperlink::OnBtnOK )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
MessageDialogPopUpWithHyperlink::MessageDialogPopUpWithHyperlink( wxWindow* pParent, wxWindowID id, const wxString& title )
    : wxDialog( pParent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE ), pBtnOK_( 0 )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    const wxIcon infoIcon = wxArtProvider::GetMessageBoxIcon( wxICON_INFORMATION );
    SetIcon( infoIcon );
    SetBackgroundColour( *wxWHITE );
    wxPanel* pPanel = new wxPanel( this );

    wxBoxSizer* pTextSizer = new wxBoxSizer( wxHORIZONTAL );
    pTextSizer->AddSpacer( 20 );
    pTextSizer->Add( new wxStaticBitmap( pPanel, wxID_ANY, infoIcon ) );
    pTextSizer->AddSpacer( 20 );
    pTextSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "In case your device does not show up in the device list, please\nconnect it to another network interface and hit F5 to refresh.\n\nFor more information on this issue please visit our relevant" ) ) );
    pTextSizer->AddSpacer( 20 );


    pTopDownSizer->Add( pTextSizer, wxSizerFlags( 3 ).Expand() );
    pTopDownSizer->Add( new wxHyperlinkCtrl( pPanel, wxID_ANY, wxT( "FAQ page" ), wxT( "http://www.matrix-vision.com/faq-reader/1253.html" ) ), wxSizerFlags().Center() );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );
}

//-----------------------------------------------------------------------------
void MessageDialogPopUpWithHyperlink::AddButtons( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    // lower line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonSizer->AddStretchSpacer( 100 );
    pBtnOK_ = new wxButton( pWindow, widBtnOK, wxT( "&OK" ) );
    pButtonSizer->Add( pBtnOK_, wxSizerFlags().Border( wxALL, 7 ) );
    pSizer->Add( pButtonSizer, wxSizerFlags().Expand() );
}

//-----------------------------------------------------------------------------
void MessageDialogPopUpWithHyperlink::FinalizeDlgCreation( wxWindow* pWindow, wxSizer* pSizer )
//-----------------------------------------------------------------------------
{
    pWindow->SetSizer( pSizer );
    pSizer->SetSizeHints( this );
    SetClientSize( pSizer->GetMinSize() );
    SetSizeHints( GetSize() );
    Centre();
}

//=============================================================================
//================= Implementation IPConfigureFrame ===========================
//=============================================================================
BEGIN_EVENT_TABLE( IPConfigureFrame, wxFrame )
    EVT_CLOSE( IPConfigureFrame::OnClose )
    EVT_MENU( miHelp_About, IPConfigureFrame::OnHelp_About )
    EVT_MENU( miHelp_OnlineDocumentation, IPConfigureFrame::OnHelp_OnlineDocumentation )
    EVT_MENU( miAction_Quit, IPConfigureFrame::OnQuit )
    EVT_MENU( miAction_AutoAssignPersistentIP, IPConfigureFrame::OnAction_AutoAssignPersistentIP )
    EVT_MENU( miAction_AutoAssignDHCPIP, IPConfigureFrame::OnAction_AutoAssignDHCPIP )
    EVT_MENU( miAction_AutoAssignTemporaryIP, IPConfigureFrame::OnAction_AutoAssignTemporaryIP )
    EVT_MENU( miAction_ManuallyAssignTemporaryIP, IPConfigureFrame::OnAction_ManuallyAssignTemporaryIP )
    EVT_MENU( miAction_ViewPotentialPerformanceIssues, IPConfigureFrame::OnAction_ViewPotentialPerformanceIssues )
    EVT_MENU( miAction_SetupUnicasts, IPConfigureFrame::OnAction_SetupUnicasts )
    EVT_MENU( miAction_UpdateDeviceList, IPConfigureFrame::OnAction_UpdateDeviceList )
    EVT_MENU( miSettings_UseAdvancedDeviceDiscovery, IPConfigureFrame::OnSettings_UseAdvancedDeviceDiscovery )
    EVT_MENU( miSettings_AutoFixNetworkingIssues, IPConfigureFrame::OnSettings_AutoFixNetworkingIssues )
    EVT_MENU( miSettings_ShowWarningOnIgnoredInterfaces, IPConfigureFrame::OnSettings_ShowWarningIfIgnoredInterfacesDetected )
    EVT_TEXT( widInterfaceSelector, IPConfigureFrame::OnInterfaceSelectorTextChanged )
    EVT_TEXT( widPersistentIPAddress, IPConfigureFrame::OnPersistentIPTextChanged )
    EVT_TEXT( widPersistentSubnetMask, IPConfigureFrame::OnPersistentNetmaskTextChanged )
    EVT_TEXT( widPersistentDefaultGateway, IPConfigureFrame::OnPersistentGatewayTextChanged )
    EVT_TEXT( widConnectedToIPAddress, IPConfigureFrame::OnConnectedToIPAddressTextChanged )
    EVT_BUTTON( widBtnApplyChanges, IPConfigureFrame::OnBtnApplyChanges )
    EVT_BUTTON( widBtnConfigure, IPConfigureFrame::OnBtnConfigure )
    EVT_CHECKBOX( widCBUsePersistentIP, IPConfigureFrame::OnCBUsePersistentIP )
    EVT_CHECKBOX( widCBUseDHCP, IPConfigureFrame::OnCBUseDHCP )
    EVT_TIMER( wxID_ANY, IPConfigureFrame::OnTimer )
END_EVENT_TABLE()

const std::string IPConfigureFrame::s_m_subnetMismatchToken( "SUBNET MISMATCH! CAN'T QUERY DATA" );
const wxString IPConfigureFrame::s_m_technologyIdentifier( wxT( TLTypeGEVName ) );

//-----------------------------------------------------------------------------
IPConfigureFrame::IPConfigureFrame( const wxString& title, const wxPoint& pos, const wxSize& size, int argc, wxChar** argv )
    : wxFrame( ( wxFrame* )NULL, wxID_ANY, title, pos, size ), m_pLogWindow( 0 ), m_pTCPersistentIPAddress( 0 ),
      m_pTCPersistentSubnetMask( 0 ), m_pTCPersistentDefaultGateway( 0 ), m_ERROR_STYLE( wxColour( 255, 0, 0 ) ),
      m_RED_PASTEL( wxColour( acRedPastel ) ), m_YELLOW_PASTEL( wxColour( acYellowPastel ) ), m_BLUE_PASTEL( wxColour( acBluePastel ) ),
      m_hTLI( 0 ), m_TLILib(), m_logFileName(),
      m_pGCInitLib( 0 ), m_pGCCloseLib( 0 ), m_pTLOpen( 0 ), m_pTLClose( 0 ), m_pTLUpdateInterfaceList( 0 ),
      m_pTLGetNumInterfaces( 0 ), m_pTLGetInterfaceID( 0 ), m_pTLOpenInterface( 0 ), m_pIFClose( 0 ),
      m_pIFGetNumDevices( 0 ), m_pIFGetDeviceID( 0 ), m_pIFGetInfo( 0 ), m_pTLIMV_IFSetInterfaceParam( 0 ),
      m_pIFUpdateDeviceList( 0 ), m_pIFGetDeviceInfo( 0 ), m_pIFOpenDevice( 0 ), m_pTLIMV_IFGetDeviceInterfaceInfo( 0 ),
      m_pTLIMV_DevSetInterfaceParam( 0 ), m_pTLIMV_DevSetParam( 0 ), m_pDevGetNumDataStreams( 0 ),
      m_pDevGetDataStreamID( 0 ), m_pDevOpenDataStream( 0 ), m_pDevClose( 0 ), m_pDSClose( 0 ), m_pDSGetInfo( 0 ),
      m_pTLIMV_MACFromSerial( 0 ), m_pTLIMV_IsValidIPv4Address( 0 ), m_pTLIMV_ForceIP( 0 ), m_pTLIMV_DoAddressesMatch( 0 ),
      m_boMustQuit( false ), m_boAtLeastOneInterfaceIsIgnored( false ), m_boMarkIPAddressConflict( false ),
      m_boMarkNetmaskConflict( false ), m_boForceAutoConfigurePersistentIP( false ), m_boForceAutoConfigureDHCPIP( false ), m_boMisconfiguredDevicesDetected( false ),
      m_ForceAutoConfigureAdapterPersistentIPAddress(), m_ForceAutoConfigureAdapterDHCPIPAddress()
//-----------------------------------------------------------------------------
{
    wxMenu* pMenuAction = new wxMenu;
    m_pMIAction_AutoAssignPersistentIP = pMenuAction->Append( miAction_AutoAssignPersistentIP, wxT( "Auto-Assign &Persistent IPv4 Address\tCTRL+P" ) );
    m_pMIAction_AutoAssignDHCPIP = pMenuAction->Append( miAction_AutoAssignDHCPIP, wxT( "Auto-Assign &DHCP IPv4 Address\tCTRL+D" ) );
    m_pMIAction_AutoAssignTemporaryIP = pMenuAction->Append( miAction_AutoAssignTemporaryIP, wxT( "Auto-Assign &Temporary IPv4 Address\tCTRL+T" ) );
    m_pMIAction_ManuallyAssignTemporaryIP = pMenuAction->Append( miAction_ManuallyAssignTemporaryIP, wxT( "&Manually Assign Temporary IPv4 Address\tCTRL+M" ) );
    m_pMIAction_ViewPotentialPerformanceIssues = pMenuAction->Append( miAction_ViewPotentialPerformanceIssues, wxT( "&View Potential Performance Issues\tCTRL+V" ) );

    pMenuAction->AppendSeparator();
    pMenuAction->Append( miAction_SetupUnicasts, wxT( "Set Up Unicast Device Discovery\tCTRL+U" ) );
    pMenuAction->AppendSeparator();
    pMenuAction->Append( miAction_UpdateDeviceList, wxT( "Update Device List\tF5" ) );
    pMenuAction->AppendSeparator();
    pMenuAction->Append( miAction_Quit, wxT( "E&xit\tALT+X" ) );

    wxMenu* pMenuSettings = new wxMenu;
    m_pMISettings_UseAdvancedDeviceDiscovery = pMenuSettings->Append( miSettings_UseAdvancedDeviceDiscovery, wxT( "Use Advanced Device Discovery" ), wxT( "" ), wxITEM_CHECK );
    m_pMISettings_AutoFixNetworkingIssues = pMenuSettings->Append( miSettings_AutoFixNetworkingIssues, wxT( "Auto-fix Invalid Network Configurations" ), wxT( "" ), wxITEM_CHECK );
    pMenuSettings->AppendSeparator();
    m_pMISettings_ShowWarningOnIgnoredInterfaces = pMenuSettings->Append( miSettings_ShowWarningOnIgnoredInterfaces, wxT( "Warn If Ignored Interfaces Are Detected" ), wxT( "" ), wxITEM_CHECK );

    wxMenu* pMenuHelp = new wxMenu;
    pMenuHelp->Append( miHelp_OnlineDocumentation, wxT( "Online Documentation...\tF12" ) );
    pMenuHelp->AppendSeparator();
    pMenuHelp->Append( miHelp_About, wxT( "About...\tF1" ) );

    wxMenuBar* pMenuBar = new wxMenuBar;
    pMenuBar->Append( pMenuAction, wxT( "&Action" ) );
    pMenuBar->Append( pMenuSettings, wxT( "&Settings" ) );
    pMenuBar->Append( pMenuHelp, wxT( "&Help" ) );
    // ... and attach this menu bar to the frame
    SetMenuBar( pMenuBar );

    // define the applications icon
    wxIcon icon( mvIPConfigure_icon_xpm );
    SetIcon( icon );

    m_iconList.Create( 20, 20, true, 0 );
    wxIcon errorIcon;
    errorIcon.CopyFromBitmap( wxBitmap( wxBitmap::NewFromPNGData( error_png, sizeof( error_png ) ).ConvertToImage().Scale( 20, 20, wxIMAGE_QUALITY_BICUBIC ) ) );
    m_iconList.Add( errorIcon );
    wxIcon okIcon;
    okIcon.CopyFromBitmap( wxBitmap( wxBitmap::NewFromPNGData( ok_png, sizeof( ok_png ) ).ConvertToImage().Scale( 20, 20, wxIMAGE_QUALITY_BICUBIC ) ) );
    m_iconList.Add( okIcon );

    wxPanel* pPanel = new wxPanel( this );

    // splitter for device list and the device info window
    m_pVerticalSplitter = new wxSplitterWindow( pPanel, widVerSplitter, wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER );
    m_pVerticalSplitter->SetMinimumPaneSize( 400 );

    // splitter for log window and the set of controls in the upper part of the dialog
    m_pHorizontalSplitter = new wxSplitterWindow( m_pVerticalSplitter, widHorSplitter, wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER );
    m_pHorizontalSplitter->SetMinimumPaneSize( 240 );

    // device list on the left side of the splitter
    m_pDevListCtrl = new DeviceListCtrl( m_pHorizontalSplitter, LIST_CTRL, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE, this );
    m_pDevListCtrl->InsertColumn( lcProduct, wxT( "Product" ) );
    m_pDevListCtrl->InsertColumn( lcSerial, wxT( "Serial" ) );
    m_pDevListCtrl->InsertColumn( lcPrimaryInterfaceIPAddress, wxT( "IP Address(Primary Interface)" ) );
    m_pDevListCtrl->InsertColumn( lcPotentialPerformanceIssues, wxT( "Potential Performance Issues" ) );
    m_pDevListCtrl->SetImageList( &m_iconList, wxIMAGE_LIST_SMALL );

    // and a new panel for the device info and controls on the right
    wxScrolledWindow* pControlsPanel = new wxScrolledWindow( m_pVerticalSplitter );
    pControlsPanel->SetScrollRate( 10, 10 );

    const int GROUPBOX_BORDER_WIDTH_PIXEL = 5;
    const int BTN_BORDER_WIDTH_PIXEL = 4;

    // device information controls
    wxStaticBoxSizer* pDeviceInfoSizer = new wxStaticBoxSizer( wxVERTICAL, pControlsPanel, wxT( "Device Information: " ) );
    wxStaticBox* pDeviceInfoSizerBox = pDeviceInfoSizer->GetStaticBox();
    wxFlexGridSizer* pDeviceInfoElementsGridSizer = new wxFlexGridSizer( 2 );
    pDeviceInfoElementsGridSizer->AddGrowableCol( 1, 3 );

    // row 1
    pDeviceInfoElementsGridSizer->Add( new wxStaticText( pDeviceInfoSizerBox, wxID_ANY, wxT( "Manufacturer: " ) ), wxSizerFlags().Left() );
    m_pSTManufacturer = new wxStaticText( pDeviceInfoSizerBox, widDeviceManufacturer, wxT( "-" ), wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE );
    m_pSTManufacturer->Wrap( -1 );
    pDeviceInfoElementsGridSizer->Add( m_pSTManufacturer, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 2
    pDeviceInfoElementsGridSizer->Add( new wxStaticText( pDeviceInfoSizerBox, wxID_ANY, wxT( "Serial Number: " ) ), wxSizerFlags().Left() );
    m_pSTSerialNumber = new wxStaticText( pDeviceInfoSizerBox, widDeviceSerial, wxT( "-" ) );
    pDeviceInfoElementsGridSizer->Add( m_pSTSerialNumber, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 3
    pDeviceInfoElementsGridSizer->Add( new wxStaticText( pDeviceInfoSizerBox, wxID_ANY, wxT( "User Defined Name (DeviceUserID): " ) ), wxSizerFlags().Left() );
    m_pTCUserDefinedName = new wxTextCtrl( pDeviceInfoSizerBox, wxID_ANY );
    pDeviceInfoElementsGridSizer->Add( m_pTCUserDefinedName, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 4
    pDeviceInfoElementsGridSizer->Add( new wxStaticText( pDeviceInfoSizerBox, wxID_ANY, wxT( "Interface Count: " ) ), wxSizerFlags().Left() );
    m_pSTInterfaceCount = new wxStaticText( pDeviceInfoSizerBox, widDeviceInterfaceCount, wxT( "-" ) );
    pDeviceInfoElementsGridSizer->Add( m_pSTInterfaceCount, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );

    pDeviceInfoSizer->Add( pDeviceInfoElementsGridSizer, wxSizerFlags().Align( wxGROW ).DoubleBorder() ); // see trac.wxwidgets.org/ticket/17239 before changing this

    // interface configuration controls
    wxStaticBoxSizer* pInterfaceConfigurationSizer = new wxStaticBoxSizer( wxVERTICAL, pControlsPanel, wxT( "Interface Configuration: " ) );
    wxStaticBox* pInterfaceConfigurationSizerBox = pInterfaceConfigurationSizer->GetStaticBox();

    // interface selector
    wxBoxSizer* pInterfaceSelector = new wxBoxSizer( wxHORIZONTAL );
    pInterfaceSelector->Add( new wxStaticText( pInterfaceConfigurationSizerBox, wxID_ANY, wxT( "Selected Interface:" ) ) );
    m_pSCInterfaceSelector = new wxSpinCtrl( pInterfaceConfigurationSizerBox, widInterfaceSelector, wxT( "0" ), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 3, 0 );
    pInterfaceSelector->Add( m_pSCInterfaceSelector, wxSizerFlags().Left() );
    pInterfaceConfigurationSizer->Add( pInterfaceSelector, wxSizerFlags().Expand().Border( wxALL, GROUPBOX_BORDER_WIDTH_PIXEL ) );

    // current IP address controls
    wxStaticBoxSizer* pCurrentIPSizer = new wxStaticBoxSizer( wxVERTICAL, pInterfaceConfigurationSizerBox, wxT( "Current Interface Parameter: " ) );
    wxStaticBox* pCurrentIPSizerBox = pCurrentIPSizer->GetStaticBox();
    // device information controls
    wxFlexGridSizer* pCurrentIPElementsGridSizer = new wxFlexGridSizer( 2 );
    pCurrentIPElementsGridSizer->AddGrowableCol( 1, 3 );

    // row 1
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "IPv4 Address: " ) ), wxSizerFlags().Left() );
    m_pSTCurrentIPAddress = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTCurrentIPAddress, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 2
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "Subnet Mask: " ) ), wxSizerFlags().Left() );
    m_pSTCurrentSubnetMask = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTCurrentSubnetMask, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 3
    pCurrentIPElementsGridSizer->Add( new wxHyperlinkCtrl( pCurrentIPSizerBox, wxID_ANY, wxT( "Default Gateway: " ), wxT( "http://en.wikipedia.org/wiki/Gateway_address" ) ), wxSizerFlags().Left() );
    m_pSTCurrentDefaultGateway = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTCurrentDefaultGateway, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 4
    pCurrentIPElementsGridSizer->Add( new wxHyperlinkCtrl( pCurrentIPSizerBox, wxID_ANY, wxT( "MAC Address: " ), wxT( "http://en.wikipedia.org/wiki/MAC_address" ) ), wxSizerFlags().Left() );
    m_pSTMACAddress = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTMACAddress, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 5
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "Connected To IPv4 Address: " ) ), wxSizerFlags().Left() );
    m_pCBConnectedToIPAddress = new wxComboBox( pCurrentIPSizerBox, widConnectedToIPAddress, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_DROPDOWN | wxCB_READONLY );
    m_pCBConnectedToIPAddress->Append( wxT( "-" ) );
    m_pCBConnectedToIPAddress->Select( 0 );
    pCurrentIPElementsGridSizer->Add( m_pCBConnectedToIPAddress, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 6
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "Connected Adapter Netmask: " ) ), wxSizerFlags().Left() );
    m_pSTConnectedToNetmask = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTConnectedToNetmask, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 7
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "Connected Adapter MTU(Bytes): " ) ), wxSizerFlags().Left() );
    m_pSTConnectedToMTU = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTConnectedToMTU, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 8
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "Connected Adapter Link Speed(MBps): " ) ), wxSizerFlags().Left() );
    m_pSTConnectedToLinkSpeed = new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "-" ) );
    pCurrentIPElementsGridSizer->Add( m_pSTConnectedToLinkSpeed, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 9
    pCurrentIPElementsGridSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( "More Information: " ) ), wxSizerFlags().Left() );
    wxBoxSizer* pCurrentIPElementsHelpSizer = new wxBoxSizer( wxHORIZONTAL );
    pCurrentIPElementsHelpSizer->Add( new wxHyperlinkCtrl( pCurrentIPSizerBox, wxID_ANY, wxT( "IPv4" ), wxT( "http://en.wikipedia.org/wiki/Ipv4" ) ) );
    pCurrentIPElementsHelpSizer->Add( new wxStaticText( pCurrentIPSizerBox, wxID_ANY, wxT( ", " ) ), wxSizerFlags().Bottom() );
    pCurrentIPElementsHelpSizer->Add( new wxHyperlinkCtrl( pCurrentIPSizerBox, wxID_ANY, wxT( "MTU" ), wxT( "http://en.wikipedia.org/wiki/Maximum_transmission_unit" ) ) );
    pCurrentIPElementsGridSizer->Add( pCurrentIPElementsHelpSizer, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );

    pCurrentIPSizer->Add( pCurrentIPElementsGridSizer, wxSizerFlags().Align( wxGROW ).DoubleBorder() ); // see trac.wxwidgets.org/ticket/17239 before changing this
    pInterfaceConfigurationSizer->Add( pCurrentIPSizer, wxSizerFlags().Expand().Border( wxALL, GROUPBOX_BORDER_WIDTH_PIXEL ) );

    // persistent IP address related controls
    wxStaticBoxSizer* pPersistentIPSizer = new wxStaticBoxSizer( wxVERTICAL, pInterfaceConfigurationSizerBox, wxT( "Persistent IPv4 Address: " ) );
    wxStaticBox* pPersistentIPSizerBox = pPersistentIPSizer->GetStaticBox();
    wxFlexGridSizer* pPersistentIPEditElementsGridSizer = new wxFlexGridSizer( 2 );
    pPersistentIPEditElementsGridSizer->AddGrowableCol( 1, 3 );

    // row 1
    pPersistentIPEditElementsGridSizer->Add( new wxStaticText( pPersistentIPSizerBox, wxID_ANY, wxT( "IPv4 Address: " ) ), wxSizerFlags().Left() );
    m_pTCPersistentIPAddress = new wxTextCtrl( pPersistentIPSizerBox, widPersistentIPAddress, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, m_IPv4StringValidator );
    pPersistentIPEditElementsGridSizer->Add( m_pTCPersistentIPAddress, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 2
    pPersistentIPEditElementsGridSizer->Add( new wxStaticText( pPersistentIPSizerBox, wxID_ANY, wxT( "Subnet Mask: " ) ), wxSizerFlags().Left() );
    m_pTCPersistentSubnetMask = new wxTextCtrl( pPersistentIPSizerBox, widPersistentSubnetMask, wxT( "255.255.255.0" ), wxDefaultPosition, wxDefaultSize, 0, m_IPv4StringValidator );
    pPersistentIPEditElementsGridSizer->Add( m_pTCPersistentSubnetMask, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    // row 3
    pPersistentIPEditElementsGridSizer->Add( new wxStaticText( pPersistentIPSizerBox, wxID_ANY, wxT( "Default Gateway: " ) ), wxSizerFlags().Left() );
    m_pTCPersistentDefaultGateway = new wxTextCtrl( pPersistentIPSizerBox, widPersistentDefaultGateway, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, m_IPv4StringValidator );
    pPersistentIPEditElementsGridSizer->Add( m_pTCPersistentDefaultGateway, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );

    pPersistentIPSizer->Add( pPersistentIPEditElementsGridSizer, wxSizerFlags().Align( wxGROW ).DoubleBorder() ); // see trac.wxwidgets.org/ticket/17239 before changing this
    pInterfaceConfigurationSizer->Add( pPersistentIPSizer, wxSizerFlags().Expand().Border( wxALL, GROUPBOX_BORDER_WIDTH_PIXEL ) );

    // IP configuration related controls
    wxStaticBoxSizer* pIPConfigurationSizer = new wxStaticBoxSizer( wxVERTICAL, pInterfaceConfigurationSizerBox, wxT( "IP Configuration: " ) );
    wxStaticBox* pIPConfigurationSizerBox = pIPConfigurationSizer->GetStaticBox();
    m_pCBUsePersistentIP = new wxCheckBox( pIPConfigurationSizerBox, widCBUsePersistentIP, wxT( "Use Persistent IP" ) );
    pIPConfigurationSizer->Add( m_pCBUsePersistentIP );
    m_pCBUseDHCP = new wxCheckBox( pIPConfigurationSizerBox, widCBUseDHCP, wxT( "Use DHCP" ) );
    pIPConfigurationSizer->Add( m_pCBUseDHCP );
    m_pCBUseLLA = new wxCheckBox( pIPConfigurationSizerBox, wxID_ANY, wxT( "Use LLA (Link-local address a.k.a auto-IP or zero config)" ) );
    pIPConfigurationSizer->Add( m_pCBUseLLA );

    wxBoxSizer* pIPConfigurationHelpSizer = new wxBoxSizer( wxHORIZONTAL );
    pIPConfigurationHelpSizer->Add( new wxStaticText( pIPConfigurationSizerBox, wxID_ANY, wxT( "More Information: " ) ) );
    pIPConfigurationHelpSizer->Add( new wxHyperlinkCtrl( pIPConfigurationSizerBox, wxID_ANY, wxT( "DHCP" ), wxT( "http://en.wikipedia.org/wiki/Dhcp" ) ) );
    pIPConfigurationHelpSizer->Add( new wxStaticText( pIPConfigurationSizerBox, wxID_ANY, wxT( ", " ) ), wxSizerFlags().Bottom() );
    pIPConfigurationHelpSizer->Add( new wxHyperlinkCtrl( pIPConfigurationSizerBox, wxID_ANY, wxT( "LLA" ), wxT( "http://en.wikipedia.org/wiki/Link-local_address" ) ) );
    pIPConfigurationSizer->Add( pIPConfigurationHelpSizer, wxSizerFlags().Expand().TripleBorder() ); // see trac.wxwidgets.org/ticket/17239 before changing this
    pInterfaceConfigurationSizer->Add( pIPConfigurationSizer, wxSizerFlags().Expand().Border( wxALL, GROUPBOX_BORDER_WIDTH_PIXEL ) );

    m_pCBUseLLA->Enable( false );
    m_pCBUseLLA->SetValue( true );

    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    m_pBtnConfigure = new wxButton( pControlsPanel, widBtnConfigure, wxT( "&Configure" ) );
    pButtonSizer->Add( m_pBtnConfigure, wxSizerFlags().Border( wxALL, BTN_BORDER_WIDTH_PIXEL ) );
    m_pBtnApplyChanges = new wxButton( pControlsPanel, widBtnApplyChanges, wxT( "&Apply Changes" ) );
    pButtonSizer->Add( m_pBtnApplyChanges, wxSizerFlags().Border( wxALL, BTN_BORDER_WIDTH_PIXEL ) );

    wxBoxSizer* pControlsSizer = new wxBoxSizer( wxVERTICAL );
    pControlsSizer->AddSpacer( 10 );
    pControlsSizer->Add( pButtonSizer, wxSizerFlags().Right() );
    pControlsSizer->Add( pDeviceInfoSizer, wxSizerFlags().Expand().Border( wxALL, GROUPBOX_BORDER_WIDTH_PIXEL ) );
    pControlsSizer->Add( pInterfaceConfigurationSizer, wxSizerFlags().Expand().Border( wxALL, GROUPBOX_BORDER_WIDTH_PIXEL ) );
    pControlsSizer->AddSpacer( 25 );

    m_pLogWindow = new wxTextCtrl( m_pHorizontalSplitter, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );

    wxBoxSizer* pSizer = new wxBoxSizer( wxVERTICAL );
    pSizer->Add( m_pVerticalSplitter, wxSizerFlags( 1 ).Expand() );

    wxRect defaultRect( 0, 0, 1024, 768 );
    pPanel->SetSizer( pSizer );
    // restore previous state
    wxConfigBase* pConfig = wxConfigBase::Get();
    bool boMaximized = false;
    wxRect rect = FramePositionStorage::Load( defaultRect, boMaximized );
    m_pMISettings_UseAdvancedDeviceDiscovery->Check( pConfig->Read( wxT( "/MainFrame/useAdvancedDeviceDiscovery" ), 1l ) != 0 );
    m_pMISettings_AutoFixNetworkingIssues->Check( pConfig->Read( wxT( "/MainFrame/autoFixNetworkingIssues" ), 1l ) != 0 );
    m_pMISettings_ShowWarningOnIgnoredInterfaces->Check( pConfig->Read( wxT( "/MainFrame/showWarningOnIgnoredInterfaces" ), 1l ) != 0 );

    int verticalSplitterPos = pConfig->Read( wxT( "/MainFrame/verticalSplitter" ), -1l );
    int horizontalSplitterPos = pConfig->Read( wxT( "/MainFrame/horizontalSplitter" ), -1l );

    m_pHorizontalSplitter->SplitHorizontally( m_pDevListCtrl, m_pLogWindow, 0 );
    m_pVerticalSplitter->SplitVertically( m_pHorizontalSplitter, pControlsPanel, 0 );

    pControlsPanel->SetSizer( pControlsSizer );
    SetClientSize( pSizer->GetMinSize() );
    pSizer->SetSizeHints( this );
    SetSize( rect );
    Maximize( boMaximized );

    m_pVerticalSplitter->SetSashPosition( ( verticalSplitterPos != -1 ) ? verticalSplitterPos : m_pVerticalSplitter->GetMaxWidth(), true );
    m_pHorizontalSplitter->SetSashPosition( ( horizontalSplitterPos != -1 ) ? horizontalSplitterPos : m_pHorizontalSplitter->GetMinHeight(), true );

    CollectUnicastDeviceDiscoveryData();

    const wxString GenTLLibLoadMessage( LoadGenTLProducer( m_TLILib ) );
    if( !GenTLLibLoadMessage.IsEmpty() )
    {
        WriteLogMessage( GenTLLibLoadMessage );
    }

    if( m_TLILib.IsLoaded() )
    {
        m_pGCInitLib = ResolveSymbol<PGCInitLib>( m_TLILib, wxT( "GCInitLib" ) );
        m_pGCCloseLib = ResolveSymbol<PGCCloseLib>( m_TLILib, wxT( "GCCloseLib" ) );
        m_pTLOpen = ResolveSymbol<PTLOpen>( m_TLILib, wxT( "TLOpen" ) );
        m_pTLClose = ResolveSymbol<PTLClose>( m_TLILib, wxT( "TLClose" ) );
        m_pTLUpdateInterfaceList = ResolveSymbol<PTLUpdateInterfaceList>( m_TLILib, wxT( "TLUpdateInterfaceList" ) );
        m_pTLGetNumInterfaces = ResolveSymbol<PTLGetNumInterfaces>( m_TLILib, wxT( "TLGetNumInterfaces" ) );
        m_pTLGetInterfaceID = ResolveSymbol<PTLGetInterfaceID>( m_TLILib, wxT( "TLGetInterfaceID" ) );
        m_pTLOpenInterface = ResolveSymbol<PTLOpenInterface>( m_TLILib, wxT( "TLOpenInterface" ) );
        m_pIFClose = ResolveSymbol<PIFClose>( m_TLILib, wxT( "IFClose" ) );
        m_pIFGetNumDevices = ResolveSymbol<PIFGetNumDevices>( m_TLILib, wxT( "IFGetNumDevices" ) );
        m_pIFGetDeviceID = ResolveSymbol<PIFGetDeviceID>( m_TLILib, wxT( "IFGetDeviceID" ) );
        m_pIFGetInfo = ResolveSymbol<PIFGetInfo>( m_TLILib, wxT( "IFGetInfo" ) );
        m_pTLIMV_IFSetInterfaceParam = ResolveSymbol<PTLIMV_IFSetInterfaceParam>( m_TLILib, wxT( "TLIMV_IFSetInterfaceParam" ) );
        m_pIFUpdateDeviceList = ResolveSymbol<PIFUpdateDeviceList>( m_TLILib, wxT( "IFUpdateDeviceList" ) );
        m_pIFGetDeviceInfo = ResolveSymbol<PIFGetDeviceInfo>( m_TLILib, wxT( "IFGetDeviceInfo" ) );
        m_pIFOpenDevice = ResolveSymbol<PIFOpenDevice>( m_TLILib, wxT( "IFOpenDevice" ) );
        m_pTLIMV_IFGetDeviceInterfaceInfo = ResolveSymbol<PTLIMV_IFGetDeviceInterfaceInfo>( m_TLILib, wxT( "TLIMV_IFGetDeviceInterfaceInfo" ) );
        m_pTLIMV_DevSetInterfaceParam = ResolveSymbol<PTLIMV_DevSetInterfaceParam>( m_TLILib, wxT( "TLIMV_DevSetInterfaceParam" ) );
        m_pTLIMV_DevSetParam = ResolveSymbol<PTLIMV_DevSetParam>( m_TLILib, wxT( "TLIMV_DevSetParam" ) );
        m_pDevGetNumDataStreams = ResolveSymbol<PDevGetNumDataStreams>( m_TLILib, wxT( "DevGetNumDataStreams" ) );
        m_pDevGetDataStreamID = ResolveSymbol<PDevGetDataStreamID>( m_TLILib, wxT( "DevGetDataStreamID" ) );
        m_pDevOpenDataStream = ResolveSymbol<PDevOpenDataStream>( m_TLILib, wxT( "DevOpenDataStream" ) );
        m_pDevClose = ResolveSymbol<PDevClose>( m_TLILib, wxT( "DevClose" ) );
        m_pDSClose = ResolveSymbol<PDSClose>( m_TLILib, wxT( "DSClose" ) );
        m_pDSGetInfo = ResolveSymbol<PDSGetInfo>( m_TLILib, wxT( "DSGetInfo" ) );
        m_pTLIMV_MACFromSerial = ResolveSymbol<PTLIMV_MACFromSerial>( m_TLILib, wxT( "TLIMV_MACFromSerial" ) );
        m_pTLIMV_IsValidIPv4Address = ResolveSymbol<PTLIMV_IsValidIPv4Address>( m_TLILib, wxT( "TLIMV_IsValidIPv4Address" ) );
        m_pTLIMV_ForceIP = ResolveSymbol<PTLIMV_ForceIP>( m_TLILib, wxT( "TLIMV_ForceIP" ) );
        m_pTLIMV_DoAddressesMatch = ResolveSymbol<PTLIMV_DoAddressesMatch>( m_TLILib, wxT( "TLIMV_DoAddressesMatch" ), wxT( "TLIMV_DoAdressesMatch" ) );
    }

    const wxTextAttr boldStyle( GetBoldUnderlinedStyle( m_pLogWindow ) );

    int status = 0;
    LOGGED_TLI_CALL( GCInitLib, (), WriteLogMessage )
    LOGGED_TLI_CALL( TLOpen, ( &m_hTLI ), WriteLogMessage )

    wxString processedCommandLineParameters;
    wxString commandLineErrors;
    wxString deviceToConfigure;
    wxString userDefinedName;
    m_boAtLeastOneInterfaceIsIgnored = IsAtLeastOneInterfaceIgnored();

    InterfaceInfo interfaceInfo[DetectedDeviceInfo::MAX_INTERFACE_COUNT];
    // Quick parsing of critical parameters that have to be assessed before UpdateDeviceList() is called for the first time
    for( int i = 1; i < argc; i++ )
    {
        const wxString param( argv[i] );
        const wxString key = param.BeforeFirst( wxT( '=' ) );
        if( ( key == wxT( "forceAutoConfigurePersistent" ) ) || ( key == wxT( "facp" ) ) )
        {
            m_boForceAutoConfigurePersistentIP = true;
        }
        if( ( key == wxT( "forceAutoConfigureDHCP" ) ) || ( key == wxT( "facd" ) ) )
        {
            m_boForceAutoConfigureDHCPIP = true;
        }
    }
    UpdateDeviceList();
    // Normal parsing of command line parameters
    for( int i = 1; i < argc; i++ )
    {
        const wxString param( argv[i] );
        const wxString key = param.BeforeFirst( wxT( '=' ) );
        const wxString value = param.AfterFirst( wxT( '=' ) );
        if( key.IsEmpty() )
        {
            commandLineErrors.Append( wxString::Format( wxT( "Invalid command line parameter: '%s'. Ignored.\n" ), param.c_str() ) );
        }
        else
        {
            if( ( key == wxT( "device" ) ) || ( key == wxT( "d" ) ) )
            {
                if( !deviceToConfigure.IsEmpty() )
                {
                    for( size_t j = 0; j < DetectedDeviceInfo::MAX_INTERFACE_COUNT; j++ )
                    {
                        m_interfaceInfo[j] = interfaceInfo[j];
                    }
                    ApplyChanges( deviceToConfigure, m_pDevListCtrl->GetItemText( m_pDevListCtrl->GetCurrentItemIndex() ), m_pCBConnectedToIPAddress->GetValue(), userDefinedName );
                }
                deviceToConfigure = wxEmptyString;
                if( SelectDevice( value ) )
                {
                    deviceToConfigure = value;
                    for( size_t j = 0; j < DetectedDeviceInfo::MAX_INTERFACE_COUNT; j++ )
                    {
                        interfaceInfo[j] = m_interfaceInfo[j];
                    }
                }
            }
            else if( ( key == wxT( "userDefinedName" ) ) || ( key == wxT( "udn" ) ) )
            {
                userDefinedName = value;
            }
            else if( key == wxT( "useDHCP" ) )
            {
                CHECK_INTERFACE_INDEX;
                interfaceInfo[interfaceIndex].DHCPEnabled_ = atoi( value.AfterLast( wxT( ';' ) ).mb_str() ) != 0;
            }
            else if( key == wxT( "usePersistentIP" ) )
            {
                CHECK_INTERFACE_INDEX;
                interfaceInfo[interfaceIndex].persistentIPEnabled_ = atoi( value.AfterLast( wxT( ';' ) ).mb_str() ) != 0;
            }
            else if( key == wxT( "persistentIPAddress" ) )
            {
                CHECK_INTERFACE_INDEX;
                interfaceInfo[interfaceIndex].persistentIPAddress_ = value.AfterLast( wxT( ';' ) ).mb_str();
            }
            else if( key == wxT( "persistentSubnetMask" ) )
            {
                CHECK_INTERFACE_INDEX;
                interfaceInfo[interfaceIndex].persistentSubnetMask_ = value.AfterLast( wxT( ';' ) ).mb_str();
            }
            else if( key == wxT( "persistentDefaultGateway" ) )
            {
                CHECK_INTERFACE_INDEX;
                interfaceInfo[interfaceIndex].persistentDefaultGateway_ = value.AfterLast( wxT( ';' ) ).mb_str();
            }
            else if( ( key == wxT( "forceAutoConfigurePersistent" ) ) || ( key == wxT( "facp" ) ) )
            {
                // This statement is redundant since this parameter has been "quickparsed" earlier.
                // However we leave it like this to satisfy the parser, since it does no damage, and
                // furthermore note the desired adapter IP mask value for future use.
                m_boForceAutoConfigurePersistentIP = true;
                m_ForceAutoConfigureAdapterPersistentIPAddress = std::string( value.mb_str() );
                if( m_ForceAutoConfigureAdapterPersistentIPAddress.empty() )
                {
                    m_ForceAutoConfigureAdapterPersistentIPAddress = std::string( "*" );
                }
            }
            else if( ( key == wxT( "forceAutoConfigureDHCP" ) ) || ( key == wxT( "facd" ) ) )
            {
                // This statement is redundant since this parameter has been "quickparsed" earlier.
                // However we leave it like this to satisfy the parser, since it does no damage, and
                // furthermore note the desired adapter IP mask value for future use.
                m_boForceAutoConfigureDHCPIP = true;
                m_ForceAutoConfigureAdapterDHCPIPAddress = std::string( value.mb_str() );
                if( m_ForceAutoConfigureAdapterDHCPIPAddress.empty() )
                {
                    m_ForceAutoConfigureAdapterDHCPIPAddress = std::string( "*" );
                }
            }
            else if( ( key == wxT( "quit" ) ) || ( key == wxT( "q" ) ) )
            {
                m_boMustQuit = true;
            }
            else if( ( key == wxT( "log_file" ) ) || ( key == wxT( "lf" ) ) )
            {
                m_logFileName = value;
            }
            else if( key != wxT( "hidden" ) )
            {
                commandLineErrors.Append( wxString::Format( wxT( "Invalid command line parameter: '%s'. Ignored.\n" ), param.c_str() ) );
            }
            processedCommandLineParameters += param;
            processedCommandLineParameters.Append( wxT( ' ' ) );
        }
    }

    if( !deviceToConfigure.IsEmpty() )
    {
        for( size_t j = 0; j < DetectedDeviceInfo::MAX_INTERFACE_COUNT; j++ )
        {
            m_interfaceInfo[j] = interfaceInfo[j];
        }
        ApplyChanges( deviceToConfigure, m_pDevListCtrl->GetItemText( m_pDevListCtrl->GetCurrentItemIndex() ), m_pCBConnectedToIPAddress->GetValue(), userDefinedName );
    }

    DisplayCommandLineProcessingInformation( m_pLogWindow, processedCommandLineParameters, commandLineErrors, GetBoldUnderlinedStyle( m_pLogWindow ) );

    if( GetMisconfiguredDevicesDetected() || m_boMustQuit || m_boAtLeastOneInterfaceIsIgnored )
    {
        m_postGUIInitializationTimer.SetOwner( this, tePostGUIInitialization );
        m_postGUIInitializationTimer.Start( 1000, true );
    }
}

//-----------------------------------------------------------------------------
IPConfigureFrame::~IPConfigureFrame()
//-----------------------------------------------------------------------------
{
    Deinit();
    {
        // store the current state of the application
        FramePositionStorage::Save( this );
        // when we e.g. try to write configuration stuff on a read-only file system the result can
        // be an annoying message box. Therefore we switch off logging during the storage operation.
        wxLogNull logSuspendScope;
        wxConfigBase* pConfig = wxConfigBase::Get();
        pConfig->Write( wxT( "/MainFrame/useAdvancedDeviceDiscovery" ), m_pMISettings_UseAdvancedDeviceDiscovery->IsChecked() );
        pConfig->Write( wxT( "/MainFrame/autoFixNetworkingIssues" ), m_pMISettings_AutoFixNetworkingIssues->IsChecked() );
        pConfig->Write( wxT( "/MainFrame/showWarningOnIgnoredInterfaces" ), m_pMISettings_ShowWarningOnIgnoredInterfaces->IsChecked() );
        pConfig->Write( wxT( "/MainFrame/verticalSplitter" ), m_pVerticalSplitter->GetSashPosition() );
        pConfig->Write( wxT( "/MainFrame/horizontalSplitter" ), m_pHorizontalSplitter->GetSashPosition() );
        pConfig->Flush();
    }
    InterfaceContainer::iterator it = m_TLIInterfaces.begin();
    InterfaceContainer::iterator itEnd = m_TLIInterfaces.end();
    int status = 0;
    while( it != itEnd )
    {
        LOGGED_TLI_CALL( IFClose, ( it->second ), WriteLogMessage )
        ++it;
    }
    LOGGED_TLI_CALL( TLClose, ( m_hTLI ), WriteLogMessage )
    LOGGED_TLI_CALL( GCCloseLib, (), WriteLogMessage )
    for_each( m_devices.begin(), m_devices.end(), ptr_fun( DeleteSecond<const string, DetectedDeviceInfo*> ) );
    m_devices.clear();
    if( !m_logFileName.IsEmpty() )
    {
        wxFFile file( m_logFileName, wxT( "w" ) );
        if( file.IsOpened() )
        {
            file.Write( m_pLogWindow->GetValue() );
        }
    }

    // when we e.g. try to write configuration stuff on a read-only file system the result can
    // be an annoying message box. Therefore we switch off logging now, as otherwise higher level
    // clean up code might produce error messages
    wxLog::EnableLogging( false );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::ApplyChanges( const wxString& serial, const wxString& product, const wxString& connectedToIPAddress, const wxString& userDefinedName )
//-----------------------------------------------------------------------------
{
    InterfaceContainer::const_iterator itInterface = m_TLIInterfaces.begin();
    DeviceMap::const_iterator itDev = m_devices.begin();
    if( !FindDeviceWithSerial( serial, connectedToIPAddress, itInterface, itDev ) )
    {
        return;
    }

    WriteLogMessage( wxString::Format( wxT( "Validating pending changes for device %s(%s)...\n" ), serial.c_str(), product.c_str() ) );
    for( unsigned int i = 0; i < itDev->second->interfaceCount_; i++ )
    {
        if( itDev->second->interfaceInfo_[i].supportsPersistentIP_ &&
            m_interfaceInfo[i].persistentIPEnabled_ )
        {
            if( !ValidateIPDataSet( m_interfaceInfo[i].persistentIPAddress_, m_interfaceInfo[i].persistentSubnetMask_, m_interfaceInfo[i].persistentDefaultGateway_, this, this, ( m_interfaceInfo[i].persistentIPAddress_ == s_m_subnetMismatchToken ) ) )
            {
                return;
            }
        }
    }

    int status = 0;
    for( unsigned int i = 0; i < itDev->second->interfaceCount_; i++ )
    {
        if( itDev->second->interfaceInfo_[i].supportsDHCP_ )
        {
            if( m_boMarkIPAddressConflict )
            {
                const wxString ipAddress = ConvertedString( GetInterfaceStringInfo( itInterface->second, INTERFACE_INFO_UNUSED_IP_STRING ).c_str() );
                const wxString subnetMask( ConvertedString( itDev->second->adapters_[i].second.netMask_.c_str() ) );
                const wxString gateway( ConvertedString( itDev->second->adapters_[i].first.c_str() ) );
                WriteLogMessage( wxString::Format( wxT( "Assigning temporary IP address %s to device %s(%s)...\n" ), ipAddress.c_str(), serial.c_str(), product.c_str() ) );
                wxString MAC( ConvertedString( m_interfaceInfo[i].MACAddress_ ) );
                MAC.Replace( wxT( ":" ), wxT( "" ) );
                ForceIP( MAC.mb_str(), ipAddress.mb_str(), subnetMask.mb_str(), gateway.mb_str(), connectedToIPAddress.mb_str(), 1000 );
                WriteLogMessage( wxT( "Done!\n" ) );
                UpdateDeviceList( false, tudlvlAllButDeviceListings );
                if( !FindDeviceWithSerial( serial, connectedToIPAddress, itInterface, itDev ) )
                {
                    return;
                }
            }
        }
    }

    WriteLogMessage( wxString::Format( wxT( "Trying to establish write access to device %s(%s).\n" ), serial.c_str(), product.c_str() ) );
    MVTLI_DEVICE_HANDLE hDev = 0;
    LOGGED_TLI_CALL( IFOpenDevice, ( itInterface->second, itDev->second->deviceName_.c_str(), DEVICE_ACCESS_EXCLUSIVE, &hDev ), WriteLogMessage )
    if( status != 0 )
    {
        return;
    }

    unsigned int timeout_ms = 2000;
    LOGGED_TLI_CALL( TLIMV_DevSetParam, ( hDev, DEVICE_INFO_GVCP_MESSAGE_TIMEOUT, &timeout_ms, sizeof( timeout_ms ) ), WriteLogMessage )

    WriteLogMessage( wxString::Format( wxT( "Trying to apply changes to device %s(%s).\n" ), serial.c_str(), product.c_str() ) );
    for( unsigned int i = 0; i < itDev->second->interfaceCount_; i++ )
    {
        if( itDev->second->interfaceInfo_[i].supportsDHCP_ )
        {
            LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_CURRENT_IP_DHCP, &m_interfaceInfo[i].DHCPEnabled_, sizeof( m_interfaceInfo[i].DHCPEnabled_ ) ), WriteLogMessage )
        }

        if( itDev->second->interfaceInfo_[i].supportsPersistentIP_ )
        {
            LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_CURRENT_IP_PERSISTENT, &m_interfaceInfo[i].persistentIPEnabled_, sizeof( m_interfaceInfo[i].persistentIPEnabled_ ) ), WriteLogMessage )
            if( m_interfaceInfo[i].persistentIPEnabled_ )
            {
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_PERSISTENT_IP_STRING, m_interfaceInfo[i].persistentIPAddress_.c_str(), m_interfaceInfo[i].persistentIPAddress_.length() ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_PERSISTENT_NETMASK_STRING, m_interfaceInfo[i].persistentSubnetMask_.c_str(), m_interfaceInfo[i].persistentSubnetMask_.length() ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_PERSISTENT_DEFAULT_GATEWAY_STRING, m_interfaceInfo[i].persistentDefaultGateway_.c_str(), m_interfaceInfo[i].persistentDefaultGateway_.length() ), WriteLogMessage )
            }
        }
    }
    if( itDev->second->supportsUserDefinedName_ )
    {
        LOGGED_TLI_CALL( TLIMV_DevSetParam, ( hDev, DEVICE_INFO_USER_DEFINED_NAME, userDefinedName.mb_str(), userDefinedName.Length() ), WriteLogMessage )
    }

    timeout_ms = 200;
    LOGGED_TLI_CALL( TLIMV_DevSetParam, ( hDev, DEVICE_INFO_GVCP_MESSAGE_TIMEOUT, &timeout_ms, sizeof( timeout_ms ) ), WriteLogMessage )
    WriteLogMessage( wxString::Format( wxT( "Trying to close device %s(%s)...\n" ), serial.c_str(), product.c_str() ) );
    LOGGED_TLI_CALL( DevClose, ( hDev ), WriteLogMessage )
    WriteLogMessage( wxT( "Done!\n" ) );
    UpdateDeviceList();
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::ManuallyAssignTemporaryIP( int listItemIndex, bool boShowDifferentSubnetWarning /* = false*/ )
//-----------------------------------------------------------------------------
{
    wxString deviceMACAddress;
    wxString connectedIPAddress;
    if( listItemIndex >= 0 )
    {
        wxString itemText( m_pDevListCtrl->GetItemText( listItemIndex ) );
        wxListItem info;
        info.m_itemId = listItemIndex;
        info.m_col = lcSerial;
        info.m_mask = wxLIST_MASK_TEXT;
        if( !m_pDevListCtrl->GetItem( info ) )
        {
            WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain serial number for selected device %s.\n" ), itemText.c_str() ), m_ERROR_STYLE );
        }

        DeviceMap::const_iterator itDev = m_devices.find( string( m_pSTSerialNumber->GetLabel().mb_str() ) );
        if( itDev == m_devices.end() )
        {
            WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain device name for selected device %s on adapter %s.\n" ), m_pSTSerialNumber->GetLabel().c_str(), m_pCBConnectedToIPAddress->GetValue().c_str() ), m_ERROR_STYLE );
            return;
        }
        deviceMACAddress = ConvertedString( itDev->second->interfaceInfo_[0].MACAddress_ );
        connectedIPAddress = m_pCBConnectedToIPAddress->GetValue();
    }
    AssignIPDlg dlg( this, deviceMACAddress, connectedIPAddress, boShowDifferentSubnetWarning, ( m_pDevListCtrl->GetCurrentItemIndex() >= 0 ) ? false : true );
    if( dlg.ShowModal() == wxID_OK )
    {
        UpdateDeviceList( !boShowDifferentSubnetWarning );
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::AutoAssignPersistentIPOrDHCP( int listItemIndex, bool boPersistentOverDHCP )
//-----------------------------------------------------------------------------
{
    if( listItemIndex < 0 )
    {
        return;
    }

    wxBusyCursor busyCursorScope;
    wxString itemText( m_pDevListCtrl->GetItemText( listItemIndex ) );
    wxListItem info;
    info.m_itemId = listItemIndex;
    info.m_col = lcSerial;
    info.m_mask = wxLIST_MASK_TEXT;
    if( !m_pDevListCtrl->GetItem( info ) )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain serial number for selected device %s.\n" ), itemText.c_str() ), m_ERROR_STYLE );
    }
    std::string serialOfCurrentDevice( info.m_text.mb_str() );
    DeviceMap::const_iterator itDev = m_devices.find( serialOfCurrentDevice );
    if( itDev == m_devices.end() )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain device name for selected device %s.\n" ), serialOfCurrentDevice.c_str() ), m_ERROR_STYLE );
    }

    std::string firstAdapterIPOfCurrentDevice, firstAdapterNetMaskOfCurrentDevice;
    const std::vector<std::pair<std::string, std::string> >::size_type adapterCnt = itDev->second->adapters_.size();
    for( std::vector<std::pair<std::string, std::string> >::size_type i = 0; i < adapterCnt; i++ )
    {
        firstAdapterIPOfCurrentDevice = ConvertedString( itDev->second->adapters_[i].first.c_str() );
        firstAdapterNetMaskOfCurrentDevice = ConvertedString( itDev->second->adapters_[i].second.netMask_.c_str() );
    }

    InterfaceContainer::const_iterator itInterface = m_TLIInterfaces.begin();
    if( !FindDeviceWithSerial( serialOfCurrentDevice, firstAdapterIPOfCurrentDevice, itInterface, itDev ) )
    {
        return;
    }
    const wxString connectedIPAddress = firstAdapterIPOfCurrentDevice;
    const wxString connectedSubnetMask = firstAdapterNetMaskOfCurrentDevice;
    const wxString validTemporaryIPAddress = ConvertedString( GetInterfaceStringInfo( itInterface->second, INTERFACE_INFO_UNUSED_IP_STRING ) );
    wxString connectedMAC = ConvertedString( m_interfaceInfo[0].MACAddress_ );
    connectedMAC.Replace( wxT( ":" ), wxT( "" ) );
    WriteLogMessage( wxString::Format( wxT( "Establishing communication with device %s(%s) by temporarily assigning IP Address %s to it...\n" ), serialOfCurrentDevice.c_str(), itemText.c_str(), validTemporaryIPAddress.c_str() ) );
    ForceIP( connectedMAC.mb_str(), validTemporaryIPAddress.mb_str(), connectedSubnetMask.mb_str(), "0.0.0.0", connectedIPAddress.mb_str(), 1000 );
    WriteLogMessage( wxT( "Done!\n" ) );
    UpdateDeviceList( false, tudlvlSilent );

    // Relocate device with the new temporary IP...
    itemText = m_pDevListCtrl->GetItemText( listItemIndex );
    info.m_itemId = listItemIndex;
    if( !m_pDevListCtrl->GetItem( info ) )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain serial number for selected device %s.\n" ), itemText.c_str() ), m_ERROR_STYLE );
    }
    itDev = m_devices.find( serialOfCurrentDevice );
    if( itDev == m_devices.end() )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain device name for selected device %s on adapter %s.\n" ), serialOfCurrentDevice.c_str(), firstAdapterIPOfCurrentDevice.c_str() ), m_ERROR_STYLE );
    }
    itInterface = m_TLIInterfaces.begin();
    if( !FindDeviceWithSerial( serialOfCurrentDevice, firstAdapterIPOfCurrentDevice, itInterface, itDev ) )
    {
        return;
    }

    // Now a persistent IP or DHCP IP Setting for this device may be set
    int status = 0;
    MVTLI_DEVICE_HANDLE hDev = 0;
    LOGGED_TLI_CALL( IFOpenDevice, ( itInterface->second, itDev->second->deviceName_.c_str(), DEVICE_ACCESS_EXCLUSIVE, &hDev ), WriteLogMessage )
    if( status != 0 )
    {
        return;
    }
    unsigned int timeout_ms = 2000;
    LOGGED_TLI_CALL( TLIMV_DevSetParam, ( hDev, DEVICE_INFO_GVCP_MESSAGE_TIMEOUT, &timeout_ms, sizeof( timeout_ms ) ), WriteLogMessage )
    bool boMustRebootDevice = false;
    for( unsigned int i = 0; i < itDev->second->interfaceCount_; i++ )
    {
        if( boPersistentOverDHCP )
        {
            if( itDev->second->interfaceInfo_[i].supportsPersistentIP_ )
            {
                const unsigned char dhcpDisable = 0;
                const unsigned char persistentIPEnable = 1;
                WriteLogMessage( wxString::Format( wxT( "Now setting IP Address %s as persistent for this device...\n" ), validTemporaryIPAddress.c_str() ) );
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_CURRENT_IP_DHCP, &dhcpDisable, sizeof( dhcpDisable ) ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_CURRENT_IP_PERSISTENT, &persistentIPEnable, sizeof( persistentIPEnable ) ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_PERSISTENT_IP_STRING, validTemporaryIPAddress.mb_str(), m_interfaceInfo[i].persistentIPAddress_.length() ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_PERSISTENT_NETMASK_STRING, connectedSubnetMask.mb_str(), connectedSubnetMask.mb_str().length() ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_PERSISTENT_DEFAULT_GATEWAY_STRING, "0.0.0.0", std::string( "0.0.0.0" ).length() ), WriteLogMessage )
                boMustRebootDevice = true; // Rebooting the device is necessary to make sure it utilises the new valid persistent IP address
            }
            else
            {
                WriteLogMessage( wxString::Format( wxT( "ERROR: Device %s on adapter %s does not support Persistent IP settings!\n" ), serialOfCurrentDevice.c_str(), firstAdapterIPOfCurrentDevice.c_str() ), m_ERROR_STYLE );
            }
        }
        else
        {
            if( itDev->second->interfaceInfo_[i].supportsDHCP_ )
            {
                const unsigned char dhcpEnable = 1;
                const unsigned char persistentIPDisable = 0;
                WriteLogMessage( wxT( "Now setting DHCP as preferred address acquisition method for this device...\n" ) );
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_CURRENT_IP_PERSISTENT, &persistentIPDisable, sizeof( persistentIPDisable ) ), WriteLogMessage )
                LOGGED_TLI_CALL( TLIMV_DevSetInterfaceParam, ( hDev, i, DEVICE_INFO_CURRENT_IP_DHCP, &dhcpEnable, sizeof( dhcpEnable ) ), WriteLogMessage )
                boMustRebootDevice = true; // Rebooting the device is necessary in order for it to become a valid DHCP address
            }
            else
            {
                WriteLogMessage( wxString::Format( wxT( "ERROR: Device %s on adapter %s does not support DHCP IP settings!\n" ), serialOfCurrentDevice.c_str(), firstAdapterIPOfCurrentDevice.c_str() ), m_ERROR_STYLE );
            }
        }
    }
    LOGGED_TLI_CALL( DevClose, ( hDev ), WriteLogMessage )
    if( boMustRebootDevice )
    {
        RebootDeviceAndWaitForItToReappear( serialOfCurrentDevice );
    }
    UpdateDeviceList( true, tudlvlSilent );
    WriteLogMessage( wxT( "Done!\n" ) );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::AutoAssignTemporaryIP( int listItemIndex )
//-----------------------------------------------------------------------------
{
    if( listItemIndex < 0 )
    {
        return;
    }
    if( !m_boMarkIPAddressConflict && !m_boMarkNetmaskConflict )
    {
        WriteLogMessage( wxT( "No connectivity issues detected for selected device, and no temporary IP address will be assigned!\nThe device is reachable and you may proceed with the device configuration by clicking the Configure Button.\n" ) );
        return;
    }

    wxString itemText( m_pDevListCtrl->GetItemText( listItemIndex ) );
    wxListItem info;
    info.m_itemId = listItemIndex;
    info.m_col = lcSerial;
    info.m_mask = wxLIST_MASK_TEXT;
    if( !m_pDevListCtrl->GetItem( info ) )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain serial number for selected device %s.\n" ), itemText.c_str() ), m_ERROR_STYLE );
    }
    DeviceMap::const_iterator itDev = m_devices.find( string( m_pSTSerialNumber->GetLabel().mb_str() ) );
    if( itDev == m_devices.end() )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain device name for selected device %s on adapter %s.\n" ), m_pSTSerialNumber->GetLabel().c_str(), m_pCBConnectedToIPAddress->GetValue().c_str() ), m_ERROR_STYLE );
    }
    InterfaceContainer::const_iterator itInterface = m_TLIInterfaces.begin();
    if( !FindDeviceWithSerial( m_pSTSerialNumber->GetLabel(), m_pCBConnectedToIPAddress->GetValue(), itInterface, itDev ) )
    {
        return;
    }
    const wxString connectedIPAddress = m_pCBConnectedToIPAddress->GetValue();
    const wxString connectedSubnetMask = m_pSTConnectedToNetmask->GetLabel();
    const wxString validTemporaryIPAddress = ConvertedString( GetInterfaceStringInfo( itInterface->second, INTERFACE_INFO_UNUSED_IP_STRING ) );
    wxString connectedMAC = ConvertedString( m_interfaceInfo[0].MACAddress_ );
    connectedMAC.Replace( wxT( ":" ), wxT( "" ) );
    WriteLogMessage( wxString::Format( wxT( "Assigning Temporary IP Address %s to device %s(%s)...\n" ), validTemporaryIPAddress.c_str(), m_pSTSerialNumber->GetLabel().c_str(), itemText.c_str() ) );
    ForceIP( connectedMAC.mb_str(), validTemporaryIPAddress.mb_str(), connectedSubnetMask.mb_str(), "0.0.0.0", connectedIPAddress.mb_str(), 1000 );
    WriteLogMessage( wxT( "Done!\n" ) );
    UpdateDeviceList( true, tudlvlAllButDeviceListings );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::AutoFillFromIP( wxTextCtrl* IPField, wxTextCtrl* netMaskField, wxTextCtrl* gateWayField )
//-----------------------------------------------------------------------------
{
    if( IsValidIPv4Address( IPField->GetValue().mb_str() ) == 0 )
    {
        const unsigned long persistentIPv4Address = GetIPv4AddressFromString( IPField->GetValue().mb_str() );
        const unsigned long firstOctet = persistentIPv4Address & 0xFF;

        if( firstOctet >= 1 && firstOctet < 128 )
        {
            //Class A Network
            const wxString persistentIPv4SubnetString = wxString::Format( wxT( "%d.0.0.0" ), firstOctet );
            netMaskField->SetValue( wxT( "255.0.0.0" ) );
            gateWayField->SetValue( persistentIPv4SubnetString );
        }
        else if( firstOctet >= 128 && firstOctet < 192 )
        {
            //Class B Network
            const wxString persistentIPv4SubnetString = wxString::Format( wxT( "%d.%d.0.0" ), firstOctet, ( ( persistentIPv4Address >> 8 ) & 0xFF ) );
            netMaskField->SetValue( wxT( "255.255.0.0" ) );
            gateWayField->SetValue( persistentIPv4SubnetString );
        }
        else if( firstOctet >= 192 && firstOctet < 224 )
        {
            //Class C Network
            const wxString persistentIPv4SubnetString = wxString::Format( wxT( "%d.%d.%d.0" ), firstOctet, ( ( persistentIPv4Address >> 8 ) & 0xFF ), ( ( persistentIPv4Address >> 16 ) & 0xFF ) );
            netMaskField->SetValue( wxT( "255.255.255.0" ) );
            gateWayField->SetValue( persistentIPv4SubnetString );
        }
        else if( firstOctet >= 224 )
        {
            //Class D & E Networks and outrageous IP values
            const unsigned long currentAdapterSubnet = GetIPv4AddressFromString( m_pCBConnectedToIPAddress->GetValue().mb_str() ) & GetIPv4AddressFromString( m_pSTConnectedToNetmask->GetLabel().mb_str() );
            const wxString currentAdapterSubnetString = wxString::Format( wxT( "%d.%d.%d.%d" ), ( currentAdapterSubnet & 0xFF ), ( ( currentAdapterSubnet >> 8 ) & 0xFF ),
                    ( ( currentAdapterSubnet >> 16 ) & 0xFF ), ( ( currentAdapterSubnet >> 24 ) & 0xFF ) );
            wxMessageBox( wxString::Format( wxT( "The persistent IP address value is either invalid, or it belongs to the multicast or experimental address ranges.\
                                                 \nPlease enter a valid persistent IP Address.\nTIP: The valid subnet for the currently selected adapter is the %s subnet" ),
                                            currentAdapterSubnetString.c_str() ), wxT( "Not a valid persistent IP address!" ), wxOK | wxICON_EXCLAMATION, this );
            netMaskField->SetValue( wxT( "This IP address is not valid!" ) );
            gateWayField->SetValue( wxT( "This IP address is not valid!" ) );
        }
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::BuildList( bool boShowWarnings /* = true*/ )
//-----------------------------------------------------------------------------
{
    m_pDevListCtrl->DeleteAllItems();
    DeviceMap::const_iterator it = m_devices.begin();
    DeviceMap::const_iterator itEND = m_devices.end();
    long devCount = 0;
    while( it != itEND )
    {
        bool boNetmasksMatch = ( it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].second.netMask_ == it->second->interfaceInfo_[0].currentSubnetMask_ );
        if( !( boNetmasksMatch == true ) &&
            !( m_boForceAutoConfigurePersistentIP || m_boForceAutoConfigureDHCPIP ) &&
            ( boShowWarnings == true ) )
        {
            WriteLogMessage( wxString::Format( wxT( "WARNING: Device %s is not configured properly to be fully accessible at adapter %s (the device and the network adapter it is connected to, don't use the same netmask). Use the 'Action' menu or right-click on it in the on the above list, to access automatic or manual configuration options.\n" ), ConvertedString( it->second->interfaceInfo_[0].currentIPAddress_ ).c_str(), ConvertedString( it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].first ).c_str() ), m_ERROR_STYLE );
        }

        bool boAddressesMatch = false;
        if( boNetmasksMatch )
        {
            if( m_pTLIMV_DoAddressesMatch )
            {
                boAddressesMatch = ( m_pTLIMV_DoAddressesMatch( it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].first.c_str(), it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].second.netMask_.c_str(), it->second->interfaceInfo_[0].currentIPAddress_.c_str(), it->second->interfaceInfo_[0].currentSubnetMask_.c_str() ) == 0 );
                if( !( boAddressesMatch == true ) &&
                    !( m_boForceAutoConfigurePersistentIP || m_boForceAutoConfigureDHCPIP ) &&
                    ( boShowWarnings == true ) &&
                    ( it->second->boHasBeenDiscoveredViaUnicast_ != true ) )
                {
                    WriteLogMessage( wxString::Format( wxT( "WARNING: Device %s is not configured properly to be fully accessible at adapter %s (the device and the adapter it is connected to, don't reside in the same subnet). Use the 'Action' menu or right-click on it in the on the above list, to access automatic or manual configuration options.\n" ), ConvertedString( it->second->interfaceInfo_[0].currentIPAddress_ ).c_str(), ConvertedString( it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].first ).c_str() ), m_ERROR_STYLE );
                }
            }
        }

        long index = 0;
        if( boNetmasksMatch && boAddressesMatch )
        {
            index = m_pDevListCtrl->InsertItem( devCount, ConvertedString( it->second->modelName_ ), 1 );
        }
        else if( it->second->boHasBeenDiscoveredViaUnicast_ )
        {
            index = m_pDevListCtrl->InsertItem( devCount, ConvertedString( it->second->modelName_ ), 1 );
            it->second->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisDiscoveredViaUnicast;
        }
        else
        {
            index = m_pDevListCtrl->InsertItem( devCount, ConvertedString( it->second->modelName_ ), 0 );
            SetMisconfiguredDevicesDetected( true );
            it->second->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisNetworkMisconfiguration;
        }
        m_pDevListCtrl->SetItem( index, lcSerial, ConvertedString( it->first ) );
        m_pDevListCtrl->SetItem( index, lcPrimaryInterfaceIPAddress, ConvertedString( it->second->interfaceInfo_[0].currentIPAddress_ ) );
        m_pDevListCtrl->SetItem( index, lcPotentialPerformanceIssues, ConvertedString( DetectedDeviceInfo::PerformanceIssueStatusToString( it->second->potentialPerformanceIssueStatus_ ) ) );
        switch( it->second->potentialPerformanceIssueStatus_ )
        {
        case DetectedDeviceInfo::pisNotChecked:
        case DetectedDeviceInfo::pisCannotAccess:
        case DetectedDeviceInfo::pisDiscoveredViaUnicast:
            m_pDevListCtrl->SetItemBackgroundColour( index, m_YELLOW_PASTEL );
            break;
        case DetectedDeviceInfo::pisNetworkMisconfiguration:
            m_pDevListCtrl->SetItemBackgroundColour( index, m_RED_PASTEL );
            break;
        case DetectedDeviceInfo::pisIssuesDetected:
            m_pDevListCtrl->SetItemBackgroundColour( index, m_BLUE_PASTEL );
            break;
        case DetectedDeviceInfo::pisNone:
            break;
        }
        m_pDevListCtrl->SetItemData( index, it->second->id_ );
        ++it;
        ++devCount;
    }

    m_pDevListCtrl->SetColumnWidth( lcProduct, ( ( devCount == 0 ) ? wxLIST_AUTOSIZE_USEHEADER : wxLIST_AUTOSIZE ) );
    m_pDevListCtrl->SetColumnWidth( lcSerial, ( ( devCount == 0 ) ? wxLIST_AUTOSIZE_USEHEADER : wxLIST_AUTOSIZE ) );
    m_pDevListCtrl->SetColumnWidth( lcPrimaryInterfaceIPAddress, wxLIST_AUTOSIZE_USEHEADER );
    m_pDevListCtrl->SetColumnWidth( lcPotentialPerformanceIssues, wxLIST_AUTOSIZE_USEHEADER );
    UpdateDlgControls( false );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::CheckForPotentialPerformanceIssues( DetectedDeviceInfo* pDeviceInfo )
//-----------------------------------------------------------------------------
{
    if( !pDeviceInfo->pPerformanceIssuesDlg_ )
    {
        pDeviceInfo->pPerformanceIssuesDlg_ = new PerformanceIssuesDlg( this, wxID_ANY, wxT( "Potential Performance Issues" ), pDeviceInfo );
    }
    wxTreeCtrl* pTreeCtrl = pDeviceInfo->pPerformanceIssuesDlg_->GetTreeCtrl();
    pTreeCtrl->DeleteAllItems();
    wxTreeItemId rootId = pTreeCtrl->AddRoot( ConvertedString( pDeviceInfo->deviceName_ ) );

    ostringstream oss;
    const std::vector<std::pair<std::string, AdapterInfo> >::size_type adapterCnt = pDeviceInfo->adapters_.size();
    for( std::vector<std::pair<std::string, AdapterInfo> >::size_type i = 0; i < adapterCnt; i++ )
    {
        InterfaceContainer::const_iterator itInterface = m_TLIInterfaces.begin();
        InterfaceContainer::const_iterator itInterfaceEND = m_TLIInterfaces.end();
        string adapterIPAddress;
        while( itInterface != itInterfaceEND )
        {
            // Only do performance tests on GEV interfaces
            const wxString interfaceTLType( ConvertedString( GetInterfaceStringInfo( itInterface->second, INTERFACE_INFO_TLTYPE ) ) );
            if( !strncmp( interfaceTLType.mb_str(), "GEV", 3 ) )
            {
                adapterIPAddress = GetInterfaceStringInfo( itInterface->second, INTERFACE_INFO_IP_STRING );
                if( adapterIPAddress == pDeviceInfo->adapters_[i].first )
                {
                    break;
                }
            }
            ++itInterface;
        }

        if( itInterface == m_TLIInterfaces.end() )
        {
            WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain interface handle to adapter %s.\n" ), pDeviceInfo->adapters_[i].first.c_str() ), m_ERROR_STYLE );
            continue;
        }

        ostringstream adapterInfoMsg;
        adapterInfoMsg << itInterface->first << " (IPv4 address: " << adapterIPAddress << ", MTU: "
                       << pDeviceInfo->adapters_[i].second.MTU_ << "): ";
        bool boMarkAdapterEntry = false;
        if( pDeviceInfo->adapters_[i].second.MTU_ <= 1500 )
        {
            adapterInfoMsg << "This adapter reports an MTU of " << pDeviceInfo->adapters_[i].second.MTU_
                           << " but it is recommended to enable Jumbo Frames for interfaces working with GEV devices. ";
            pDeviceInfo->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisIssuesDetected;
            boMarkAdapterEntry = true;
        }

        if( pDeviceInfo->adapters_[i].second.linkSpeed_ < 1000 )
        {
            adapterInfoMsg << "This adapter reports a link speed of " << pDeviceInfo->adapters_[i].second.linkSpeed_
                           << " while at least 1000 is recommended for interfaces working with GEV devices. ";
            pDeviceInfo->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisIssuesDetected;
            boMarkAdapterEntry = true;
        }

        if( pDeviceInfo->potentialPerformanceIssueStatus_ == DetectedDeviceInfo::pisNotChecked )
        {
            pDeviceInfo->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisNone;
            adapterInfoMsg << "No issues detected.";
        }

        wxTreeItemId adapterTreeItemId = pTreeCtrl->AppendItem( rootId, ConvertedString( adapterInfoMsg.str() ) );
        if( boMarkAdapterEntry )
        {
            pTreeCtrl->SetItemTextColour( adapterTreeItemId, acDarkBluePastel );
        }
        oss << adapterInfoMsg.str() << endl;

        MVTLI_DEVICE_HANDLE hDev = 0;
        // this will only work for devices not currently under control by someone else
        if( m_pIFOpenDevice( itInterface->second, pDeviceInfo->deviceName_.c_str(), DEVICE_ACCESS_CONTROL, &hDev ) == 0 )
        {
            int status = 0;
            unsigned int streamChannelCnt = 0;
            LOGGED_TLI_CALL( DevGetNumDataStreams, ( hDev, &streamChannelCnt ), WriteLogMessage )
            for( unsigned int streamChannelIndex = 0; streamChannelIndex < streamChannelCnt; streamChannelIndex++ )
            {
                ostringstream datastreamInfoMsg;
                bool boMark = true;
                const string streamID( GetStreamID( hDev, streamChannelIndex ) );
                datastreamInfoMsg << " DataStream[" << streamChannelIndex << "]: ";
                if( !streamID.empty() )
                {
                    datastreamInfoMsg << "(ID: " << streamID << "): ";
                    MVTLI_DATASTREAM_HANDLE hDS = 0;
                    LOGGED_TLI_CALL( DevOpenDataStream, ( hDev, streamID.c_str(), &hDS ), WriteLogMessage )
                    if( status == 0 )
                    {
                        uint64_type SCPS = 0ULL;
                        size_t bufSize( sizeof( SCPS ) );
                        INFO_DATATYPE dataType = INFO_DATATYPE_UNKNOWN;
                        LOGGED_TLI_CALL( DSGetInfo, ( hDS, STREAM_INFO_SCPS, &dataType, &SCPS, &bufSize ), WriteLogMessage )
                        if( status == 0 )
                        {
                            datastreamInfoMsg << "Negotiated packet size: " << SCPS << ". ";
                            if( SCPS < 1500ULL )
                            {
                                datastreamInfoMsg << "This is too small and indicates that at least one network component (device, switch, network interface card) is not configured to use Jumbo Frames!";
                                pDeviceInfo->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisIssuesDetected;
                            }
                            else
                            {
                                boMark = false;
                            }
                        }
                        else
                        {
                            datastreamInfoMsg << "ERROR: Could not obtain packet size.";
                        }
                        LOGGED_TLI_CALL( DSClose, ( hDS ), WriteLogMessage )
                    }
                    else
                    {
                        datastreamInfoMsg << "ERROR: Could not open data stream.";
                    }
                }
                else
                {
                    datastreamInfoMsg << "ERROR: Could not obtain stream ID.";
                }

                wxTreeItemId dataStreamTreeItemId = pTreeCtrl->AppendItem( adapterTreeItemId, ConvertedString( datastreamInfoMsg.str() ) );
                if( boMark )
                {
                    pTreeCtrl->SetItemTextColour( dataStreamTreeItemId, acDarkBluePastel );
                }
                oss << datastreamInfoMsg.str() << endl;
            }
            LOGGED_TLI_CALL( DevClose, ( hDev ), WriteLogMessage )
        }
        else
        {
            oss << " Cannot open device thus cannot check data stream parameters" << endl;
            pDeviceInfo->potentialPerformanceIssueStatus_ = DetectedDeviceInfo::pisCannotAccess;
            wxTreeItemId dataStreamTreeItemId = pTreeCtrl->AppendItem( adapterTreeItemId, wxT( "Cannot open device thus cannot check data stream parameters" ) );
            pTreeCtrl->SetItemTextColour( dataStreamTreeItemId, wxColour( 255, 0, 0 ) );
        }
    }

    pDeviceInfo->potentialPerformanceIssuesMsg_ = oss.str();
    pDeviceInfo->pPerformanceIssuesDlg_->RefreshDlgControls();
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::CollectUnicastDeviceDiscoveryData( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    std::vector<mvIMPACT::acquire::GenICam::InterfaceModule*> validInterfaces_;
    mvIMPACT::acquire::GenICam::SystemModule sm;
    const int64_type interfaceCnt = sm.getInterfaceModuleCount();
    if( interfaceCnt > 0 )
    {
        for( int64_type interfaceIndex = 0; interfaceIndex < interfaceCnt; interfaceIndex++ )
        {
            mvIMPACT::acquire::GenICam::InterfaceModule* pIm = new mvIMPACT::acquire::GenICam::InterfaceModule( sm, interfaceIndex );
            if( pIm->interfaceType.readS() != "GEV" ||
                !pIm->deviceSelector.isValid() ||
                !pIm->gevInterfaceSubnetSelector.isValid() ||
                !pIm->gevInterfaceSubnetIPAddress.isValid() ||
                !pIm->interfaceDisplayName.isValid() )
            {
                // most likely this is a 3rd party producer. As we cannot obtain the
                // information we are after just skip this interface!
                DeleteElement( pIm );
                continue;
            }
            validInterfaces_.push_back( pIm );
        }
    }

    const size_t validInterfaceCnt = validInterfaces_.size();
    for( size_t i = 0; i < validInterfaceCnt; i++ )
    {
        const int64_type adapterSubnetCnt = validInterfaces_[i]->gevInterfaceSubnetSelector.getMaxValue() + 1;
        int j = 0;
        do
        {
            validInterfaces_[i]->gevInterfaceSubnetSelector.write( j );
            const int64_type unicastCount( validInterfaces_[i]->mvUnicastDeviceDiscoveryCommandCount.read() );
            for( int64_type k = 0; k < unicastCount; k++ )
            {
                validInterfaces_[i]->mvUnicastDeviceDiscoveryCommandSelector.write( k );
                m_unicastDestinationIPs.push_back( GetIPv4AddressAsString( static_cast< unsigned long >( validInterfaces_[i]->mvUnicastDeviceDiscoveryDestinationIPAddress.read() ) ) );
            }
            j++;
        }
        while( adapterSubnetCnt < j );
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::Deinit( void )
//-----------------------------------------------------------------------------
{
    if( m_postGUIInitializationTimer.IsRunning() )
    {
        m_postGUIInitializationTimer.Stop();
    }
}

//-----------------------------------------------------------------------------
bool IPConfigureFrame::FindDeviceWithSerial( const wxString& serial, const wxString& connectedToIPAddress, InterfaceContainer::const_iterator& itInterface, DeviceMap::const_iterator& itDev )
//-----------------------------------------------------------------------------
{
    itInterface = m_TLIInterfaces.begin();
    InterfaceContainer::const_iterator itInterfaceEND = m_TLIInterfaces.end();
    while( itInterface != itInterfaceEND )
    {
        wxString adapterIPAddress( ConvertedString( GetInterfaceStringInfo( itInterface->second, INTERFACE_INFO_IP_STRING ) ) );
        if( adapterIPAddress == connectedToIPAddress )
        {
            break;
        }
        ++itInterface;
    }

    if( itInterface == m_TLIInterfaces.end() )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain interface handle to adapter %s.\n" ), connectedToIPAddress.c_str() ), m_ERROR_STYLE );
        return false;
    }

    itDev = m_devices.find( string( serial.mb_str() ) );
    if( itDev == m_devices.end() )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain device name for device %s on adapter %s.\n" ), serial.c_str(), connectedToIPAddress.c_str() ), m_ERROR_STYLE );
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
int IPConfigureFrame::ForceIP( const char* pMACAddress, const char* pNewDeviceIPAddress, const char* pStaticSubnetMask, const char* pStaticDefaultGateway, const char* pAdapterIPAddress, unsigned int timeout_ms )
//-----------------------------------------------------------------------------
{
    int status = 0;
    CHECKED_TLI_CALL_WITH_RETURN( TLIMV_ForceIP, ( pMACAddress, pNewDeviceIPAddress, pStaticSubnetMask, pStaticDefaultGateway, pAdapterIPAddress, timeout_ms ), WriteLogMessage )
}

//-----------------------------------------------------------------------------
std::string IPConfigureFrame::GetDeviceInterfaceStringInfo( MVTLI_INTERFACE_HANDLE hInterface, const std::string& deviceName, unsigned int interfaceIndex, DEVICE_INFO_CMD info )
//-----------------------------------------------------------------------------
{
    // In case the interface handle does not belong to a GEV interface the function should neither do nor output anything
    if( strncmp( ConvertedString( GetInterfaceStringInfo( hInterface, INTERFACE_INFO_TLTYPE ) ).mb_str(), "GEV", 3 ) )
    {
        return string( "" );
    }

    if( !m_pTLIMV_IFGetDeviceInterfaceInfo )
    {
        WriteLogMessage( wxT( "TLIMV_IFGetDeviceInterfaceInfo is not available.\n" ), m_ERROR_STYLE );
        return string( "" );
    }

    size_t stringSize = 0;
    INFO_DATATYPE dataType = INFO_DATATYPE_UNKNOWN;
    int result = m_pTLIMV_IFGetDeviceInterfaceInfo( hInterface, deviceName.c_str(), interfaceIndex, info, &dataType, 0, &stringSize );
    if( result != 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR during call to TLIMV_IFGetDeviceInterfaceInfo( %p, %s, %d, %d, %d, 0, %p ): %d.\n" ), hInterface, deviceName.c_str(), interfaceIndex, info, dataType, reinterpret_cast<void*>( &stringSize ), result ), m_ERROR_STYLE );
        return string( "" );
    }
    auto_array_ptr<char> pStringBuffer( stringSize );
    dataType = INFO_DATATYPE_UNKNOWN;
    result = m_pTLIMV_IFGetDeviceInterfaceInfo( hInterface, deviceName.c_str(), interfaceIndex, info, &dataType, pStringBuffer.get(), &stringSize );
    if( result != 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR during call to TLIMV_IFGetDeviceInterfaceInfo( %p, %s, %d, %d, %d, %p, %p ): %d.\n" ), hInterface, deviceName.c_str(), interfaceIndex, info, dataType, pStringBuffer.get(), reinterpret_cast<void*>( &stringSize ), result ), m_ERROR_STYLE );
        return string( "" );
    }
    return string( pStringBuffer.get() );
}

//-----------------------------------------------------------------------------
string IPConfigureFrame::GetDeviceStringInfo( MVTLI_INTERFACE_HANDLE hInterface, const string& deviceName, DEVICE_INFO_CMD info )
//-----------------------------------------------------------------------------
{
    // In case the interface handle does not belong to a GEV interface the function should neither do nor output anything
    if( strncmp( ConvertedString( GetInterfaceStringInfo( hInterface, INTERFACE_INFO_TLTYPE ) ).mb_str(), "GEV", 3 ) )
    {
        return string( "" );
    }

    if( !m_pIFGetDeviceInfo )
    {
        WriteLogMessage( wxT( "IFGetDeviceInfo is not available.\n" ), m_ERROR_STYLE );
        return string( "" );
    }

    size_t stringSize = 0;
    INFO_DATATYPE dataType = INFO_DATATYPE_UNKNOWN;
    int result = m_pIFGetDeviceInfo( hInterface, deviceName.c_str(), info, &dataType, 0, &stringSize );
    if( result != 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR during call to IFGetDeviceInfo( %p, %s, %d, %d, 0, %p ): %d.\n" ), hInterface, deviceName.c_str(), info, dataType, reinterpret_cast<void*>( &stringSize ), result ), m_ERROR_STYLE );
        return string( "" );
    }
    auto_array_ptr<char> pStringBuffer( stringSize );
    dataType = INFO_DATATYPE_UNKNOWN;
    result = m_pIFGetDeviceInfo( hInterface, deviceName.c_str(), info, &dataType, pStringBuffer.get(), &stringSize );
    if( result != 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR during call to IFGetDeviceInfo( %p, %s, %d, %d, %p, %p ): %d.\n" ), hInterface, deviceName.c_str(), info, dataType, pStringBuffer.get(), reinterpret_cast<void*>( &stringSize ), result ), m_ERROR_STYLE );
        return string( "" );
    }
    return string( pStringBuffer.get() );
}

//-----------------------------------------------------------------------------
std::string IPConfigureFrame::GetInterfaceStringInfo( MVTLI_INTERFACE_HANDLE hInterface, INTERFACE_INFO_CMD info )
//-----------------------------------------------------------------------------
{
    // In case the interface handle does not belong to a GEV interface the function should neither do nor output anything
    if( ( info != INTERFACE_INFO_TLTYPE ) &&
        ( GetInterfaceStringInfo( hInterface, INTERFACE_INFO_TLTYPE ) != std::string( "GEV" ) ) )
    {
        return string( "" );
    }

    if( !m_pIFGetInfo )
    {
        WriteLogMessage( wxT( "IFGetInfo is not available.\n" ), m_ERROR_STYLE );
        return string( "" );
    }

    size_t stringSize = 0;
    INFO_DATATYPE dataType = INFO_DATATYPE_UNKNOWN;
    int result = m_pIFGetInfo( hInterface, info, &dataType, 0, &stringSize );
    if( result != 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR during call to IFGetInfo( %p, %d, %d, 0, %p ): %d.\n" ), hInterface, info, dataType, reinterpret_cast<void*>( &stringSize ), result ), m_ERROR_STYLE );
        return string( "" );
    }
    auto_array_ptr<char> pStringBuffer( stringSize );
    dataType = INFO_DATATYPE_UNKNOWN;
    result = m_pIFGetInfo( hInterface, info, &dataType, pStringBuffer.get(), &stringSize );
    if( result != 0 )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR during call to IFGetInfo( %p, %d, %d, %p, %p ): %d.\n" ), hInterface, info, dataType, pStringBuffer.get(), reinterpret_cast<void*>( &stringSize ), result ), m_ERROR_STYLE );
        return string( "" );
    }
    return string( pStringBuffer.get() );
}

//-----------------------------------------------------------------------------
std::string IPConfigureFrame::GetStreamID( MVTLI_DEVICE_HANDLE hDev, unsigned int streamChannelIndex ) const
//-----------------------------------------------------------------------------
{
    size_t stringSize = 0;
    int status = 0;
    LOGGED_TLI_CALL( DevGetDataStreamID, ( hDev, streamChannelIndex, 0, &stringSize ), WriteLogMessage )
    if( status == 0 )
    {
        auto_array_ptr<char> pStringBuffer( stringSize );
        LOGGED_TLI_CALL( DevGetDataStreamID, ( hDev, streamChannelIndex, pStringBuffer.get(), &stringSize ), WriteLogMessage )
        if( status == 0 )
        {
            return std::string( pStringBuffer.get() );
        }
    }
    return "";
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::HandleMisconfiguredDevices( void )
//-----------------------------------------------------------------------------
{
    if( GetMisconfiguredDevicesDetected() == false )
    {
        return;
    }

    bool boDoAutoConfigureDetectedMisconfiguredDevices = false;
    bool boDoConfigurePersistentIP = false;

    if( ( m_boForceAutoConfigurePersistentIP || m_boForceAutoConfigureDHCPIP ) == true )
    {
        boDoAutoConfigureDetectedMisconfiguredDevices = true;
    }
    else if( m_boMustQuit == false )
    {
        MessageDialogPopUpWithCheckbox autoFixDlg( this,
                wxID_ANY,
                wxT( "Auto-fix Invalid Network Configurations?" ),
                wxT( "At least one device with an invalid network configuration\nhas been detected.\nShould mvIPConfigure attempt to automatically assign\nvalid network configurations to misconfigured devices?" ),
                wxT( "Always try to auto-fix invalid network configurations on start" ),
                wxT( "Yes" ),
                wxT( "No" ),
                m_pMISettings_AutoFixNetworkingIssues->IsChecked() );
        if( autoFixDlg.ShowModal() == wxID_YES )
        {
            boDoAutoConfigureDetectedMisconfiguredDevices = true;
            wxMessageDialog persistentOrDHCPDialog( this, wxT( "Should the devices get a valid persistent IP or should they\nget their IP from a DHCP server?" ), wxT( "Persistent IP or DHCP?" ), wxNO_DEFAULT | wxYES_NO | wxCANCEL | wxICON_INFORMATION );
            persistentOrDHCPDialog.SetYesNoCancelLabels( wxMessageDialogBase::ButtonLabel( wxT( "Persistent IP" ) ), wxMessageDialogBase::ButtonLabel( wxT( "DHCP IP" ) ), wxMessageDialogBase::ButtonLabel( wxT( "Cancel" ) ) );
            const int choice = persistentOrDHCPDialog.ShowModal();
            if( choice == wxID_YES )
            {
                boDoConfigurePersistentIP = true;
            }
            else if( choice == wxID_CANCEL )
            {
                boDoAutoConfigureDetectedMisconfiguredDevices = false;
                wxMessageBox( wxT( "You may at any time assign valid network configurations\nto individual devices by right-clicking them on the list." ), wxT( "Auto-fixing Invalid Network Configurations" ), wxOK | wxICON_INFORMATION, this );
            }
        }
        else
        {
            wxMessageBox( wxT( "You may at any time assign valid network configurations\nto individual devices by right-clicking them on the list." ), wxT( "Auto-fixing Invalid Network Configurations" ), wxOK | wxICON_INFORMATION, this );
        }
        m_pMISettings_AutoFixNetworkingIssues->Check( autoFixDlg.IsCheckboxChecked() );
    }

    if( boDoAutoConfigureDetectedMisconfiguredDevices == true )
    {
        wxBusyCursor busyCursorScope;
        WriteLogMessage( wxT( "Trying to fix issues on detected devices with an invalid network configuration...\n" ) );
        wxListItem info;
        info.m_col = lcProduct;
        info.m_mask = wxLIST_MASK_IMAGE | wxLIST_MASK_TEXT;
        for( int i = 0; i < m_pDevListCtrl->GetItemCount(); i++ )
        {
            info.m_itemId = i;
            m_pDevListCtrl->GetItem( info );
            if( info.m_image == 0 )
            {
                m_pDevListCtrl->SetCurrentItemIndex( i );
                OnListItemSelected( i );

                const std::string productName = std::string( info.m_text.mb_str() );
                const std::string connectedToAdapterIP = std::string( m_pCBConnectedToIPAddress->GetValue().mb_str() );
                const std::string serial = std::string( m_pSTSerialNumber->GetLabel().mb_str() );
                // In case 'forceAutoConfigure*' or 'fac*' command-line parameters have been used, valid network configurations
                // should only be configured for devices connected to adapters matching the respective wildcard masks.
                if( m_boForceAutoConfigurePersistentIP )
                {
                    if( match( connectedToAdapterIP, m_ForceAutoConfigureAdapterPersistentIPAddress, '*' ) != 0 )
                    {
                        WriteLogMessage( wxString::Format( wxT( "Ignoring device %s(%s) for persistent IP configuration(adapter IP: %s wildcard IP mask: %s)\n" ), serial.c_str(), productName.c_str(), connectedToAdapterIP.c_str(), m_ForceAutoConfigureAdapterPersistentIPAddress.c_str() ) );
                    }
                    else
                    {
                        WriteLogMessage( wxString::Format( wxT( "Trying to set up device %s(%s) on adapter %s with a persistent IP configuration... \n" ), serial.c_str(), productName.c_str(), connectedToAdapterIP.c_str() ) );
                        AutoAssignPersistentIPOrDHCP( i, true );
                    }
                }
                if( m_boForceAutoConfigureDHCPIP )
                {
                    if( match( connectedToAdapterIP, m_ForceAutoConfigureAdapterDHCPIPAddress, '*' ) != 0 )
                    {
                        WriteLogMessage( wxString::Format( wxT( "Ignoring device %s(%s) for DHCP IP configuration(adapter IP: %s wildcard IP mask: %s)\n" ), serial.c_str(), productName.c_str(), connectedToAdapterIP.c_str(), m_ForceAutoConfigureAdapterDHCPIPAddress.c_str() ) );
                    }
                    else
                    {
                        WriteLogMessage( wxString::Format( wxT( "Trying to set up device %s(%s) on adapter %s with a DHCP IP configuration... \n" ), serial.c_str(), productName.c_str(), connectedToAdapterIP.c_str() ) );
                        AutoAssignPersistentIPOrDHCP( i, false );
                    }
                }
                if( !m_boForceAutoConfigurePersistentIP && !m_boForceAutoConfigureDHCPIP )
                {
                    AutoAssignPersistentIPOrDHCP( i, boDoConfigurePersistentIP );
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
bool IPConfigureFrame::DoSubnetsMatch( const std::string& adapterIPAddress, const std::string& adapterSubnetMask, const std::string& deviceIPAddress, const std::string& deviceSubnetMask ) const
//-----------------------------------------------------------------------------
{
    const unsigned long currentAdapterSubnet = GetIPv4AddressFromString( adapterIPAddress.c_str() ) & GetIPv4AddressFromString( adapterSubnetMask.c_str() );
    const unsigned long currentDeviceSubnet = GetIPv4AddressFromString( deviceIPAddress.c_str() ) & GetIPv4AddressFromString( deviceSubnetMask.c_str() );
    return currentAdapterSubnet == currentDeviceSubnet;
}

//-----------------------------------------------------------------------------
bool IPConfigureFrame::HasBeenDiscoveredViaUnicast( const std::string& ip ) const
//-----------------------------------------------------------------------------
{
    return find( m_unicastDestinationIPs.begin(), m_unicastDestinationIPs.end(), ip ) != m_unicastDestinationIPs.end();
}

//-----------------------------------------------------------------------------
int IPConfigureFrame::IsValidIPv4Address( const char* pData ) const
//-----------------------------------------------------------------------------
{
    int status = 0;
    CHECKED_TLI_CALL_WITH_RETURN( TLIMV_IsValidIPv4Address, ( pData ), WriteLogMessage )
}

//-----------------------------------------------------------------------------
int IPConfigureFrame::MACFromSerial( const char* pSerial, char* pBuf, size_t* pBufSize ) const
//-----------------------------------------------------------------------------
{
    int status = 0;
    CHECKED_TLI_CALL_WITH_RETURN( TLIMV_MACFromSerial, ( pSerial, pBuf, pBufSize ), WriteLogMessage )
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnAction_ManuallyAssignTemporaryIP( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    ManuallyAssignTemporaryIP( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnAction_AutoAssignPersistentIP( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    AutoAssignPersistentIPOrDHCP( m_pDevListCtrl->GetCurrentItemIndex(), true );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnAction_AutoAssignDHCPIP( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    AutoAssignPersistentIPOrDHCP( m_pDevListCtrl->GetCurrentItemIndex(), false );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnAction_AutoAssignTemporaryIP( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    AutoAssignTemporaryIP( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnAction_ViewPotentialPerformanceIssues( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    ViewPotentialPerformanceIssues( m_pDevListCtrl->GetCurrentItemIndex() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnAction_SetupUnicasts( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    ShutdownMVIMPACTAcquireCAPI();
    {
        SetupUnicastDlg dlg( this );
        dlg.ShowModal();
        CollectUnicastDeviceDiscoveryData();
    }
    InitMVIMPACTAcquireCAPI();
    UpdateDeviceList();
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnBtnApplyChanges( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    int currentItem = m_pDevListCtrl->GetCurrentItemIndex();
    if( currentItem < 0 )
    {
        WriteLogMessage( wxT( "ERROR: No device selected.\n" ), m_ERROR_STYLE );
        return;
    }

    wxString itemText( m_pDevListCtrl->GetItemText( currentItem ) );
    wxListItem info;
    info.m_itemId = currentItem;
    info.m_col = lcSerial;
    info.m_mask = wxLIST_MASK_TEXT;
    if( !m_pDevListCtrl->GetItem( info ) )
    {
        WriteLogMessage( wxString::Format( wxT( "ERROR: Could not obtain serial number for device %s.\n" ), itemText.c_str() ), m_ERROR_STYLE );
        return;
    }

    ApplyChanges( info.m_text, itemText, m_pCBConnectedToIPAddress->GetValue(), m_pTCUserDefinedName->GetValue() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnCBUseDHCP( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].DHCPEnabled_ = m_pCBUseDHCP->GetValue();
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnCBUsePersistentIP( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].persistentIPEnabled_ = m_pCBUsePersistentIP->GetValue();
    m_pTCPersistentIPAddress->Enable( m_pCBUsePersistentIP->GetValue() );
    m_pTCPersistentSubnetMask->Enable( m_pCBUsePersistentIP->GetValue() );
    m_pTCPersistentDefaultGateway->Enable( m_pCBUsePersistentIP->GetValue() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnClose( wxCloseEvent& )
//-----------------------------------------------------------------------------
{
    Deinit();
    Destroy();
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnConnectedToIPAddressTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    int currentItem = m_pDevListCtrl->GetCurrentItemIndex();
    wxListItem info;
    if( currentItem >= 0 )
    {
        info.m_itemId = currentItem;
        info.m_col = lcSerial;
        info.m_mask = wxLIST_MASK_TEXT;
        if( !m_pDevListCtrl->GetItem( info ) )
        {
            return;
        }
    }
    SetupNetworkGUIElements( m_devices.find( string( info.m_text.mb_str() ) ), m_pSCInterfaceSelector->GetValue() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnHelp_About( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    AboutDialogInformation info( GetIcon(), this );
    info.applicationName_ = wxT( "mvIPConfigure" );
    info.briefDescription_ = wxT( "Configuration tool for network related settings of GigE Vision(tm) devices" );
    info.yearOfInitialRelease_ = 2008;
    info.boAddIconInfo_ = true;

    info.usageHints_.push_back( make_pair( wxT( "mvIPConfigure device=GX000066 usePersistentIP=0;1 persistentIPAddress=0;172.111.2.1 persistentSubnetMask=0;255.255.255.0 persistentDefaultGateway=0;172.111.2.2 quit\n\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvIPConfigure forceAutoConfigurePersistent=192.168.* quit (will automatically enable persistent IPv4 address acquisition for all devices with an incorrect network setup on Ethernet adapters with an IP address matching the value after the '=' character and will terminate the application automatically afterwards)\n\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "mvIPConfigure facd=192.168.1.* (will automatically enable DHCP address acquisition for all devices with an incorrect network setup on Ethernet adapters with an IP address matching the value after the '=' character)\n\n" ), wxTextAttr( *wxBLACK ) ) );
    info.usageHints_.push_back( make_pair( wxT( "Both 'force*' parameters will only apply to misconfigured devices utilizing a persistent IP address and the 'Auto-fix Invalid Network Configurations' option from 'Settings' menu is active.\n" ), wxTextAttr( *wxBLACK ) ) );

    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+D" ), wxT( "Auto-Assign DHCP IPv4 Address" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+M" ), wxT( "Manually Assign Temporary IPv4 Address" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+P" ), wxT( "Auto-Assign Persistent IPv4 Address" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+T" ), wxT( "Auto-Assign Temporary IPv4 Address" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "CTRL+V" ), wxT( "View Potential Performance Issues" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "ALT+X" ), wxT( "Exit" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "F1" ), wxT( "Display 'About' dialog" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "F5" ), wxT( "Update device list" ) ) );
    info.keyboardShortcuts_.push_back( make_pair( wxT( "F12" ), wxT( "Online Documentation" ) ) );

    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'device' or 'd'" ), wxT( "Selects a serial number of a device for configuration" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'forceAutoConfigureDHCP' or 'facd'" ), wxT( "Configures devices using misconfigured IP addresses connected to the adapter specified by a wildcard to use DHCP mode. If no wildcard is specified every detected misconfigured device will be configured to use DHCP mode. (syntax: <Adapter IP address wildcard>)" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'forceAutoConfigurePersistent' or 'facp'" ), wxT( "Assigns automatically a persistent IP address to devices connected to the adapter specified by a wildcard. If no wildcard is specified every misconfigured device will set up for a persistant IP address. (syntax: <Adapter IP address wildcard>)" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'hidden'" ), wxT( "Will start the application without showing the splash screen or the application window. This might be useful when doing a silent configuration in combination with the 'quit' parameter" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'log_file' or 'lf'" ), wxT( "Specifies a log file storing the content of this text control upon application shutdown" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'persistentIPAddress'" ), wxT( "Defines a persistent IP address for the device currently selected(value syntax: <interface index>;<value>)" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'persistentSubnetMask'" ), wxT( "Defines a persistent subnet mask for the device currently selected(value syntax: <interface index>;<value>)" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'persistentDefaultGateway'" ), wxT( "Defines a persistent default gateway for the device currently selected(value syntax: <interface index>;<value>)" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'quit' or 'q'" ), wxT( "Terminates the application automatically after all the configuration has been applied" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'useDHCP'" ), wxT( "Enables/disables the usage of DHCP for the device currently selected(value syntax: <interface index>;<value>)" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'userDefinedName' or 'udn'" ), wxT( "Sets a user defined name for the device currently selected" ) ) );
    info.availableCommandLineOptions_.push_back( make_pair( wxT( "'usePersistentIP'" ), wxT( "Enables/disables the usage of a persistent IP address for the device currently selected(value syntax: <interface index>;<value>)" ) ) );

    DisplayCommonAboutDialog( info );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnInterfaceSelectorTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    WriteLogMessage( wxString::Format( wxT( "Interface %d selected\n" ), m_pSCInterfaceSelector->GetValue() ) );
    UpdateDlgControls( true );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnListItemSelected( int /*listItemIndex*/ )
//-----------------------------------------------------------------------------
{
    m_pCBConnectedToIPAddress->Clear();
    UpdateDlgControls( false );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnPersistentGatewayTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pTCPersistentDefaultGateway )
    {
        m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].persistentDefaultGateway_ = m_pTCPersistentDefaultGateway->GetValue().mb_str();
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnPersistentIPTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pTCPersistentIPAddress )
    {
        AutoFillFromIP( m_pTCPersistentIPAddress, m_pTCPersistentSubnetMask, m_pTCPersistentDefaultGateway );
        m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].persistentIPAddress_ = m_pTCPersistentIPAddress->GetValue().mb_str();
        m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].persistentSubnetMask_ = m_pTCPersistentSubnetMask->GetValue().mb_str();
        m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].persistentDefaultGateway_ = m_pTCPersistentDefaultGateway->GetValue().mb_str();
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnPersistentNetmaskTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( m_pTCPersistentSubnetMask )
    {
        m_interfaceInfo[m_pSCInterfaceSelector->GetValue()].persistentSubnetMask_ = m_pTCPersistentSubnetMask->GetValue().mb_str();
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::OnTimer( wxTimerEvent& e )
//-----------------------------------------------------------------------------
{
    switch( e.GetId() )
    {
    case tePostGUIInitialization:
        if( m_pMISettings_ShowWarningOnIgnoredInterfaces->IsChecked() &&
            m_boAtLeastOneInterfaceIsIgnored )
        {
            WarnAboutIgnoredInterfaces();
            wxMilliSleep( 500 );
        }
        if( m_pMISettings_AutoFixNetworkingIssues->IsChecked() )
        {
            HandleMisconfiguredDevices();
        }
        SetMisconfiguredDevicesDetected( false );
        if( m_boMustQuit )
        {
            Close( true );
        }
        break;
    default:
        break;
    }
    e.Skip();
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::InitMVIMPACTAcquireCAPI( void )
//-----------------------------------------------------------------------------
{
    int status = 0;
    LOGGED_TLI_CALL( GCInitLib, ( ), WriteLogMessage )
    LOGGED_TLI_CALL( TLOpen, ( &m_hTLI ), WriteLogMessage )
}

//-----------------------------------------------------------------------------
bool IPConfigureFrame::IsAtLeastOneInterfaceIgnored( void )
//-----------------------------------------------------------------------------
{
    wxString filePath;
    if( !wxGetEnv( wxT( "MVIMPACT_ACQUIRE_DATA_DIR" ), &filePath ) )
    {
        return false;
    }

    if( filePath.IsEmpty() )
    {
        return false;
    }

    filePath.Append( wxString::Format( wxT( "/%sogs/ignoredInterfaces.txt" ), ( wxPlatformInfo().GetOperatingSystemId() & wxOS_WINDOWS ) ? wxT( "L" ) : wxT( "l" ) ) );
    if( !wxFile::Exists( filePath ) )
    {
        return false;
    }

    wxFile file( filePath, wxFile::read );
    wxString data;
    if( !file.ReadAll( &data ) )
    {
        return false;
    }

    // Analyze the file
    wxArrayString lines = ::wxSplit( data, wxT( '\n' ) );
    for( wxArrayString::size_type i = 0; i < lines.size(); i++ )
    {
        lines[i].Trim( false );
        if( lines[i].IsEmpty() || ( lines[i].First( wxT( '#' ) ) == 0 ) )
        {
            continue;
        }

        wxArrayString tokens = ::wxSplit( lines[i], wxT( '=' ) );
        if( tokens.size() == 2 )
        {
            tokens[0].Trim( true );
            tokens[0].Trim( false );
            tokens[1].Trim( true );
            tokens[1].Trim( false );
            if( tokens[0] == wxT( "GEV" ) && tokens[1] == wxT( "I" ) )
            {
                return true;
            }
            if( ( tokens[0].size() == 17 ) &&
                ( count( tokens[0].begin(), tokens[0].end(), wxT( ':' ) ) == 5 ) &&
                ( tokens[1] == wxT( "I" ) ) )
            {
                if( tokens[0].find_first_not_of( wxT( "0123456789:abcdefABCDEF" ) ) != string::npos )
                {
                    continue;
                }
                if( ( tokens[0][2] ) != wxT( ':' ) ||
                    ( tokens[0][5] ) != wxT( ':' ) ||
                    ( tokens[0][8] ) != wxT( ':' ) ||
                    ( tokens[0][11] ) != wxT( ':' ) ||
                    ( tokens[0][14] ) != wxT( ':' ) )
                {
                    continue;
                }
                return true;
            }
        }
    }
    return false;
}


//-----------------------------------------------------------------------------
void IPConfigureFrame::RebootDeviceAndWaitForItToReappear( const std::string& deviceSerial )
//-----------------------------------------------------------------------------
{
    ShutdownMVIMPACTAcquireCAPI();
    {
        mvIMPACT::acquire::DeviceManager devMgr;
        devMgr.updateDeviceList();
        const unsigned int devCnt = devMgr.deviceCount();
        for( unsigned int i = 0; i < devCnt; i++ )
        {
            Device* pDev = devMgr.getDevice( i );
            if( !pDev )
            {
                continue;
            }

            if( deviceSerial.compare( pDev->serial.readS() ) != 0 )
            {
                continue;
            }
            if( pDev->interfaceLayout.isValid() && pDev->interfaceLayout.isWriteable() )
            {
                pDev->interfaceLayout.write( dilGenICam );
            }
            else
            {
                WriteLogMessage( wxString::Format( wxT( "Device %s has no 'interfaceLayout' property, cannot proceed with deviceReset. Please Reset the device manually...\n" ), deviceSerial.c_str() ) );
                if( !( m_boForceAutoConfigurePersistentIP || m_boForceAutoConfigureDHCPIP ) )
                {
                    wxMessageDialog errDlg( GetParent(), wxT( "The device has no 'interfaceLayout' property, so it cannot be reset by this tool.\nPlease do a manual restart, in order to get a valid DHCP address for this device." ), wxString::Format( wxT( "Restart of %s required!" ), deviceSerial.c_str() ), wxYES | wxICON_INFORMATION );
                    errDlg.ShowModal();
                }
            }

            try
            {
                WriteLogMessage( wxString::Format( wxT( "Resetting device %s...\n" ), deviceSerial.c_str() ) );
                pDev->open();
                GenICam::DeviceControl devCtrl( pDev );
                devCtrl.deviceReset.call();

                ComponentLocator locator( pDev->hDev() );
                PropertyI state;
                locator.bindComponent( state, "State" );
                if( !state.isValid() )
                {
                    continue;
                }

                WriteLogMessage( wxString::Format( wxT( "Waiting up to one minute for device %s to reappear...\n" ), deviceSerial.c_str() ) );
                bool boTerminatePolling = false;
                for( unsigned int t = 0; t < 12; t++ )
                {
                    if( boTerminatePolling == true )
                    {
                        break;
                    }
                    wxSleep( 5 );
                    if( state.read() == dsPresent )
                    {
                        boTerminatePolling = true;
                    }
                }

                if( boTerminatePolling == false )
                {
                    WriteLogMessage( wxString::Format( wxT( "Device %s did not reappear within 60 seconds after reset...\n" ), deviceSerial.c_str() ) );
                }
                else
                {
                    WriteLogMessage( wxString::Format( wxT( "Device %s reappeared!\n" ), deviceSerial.c_str() ) );
                }
                pDev->close();
            }
            catch( const ImpactAcquireException& ex )
            {
                WriteLogMessage( wxString::Format( wxT( "An exception occurred when trying to reset device %s! Error message:%s\n" ), deviceSerial.c_str(), ex.getErrorString().c_str() ) );
            }
        }
    }
    InitMVIMPACTAcquireCAPI();
}

//-----------------------------------------------------------------------------
template<typename _Ty>
_Ty IPConfigureFrame::ResolveSymbol( const wxDynamicLibrary& lib, const wxString& name, const wxString& deprecatedName /* = wxEmptyString */ )
//-----------------------------------------------------------------------------
{
    function_cast<_Ty> pFunc;
    pFunc.pI = lib.GetSymbol( name );
    if( !pFunc.pI )
    {
        if( !deprecatedName.IsEmpty() )
        {
            pFunc.pI = lib.GetSymbol( deprecatedName );
        }
        if( !pFunc.pI )
        {
            string functionName( __FUNCTION__ );
            WriteLogMessage( wxString::Format( wxT( "%s: Exported symbol '%s' could not be resolved/extracted from GenTL producer.\n" ), ConvertedString( functionName ).c_str(), name.c_str() ), wxColour( 255, 0, 0 ) );
        }
    }
    return pFunc.pO;
}

//-----------------------------------------------------------------------------
bool IPConfigureFrame::SelectDevice( const wxString& deviceToConfigure )
//-----------------------------------------------------------------------------
{
    if( !deviceToConfigure.IsEmpty() )
    {
        const int cnt = m_pDevListCtrl->GetItemCount();
        for( int i = 0; i < cnt; i++ )
        {
            wxListItem info;
            info.m_itemId = i;
            info.m_col = lcSerial;
            info.m_mask = wxLIST_MASK_TEXT;
            if( m_pDevListCtrl->GetItem( info ) )
            {
                if( info.m_text == deviceToConfigure )
                {
                    m_pDevListCtrl->SetCurrentItemIndex( i );
                    UpdateDlgControls( true );
                    return true;
                }
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::SetupNetworkGUIElements( DeviceMap::const_iterator it, const int interfaceIndex )
//-----------------------------------------------------------------------------
{
    bool boDeviceValid = ( it != m_devices.end() );
    m_boMarkNetmaskConflict = false;
    m_boMarkIPAddressConflict = false;

    if( boDeviceValid )
    {
        m_pSTConnectedToNetmask->SetLabel( ConvertedString( it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].second.netMask_.c_str() ) );
        m_pSTConnectedToMTU->SetLabel( wxString::Format( wxT( "%d" ), it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].second.MTU_ ) );
        m_pSTConnectedToLinkSpeed->SetLabel( wxString::Format( wxT( "%d" ), it->second->adapters_[m_pCBConnectedToIPAddress->GetSelection()].second.linkSpeed_ ) );
        if( interfaceIndex == 0 )
        {
            m_boMarkNetmaskConflict = ( string( m_pSTConnectedToNetmask->GetLabel().mb_str() ) != it->second->interfaceInfo_[0].currentSubnetMask_ );
            m_boMarkIPAddressConflict = m_pTLIMV_DoAddressesMatch ? ( m_pTLIMV_DoAddressesMatch( m_pCBConnectedToIPAddress->GetValue().mb_str(), m_pSTConnectedToNetmask->GetLabel().mb_str(), it->second->interfaceInfo_[0].currentIPAddress_.c_str(), it->second->interfaceInfo_[0].currentSubnetMask_.c_str() ) != 0 ) : true;
            m_boMarkIPAddressConflict = ( m_boMarkIPAddressConflict && !it->second->boHasBeenDiscoveredViaUnicast_ ) ? true : false;
        }
    }
    else
    {
        m_pSTConnectedToNetmask->SetLabel( wxT( "-" ) );
        m_pSTConnectedToMTU->SetLabel( wxT( "-" ) );
        m_pSTConnectedToLinkSpeed->SetLabel( wxT( "-" ) );
    }

    const wxColour defaultColour( m_pSTMACAddress->GetBackgroundColour() );
    m_pSTCurrentIPAddress->SetBackgroundColour( m_boMarkIPAddressConflict ? m_RED_PASTEL : defaultColour );
    m_pSTCurrentSubnetMask->SetBackgroundColour( m_boMarkNetmaskConflict ? m_RED_PASTEL : defaultColour );
    m_pCBConnectedToIPAddress->SetBackgroundColour( m_boMarkIPAddressConflict ? m_RED_PASTEL : *wxWHITE );
    m_pSTConnectedToNetmask->SetBackgroundColour( m_boMarkNetmaskConflict ? m_RED_PASTEL : defaultColour );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::SetupPersistentIPGUIElement( wxTextCtrl* pTC, const string& value, const bool boPersistentIPControlsAvailable, const bool boEnablePersistentIPControls ) const
//-----------------------------------------------------------------------------
{
    pTC->ChangeValue( ConvertedString( boPersistentIPControlsAvailable ? value.c_str() : "-" ) );
    pTC->Enable( boEnablePersistentIPControls );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::ShutdownMVIMPACTAcquireCAPI( void )
//-----------------------------------------------------------------------------
{
    InterfaceContainer::iterator it = m_TLIInterfaces.begin();
    InterfaceContainer::iterator itEnd = m_TLIInterfaces.end();
    int status = 0;
    while( it != itEnd )
    {
        LOGGED_TLI_CALL( IFClose, ( it->second ), WriteLogMessage )
        ++it;
    }
    m_TLIInterfaces.clear();
    LOGGED_TLI_CALL( TLClose, ( m_hTLI ), WriteLogMessage )
    LOGGED_TLI_CALL( GCCloseLib, ( ), WriteLogMessage )
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::UpdateDeviceList( bool boBuildList /* = true*/, TUpdateDeviceListVerboseLevel verboseLevel /* = tudlvlAll*/ )
//-----------------------------------------------------------------------------
{
    wxBusyCursor busyCursorScope;
    // The next command yields control to pending messages in the windowing system. It is necessary in order
    // to get a busy cursor in Linux systems when choosing menu Item Update Device List via mouse.
    wxYield();
    m_pLogWindow->SetInsertionPointEnd();
    if( verboseLevel != tudlvlSilent )
    {
        WriteLogMessage( wxString( wxT( "Updating device list...\n" ) ) );
    }
    int status = 0;
    char hasChanged = 0;
    LOGGED_TLI_CALL( TLUpdateInterfaceList, ( m_hTLI, &hasChanged, 0 ), WriteLogMessage );
    unsigned int interfaceCnt = 0;
    LOGGED_TLI_CALL( TLGetNumInterfaces, ( m_hTLI, &interfaceCnt ), WriteLogMessage );
    if( status != 0 )
    {
        return;
    }

    if( interfaceCnt < 1 )
    {
        WriteLogMessage( wxT( "No interfaces detected.\n" ), m_ERROR_STYLE );
        return;
    }

    if( verboseLevel != tudlvlSilent )
    {
        WriteLogMessage( wxString::Format( wxT( "%d interface%s detected.\n" ), interfaceCnt, ( interfaceCnt > 1 ) ? wxT( "s" ) : wxT( "" ) ) );
    }

    InterfaceContainer lastInterfaceList( m_TLIInterfaces ); // after this loop, this list will contain all the interfaces, that have disappeared...
    for( unsigned int i = 0; i < interfaceCnt; i++ )
    {
        size_t stringSize = 0;
        LOGGED_TLI_CALL_WITH_CONTINUE( TLGetInterfaceID, ( m_hTLI, i, 0, &stringSize ), WriteLogMessage )
        auto_array_ptr<char> pStringBuffer( stringSize );
        LOGGED_TLI_CALL_WITH_CONTINUE( TLGetInterfaceID, ( m_hTLI, i, pStringBuffer.get(), &stringSize ), WriteLogMessage )
        if( m_TLIInterfaces.find( string( pStringBuffer.get() ) ) == m_TLIInterfaces.end() )
        {
            // this is a new interface
            MVTLI_INTERFACE_HANDLE hInterface = 0;
            LOGGED_TLI_CALL_WITH_CONTINUE( TLOpenInterface, ( m_hTLI, pStringBuffer.get(), &hInterface ), WriteLogMessage )
            m_TLIInterfaces.insert( make_pair( string( pStringBuffer.get() ), hInterface ) );
        }
        else
        {
            // this interface should still be there
            InterfaceContainer::iterator it = lastInterfaceList.find( string( pStringBuffer.get() ) );
            assert( ( it != lastInterfaceList.end() ) && "BUG detected in interface handling. If this interface is missing in the list of interfaces detected last time there is a bug in the implementation" );
            if( it != lastInterfaceList.end() )
            {
                lastInterfaceList.erase( it );
            }
        }
    }
    InterfaceContainer::iterator itInterface = m_TLIInterfaces.begin();
    InterfaceContainer::iterator itInterfaceEND = m_TLIInterfaces.end();
    while( itInterface != itInterfaceEND )
    {
        InterfaceContainer::iterator itLast = lastInterfaceList.find( itInterface->first );
        if( itLast != lastInterfaceList.end() )
        {
            // this interface is gone now...
            /// \todo close it?
            m_TLIInterfaces.erase( itInterface );
            itInterface = m_TLIInterfaces.begin();
        }
        else
        {
            ++itInterface;
        }
    }

    for_each( m_devices.begin(), m_devices.end(), ptr_fun( DeleteSecond<const string, DetectedDeviceInfo*> ) );
    m_devices.clear();
    InterfaceContainer::iterator itInterfaces = m_TLIInterfaces.begin();
    InterfaceContainer::iterator itInterfacesEnd = m_TLIInterfaces.end();
    map<unsigned int, set<string> > detectedNets;

    while( itInterfaces != itInterfacesEnd )
    {
        const wxString interfaceTLType( ConvertedString( GetInterfaceStringInfo( itInterfaces->second, INTERFACE_INFO_TLTYPE ) ) );
        if( interfaceTLType == s_m_technologyIdentifier )
        {
            unsigned int deviceDiscoveryMode = m_pMISettings_UseAdvancedDeviceDiscovery->IsChecked() ? 1 : 0;
            size_t deviceDiscoveryModeSize = sizeof( deviceDiscoveryMode );
            LOGGED_TLI_CALL( TLIMV_IFSetInterfaceParam, ( itInterfaces->second, INTERFACE_INFO_ADVANCED_DEVICE_DISCOVERY_MODE, 0, &deviceDiscoveryMode, deviceDiscoveryModeSize ), WriteLogMessage );
            // each device is supposed to answer within 1 second
            hasChanged = 0;
            LOGGED_TLI_CALL( IFUpdateDeviceList, ( itInterfaces->second, &hasChanged, 1100 ), WriteLogMessage )
            unsigned int deviceCnt = 0;
            if( m_pIFGetNumDevices )
            {
                status = m_pIFGetNumDevices( itInterfaces->second, &deviceCnt );
                if( status == 0 )
                {
                    if( verboseLevel != tudlvlSilent )
                    {
                        WriteLogMessage( wxString::Format( wxT( "Interface %s reported %d device%s.\n" ), ConvertedString( itInterfaces->first ).c_str(), deviceCnt, ( deviceCnt != 1 ) ? wxT( "s" ) : wxT( "" ) ) );
                    }
                    if( m_pIFGetDeviceID && m_pIFGetDeviceInfo && m_pTLIMV_IFGetDeviceInterfaceInfo )
                    {
                        for( unsigned int i = 0; i < deviceCnt; i++ )
                        {
                            size_t stringSize = 0;
                            LOGGED_TLI_CALL_WITH_CONTINUE( IFGetDeviceID, ( itInterfaces->second, i, 0, &stringSize ), WriteLogMessage )
                            auto_array_ptr<char> pStringBuffer( stringSize );
                            LOGGED_TLI_CALL_WITH_CONTINUE( IFGetDeviceID, ( itInterfaces->second, i, pStringBuffer.get(), &stringSize ), WriteLogMessage )

                            string deviceName( pStringBuffer.get() );
                            string serial = GetDeviceStringInfo( itInterfaces->second, deviceName, DEVICE_INFO_SERIAL_NUMBER );
                            DeviceMap::iterator itDev = m_devices.find( serial );

                            unsigned int adapterMTU = numeric_limits<unsigned int>::max();
                            size_t bufferSize = sizeof( adapterMTU );
                            INFO_DATATYPE dataType = INFO_DATATYPE_UNKNOWN;
                            LOGGED_TLI_CALL( IFGetInfo, ( itInterfaces->second, INTERFACE_INFO_MTU, &dataType, &adapterMTU, &bufferSize ), WriteLogMessage )
                            unsigned int adapterLinkSpeed = 0;
                            bufferSize = sizeof( adapterLinkSpeed );
                            LOGGED_TLI_CALL( IFGetInfo, ( itInterfaces->second, INTERFACE_INFO_LINK_SPEED, &dataType, &adapterLinkSpeed, &bufferSize ), WriteLogMessage )
                            const string adapterIPAddress( GetInterfaceStringInfo( itInterfaces->second, INTERFACE_INFO_IP_STRING ) );
                            const string adapterNetmask( GetInterfaceStringInfo( itInterfaces->second, INTERFACE_INFO_NETMASK_STRING ) );

                            unsigned int interfaceIP = 0;
                            bufferSize = sizeof( interfaceIP );
                            LOGGED_TLI_CALL( IFGetInfo, ( itInterfaces->second, INTERFACE_INFO_IP, &dataType, &interfaceIP, &bufferSize ), WriteLogMessage )
                            unsigned int interfaceNetmask = 0;
                            bufferSize = sizeof( interfaceNetmask );
                            LOGGED_TLI_CALL( IFGetInfo, ( itInterfaces->second, INTERFACE_INFO_NETMASK, &dataType, &interfaceNetmask, &bufferSize ), WriteLogMessage )
                            unsigned int net = interfaceIP & interfaceNetmask;
                            if( detectedNets.find( net ) == detectedNets.end() )
                            {
                                detectedNets.insert( make_pair( net, set<string>() ) );
                            }
                            map<unsigned int, set<string> >::iterator itNets = detectedNets.find( net );
                            itNets->second.insert( itInterfaces->first );

                            if( itDev == m_devices.end() )
                            {
                                // this is the first time this device has been located in this 'Enumerate' run. It might
                                // however be found again at a different network adapter
                                const wxString deviceTLType( ConvertedString( GetDeviceStringInfo( itInterfaces->second, deviceName, DEVICE_INFO_TLTYPE ) ) );
                                if( deviceTLType != s_m_technologyIdentifier )
                                {
                                    WriteLogMessage( wxString::Format( wxT( "Device %s reports its transport layer technology as '%s' while this application supports '%s' devices only. This device therefore will be ignored here.\n" ), ConvertedString( serial ).c_str(), deviceTLType.c_str(), s_m_technologyIdentifier.c_str() ), wxTextAttr( wxColour( 0, 0, 255 ) ) );
                                    continue;
                                }
                                string model( GetDeviceStringInfo( itInterfaces->second, deviceName, DEVICE_INFO_MODEL ) );
                                string manufacturer( GetDeviceStringInfo( itInterfaces->second, deviceName, DEVICE_INFO_VENDOR ) );
                                string userDefinedName( GetDeviceStringInfo( itInterfaces->second, deviceName, DEVICE_INFO_USER_DEFINED_NAME ) );
                                unsigned int interfaceCount = 1;
                                bufferSize = sizeof( interfaceCount );
                                LOGGED_TLI_CALL( IFGetDeviceInfo, ( itInterfaces->second, deviceName.c_str(), DEVICE_INFO_INTERFACE_COUNT, &dataType, &interfaceCount, &bufferSize ), WriteLogMessage )
                                if( interfaceCount > DetectedDeviceInfo::MAX_INTERFACE_COUNT )
                                {
                                    WriteLogMessage( wxString::Format( wxT( "ERROR!!! This device claims to support %u interfaces, while the current version of the standard only allows %lu interfaces.\n" ), interfaceCount, ( long unsigned int ) DetectedDeviceInfo::MAX_INTERFACE_COUNT ), m_ERROR_STYLE );
                                    interfaceCount = 4;
                                }
                                unsigned char supportsUserDefinedName = 0;
                                bufferSize = sizeof( supportsUserDefinedName );
                                LOGGED_TLI_CALL( IFGetDeviceInfo, ( itInterfaces->second, deviceName.c_str(), DEVICE_INFO_SUPPORTS_USER_DEFINED_NAME, &dataType, &supportsUserDefinedName, &bufferSize ), WriteLogMessage )
                                DetectedDeviceInfo* p = new DetectedDeviceInfo( deviceName, serial, model, manufacturer, userDefinedName, supportsUserDefinedName, adapterIPAddress, adapterNetmask, adapterMTU, adapterLinkSpeed, interfaceCount, static_cast<long>( m_devices.size(), false ) );
                                pair<DeviceMap::iterator, bool> insertPair = m_devices.insert( make_pair( serial, p ) );
                                if( !insertPair.second )
                                {
                                    WriteLogMessage( wxT( "Internal ERROR!!! This device has been inserted already.\n" ), m_ERROR_STYLE );
                                }
                                else
                                {
                                    if( verboseLevel == tudlvlAll )
                                    {
                                        WriteLogMessage( wxString::Format( wxT( "Device %s detected on interface %s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( adapterIPAddress ).c_str() ) );
                                    }
                                    for( unsigned int j = 0; j < interfaceCount; j++ )
                                    {
                                        InterfaceInfo& info = insertPair.first->second->interfaceInfo_[j];
                                        info.currentIPAddress_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_IP_STRING );
                                        info.currentSubnetMask_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_CURRENT_NETMASK_STRING );
                                        info.currentDefaultGateway_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_CURRENT_DEFAULT_GATEWAY_STRING );
                                        if( DoSubnetsMatch( adapterIPAddress, adapterNetmask, info.currentIPAddress_, info.currentSubnetMask_ ) )
                                        {
                                            info.persistentIPAddress_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_PERSISTENT_IP_STRING );
                                            info.persistentSubnetMask_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_PERSISTENT_NETMASK_STRING );
                                            info.persistentDefaultGateway_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_PERSISTENT_DEFAULT_GATEWAY_STRING );
                                        }
                                        else if( HasBeenDiscoveredViaUnicast( info.currentIPAddress_ ) )
                                        {
                                            p->boHasBeenDiscoveredViaUnicast_ = true;
                                        }
                                        else
                                        {
                                            info.persistentDefaultGateway_ = info.persistentSubnetMask_ = info.persistentIPAddress_ = s_m_subnetMismatchToken;
                                        }
                                        info.MACAddress_ = GetDeviceInterfaceStringInfo( itInterfaces->second, deviceName, j, DEVICE_INFO_MAC_STRING );
                                        bufferSize = sizeof( info.supportsLLA_ );
                                        LOGGED_TLI_CALL( TLIMV_IFGetDeviceInterfaceInfo, ( itInterfaces->second, deviceName.c_str(), j, DEVICE_INFO_SUPPORTS_IP_LLA, &dataType, &info.supportsLLA_, &bufferSize ), WriteLogMessage )
                                        bufferSize = sizeof( info.supportsDHCP_ );
                                        LOGGED_TLI_CALL( TLIMV_IFGetDeviceInterfaceInfo, ( itInterfaces->second, deviceName.c_str(), j, DEVICE_INFO_SUPPORTS_IP_DHCP, &dataType, &info.supportsDHCP_, &bufferSize ), WriteLogMessage )
                                        bufferSize = sizeof( info.supportsPersistentIP_ );
                                        LOGGED_TLI_CALL( TLIMV_IFGetDeviceInterfaceInfo, ( itInterfaces->second, deviceName.c_str(), j, DEVICE_INFO_SUPPORTS_IP_PERSISTENT, &dataType, &info.supportsPersistentIP_, &bufferSize ), WriteLogMessage )
                                        bufferSize = sizeof( info.LLAEnabled_ );
                                        LOGGED_TLI_CALL( TLIMV_IFGetDeviceInterfaceInfo, ( itInterfaces->second, deviceName.c_str(), j, DEVICE_INFO_CURRENT_IP_LLA, &dataType, &info.LLAEnabled_, &bufferSize ), WriteLogMessage )
                                        bufferSize = sizeof( info.DHCPEnabled_ );
                                        LOGGED_TLI_CALL( TLIMV_IFGetDeviceInterfaceInfo, ( itInterfaces->second, deviceName.c_str(), j, DEVICE_INFO_CURRENT_IP_DHCP, &dataType, &info.DHCPEnabled_, &bufferSize ), WriteLogMessage )
                                        bufferSize = sizeof( info.persistentIPEnabled_ );
                                        LOGGED_TLI_CALL( TLIMV_IFGetDeviceInterfaceInfo, ( itInterfaces->second, deviceName.c_str(), j, DEVICE_INFO_CURRENT_IP_PERSISTENT, &dataType, &info.persistentIPEnabled_, &bufferSize ), WriteLogMessage )
                                    }
                                }
                            }
                            else
                            {
                                WriteLogMessage( wxString::Format( wxT( "Device %s (first detected at interface %s) can also be reached via interface %s(%s).\n" ), ConvertedString( serial ).c_str(), ConvertedString( itDev->second->adapters_[0].first ).c_str(), ConvertedString( adapterIPAddress ).c_str(), ConvertedString( itInterfaces->first ).c_str() ) );
                                itDev->second->adapters_.push_back( make_pair( adapterIPAddress, AdapterInfo( adapterNetmask, adapterMTU, adapterLinkSpeed ) ) );
                            }
                        }
                    }
                    else
                    {
                        function_cast<PIFGetDeviceID> pIFGetDeviceID;
                        pIFGetDeviceID.pO = m_pIFGetDeviceID;
                        function_cast<PIFGetDeviceInfo> pIFGetDeviceInfo;
                        pIFGetDeviceInfo.pO = m_pIFGetDeviceInfo;
                        function_cast<PTLIMV_IFGetDeviceInterfaceInfo> pTLIMV_IFGetDeviceInterfaceInfo;
                        pTLIMV_IFGetDeviceInterfaceInfo.pO = m_pTLIMV_IFGetDeviceInterfaceInfo;
                        WriteLogMessage( wxString::Format( wxT( "At least one function pointer needed for the enumerate run could not be resolved. IFGetDeviceID: %p, IFGetDeviceInfo: %p, TLIMV_IFGetDeviceInterfaceInfo: %p.\n" ), pIFGetDeviceID.pI, pIFGetDeviceInfo.pI, pTLIMV_IFGetDeviceInterfaceInfo.pI ), m_ERROR_STYLE );
                    }
                }
                else
                {
                    WriteLogMessage( wxString::Format( wxT( "ERROR during call to IFGetNumDevices( %p, %p ).\n" ), itInterfaces->second, reinterpret_cast<void*>( &deviceCnt ) ), m_ERROR_STYLE );
                }
            }
            else
            {
                WriteLogMessage( wxT( "Pointer to IFGetNumDevices is invalid.\n" ), m_ERROR_STYLE );
            }
        }
        else
        {
            //WriteLogMessage( wxString::Format( wxT( "Interface %s reports its transport layer technology as '%s' while this application supports '%s' interfaces only. Devices connected to this interface will be ignored.\n" ), ConvertedString( itInterfaces->first ).c_str(), interfaceTLType.c_str(), s_m_technologyIdentifier.c_str() ) );
        }
        ++itInterfaces;
    }

    map<unsigned int, set<string> >::const_iterator itNet = detectedNets.begin();
    const map<unsigned int, set<string> >::const_iterator itNetEND = detectedNets.end();
    wxString netConflicts;
    while( itNet != itNetEND )
    {
        if( itNet->second.size() > 1 )
        {
            netConflicts.Append( wxString::Format( wxT( "- Net %d.%d.%d.%d is used by the following adapters: " ), itNet->first >> 24, ( itNet->first >> 16 ) & 0xFF, ( itNet->first >> 8 ) & 0xFF, itNet->first & 0xFF ) );
            set<string>::const_iterator itConflictingNet = itNet->second.begin();
            const set<string>::const_iterator itConflictingNetEND = itNet->second.end();
            while( itConflictingNet != itConflictingNetEND )
            {
                netConflicts.Append( ConvertedString( *itConflictingNet ) );
                netConflicts.Append( wxT( ", " ) );
                ++itConflictingNet;
            }
            // remove last ", " again
            netConflicts.RemoveLast( 2 );
            netConflicts.Append( wxT( ".\n" ) );
        }
        ++itNet;
    }

    if( !netConflicts.IsEmpty() )
    {
        wxString conflictMessage( wxString::Format( wxT( "More than one adapter resides in the same subnet. This is almost certainly a potential source of routing problems and should be resolved. This is a list of all detected conflicts:\n\n%s" ), netConflicts.c_str() ) );
        WriteLogMessage( conflictMessage, m_ERROR_STYLE );
        wxMessageBox( conflictMessage, wxT( "Subnet Conflict(s) Detected" ), wxOK | wxICON_EXCLAMATION, this );
    }

    DeviceMap::iterator itDev = m_devices.begin();
    const DeviceMap::iterator itDevEND = m_devices.end();
    while( itDev != itDevEND )
    {
        CheckForPotentialPerformanceIssues( itDev->second );
        ++itDev;
    }

    if( boBuildList )
    {
        BuildList( true );
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::UpdateDlgControls( bool boEdit )
//-----------------------------------------------------------------------------
{
    int currentItem = m_pDevListCtrl->GetCurrentItemIndex();
    wxListItem info;
    if( currentItem >= 0 )
    {
        info.m_itemId = currentItem;
        info.m_col = lcSerial;
        info.m_mask = wxLIST_MASK_TEXT;
        if( !m_pDevListCtrl->GetItem( info ) )
        {
            return;
        }
    }

    DeviceMap::const_iterator it = m_devices.find( string( info.m_text.mb_str() ) );
    bool boDeviceValid = ( it != m_devices.end() );
    if( boDeviceValid )
    {
        for( unsigned int i = 0; i < DetectedDeviceInfo::MAX_INTERFACE_COUNT; i++ )
        {
            m_interfaceInfo[0] = it->second->interfaceInfo_[0];
        }
    }
    else
    {
        boEdit = false;
    }

    m_pMIAction_AutoAssignDHCPIP->Enable( boDeviceValid );
    m_pMIAction_AutoAssignPersistentIP->Enable( boDeviceValid );
    m_pMIAction_AutoAssignTemporaryIP->Enable( boDeviceValid );
    m_pMIAction_ManuallyAssignTemporaryIP->Enable( boDeviceValid );
    m_pMIAction_ViewPotentialPerformanceIssues->Enable( boDeviceValid );

    // device info controls
    m_pSTManufacturer->SetLabel( ConvertedString( boDeviceValid ? it->second->manufacturer_.c_str() : "-" ) );
    m_pSTSerialNumber->SetLabel( ConvertedString( boDeviceValid ? it->second->deviceSerial_.c_str() : "-" ) );
    m_pTCUserDefinedName->SetValue( ConvertedString( boDeviceValid ? it->second->userDefinedName_.c_str() : "-" ) );
    m_pTCUserDefinedName->Enable( boEdit && it->second->supportsUserDefinedName_ );
    m_pSTInterfaceCount->SetLabel( boDeviceValid ? wxString::Format( wxT( "%d" ), it->second->interfaceCount_ ) : wxT( "-" ) );

    // interface selector
    m_pSCInterfaceSelector->Enable( boEdit );
    m_pSCInterfaceSelector->SetRange( 0, ( boDeviceValid ? it->second->interfaceCount_ - 1 : 0 ) );
    if( m_pSCInterfaceSelector->GetValue() > m_pSCInterfaceSelector->GetMax() )
    {
        m_pSCInterfaceSelector->SetValue( 0 );
    }

    const int interfaceIndex = m_pSCInterfaceSelector->GetValue();

    // current IP controls
    m_pSTCurrentIPAddress->SetLabel( ConvertedString( boDeviceValid ? m_interfaceInfo[interfaceIndex].currentIPAddress_.c_str() : "-" ) );
    m_pSTCurrentSubnetMask->SetLabel( ConvertedString( boDeviceValid ? m_interfaceInfo[interfaceIndex].currentSubnetMask_.c_str() : "-" ) );
    m_pSTCurrentDefaultGateway->SetLabel( ConvertedString( boDeviceValid ? m_interfaceInfo[interfaceIndex].currentDefaultGateway_.c_str() : "-" ) );
    m_pSTMACAddress->SetLabel( ConvertedString( boDeviceValid ? m_interfaceInfo[interfaceIndex].MACAddress_.c_str() : "-" ) );
    if( !boDeviceValid )
    {
        m_pCBConnectedToIPAddress->Clear();
        m_pCBConnectedToIPAddress->Append( wxT( "-" ) );
        m_pCBConnectedToIPAddress->Select( 0 );
    }
    else if( IsListOfChoicesEmpty( m_pCBConnectedToIPAddress ) )
    {
        const std::vector<std::pair<std::string, std::string> >::size_type adapterCnt = it->second->adapters_.size();
        for( std::vector<std::pair<std::string, std::string> >::size_type i = 0; i < adapterCnt; i++ )
        {
            m_pCBConnectedToIPAddress->Append( ConvertedString( it->second->adapters_[i].first.c_str() ) );
        }
        m_pCBConnectedToIPAddress->Select( 0 );
    }
    m_pCBConnectedToIPAddress->Enable( boDeviceValid );
    SetupNetworkGUIElements( it, interfaceIndex );

    // persistent IP controls
    const bool boPersistentIPControlsAvailable = boDeviceValid && m_interfaceInfo[interfaceIndex].supportsPersistentIP_;
    const bool boEnablePersistentIPControls = boEdit && m_interfaceInfo[interfaceIndex].supportsPersistentIP_ && m_interfaceInfo[interfaceIndex].persistentIPEnabled_;
    SetupPersistentIPGUIElement( m_pTCPersistentIPAddress, m_interfaceInfo[interfaceIndex].persistentIPAddress_, boPersistentIPControlsAvailable, boEnablePersistentIPControls );
    SetupPersistentIPGUIElement( m_pTCPersistentSubnetMask, m_interfaceInfo[interfaceIndex].persistentSubnetMask_, boPersistentIPControlsAvailable, boEnablePersistentIPControls );
    SetupPersistentIPGUIElement( m_pTCPersistentDefaultGateway, m_interfaceInfo[interfaceIndex].persistentDefaultGateway_, boPersistentIPControlsAvailable, boEnablePersistentIPControls );

    // IP configuration controls
    m_pCBUsePersistentIP->Enable( boEdit && m_interfaceInfo[interfaceIndex].supportsPersistentIP_ );
    m_pCBUsePersistentIP->SetValue( boDeviceValid && m_interfaceInfo[interfaceIndex].persistentIPEnabled_ );
    m_pCBUseDHCP->Enable( boEdit && m_interfaceInfo[interfaceIndex].supportsDHCP_ );
    m_pCBUseDHCP->SetValue( boDeviceValid && m_interfaceInfo[interfaceIndex].DHCPEnabled_ );
    // never enable this check box as this feature must always be active anyway
    m_pCBUseLLA->SetValue( boDeviceValid && m_interfaceInfo[interfaceIndex].LLAEnabled_ );

    // buttons
    m_pBtnConfigure->Enable( boDeviceValid && !boEdit );
    m_pBtnApplyChanges->Enable( boEdit );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::ViewPotentialPerformanceIssues( int listItemIndex )
//-----------------------------------------------------------------------------
{
    if( listItemIndex >= 0 )
    {
        wxString itemText( m_pDevListCtrl->GetItemText( listItemIndex ) );
        wxListItem info;
        info.m_itemId = listItemIndex;
        info.m_col = lcSerial;
        info.m_mask = wxLIST_MASK_TEXT;
        if( !m_pDevListCtrl->GetItem( info ) )
        {
            wxMessageBox( wxString::Format( wxT( "Could not obtain serial number for selected device %s.\n" ), itemText.c_str() ), wxT( "ERROR" ), wxOK | wxICON_EXCLAMATION, this );
            return;
        }

        DeviceMap::const_iterator itDev = m_devices.find( string( m_pSTSerialNumber->GetLabel().mb_str() ) );
        if( itDev == m_devices.end() )
        {
            wxMessageBox( wxString::Format( wxT( "ERROR: Could not obtain device name for selected device %s on adapter %s.\n" ), m_pSTSerialNumber->GetLabel().c_str(), m_pCBConnectedToIPAddress->GetValue().c_str() ), wxT( "ERROR" ), wxOK | wxICON_EXCLAMATION, this );
            return;
        }

        itDev->second->pPerformanceIssuesDlg_->ShowModal();
        //wxMessageBox( ConvertedString(itDev->second->potentialPerformanceIssuesMsg_), wxString::Format( wxT("Potential Performance Issues For Device '%s'"), m_pSTSerialNumber->GetLabel().c_str() ), wxOK | wxICON_EXCLAMATION, this );
    }
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::WarnAboutIgnoredInterfaces( void )
//-----------------------------------------------------------------------------
{
    MessageDialogPopUpWithCheckbox dlg( this,
                                        wxID_ANY,
                                        wxT( "Ignored Interfaces Detected!" ),
                                        wxT( "At least one network interface on this system will not show\n any devices connected to it.\n\nClick 'Fix Issue' to fix this issue (will launch wxPropView)\n\nClick 'Ignore' to ignore this issue and continue" ),
                                        wxT( "Always do this check on mvIPConfigure start" ),
                                        wxT( "Fix Issue" ),
                                        wxT( "Ignore" ),
                                        m_pMISettings_ShowWarningOnIgnoredInterfaces->IsChecked() );
    if( dlg.ShowModal() == wxID_YES )
    {
        const wxString commandString( wxT( "wxPropView interfaceConfiguration=1" ) );
        ::wxExecute( commandString );
        m_boMustQuit = true;
    }
    else
    {
        WriteLogMessage( wxT( "\nWARNING: Some interfaces are being ignored during device enumeration!!! Devices connected to those interfaces will NOT appear in the device list above. To fix this issue re-enable Menu->Settings->'Warn If Ignored Interfaces Are Detected'" ) );
    }
    m_pMISettings_ShowWarningOnIgnoredInterfaces->Check( dlg.IsCheckboxChecked() );
}

//-----------------------------------------------------------------------------
void IPConfigureFrame::WriteLogMessage( const wxString& msg, const wxTextAttr& style /* = wxTextAttr(wxColour(0, 0, 0)) */ ) const
//-----------------------------------------------------------------------------
{
    if( m_pLogWindow )
    {
        const long posBefore = m_pLogWindow->GetLastPosition();
        m_pLogWindow->WriteText( msg );
        const long posAfter = m_pLogWindow->GetLastPosition();
        m_pLogWindow->SetStyle( posBefore, posAfter, style );
        m_pLogWindow->ScrollLines( m_pLogWindow->GetNumberOfLines() );
        m_pLogWindow->Update();
    }
}
