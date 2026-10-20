#include <apps/Common/wxAbstraction.h>
#include "icons.h"
#include "PropViewFrame.h"
#include "WizardMultiCoreAcquisition.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>
#include <apps/Common/wxIncludeEpilogue.h>

#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>

using namespace std;
using namespace GenICam;

//=============================================================================
//============== Implementation WizardMultiCoreAcquisition ====================
//=============================================================================
//-----------------------------------------------------------------------------
WizardMultiCoreAcquisition::WizardMultiCoreAcquisition( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, mvIMPACT::acquire::FunctionInterface* pFI )
    : OkAndCancelDlg( pParent, widMainFrame, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX ),
      pDev_( pDev ), pFI_( pFI ), info_( pDev ), dsm_( pDev, 0 ), pParent_( pParent ), boMultiCoreAcquisitionEnableStateOnEnter_( bFalse ),
      boAcquisitionStateOnCancel_( false ), pSCFirstCoreIndex_( 0 ), pSCCoreCount_( 0 ), pSCCoreSwitchInterval_( 0 )
//-----------------------------------------------------------------------------
{
    boMultiCoreAcquisitionEnableStateOnEnter_ = dsm_.mvMultiCoreAcquisitionEnable.read();
    dsm_.mvMultiCoreAcquisitionEnable.write( bTrue );
    CreateGUI();
}

//-----------------------------------------------------------------------------
WizardMultiCoreAcquisition::~WizardMultiCoreAcquisition()
//-----------------------------------------------------------------------------
{
}

//-----------------------------------------------------------------------------
void WizardMultiCoreAcquisition::ApplyCurrentSettings( void )
//-----------------------------------------------------------------------------
{
    pParent_->EnsureAcquisitionState( false );
    dsm_.mvMultiCoreAcquisitionCoreCount.write( pSCCoreCount_->GetValue() );
    dsm_.mvMultiCoreAcquisitionFirstCoreIndex.write( pSCFirstCoreIndex_->GetValue() );
    if( dsm_.mvMultiCoreAcquisitionCoreSwitchInterval.isWriteable() )
    {
        dsm_.mvMultiCoreAcquisitionCoreSwitchInterval.write( static_cast< unsigned int >( pSCCoreSwitchInterval_->GetValue() ) );
    }
    pParent_->EnsureAcquisitionState( true );
    SetupDlgControls();
}

