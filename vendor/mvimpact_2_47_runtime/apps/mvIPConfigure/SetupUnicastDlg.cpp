//-----------------------------------------------------------------------------
#if defined(linux) || defined(__linux) || defined(__linux__)
#   include <arpa/inet.h>
#else
#   include <Ws2tcpip.h>
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
#include <algorithm>
#include <apps/Common/CommonGUIFunctions.h>
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "IPConfigureFrame.h"
#include "SetupUnicastDlg.h"

#include <apps/Common/wxIncludePrologue.h>
#include "wx/button.h"
#include "wx/combobox.h"
#include "wx/grid.h"
#include "wx/socket.h"
#include "wx/spinctrl.h"
#include "wx/tokenzr.h"
#include "wx/textctrl.h"
#include "wx/textdlg.h"
#include <apps/Common/wxIncludeEpilogue.h>

// Handle version 2.8 functions even if current version has dropped compatibility
#if wxCHECK_VERSION(2, 9, 0) && !WXWIN_COMPATIBILITY_2_8
#  define SET_CELL_BACKGROUND_COLOUR( cl, r,c ) SetCellBackgroundColour( r, c, cl )
#  define SET_CELL_VALUE( v, r, c ) SetCellValue( r, c, v )
#else
#  define SET_CELL_BACKGROUND_COLOUR( cl, r, c ) SetCellBackgroundColour( cl, r, c )
#  define SET_CELL_VALUE( v, r, c ) SetCellValue( v, r, c )
#endif

//=============================================================================
//=================== internal helper functions ===============================
//=============================================================================
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
//================== Implementation LogOutputHandlerDlg =======================
//=============================================================================

BEGIN_EVENT_TABLE( SetupUnicastDlg, wxDialog )
    EVT_BUTTON( widBtnAbort, SetupUnicastDlg::OnBtnAbort )
    EVT_BUTTON( widBtnApply, SetupUnicastDlg::OnBtnApply )
    EVT_BUTTON( widBtnClear, SetupUnicastDlg::OnBtnClear )
    EVT_BUTTON( widBtnDiscover, SetupUnicastDlg::OnBtnDiscover )
    EVT_GRID_CMD_CELL_CHANGED( widGridIPs, SetupUnicastDlg::OnGridIPAddressChanged )
    EVT_TEXT( widAdapterList, SetupUnicastDlg::OnComboboxInterfaceChanged )
END_EVENT_TABLE()

