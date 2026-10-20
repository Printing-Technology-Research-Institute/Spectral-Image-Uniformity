//-----------------------------------------------------------------------------
#ifndef SetupUnicastDlgH
#define SetupUnicastDlgH SetupUnicastDlgH
//-----------------------------------------------------------------------------
#include <apps/Common/wxIncluder.h>
#include <map>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>

class wxButton;
class wxComboBox;
class wxGrid;
class wxGridEvent;
class wxTextCtrl;
class IPConfigureFrame;

//-----------------------------------------------------------------------------
class SetupUnicastDlg : public wxDialog
//-----------------------------------------------------------------------------
{
    // any class wishing to process wxWidgets events must use this macro
    DECLARE_EVENT_TABLE()

    wxButton* pBtnAbort_;
    wxButton* pBtnApply_;
    wxButton* pBtnClear_;
    wxButton* pBtnDiscover_;
    wxComboBox* pCBAdapterList_;
    wxGrid* pGridUnicastIPs_;
    IPConfigureFrame* pParent_;

    IPv4StringValidator iPv4StringValidator_;

    int maxUnicastDestinationCount_;

    mvIMPACT::acquire::DeviceManager devMgr_;

    typedef std::vector<mvIMPACT::acquire::GenICam::InterfaceModule*> InterfaceModuleContainer;
    InterfaceModuleContainer validInterfaces_;

    typedef std::map<wxString, std::vector<unsigned int> > UnicastDestinationData;
    UnicastDestinationData unicastDestinationData_;
    UnicastDestinationData initialUnicastDestinationData_;
    //-----------------------------------------------------------------------------
    enum TWidgetIDs
    //-----------------------------------------------------------------------------
    {
        widBtnExecute = wxID_HIGHEST,
        widAdapterList,
        widUnicastCount,
        widBtnAbort,
        widBtnApply,
        widBtnClear,
        widBtnDiscover,
        widGridIPs
    };

    //-----------------------------------------------------------------------------
    enum TGridColors
    //-----------------------------------------------------------------------------
    {
        tgrWhite,
        tgrGreen,
        tgrRed,
        tgrGrey
    };

    void ChangeCellReadWriteAccess( void ) const;
    void EnumerateInterfaces( void );
    wxString GetIPv4AddressAsString( unsigned int ip ) const
    {
        unsigned char bytes[4];
        bytes[0] = ip & 0xFF;
        bytes[1] = ( ip >> 8 ) & 0xFF;
        bytes[2] = ( ip >> 16 ) & 0xFF;
        bytes[3] = ( ip >> 24 ) & 0xFF;
        return wxString::Format( "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0] );
    }

    mvIMPACT::acquire::GenICam::InterfaceModule* GetInterfaceModule( const wxString& identifier ) const;
    void CollectCompatibleGEVInterfaces( void );
    void ColorizeRow( int row, TGridColors color ) const;
    void OnBtnAbort( wxCommandEvent& );
    void OnBtnApply( wxCommandEvent& );
    void OnBtnClear( wxCommandEvent& );
    void OnBtnDiscover( wxCommandEvent& );
    void OnClose( wxCloseEvent& e );
    void OnComboboxInterfaceChanged( wxCommandEvent& );
    void OnGridIPAddressChanged( wxGridEvent& );
    wxString ReadGridData( void );
    void UpdateGrid( void );
    bool ValidateIPv4Address( const wxString& ipv4String );
    void WriteUnicastDataEntrys( const wxString& selectedInterface ) const;
    wxString GetInterfaceIdentifier( unsigned int index ) const;
public:
    SetupUnicastDlg( IPConfigureFrame* pParent );
};

#endif // SetupUnicastDlgH