//-----------------------------------------------------------------------------
void WizardMultiCoreAcquisition::CreateGUI( void )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | controls                        | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | CPU core grid                   | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
    */

    wxPanel* pPanel = new wxPanel( this );

    pToggleButtonUndefined_ = new wxBitmap( wxBitmap::NewFromPNGData( checkbox_undefined_png, sizeof( checkbox_undefined_png ) ).ConvertToImage().Scale( 16, 16 ) );
    pToggleButtonChecked_ = new wxBitmap( wxBitmap::NewFromPNGData( checkbox_checked_png, sizeof( checkbox_checked_png ) ).ConvertToImage().Scale( 16, 16 ) );
    pToggleButtonCheckedUndefined_ = new wxBitmap( wxBitmap::NewFromPNGData( checkbox_checked_undefined_png, sizeof( checkbox_checked_undefined_png ) ).ConvertToImage().Scale( 16, 16 ) );
    pToggleButtonUnchecked_ = new wxBitmap( wxBitmap::NewFromPNGData( checkbox_unchecked_png, sizeof( checkbox_unchecked_png ) ).ConvertToImage().Scale( 16, 16 ) );
    pToggleButtonUncheckedUndefined_ = new wxBitmap( wxBitmap::NewFromPNGData( checkbox_unchecked_undefined_png, sizeof( checkbox_unchecked_undefined_png ) ).ConvertToImage().Scale( 16, 16 ) );
    pToggleButtonUnavailable_ = new wxBitmap( wxBitmap::NewFromPNGData( unavailable_png, sizeof( checkbox_unchecked_png ) ).ConvertToImage().Scale( 16, 16 ) );

    wxStaticBoxSizer* pControlsSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Controls:" ) );
    wxFlexGridSizer* pControlsGridSizer = new wxFlexGridSizer( 2 );
    pControlsGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "Base Core: " ) ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pTCBaseCore_ = new wxTextCtrl( pPanel, wxID_ANY, ConvertedString( dsm_.mvMultiCoreAcquisitionBaseCore.readS() ), wxDefaultPosition, wxDefaultSize, wxTE_READONLY );
    pTCBaseCore_->SetToolTip( wxString( wxT( "The base core configured in the NIC driver. No core with an index less the base core will be usable for receive operations." ) ) );
    pControlsGridSizer->Add( pTCBaseCore_, wxSizerFlags().Expand() );

    pControlsGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "First Core Index:" ) ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pSCFirstCoreIndex_ = new wxSpinCtrl( pPanel,
                                         widSCFirstCoreIndex,
                                         ConvertedString( dsm_.mvMultiCoreAcquisitionFirstCoreIndex.readS() ),
                                         wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                                         static_cast< int >( dsm_.mvMultiCoreAcquisitionFirstCoreIndex.read() ),
                                         static_cast< int >( dsm_.mvMultiCoreAcquisitionFirstCoreIndex.getMinValue() ),
                                         static_cast< int >( dsm_.mvMultiCoreAcquisitionFirstCoreIndex.getMaxValue() ) );
    pSCFirstCoreIndex_->SetToolTip( wxString( wxT( "Specifies the offset relative to the base core configured in the NIC driver configuration. Only physical cores will be included." ) ) );
    pControlsGridSizer->Add( pSCFirstCoreIndex_, wxSizerFlags().Expand() );

    pControlsGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "Core Count:" ) ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pSCCoreCount_ = new wxSpinCtrl( pPanel,
                                    widSCCoreCount,
                                    ConvertedString( dsm_.mvMultiCoreAcquisitionCoreCount.readS() ),
                                    wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                                    1,
                                    static_cast< int >( dsm_.mvMultiCoreAcquisitionCoreCount.getMaxValue() ),
                                    static_cast< int >( dsm_.mvMultiCoreAcquisitionCoreCount.read() ) );
    pSCCoreCount_->SetToolTip( wxString( wxT( "Specifies the number of physical cores which should be used to process received packets of the network stream." ) ) );
    pControlsGridSizer->Add( pSCCoreCount_, wxSizerFlags().Expand() );

    pControlsGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "Core Switch Interval:" ) ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
    pSCCoreSwitchInterval_ = new wxSpinCtrl( pPanel,
            widSCCoreSwitchInterval,
            ConvertedString( dsm_.mvMultiCoreAcquisitionCoreSwitchInterval.readS() ),
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
            static_cast<int>( dsm_.mvMultiCoreAcquisitionCoreSwitchInterval.getMinValue() ),
            static_cast<int>( dsm_.mvMultiCoreAcquisitionCoreSwitchInterval.getMaxValue() ),
            static_cast<int>( dsm_.mvMultiCoreAcquisitionCoreSwitchInterval.read() ) );
    pSCCoreSwitchInterval_->SetToolTip( wxString( wxT( "The number of received network packets after which the CPU core should be changed." ) ) );
    pControlsGridSizer->Add( pSCCoreSwitchInterval_, wxSizerFlags().Expand() );

    pControlsGridSizer->AddGrowableCol( 1, 3 );
    pControlsSizer->Add( pControlsGridSizer, wxSizerFlags().Expand() );

    wxStaticBoxSizer* pCPUCoreGridBoxSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Resulting CPU Cores Used In The Current Configuration:" ) );
    static const int s_CPUCoresPerRow = 4;
    wxFlexGridSizer* pCPUGridSizer = new wxFlexGridSizer( s_CPUCoresPerRow );
    const int logicalProcessorCount = info_.systemLogicalProcessorCount.read();
    for( int i = 0; i < logicalProcessorCount; i++ )
    {
        wxBitmapToggleButton* pBTB = new wxBitmapToggleButton( pCPUCoreGridBoxSizer->GetStaticBox(), wxID_ANY, wxBitmap::NewFromPNGData( checkbox_undefined_png, sizeof( checkbox_undefined_png ) ).ConvertToImage().Scale( 16, 16 ), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxString::Format( wxT( "CPU %d" ), i ) );
        pBTB->Enable( false );
        pBTB->SetBitmapDisabled( *pToggleButtonUndefined_ );
        pBTB->SetLabel( wxString::Format( wxT( "Core %d" ), i ) );
        CPUCores_.push_back( pBTB );
        pCPUGridSizer->Add( pBTB );
    }
    for( int i = 0; i < s_CPUCoresPerRow; i++ )
    {
        pCPUGridSizer->AddGrowableCol( i, 1 );
    }
    pCPUCoreGridBoxSizer->Add( pCPUGridSizer, wxSizerFlags().Expand() );

    wxStaticBoxSizer* pLegendGridBoxSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Symbols:" ) );
    wxStaticText* pSTChecked = new wxStaticText( pPanel, wxID_ANY, wxString( wxT( "Currently used to process received image data network packets" ) ) );
    wxStaticText* pSTUnchecked = new wxStaticText( pPanel, wxID_ANY, wxString( wxT( "Currently NOT used to process received image data network packets due to current settings" ) ) );
    wxStaticText* pSTUndefined = new wxStaticText( pPanel, wxID_ANY, wxString( wxT( "Base CPU core as defined by the settings of the NIC driver" ) ) );
    wxStaticText* pSTUnavailable = new wxStaticText( pPanel, wxID_ANY, wxString( wxT( "Logical CPU core (e.g. Hyper-Threading core, not usable to receive image data network packets)" ) ) );

    wxStaticBitmap* checkedStaticBitmap = new wxStaticBitmap( pPanel, wxID_ANY, wxBitmap( *pToggleButtonChecked_ ), wxDefaultPosition, wxDefaultSize, 0 );
    wxStaticBitmap* uncheckedStaticBitmap = new wxStaticBitmap( pPanel, wxID_ANY, wxBitmap( *pToggleButtonUnchecked_ ), wxDefaultPosition, wxDefaultSize, 0 );
    wxStaticBitmap* undefinedStaticBitmap = new wxStaticBitmap( pPanel, wxID_ANY, wxBitmap( *pToggleButtonUndefined_ ), wxDefaultPosition, wxDefaultSize, 0 );
    wxStaticBitmap* unavailableStaticBitmap = new wxStaticBitmap( pPanel, wxID_ANY, wxBitmap( *pToggleButtonUnavailable_ ), wxDefaultPosition, wxDefaultSize, 0 );

    wxFlexGridSizer* pLegendElementGridSizer = new wxFlexGridSizer( 2 );
    pLegendElementGridSizer->SetHGap( 12 );
    pLegendElementGridSizer->SetVGap( 3 );
    pLegendElementGridSizer->Add( checkedStaticBitmap );
    pLegendElementGridSizer->Add( pSTChecked );
    pLegendElementGridSizer->Add( uncheckedStaticBitmap );
    pLegendElementGridSizer->Add( pSTUnchecked );
    pLegendElementGridSizer->Add( undefinedStaticBitmap );
    pLegendElementGridSizer->Add( pSTUndefined );
    pLegendElementGridSizer->Add( unavailableStaticBitmap );
    pLegendElementGridSizer->Add( pSTUnavailable );
    pLegendGridBoxSizer->Add( pLegendElementGridSizer );

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pControlsSizer, wxSizerFlags( 2 ).Expand().DoubleBorder() );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pCPUCoreGridBoxSizer, wxSizerFlags( 2 ).Expand().DoubleBorder() );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pLegendGridBoxSizer, wxSizerFlags( 2 ).Expand().DoubleBorder() );
    pTopDownSizer->AddSpacer( 10 );
    AddButtons( pPanel, pTopDownSizer );
    FinalizeDlgCreation( pPanel, pTopDownSizer );

    Bind( wxEVT_SPINCTRL, &WizardMultiCoreAcquisition::OnSCChanged, this, widSCCoreCount );
    Bind( wxEVT_SPINCTRL, &WizardMultiCoreAcquisition::OnSCChanged, this, widSCCoreSwitchInterval );
    Bind( wxEVT_SPINCTRL, &WizardMultiCoreAcquisition::OnSCChanged, this, widSCFirstCoreIndex );
#ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL // Unfortunately on Linux wxWidgets 2.6.x - ??? handling these messages will cause problems, while on Windows not doing so will not always update the GUI as desired :-(
    Bind( wxEVT_TEXT, &WizardMultiCoreAcquisition::OnSCTextChanged, this, widSCCoreCount );
    Bind( wxEVT_TEXT, &WizardMultiCoreAcquisition::OnSCTextChanged, this, widSCCoreSwitchInterval );
    Bind( wxEVT_TEXT, &WizardMultiCoreAcquisition::OnSCTextChanged, this, widSCFirstCoreIndex );
#endif // #ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL

    SetupDlgControls();
}

//-----------------------------------------------------------------------------
void WizardMultiCoreAcquisition::OnClose( wxCloseEvent& e )
//-----------------------------------------------------------------------------
{
    CloseDlg( wxID_CANCEL );
    Hide();
    if( e.CanVeto() )
    {
        e.Veto();
    }
}

//-----------------------------------------------------------------------------
void WizardMultiCoreAcquisition::CloseDlg( int result )
//-----------------------------------------------------------------------------
{
    if( result != wxID_OK )
    {
        pParent_->EnsureAcquisitionState( false );
        dsm_.mvMultiCoreAcquisitionEnable.write( boMultiCoreAcquisitionEnableStateOnEnter_ ? bTrue : bFalse );
    }
    pParent_->RemoveCaptureSettingFromStack( pDev_, pFI_, result != wxID_OK );
    pParent_->EnsureAcquisitionState( ( result == wxID_OK ) ? true : boAcquisitionStateOnCancel_ );
}