//-----------------------------------------------------------------------------
SetupUnicastDlg::SetupUnicastDlg( IPConfigureFrame* pParent )
    : wxDialog( pParent, wxID_ANY, wxString( wxT( "Setup Unicast Device Discovery Destinations" ) ), wxDefaultPosition,
                wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX ), pBtnAbort_( 0 ), pBtnApply_( 0 ), pBtnDiscover_( 0 ), pCBAdapterList_( 0 ),
      pGridUnicastIPs_( 0 ), pParent_( pParent ), maxUnicastDestinationCount_( 0 ), unicastDestinationData_(), initialUnicastDestinationData_()
      //-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        | |---------------------------------| |
        | |          spacer                 | |
        | | pGroupBoxSizer                  | |
        | | |-----------------------------| | |
        | | | pEditElementsGridSizer      | | |
        | | | |-------------------------| | | |
        | | | |                         | | | |
        | | | |-------------------------| | | |
        | | | pButtonGridSizer          | | | |
        | | | |-------------------------| | | |
        | | | |                         | | | |
        | | | |-------------------------| | | |
        | | | pEditElementsGridSizer    | | | |
        | | | |-------------------------| | | |
        | | | |                         | | | |
        | | | |-------------------------| | | |
        | | |-----------------------------| | |
        | |          spacer                 | |
        | |                                 | |
        | |---------------------------------| |
        |-------------------------------------|
    */
    EnumerateInterfaces();

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    wxPanel* pPanel = new wxPanel( this );
    pTopDownSizer->AddSpacer( 10 );

    wxStaticBoxSizer* pGroupBoxSizer = new wxStaticBoxSizer( wxHORIZONTAL, pPanel, wxT( "Interface:" ) );
    wxStaticBox* pGroupBoxSizerBox = pGroupBoxSizer->GetStaticBox();

    wxFlexGridSizer* pEditElementsGridSizer = new wxFlexGridSizer( 2 );
    pEditElementsGridSizer->AddGrowableCol( 1, 2 );

    // interface selector
    //pEditElementsGridSizer->Add( new wxStaticText( pGroupBoxSizerBox, wxID_ANY, wxT( "Local IP To Use For Sending:" ) ), wxSizerFlags().Left() );
    pCBAdapterList_ = new wxComboBox( pGroupBoxSizerBox, widAdapterList, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_DROPDOWN | wxCB_READONLY );
    UnicastDestinationData::iterator it = unicastDestinationData_.begin();
    UnicastDestinationData::const_iterator itEND = unicastDestinationData_.end();
    while( it != itEND )
    {
        pCBAdapterList_->AppendString( it->first );
        it++;
    }
    pCBAdapterList_->Select( 0 );
    pCBAdapterList_->SetToolTip( wxT( "Selects the adapter over witch the device(s) should be discovered." ) );

    pEditElementsGridSizer->Add( pCBAdapterList_, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_VERTICAL ) );
    pEditElementsGridSizer->AddSpacer( 10 );
    pGroupBoxSizer->Add( pEditElementsGridSizer, wxSizerFlags().Align( wxGROW ).DoubleBorder() ); // see trac.wxwidgets.org/ticket/17239 before changing this

    wxFlexGridSizer* pButtonGridSizer = new wxFlexGridSizer( 4 );
    pButtonGridSizer->AddGrowableCol( 1, 4 );

    // Apply button
    pBtnApply_ = new wxButton( pPanel, widBtnApply, wxT( "&Apply" ) );
    pBtnApply_->SetToolTip( wxT( "Apply all settings and close this dialog." ) );
    pButtonGridSizer->Add( pBtnApply_, wxSizerFlags().Left().Border( wxALL, 10 ) );

    // Abort button
    pBtnAbort_ = new wxButton( pPanel, widBtnAbort, wxT( "&Cancel" ) );
    pBtnAbort_->SetToolTip( wxT( "Revert all settings to the initial state." ) );
    pButtonGridSizer->Add( pBtnAbort_, wxSizerFlags().Center().Border( wxALL, 10 ) );

    // Discover button
    pBtnDiscover_ = new wxButton( pPanel, widBtnDiscover, wxT( "&Discover" ) );
    pBtnDiscover_->SetToolTip( wxT( "Search for devices at the IP addresses configured in the grid." ) );
    pButtonGridSizer->Add( pBtnDiscover_, wxSizerFlags().Right().Border( wxALL, 10 ) );

    // Clear button
    pBtnClear_ = new wxButton( pPanel, widBtnClear, wxT( "&Clear" ) );
    pBtnClear_->SetToolTip( wxT( "Clear the grid." ) );
    pButtonGridSizer->Add( pBtnClear_, wxSizerFlags().Right().Border( wxALL, 10 ) );


    wxStaticBoxSizer* pGridBoxSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Unicast Destinations:" ) );
    wxStaticBox* pGridBoxSizerBox = pGridBoxSizer->GetStaticBox();

    // Unicast IPs
    pGridUnicastIPs_ = new wxGrid( pGridBoxSizerBox, widGridIPs, wxDefaultPosition, wxDefaultSize );
    pGridBoxSizer->Add( pGridUnicastIPs_, wxSizerFlags( 2 ).Align( wxGROW | wxALIGN_CENTER_HORIZONTAL ) );
    pGridUnicastIPs_->CreateGrid( maxUnicastDestinationCount_, 2 );

    wxGridCellAttr* attrRO;
    wxGridCellTextEditor* editorValidator;
    for( int r = 0; r < pGridUnicastIPs_->GetNumberCols(); r++ )
    {
        if( r == 1 )
        {
            attrRO = new wxGridCellAttr;
            attrRO->SetReadOnly();
            pGridUnicastIPs_->SetColAttr( r, attrRO );
        }
        else if( r == 0 )
        {
            editorValidator = new wxGridCellTextEditor;
            editorValidator->SetValidator( iPv4StringValidator_ );
        }
    }

    pEditElementsGridSizer->AddSpacer( 5 );
    pGridUnicastIPs_->SetColLabelValue( 0, wxT( "IP Address" ) );
    pGridUnicastIPs_->SetColLabelValue( 1, wxT( "Discovered" ) );
    pGridUnicastIPs_->EnableDragGridSize( false );
    pGridUnicastIPs_->EnableDragRowSize( false );
    pGridUnicastIPs_->EnableDragColSize( false );
    pGridUnicastIPs_->EnableDragCell( false );


    pGridBoxSizer->AddSpacer( 10 );

    pTopDownSizer->Add( pGroupBoxSizer, wxSizerFlags().Expand() );
    pTopDownSizer->Add( pButtonGridSizer, wxSizerFlags().Expand() );
    pTopDownSizer->Add( pGridBoxSizer, wxSizerFlags().Expand() );
    pTopDownSizer->AddSpacer( 10 );

    pPanel->SetSizer( pTopDownSizer );
    pTopDownSizer->SetSizeHints( this );

    const int columnSize = ( ( pGridUnicastIPs_->GetClientRect().GetWidth() - pGridUnicastIPs_->GetRowLabelSize() ) / 2 );
    pGridUnicastIPs_->SetColMinimalWidth( 0, columnSize );
    pGridUnicastIPs_->SetColMinimalWidth( 1, columnSize );
    pGridUnicastIPs_->SetColSize( 0, columnSize );
    pGridUnicastIPs_->SetColSize( 1, columnSize );
    pGridUnicastIPs_->ForceRefresh();
    pGridUnicastIPs_->SetMargins( 0 - wxSYS_VSCROLL_X, 0 );

    ChangeCellReadWriteAccess();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::OnClose( wxCloseEvent& )