//-----------------------------------------------------------------------------
void WizardMultiCoreAcquisition::RefreshControls( void )
//-----------------------------------------------------------------------------
{
    SetupDlgControls();
}

//-----------------------------------------------------------------------------
void WizardMultiCoreAcquisition::SetupDlgControls( void )
//-----------------------------------------------------------------------------
{
    dsm_.mvMultiCoreAcquisitionEnable.write( bTrue );
    const unsigned int baseCPUCore( static_cast<unsigned int>( dsm_.mvMultiCoreAcquisitionBaseCore.read() ) );
    const unsigned int firstCPUCoreIndex = pSCFirstCoreIndex_->GetValue();
    const int logicalProcessorCount = info_.systemLogicalProcessorCount.read();
    const int physicalProcessorCount = info_.systemPhysicalProcessorCount.read();
    const int CPUCoreIncrement = logicalProcessorCount / physicalProcessorCount;
    const unsigned int start = ( baseCPUCore + firstCPUCoreIndex ) * CPUCoreIncrement;
    const unsigned int end = ( baseCPUCore + firstCPUCoreIndex + pSCCoreCount_->GetValue() ) * CPUCoreIncrement;

    pSCCoreCount_->SetRange( pSCCoreCount_->GetMin(), static_cast<int>( dsm_.mvMultiCoreAcquisitionCoreCount.getMaxValue() ) );
    pSCFirstCoreIndex_->SetRange( pSCFirstCoreIndex_->GetMin(), static_cast< int >( dsm_.mvMultiCoreAcquisitionFirstCoreIndex.getMaxValue() ) );
    pSCCoreSwitchInterval_->Enable( pSCCoreCount_->GetMax() > 1 );

    if( pSCFirstCoreIndex_->GetMax() == 0 )
    {
        pSCFirstCoreIndex_->SetToolTip( wxString( wxT( "Based on the current settings the first CPU core can not be modified as ALL CPU cores are currently used to process the incoming data." ) ) );
    }
    else
    {
        pSCFirstCoreIndex_->SetToolTip( wxString( wxT( "Specifies the offset to the base CPU core configured in the NIC driver configuration. Only physical cores will be included." ) ) );
    }

    for( vector<wxBitmapToggleButton*>::size_type i = 0; i < static_cast<vector<wxBitmapToggleButton*>::size_type>( logicalProcessorCount ); i++ )
    {
        if( i == baseCPUCore )
        {
            if( ( i >= start ) && ( i < end ) && ( ( i % CPUCoreIncrement ) == 0 ) )
            {
                CPUCores_[i]->SetBitmapDisabled( *pToggleButtonCheckedUndefined_ );
            }
            else
            {
                CPUCores_[i]->SetBitmapDisabled( *pToggleButtonUncheckedUndefined_ );
            }
        }
        else if( ( i >= start ) && ( i < end ) && ( ( i % CPUCoreIncrement ) == 0 ) )
        {
            CPUCores_[i]->SetBitmapDisabled( *pToggleButtonChecked_ );
        }
        else if( ( ( i % CPUCoreIncrement ) != 0 ) )
        {
            CPUCores_[i]->SetBitmapDisabled( *pToggleButtonUnavailable_ );
        }
        else
        {
            CPUCores_[i]->SetBitmapDisabled( *pToggleButtonUnchecked_ );
        }
    }
}