//-----------------------------------------------------------------------------
{
    for_each( validInterfaces_.begin(), validInterfaces_.end(), std::ptr_fun( DeleteElement<mvIMPACT::acquire::GenICam::InterfaceModule*> ) );
    Destroy();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::ChangeCellReadWriteAccess( void ) const
//-----------------------------------------------------------------------------
{
    const int numRows = pGridUnicastIPs_->GetNumberRows();
    bool boWriteableRowGenerated = false;
    for( int row = 0; row < numRows; row++ )
    {
        const wxString cellValue( pGridUnicastIPs_->GetCellValue( row, 0 ) );
        if( row > 0 )
        {
            if( boWriteableRowGenerated == false && !pGridUnicastIPs_->GetCellValue( ( row - 1 ), 0 ).empty() )
            {
                pGridUnicastIPs_->SetReadOnly( row, 0, false );
                ColorizeRow( row, tgrWhite );
                boWriteableRowGenerated = true;
            }
            else if( cellValue.empty() )
            {
                pGridUnicastIPs_->SetReadOnly( row, 0, true );
                ColorizeRow( row, tgrGrey );
            }
        }
        else
        {
            ColorizeRow( row, tgrWhite );
        }
    }
    pGridUnicastIPs_->ForceRefresh();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::CollectCompatibleGEVInterfaces( void )
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::SystemModule sm( int64_type( 0 ) );
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
                !pIm->interfaceDisplayName.isValid() ||
                !pIm->deviceTLVersionMajor.isValid() ||
                !pIm->deviceVendorName.isValid() )
            {
                // most likely this is a 3rd party producer. As we cannot obtain the
                // information we are after just skip this interface!
                DeleteElement( pIm );
                continue;
            }
            validInterfaces_.push_back( pIm );
        }
    }
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::ColorizeRow( int row, TGridColors color ) const
//-----------------------------------------------------------------------------
{
    switch( color )
    {
    case SetupUnicastDlg::tgrWhite:
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( *wxWHITE, row, 0 );
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( *wxWHITE, row, 1 );
        break;
    case SetupUnicastDlg::tgrGreen:
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( wxColour( acGreenPastel ), row, 0 );
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( wxColour( acGreenPastel ), row, 1 );
        break;
    case SetupUnicastDlg::tgrRed:
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( wxColour( acRedPastel ), row, 0 );
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( wxColour( acRedPastel ), row, 1 );
        break;
    case SetupUnicastDlg::tgrGrey:
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( wxColour( acGreyPastel ), row, 0 );
        pGridUnicastIPs_->SET_CELL_BACKGROUND_COLOUR( wxColour( acGreyPastel ), row, 1 );
        break;
    default:
        break;
    }
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::EnumerateInterfaces( void )
//-----------------------------------------------------------------------------
{
    CollectCompatibleGEVInterfaces ();
    // As every interface module should support the same maximum value, the max value of the first interface might be sufficient
    maxUnicastDestinationCount_ = static_cast<int>( validInterfaces_[0]->mvUnicastDeviceDiscoveryCommandCount.getMaxValue() );

    for( size_t i = 0; i < validInterfaces_.size(); i++ )
    {
        const int64_type adapterSubnetCnt = validInterfaces_[i]->gevInterfaceSubnetSelector.getMaxValue();
        int j = 0;
        do
        {
            const wxString interfaceIdentifier = GetInterfaceIdentifier( static_cast<unsigned int>( i ) );
            std::vector<unsigned int> tmpUnicastIPs;
            for( int64_type k = 0; k < validInterfaces_[i]->mvUnicastDeviceDiscoveryCommandCount.read(); k++ )
            {
                validInterfaces_[i]->mvUnicastDeviceDiscoveryCommandSelector.write( k );
                tmpUnicastIPs.push_back( static_cast<unsigned int>( validInterfaces_[i]->mvUnicastDeviceDiscoveryDestinationIPAddress.read() ) );
            }
            unicastDestinationData_.insert( make_pair( interfaceIdentifier, tmpUnicastIPs ) );
        }
        while( adapterSubnetCnt < j );
    }
    initialUnicastDestinationData_ = unicastDestinationData_;
}

//-----------------------------------------------------------------------------
wxString SetupUnicastDlg::GetInterfaceIdentifier( unsigned int index ) const
//-----------------------------------------------------------------------------
{
    return ConvertedString( validInterfaces_[index]->interfaceDisplayName.readS() ) + wxT( " (" ) + GetIPv4AddressAsString( static_cast<unsigned int>( validInterfaces_[index]->gevInterfaceSubnetIPAddress.read() ) ) + wxT( ")" );
}

//-----------------------------------------------------------------------------
mvIMPACT::acquire::GenICam::InterfaceModule* SetupUnicastDlg::GetInterfaceModule( const wxString& identifier ) const
//-----------------------------------------------------------------------------
{
    const unsigned int interfaceCount = static_cast<unsigned int>( validInterfaces_.size() );
    for( unsigned int i = 0; i < interfaceCount; i++ )
    {
        if( GetInterfaceIdentifier( i ) == identifier )
        {
            return validInterfaces_[i];
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::OnBtnAbort( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    UnicastDestinationData::iterator it = initialUnicastDestinationData_.begin();
    UnicastDestinationData::const_iterator itEND = initialUnicastDestinationData_.end();
    while( it != itEND )
    {
        mvIMPACT::acquire::GenICam::InterfaceModule* pIm = GetInterfaceModule( it->first );
        const size_t unicastIPCnt = it->second.size();
        pIm->mvUnicastDeviceDiscoveryCommandCount.write( unicastIPCnt );
        for( size_t i = 0; i < unicastIPCnt; i++ )
        {
            pIm->mvUnicastDeviceDiscoveryCommandSelector.write( i );
            pIm->mvUnicastDeviceDiscoveryDestinationIPAddress.write( it->second[i] );
        }
        it++;
    }
    Close();
}

//-----------------------------------------------------------------------------
inline void SetupUnicastDlg::OnBtnApply( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const wxString invalidIP = ReadGridData();
    if( invalidIP != wxEmptyString )
    {
        wxMessageBox( wxString::Format( wxT( "'%s' is not a valid IP address. " ), invalidIP ), wxT( "Parameter validation check failed" ), wxOK | wxICON_EXCLAMATION, pParent_ );
        pGridUnicastIPs_->ForceRefresh();
        return;
    }
    Close();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::OnBtnClear( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const wxString selectedInterface = pCBAdapterList_->GetValue();
    mvIMPACT::acquire::GenICam::InterfaceModule* pIm = GetInterfaceModule( selectedInterface );
    if( pIm != 0 )
    {
        pIm->mvUnicastDeviceDiscoveryCommandCount.write( 0 );
        UnicastDestinationData::iterator it = unicastDestinationData_.find( selectedInterface );
        if( it != unicastDestinationData_.end() )
        {
            it->second.clear();
        }
    }
    ChangeCellReadWriteAccess();
    UpdateGrid();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::OnBtnDiscover( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const wxString invalidIP = ReadGridData();
    if( invalidIP != wxEmptyString )
    {
        wxMessageBox( wxString::Format( wxT( "'%s' is not a valid IP address. " ), invalidIP ), wxT( "Parameter validation check failed" ), wxOK | wxICON_EXCLAMATION, pParent_ );
        pGridUnicastIPs_->ForceRefresh();
        return;
    }

    wxBusyCursor busyCursorScope;
    // The next command yields control to pending messages in the windowing system. It is necessary in order
    // to get a busy cursor in Linux systems when choosing menu Item Update Device List via mouse.
    wxYield();
    UpdateDeviceListWithProgressMessage( this, devMgr_ );
    const wxString selectedInterface = pCBAdapterList_->GetValue();
    size_t numberOfValidInterface = validInterfaces_.size();
    for( size_t i = 0; i < numberOfValidInterface; i++ )
    {
        const wxString interfaceIdentifier = GetInterfaceIdentifier( static_cast<unsigned int>( i ) );
        if( interfaceIdentifier == selectedInterface )
        {
            UnicastDestinationData::const_iterator it = unicastDestinationData_.find( interfaceIdentifier );
            if( it != unicastDestinationData_.end() )
            {
                const int64_type numberOfDiscoveredDevices = validInterfaces_[i]->deviceSelector.getMaxValue() + 1;
                if( ( validInterfaces_[i]->deviceTLVersionMajor.read() != 0 ) &&
                    ( validInterfaces_[i]->deviceVendorName.readS() != "" ) )
                {
                    for( int64_type j = 0; j < numberOfDiscoveredDevices; j++ )
                    {
                        validInterfaces_[i]->deviceSelector.write( j );
                        const std::vector<unsigned int>::const_iterator itIP = std::find( it->second.begin(), it->second.end(), validInterfaces_[i]->gevDeviceIPAddress.read() );
                        const int rowPosition = static_cast< int >( itIP - it->second.begin() );
                        if( itIP != it->second.end() )
                        {
                            pGridUnicastIPs_->SET_CELL_VALUE( validInterfaces_[i]->deviceSerialNumber.readS(), rowPosition, 1 );
                            ColorizeRow(  rowPosition, tgrGreen );
                        }
                    }
                }
                else
                {
                    wxMessageBox( wxT( "No device could be discovered using the given IP addresses." ), wxT( "No device discovered" ), wxOK | wxICON_INFORMATION, pParent_ );
                    pGridUnicastIPs_->ForceRefresh();
                }
            }
        }
    }
    pGridUnicastIPs_->ForceRefresh();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::OnComboboxInterfaceChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const int rowCount = static_cast<int>( pGridUnicastIPs_->GetNumberRows() );
    for( int row = 0; row < rowCount; row++ )
    {
        ColorizeRow( row, tgrWhite );
    }
    UpdateGrid();
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::OnGridIPAddressChanged( wxGridEvent& e )
//-----------------------------------------------------------------------------
{
    const unsigned int row = e.GetRow();
    if( pGridUnicastIPs_->GetCellValue( row, 0 ) == wxEmptyString )
    {
        pGridUnicastIPs_->DeleteRows( row );
        const int rowCount = static_cast< int >( pGridUnicastIPs_->GetNumberRows() );
        pGridUnicastIPs_->InsertRows( rowCount, 1, true );
        ReadGridData();
    }
    if( pGridUnicastIPs_->GetCellValue( row, 1 ) != wxEmptyString )
    {
        pGridUnicastIPs_->SET_CELL_VALUE( wxString( wxEmptyString ), row, 1 );
    }
    ChangeCellReadWriteAccess();
}

//-----------------------------------------------------------------------------
wxString SetupUnicastDlg::ReadGridData( void )
//-----------------------------------------------------------------------------
{
    const wxString selectedInterface = pCBAdapterList_->GetValue();
    UnicastDestinationData::iterator it = unicastDestinationData_.find( selectedInterface );

    if( it != unicastDestinationData_.end() )
    {
        it->second.clear();
        for( int row = 0; row < pGridUnicastIPs_->GetNumberRows(); row++ )
        {
            const wxString cellValueString = pGridUnicastIPs_->GetCellValue( row, 0 ).mb_str();
            if( !cellValueString.empty() )
            {
                if( !ValidateIPv4Address( cellValueString ) )
                {
                    ColorizeRow( row, tgrRed );
                    return cellValueString;
                }
                const unsigned long val = ntohl( GetIPv4AddressFromString( cellValueString.c_str() ) );
                if( static_cast<int>( it->second.size() ) >= row )
                {
                    it->second.push_back( val );
                }
                else
                {
                    it->second[row] = val;
                }
            }
            std::sort( it->second.begin(), it->second.end() );
            it->second.erase( std::unique( it->second.begin(), it->second.end() ), it->second.end() );
        }
    }
    UpdateGrid();
    WriteUnicastDataEntrys( selectedInterface );
    return wxString( wxEmptyString );
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::UpdateGrid( void )
//-----------------------------------------------------------------------------
{
    UnicastDestinationData::const_iterator it = unicastDestinationData_.find( pCBAdapterList_->GetValue() );
    pGridUnicastIPs_->ClearGrid();
    if( it != unicastDestinationData_.end() )
    {
        const unsigned int unicastIPCount = static_cast<unsigned int>( it->second.size() );
        for( unsigned int i = 0; i < unicastIPCount; i++ )
        {
            if( i < unicastIPCount )
            {
                pGridUnicastIPs_->SET_CELL_VALUE( GetIPv4AddressAsString( static_cast<unsigned int>( it->second[i] ) ), i, 0 );
            }
        }
        ChangeCellReadWriteAccess();
    }
}

//-----------------------------------------------------------------------------
bool SetupUnicastDlg::ValidateIPv4Address( const wxString& ipv4String )
//-----------------------------------------------------------------------------
{
    wxStringTokenizer tokenizer( ipv4String, "." );
    if( ( pParent_->IsValidIPv4Address( ipv4String.mb_str() ) != 0 ) || ( tokenizer.CountTokens() != 4 ) )
    {
        return false;
    }

    while( tokenizer.HasMoreTokens() )
    {
        wxString token = tokenizer.GetNextToken();
        const unsigned int ipOctet = wxAtoi( token );
        if( ipOctet > ( unsigned int )( 255 ) )
        {
            return false;
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
void SetupUnicastDlg::WriteUnicastDataEntrys( const wxString& selectedInterface ) const
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::GenICam::InterfaceModule* pIm = GetInterfaceModule( selectedInterface );
    if( pIm != 0 )
    {
        const UnicastDestinationData::const_iterator it = unicastDestinationData_.find( selectedInterface );
        if( it != unicastDestinationData_.end() )
        {
            const size_t discoveryCount = it->second.size();
            pIm->mvUnicastDeviceDiscoveryCommandCount.write( discoveryCount );
            for( size_t i = 0; i < discoveryCount; i++ )
            {
                pIm->mvUnicastDeviceDiscoveryCommandSelector.write( i );
                pIm->mvUnicastDeviceDiscoveryDestinationIPAddress.write( it->second[i] );
            }
        }
    }
}

