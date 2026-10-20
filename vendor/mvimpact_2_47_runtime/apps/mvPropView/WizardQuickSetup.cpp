#include <apps/Common/wxAbstraction.h>
#include <apps/Common/exampleHelper.h>
#include <common/STLHelper.h>
#include "GlobalDataStorage.h"
#include "icons.h"
#include <math.h> // for 'pow'
#include <algorithm> // for 'sort'
#include "PropData.h"
#include "WizardQuickSetup.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/slider.h>
#include <wx/tglbtn.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;

//=============================================================================
//============== Implementation WizardQuickSetup ==============================
//=============================================================================
BEGIN_EVENT_TABLE( WizardQuickSetup, OkAndCancelDlg )
    EVT_BUTTON( widBtnPresetColorCamera, WizardQuickSetup::OnBtnPresetColorCamera )
    EVT_BUTTON( widBtnPresetColorHost, WizardQuickSetup::OnBtnPresetColorHost )
    EVT_BUTTON( widBtnPresetGray, WizardQuickSetup::OnBtnPresetGray )
    EVT_BUTTON( widBtnPresetFactory, WizardQuickSetup::OnBtnPresetFactory )
    EVT_BUTTON( widBtnClearHistory, WizardQuickSetup::OnBtnClearHistory )
    EVT_CHECKBOX( widCBShowDialogAtStartup, WizardQuickSetup::OnCBShowOnUseDevice )
    EVT_TOGGLEBUTTON( widBtnExposureAuto, WizardQuickSetup::OnBtnExposureAuto )
    EVT_TOGGLEBUTTON( widBtnGainAuto, WizardQuickSetup::OnBtnGainAuto )
    EVT_TOGGLEBUTTON( widBtnGamma, WizardQuickSetup::OnBtnGamma )
    EVT_TOGGLEBUTTON( widBtnWhiteBalanceAuto, WizardQuickSetup::OnBtnWhiteBalanceAuto )
    EVT_TOGGLEBUTTON( widBtnCCM, WizardQuickSetup::OnBtnCCM )
    EVT_TOGGLEBUTTON( widBtnFrameRateMaximum, WizardQuickSetup::OnBtnFrameRateMaximum )
    EVT_TOGGLEBUTTON( widBtnHistory, WizardQuickSetup::OnBtnHistory )
    EVT_COMMAND_SCROLL( widSLOptimization, WizardQuickSetup::OnSLOptimization )
    EVT_COMMAND_SCROLL( widSLExposure, WizardQuickSetup::OnSLExposure )
    EVT_COMMAND_SCROLL( widSLGain, WizardQuickSetup::OnSLGain )
    EVT_COMMAND_SCROLL( widSLBlackLevel, WizardQuickSetup::OnSLBlackLevel )
    EVT_COMMAND_SCROLL( widSLWhiteBalanceR, WizardQuickSetup::OnSLWhiteBalanceR )
    EVT_COMMAND_SCROLL( widSLWhiteBalanceB, WizardQuickSetup::OnSLWhiteBalanceB )
    EVT_COMMAND_SCROLL( widSLSaturation, WizardQuickSetup::OnSLSaturation )
    EVT_COMMAND_SCROLL( widSLFrameRate, WizardQuickSetup::OnSLFrameRate )
    EVT_CLOSE( WizardQuickSetup::OnClose )
    EVT_SPINCTRL( widSCExposure, WizardQuickSetup::OnSCExposureChanged )
    EVT_SPINCTRL( widSCGain, WizardQuickSetup::OnSCGainChanged )
    EVT_SPINCTRL( widSCBlackLevel, WizardQuickSetup::OnSCBlackLevelChanged )
    EVT_SPINCTRL( widSCWhiteBalanceR, WizardQuickSetup::OnSCWhiteBalanceRChanged )
    EVT_SPINCTRL( widSCWhiteBalanceB, WizardQuickSetup::OnSCWhiteBalanceBChanged )
    EVT_SPINCTRL( widSCSaturation, WizardQuickSetup::OnSCSaturationChanged )
    EVT_SPINCTRL( widSCFrameRate, WizardQuickSetup::OnSCFrameRateChanged )
#ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL // Unfortunately on Linux wxWidgets 2.6.x - ??? handling these messages will cause problems, while on Windows not doing so will not always update the GUI as desired :-(
    EVT_TEXT_ENTER( widSCExposure, WizardQuickSetup::OnSCExposureTextChanged )
    EVT_TEXT_ENTER( widSCGain, WizardQuickSetup::OnSCGainTextChanged )
    EVT_TEXT_ENTER( widSCBlackLevel, WizardQuickSetup::OnSCBlackLevelTextChanged )
    EVT_TEXT_ENTER( widSCWhiteBalanceR, WizardQuickSetup::OnSCWhiteBalanceRTextChanged )
    EVT_TEXT_ENTER( widSCWhiteBalanceB, WizardQuickSetup::OnSCWhiteBalanceBTextChanged )
    EVT_TEXT_ENTER( widSCSaturation, WizardQuickSetup::OnSCSaturationTextChanged )
    EVT_TEXT_ENTER( widSCFrameRate, WizardQuickSetup::OnSCFrameRateTextChanged )
#endif // #ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL
END_EVENT_TABLE()

const double WizardQuickSetup::EXPOSURE_SLIDER_GAMMA_ = 2.;
const double WizardQuickSetup::SLIDER_GRANULARITY_ = 100.;
const double WizardQuickSetup::GAMMA_CORRECTION_VALUE_ = 2.2;

//-----------------------------------------------------------------------------
class SuspendAcquisitionScopeLock
//-----------------------------------------------------------------------------
{
    PropViewFrame* pParent_;
public:
    explicit SuspendAcquisitionScopeLock( PropViewFrame* pParent )
        : pParent_( pParent )
    {
        pParent_->EnsureAcquisitionState( false );
    }
    ~SuspendAcquisitionScopeLock()
    {
        pParent_->EnsureAcquisitionState( true );
    }
};

//-----------------------------------------------------------------------------
WizardQuickSetup::WizardQuickSetup( PropViewFrame* pParent, const wxString& title, Device* pDev, FunctionInterface* pFI, bool boAutoConfigureOnStart )
    : OkAndCancelDlg( pParent, widMainFrame, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX ),
      pTopDownSizer_( 0 ), pBtnPresetColorHost_( 0 ), pBtnPresetColorCamera_( 0 ), pBtnPresetFactory_( 0 ), pBtnPresetGray_( 0 ), pSLOptimization_( 0 ),
      pStaticBitmapQuality_( 0 ), pStaticBitmapSpeed_( 0 ), pSLExposure_( 0 ), pSCExposure_( 0 ), pBtnExposureAuto_( 0 ),
      pCBShowDialogAtStartup_( 0 ), boGUILocked_( true ), boAutoConfigureOnStart_( boAutoConfigureOnStart ), pDev_( pDev ),
      pFI_( pFI ), pIP_( new ImageProcessing( pDev ) ), boLastWBChannelRead_( wbcRed ), pParentPropViewFrame_( pParent ), propertyChangeHistory_(), pID_( 0 )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pPresetsSizer                   | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pParametersSizer                | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pSettingsSizer                  | |
        | |---------------------------------| |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
        */
    wxPanel* pPanel = new wxPanel( this );

    // In the future, if GUI layout problems occur, a FlexGridSizer approach has to be considered!
    // 'Presets' controls
    pBtnPresetColorHost_ = new wxBitmapButton( pPanel, widBtnPresetColorHost, PNGToIconImage( qsw_computer_disabled_png, sizeof( qsw_computer_disabled_png ), false ), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "High-Quality color" ) );
    pBtnPresetColorHost_->SetToolTip( wxT( "Will setup the device for color image capture. Color processing will be done on host side.\nCurrent settings will be overwritten!\n\nThis preset will cause the color processing to be done on host side.\nThe CPU load will be higher than if the color processing is done on camera side, but the\nneeded bandwidth be only a third of the camera based color processing." ) );
    pBtnPresetColorCamera_ = new wxBitmapButton( pPanel, widBtnPresetColorCamera, PNGToIconImage( qsw_camera_disabled_png, sizeof( qsw_camera_disabled_png ), false ), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "High-Quality color" ) );
    pBtnPresetColorCamera_->SetToolTip( wxT( "Will setup the device for color image capture. Color processing will be done on camera side.\nCurrent settings will be overwritten!\n\nThis preset will cause the color processing to be done within the camera which will\nhave a positive impact on the host's CPU load. Since it is necessary to deliver the image data in\nRGB pixel format the necessary bandwidth will be three times as much as if a Bayer format is used." ) );
    pBtnPresetGray_ = new wxBitmapButton( pPanel, widBtnPresetGray, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_GreyPreset ), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "High-Quality grayscale" ) );
    pBtnPresetGray_->SetToolTip( wxT( "Will setup the device for gray-scale image capture.\nCurrent settings will be overwritten!" ) );
    pBtnPresetFactory_ = new wxBitmapButton( pPanel, widBtnPresetFactory, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_FactoryPreset ), wxDefaultPosition, wxDefaultSize, wxBU_AUTODRAW, wxDefaultValidator, wxT( "Factory preset" ) );
    pBtnPresetFactory_->SetToolTip( wxT( "Will restore the factory default settings for this device!\n" ) );

    wxFlexGridSizer* pColorHostFlexGridSizer = new wxFlexGridSizer( 3 );
    pColorHostFlexGridSizer->AddSpacer( 10 );
    pColorHostFlexGridSizer->Add( pBtnPresetColorHost_, wxSizerFlags().Center() );
    pColorHostFlexGridSizer->AddSpacer( 10 );
    pColorHostFlexGridSizer->AddSpacer( 10 );
    pColorHostFlexGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Color (Host)" ) ), wxSizerFlags().Center() );

    wxFlexGridSizer* pColorCameraFlexGridSizer = new wxFlexGridSizer( 3 );
    pColorCameraFlexGridSizer->AddSpacer( 10 );
    pColorCameraFlexGridSizer->Add( pBtnPresetColorCamera_, wxSizerFlags().Center() );
    pColorCameraFlexGridSizer->AddSpacer( 10 );
    pColorCameraFlexGridSizer->AddSpacer( 10 );
    pColorCameraFlexGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Color (Camera)" ) ), wxSizerFlags().Center() );

    wxFlexGridSizer* pGrayscaleFlexGridSizer = new wxFlexGridSizer( 3 );
    pGrayscaleFlexGridSizer->AddSpacer( 10 );
    pGrayscaleFlexGridSizer->Add( pBtnPresetGray_, wxSizerFlags().Center() );
    pGrayscaleFlexGridSizer->AddSpacer( 10 );
    pGrayscaleFlexGridSizer->AddSpacer( 10 );
    pGrayscaleFlexGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Grayscale" ) ), wxSizerFlags().Center() );
    pGrayscaleFlexGridSizer->AddSpacer( 5 );

    pSLOptimization_ = new wxSlider( pPanel, widSLOptimization, 0, 0, 2, wxDefaultPosition, wxSize( 120, -1 ), wxSL_HORIZONTAL );
    pStaticBitmapQuality_ = new wxStaticBitmap( pPanel, widStaticBitmapQuality, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Diamond ), wxDefaultPosition, wxDefaultSize, 0, wxT( "Quality" ) );
    pStaticBitmapQuality_->SetToolTip( wxT( "Will setup the device for optimal image fidelity.\nCurrent settings will be overwritten!" ) );
    pStaticBitmapSpeed_ = new wxStaticBitmap( pPanel, widStaticBitmapSpeed, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Tacho ), wxDefaultPosition, wxDefaultSize, 0, wxT( "Speed" ) );
    pStaticBitmapSpeed_->SetToolTip( wxT( "Will setup the device for maximum speed.\nCurrent settings will be overwritten!" ) );

    wxBoxSizer* pSliderDisplacementSizer = new wxBoxSizer( wxVERTICAL );
    pSliderDisplacementSizer->AddSpacer( 5 );
    pSliderDisplacementSizer->Add( pSLOptimization_ );

    wxFlexGridSizer* pSliderFlexGridSizer = new wxFlexGridSizer( 3 );
    pSliderFlexGridSizer->AddSpacer( 1 );
    pSliderFlexGridSizer->AddSpacer( 1 );
    pSliderFlexGridSizer->AddSpacer( 1 );
    pSliderFlexGridSizer->Add( pStaticBitmapQuality_, wxSizerFlags().Center() );
    pSliderFlexGridSizer->Add( pSliderDisplacementSizer );
    pSliderFlexGridSizer->Add( pStaticBitmapSpeed_, wxSizerFlags().Center() );
    pSliderFlexGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "Best Quality" ) ), wxSizerFlags().Center() );
    pSliderFlexGridSizer->AddSpacer( 10 );
    pSliderFlexGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( "Highest Speed" ) ), wxSizerFlags().Center() );

    wxFlexGridSizer* pFactoryFlexGridSizer = new wxFlexGridSizer( 3 );
    pFactoryFlexGridSizer->AddSpacer( 10 );
    pFactoryFlexGridSizer->Add( pBtnPresetFactory_, wxSizerFlags().Center() );
    pFactoryFlexGridSizer->AddSpacer( 10 );
    pFactoryFlexGridSizer->AddSpacer( 10 );
    pFactoryFlexGridSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Factory" ) ), wxSizerFlags().Center() );
    pFactoryFlexGridSizer->AddSpacer( 10 );

    wxBoxSizer* pPresetsSizer = new wxStaticBoxSizer( wxHORIZONTAL, pPanel, wxT( "Presets: " ) );
    pPresetsSizer->Add( pColorHostFlexGridSizer );
    pPresetsSizer->Add( pColorCameraFlexGridSizer );
    pPresetsSizer->Add( pGrayscaleFlexGridSizer );
    pPresetsSizer->AddSpacer( 52 );
    pPresetsSizer->Add( pSliderFlexGridSizer );
    pPresetsSizer->AddSpacer( 46 );
    pPresetsSizer->AddStretchSpacer( 100 );
    pPresetsSizer->Add( pFactoryFlexGridSizer );

    // 'Parameters' controls
    pSLExposure_ = new wxSlider( pPanel, widSLExposure, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSCExposure_ = new wxSpinCtrlDbl();
    pSCExposure_->SetMode( mDouble );
    pSCExposure_->Create( pPanel, widSCExposure, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, -10., 10., 1., 100., wxSPINCTRLDBL_AUTODIGITS, wxT( "%.0f" ) );
    pBtnExposureAuto_ = new wxToggleButton( pPanel, widBtnExposureAuto, wxT( "Auto" ) );
    pBtnExposureAuto_->SetToolTip( wxT( "Toggles Auto-Exposure" ) );

    wxBoxSizer* pExposureControlsSizer = new wxBoxSizer( wxHORIZONTAL );
    pExposureControlsSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Exposure [us]:" ) ), wxSizerFlags( 3 ).Expand() );
    pExposureControlsSizer->Add( pSCExposure_, wxSizerFlags().Expand() );
    pExposureControlsSizer->Add( pSLExposure_, wxSizerFlags( 6 ).Expand() );
    pExposureControlsSizer->Add( pBtnExposureAuto_, wxSizerFlags().Expand() );

    pSLGain_ = new wxSlider( pPanel, widSLGain, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSCGain_ = new wxSpinCtrlDbl();
    pSCGain_->SetMode( mDouble );
    pSCGain_->Create( pPanel, widSCGain, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, -10., 10., 1., 0.001, wxSPINCTRLDBL_AUTODIGITS, wxT( "%.3f" ) );
    pBtnGainAuto_ = new wxToggleButton( pPanel, widBtnGainAuto, wxT( "Auto" ) );
    pBtnGainAuto_->SetToolTip( wxT( "Toggles Auto-Gain" ) );

    wxBoxSizer* pGainControlsSizer = new wxBoxSizer( wxHORIZONTAL );
    pGainControlsSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Gain [dB]:" ) ), wxSizerFlags( 3 ).Expand() );
    pGainControlsSizer->Add( pSCGain_, wxSizerFlags().Expand() );
    pGainControlsSizer->Add( pSLGain_, wxSizerFlags( 6 ).Expand() );
    pGainControlsSizer->Add( pBtnGainAuto_, wxSizerFlags().Expand() );

    pSLBlackLevel_ = new wxSlider( pPanel, widSLBlackLevel, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSCBlackLevel_ = new wxSpinCtrlDbl();
    pSCBlackLevel_->SetMode( mDouble );
    pSCBlackLevel_->Create( pPanel, widSCBlackLevel, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, -10., 10., 1., 0.001, wxSPINCTRLDBL_AUTODIGITS, wxT( "%.3f" ) );

    wxBoxSizer* pBlackLevelControlsSizer = new wxBoxSizer( wxHORIZONTAL );
    pBlackLevelControlsSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Black Level [%]:" ) ), wxSizerFlags( 3 ).Expand() );
    pBlackLevelControlsSizer->Add( pSCBlackLevel_, wxSizerFlags().Expand() );
    pBlackLevelControlsSizer->Add( pSLBlackLevel_, wxSizerFlags( 6 ).Expand() );
    pBlackLevelControlsSizer->Add( pBtnGainAuto_->GetSize().GetWidth(), 0, 0 );

    pSLSaturation_ = new wxSlider( pPanel, widSLSaturation, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSCSaturation_ = new wxSpinCtrlDbl();
    pSCSaturation_->SetMode( mDouble );
    pSCSaturation_->Create( pPanel, widSCSaturation, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, 1., 200., 100., 0.01, wxSPINCTRLDBL_AUTODIGITS, wxT( "%.2f" ) );

    wxBoxSizer* pSaturationControlsSizer = new wxBoxSizer( wxHORIZONTAL );
    pSaturationControlsSizer->Add( new wxStaticText( pPanel, wxID_ANY, wxT( " Saturation [%]:" ) ), wxSizerFlags( 3 ).Expand() );
    pSaturationControlsSizer->Add( pSCSaturation_, wxSizerFlags().Expand() );
    pSaturationControlsSizer->Add( pSLSaturation_, wxSizerFlags( 6 ).Expand() );
    pSaturationControlsSizer->Add( pBtnGainAuto_->GetSize().GetWidth(), 0, 0 );

    const wxString ttWB( wxT( "This is in percent relative to the green gain" ) );
    pSLWhiteBalanceR_ = new wxSlider( pPanel, widSLWhiteBalanceR, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSLWhiteBalanceR_->SetToolTip( ttWB );
    pSCWhiteBalanceR_ = new wxSpinCtrlDbl();
    pSCWhiteBalanceR_->SetMode( mDouble );
    pSCWhiteBalanceR_->Create( pPanel, widSCWhiteBalanceR, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, 1., 500., 100., 0.01, wxSPINCTRLDBL_AUTODIGITS, wxT( "%.2f" ) );
    pSCWhiteBalanceR_->SetToolTip( ttWB );
    pSLWhiteBalanceB_ = new wxSlider( pPanel, widSLWhiteBalanceB, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSLWhiteBalanceB_->SetToolTip( ttWB );
    pSCWhiteBalanceB_ = new wxSpinCtrlDbl();
    pSCWhiteBalanceB_->SetMode( mDouble );
    pSCWhiteBalanceB_->Create( pPanel, widSCWhiteBalanceB, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, 1., 500., 100., 0.01, wxSPINCTRLDBL_AUTODIGITS, wxT( "%.2f" ) );
    pSCWhiteBalanceB_->SetToolTip( ttWB );
    pBtnWhiteBalanceAuto_ = new wxToggleButton( pPanel, widBtnWhiteBalanceAuto, wxT( "Auto" ) );
    pBtnWhiteBalanceAuto_->SetToolTip( wxT( "Toggles Auto White Balance" ) );

    wxBoxSizer* pWhiteBalanceDoubleSliderSizer = new wxBoxSizer( wxVERTICAL );
    pWhiteBalanceDoubleSliderSizer->AddSpacer( 5 );
    pWhiteBalanceDoubleSliderSizer->Add( pSLWhiteBalanceR_, wxSizerFlags( 6 ).Expand() );
    pWhiteBalanceDoubleSliderSizer->AddSpacer( 5 );
    pWhiteBalanceDoubleSliderSizer->Add( pSLWhiteBalanceB_, wxSizerFlags( 6 ).Expand() );

    wxBoxSizer* pWhiteBalanceDoubleSpinControlSizer = new wxBoxSizer( wxVERTICAL );
    pWhiteBalanceDoubleSpinControlSizer->Add( pSCWhiteBalanceR_, wxSizerFlags().Expand() );
    pWhiteBalanceDoubleSpinControlSizer->AddSpacer( 5 );
    pWhiteBalanceDoubleSpinControlSizer->Add( pSCWhiteBalanceB_, wxSizerFlags().Expand() );

    wxBoxSizer* pWhiteBalanceDoubleTextControlSizer = new wxBoxSizer( wxVERTICAL );
    wxStaticText* pSTWBR = new wxStaticText( pPanel, wxID_ANY, wxT( " White Balance R [%]:\n" ) );
    pSTWBR->SetToolTip( ttWB );
    wxStaticText* pSTWBB = new wxStaticText( pPanel, wxID_ANY, wxT( " White Balance B [%]:\n" ) );
    pSTWBB->SetToolTip( ttWB );
    pWhiteBalanceDoubleTextControlSizer->Add( pSTWBR, wxSizerFlags( 3 ) );
    pWhiteBalanceDoubleTextControlSizer->Add( pSTWBB, wxSizerFlags( 3 ) );

    wxBoxSizer* pWhiteBalanceControlsSizer = new wxBoxSizer( wxHORIZONTAL );
    pWhiteBalanceControlsSizer->Add( pWhiteBalanceDoubleTextControlSizer, wxSizerFlags( 2 ).Expand() );
    pWhiteBalanceControlsSizer->AddSpacer( 28 );
    pWhiteBalanceControlsSizer->Add( pWhiteBalanceDoubleSpinControlSizer );
    pWhiteBalanceControlsSizer->Add( pWhiteBalanceDoubleSliderSizer, wxSizerFlags( 5 ).Expand() );
    pWhiteBalanceControlsSizer->Add( pBtnWhiteBalanceAuto_, wxSizerFlags().Top().Expand() );

    pBtnCCM_ = new wxToggleButton( pPanel, widBtnCCM, wxT( "Color+" ) );
    pBtnCCM_->SetToolTip( wxT( "Will apply sensor-specific colorspace transformations. Only use with a monitor configured to display sRGB!" ) );
    pBtnGamma_ = new wxToggleButton( pPanel, widBtnGamma, wxT( "Gamma" ) );
    pBtnGamma_->SetToolTip( wxT( "Will apply a gamma-correction transformation with a gamma value of 2.2" ) );

    wxBoxSizer* pButtonsSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonsSizer->Add( pBtnGamma_, wxSizerFlags().Expand() );
    pButtonsSizer->AddSpacer( 10 );
    pButtonsSizer->Add( pBtnCCM_, wxSizerFlags().Expand() );

    pStaticBitmapWarning_ = new wxStaticBitmap( pPanel, widStaticBitmapWarning, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Warning ), wxDefaultPosition, wxDefaultSize, 0, wxT( "" ) );
    pSLFrameRate_ = new wxSlider( pPanel, widSLFrameRate, 1000, -10000, 10000, wxDefaultPosition, wxSize( 250, -1 ), wxSL_HORIZONTAL );
    pSCFrameRate_ = new wxSpinCtrlDbl();
    pSCFrameRate_->SetMode( mDouble );
    pSCFrameRate_->Create( pPanel, widSCFrameRate, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, -10., 10., 1., 0.001, wxSPINCTRLDBL_AUTODIGITS, wxT( "%.3f" ) );
    pBtnFrameRateMaximum_ = new wxToggleButton( pPanel, widBtnFrameRateMaximum, wxT( "Maximum" ) );
    pBtnFrameRateMaximum_->SetToolTip( wxT( "'Will toggle between maximum frame rate and fixed frame rate" ) );

    wxBoxSizer* pFrameRateControlsSizer = new wxBoxSizer( wxHORIZONTAL );
    pFrameRateControlStaticText_ = new wxStaticText( pPanel, wxID_ANY, wxT( " Frame Rate [Hz]:" ) );
    pFrameRateControlsSizer->Add( pFrameRateControlStaticText_, wxSizerFlags( 2 ).Expand() );
    pFrameRateControlsSizer->AddSpacer( 20 );
    pFrameRateControlsSizer->Add( pStaticBitmapWarning_ );
    pFrameRateControlsSizer->AddSpacer( 2 );
    pFrameRateControlsSizer->Add( pSCFrameRate_, wxSizerFlags().Expand() );
    pFrameRateControlsSizer->Add( pSLFrameRate_, wxSizerFlags( 6 ).Expand() );
    pFrameRateControlsSizer->Add( pBtnFrameRateMaximum_, wxSizerFlags().Expand() );

    wxBoxSizer* pParametersSizer = new wxStaticBoxSizer( wxVERTICAL, pPanel, wxT( "Parameters: " ) );
    pParametersSizer->Add( pExposureControlsSizer, wxSizerFlags().Expand() );
    pParametersSizer->AddSpacer( 12 );
    pParametersSizer->Add( pGainControlsSizer, wxSizerFlags().Expand() );
    pParametersSizer->AddSpacer( 12 );
    pParametersSizer->Add( pBlackLevelControlsSizer, wxSizerFlags().Expand() );
    pParametersSizer->AddSpacer( 12 );
    pParametersSizer->Add( pSaturationControlsSizer, wxSizerFlags().Expand() );
    pParametersSizer->AddSpacer( 12 );
    pParametersSizer->Add( pWhiteBalanceControlsSizer, wxSizerFlags().Expand() );
    pParametersSizer->AddSpacer( 5 );
    pParametersSizer->Add( pButtonsSizer, wxSizerFlags().Expand() );
    pParametersSizer->AddSpacer( 12 );
    pParametersSizer->Add( pFrameRateControlsSizer, wxSizerFlags().Expand() );

    // 'Settings' controls
    pCBShowDialogAtStartup_ = new wxCheckBox( pPanel, widCBShowDialogAtStartup, wxString( pParent->HasEnforcedQSWBehavior() ?
            wxT( "Show Quick Setup On Device Open ( Not available when the 'qsw' parameter is used! )" ) : wxT( "Show Quick Setup On Device Open" ) ) );
    pCBShowDialogAtStartup_->SetValue( pParent->GetAutoShowQSWOnUseDevice() );
    pCBShowDialogAtStartup_->Enable( !pParent->HasEnforcedQSWBehavior() );

    wxBoxSizer* pSettingsSizer = new wxStaticBoxSizer( wxHORIZONTAL, pPanel, wxT( "Settings: " ) );
    pSettingsSizer->Add( pCBShowDialogAtStartup_, wxSizerFlags().Expand() );

    // History controls
    pTCHistory_ = new wxTextCtrl( pPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 200, 240 ), wxTE_DONTWRAP | wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );
    pTCHistory_->Hide();
    pBtnHistory_ = new wxToggleButton( pPanel, widBtnHistory, wxT( "&History >>>" ) );
    pBtnHistory_->SetToolTip( wxT( "Shows/Hides the History Window" ) );
    pBtnClearHistory_ = new wxButton( pPanel, widBtnClearHistory, wxT( "Clear History" ) );
    pBtnClearHistory_->SetToolTip( wxT( "Clears the History Window" ) );
    pBtnClearHistory_->Hide();
    wxBoxSizer* pHistoryButtonsSizer_ = new wxBoxSizer( wxHORIZONTAL );
    pHistoryButtonsSizer_->Add( pBtnHistory_, wxSizerFlags().Left() );
    pHistoryButtonsSizer_->AddSpacer( 12 );
    pHistoryButtonsSizer_->Add( pBtnClearHistory_, wxSizerFlags().Left() );

    // putting it all together
    pTopDownSizer_ = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer_->AddSpacer( 12 );
    pTopDownSizer_->Add( pPresetsSizer, wxSizerFlags().Expand() );
    pTopDownSizer_->AddSpacer( 12 );
    pTopDownSizer_->Add( pParametersSizer, wxSizerFlags().Expand() );
    pTopDownSizer_->AddSpacer( 12 );
    pTopDownSizer_->Add( pSettingsSizer, wxSizerFlags().Expand() );
    pTopDownSizer_->AddSpacer( 12 );
    pTopDownSizer_->Add( pHistoryButtonsSizer_ );
    pTopDownSizer_->Add( pTCHistory_, wxSizerFlags().Expand() );
    pTopDownSizer_->AddSpacer( 12 );

    AddButtons( pPanel, pTopDownSizer_ );
    pBtnOk_->SetToolTip( wxT( "Applies Current Settings And Exits" ) );
    pBtnCancel_->SetToolTip( wxT( "Discards Current Settings And Exits" ) );

    wxBoxSizer* pOuterSizer = new wxBoxSizer( wxHORIZONTAL );
    pOuterSizer->AddSpacer( 5 );
    pOuterSizer->Add( pTopDownSizer_, wxSizerFlags().Expand() );
    pOuterSizer->AddSpacer( 5 );

    FinalizeDlgCreation( pPanel, pOuterSizer );

    pStaticBitmapQuality_->Connect( widStaticBitmapQuality, wxEVT_LEFT_DOWN, wxMouseEventHandler( WizardQuickSetup::OnLeftMouseButtonDown ), NULL, this );
    pStaticBitmapSpeed_->Connect( widStaticBitmapSpeed, wxEVT_LEFT_DOWN, wxMouseEventHandler( WizardQuickSetup::OnLeftMouseButtonDown ), NULL, this );

    boGUILocked_ = false;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::AnalyzeDeviceAndGatherInformation( void )
//-----------------------------------------------------------------------------
{
    try
    {
        pBtnPresetFactory_->Enable( HasFactoryDefault() );
        const bool hasColorFormat = HasColorFormat();
        featuresSupported_.boColorOptionsSupport = hasColorFormat;
        currentSettings_.boColorEnabled = hasColorFormat;
        featuresSupported_.boBlackLevelSupport = HasBlackLevel();
        featuresSupported_.boGainSupport = HasGain();
        featuresSupported_.boUnifiedGainSupport = HasUnifiedGain();
        featuresSupported_.boAutoExposureSupport = HasAEC();
        featuresSupported_.boAutoGainSupport = HasAGC();
        featuresSupported_.boAutoWhiteBalanceSupport = HasAWB();
        featuresSupported_.boMaximumFrameRateSupport = HasMaximumFrameRate();
        featuresSupported_.boRegulateFrameRateSupport = HasFrameRateEnable();
        featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing = DoesDriverOfferColorCorrectionMatrixProcessing();
        featuresSupported_.boGammaSupport = HasNativeGamma();
        featuresSupported_.boDeviceSupportsColorCorrectionMatrixProcessing = HasNativeColorCorrection();
        if( !Supports12BitPixelFormats() )
        {
            pSLOptimization_->SetMax( 1 );
            pSLOptimization_->SetToolTip( wxT( "Configures the pixel format of the device.\n'Best Quality' will set 12 Bit.\n'Highest Speed' will set 8 Bit." ) );
        }
        else
        {
            pSLOptimization_->SetToolTip( wxT( "Configures the pixel format of the device.\n'Best Quality' will set 12 Bit.\nThe center position will set 10 Bit.\n'Highest Speed' will set 8 Bit." ) );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to analyze device (Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::CleanUp( void )
//-----------------------------------------------------------------------------
{
    DeleteInterfaceLayoutSpecificControls();
    DeleteElement( pID_ );
    DeleteElement( pIP_ );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplyExposure( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        try
        {
            SetExposureTime( pSCExposure_->GetValue() );
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to apply exposure time(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
    // The exposure is a special case. It influences the FrameRate properties, thus the FrameRate controls have to be redrawn
    if( !currentSettings_.boMaximumFrameRateEnabled )
    {
        SetupFrameRateControls();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplyGain( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        try
        {
            WriteUnifiedGainData( pSCGain_->GetValue() );
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to apply gain(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplyBlackLevel( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        try
        {
            WriteUnifiedBlackLevelData( pSCBlackLevel_->GetValue() );
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to apply gain(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplyWhiteBalance( TWhiteBalanceChannel channel )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ && ( featuresSupported_.boColorOptionsSupport ) )
    {
        try
        {
            SetWhiteBalance( channel, ( ( channel == wbcRed ) ? pSCWhiteBalanceR_->GetValue() : pSCWhiteBalanceB_->GetValue() ) / 100.0 );
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to apply white balance (Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplySaturation( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ && ( featuresSupported_.boColorOptionsSupport ) )
    {
        try
        {
            WriteSaturationData( pSCSaturation_->GetValue() );
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to apply saturation(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplyFrameRate( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        try
        {
            SetFrameRateEnable( true );
            SetFrameRate( pSCFrameRate_->GetValue() );
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to apply frame rate limit(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
        // Since frame rate controls are special due to their dynamic nature ( they depend on the exposure values )
        // many functions rely on the frame rate member of the Device Settings structure. Thus we have to keep it
        // up to date with every change.
        currentSettings_.frameRate = pSCFrameRate_->GetValue();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::DoSetupExposureControls( double exposureMin, double exposureMax, double exposure, bool boHasStepWidth, double increment )
//-----------------------------------------------------------------------------
{
    VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
    boGUILocked_ = true;
    pSLExposure_->SetRange( static_cast< int >( exposureMin ), static_cast< int >( exposureMax ) );
    pSCExposure_->SetRange( exposureMin, exposureMax );
    if( boHasStepWidth )
    {
        pSCExposure_->SetIncrement( increment );
    }
    pSCExposure_->SetValue( exposure );
    pSLExposure_->SetValue( ExposureToSliderValue( exposure ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::DoConfigureDriverBaseColorCorrection( bool boActive )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing )
    {
        LoggedWriteProperty( pIP_->colorTwistInputCorrectionMatrixEnable, boActive ? bTrue : bFalse );
        LoggedWriteProperty( pIP_->colorTwistEnable, boActive ? bTrue : bFalse );
        LoggedWriteProperty( pIP_->colorTwistOutputCorrectionMatrixEnable, boActive ? bTrue : bFalse );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::DoSetupGainControls( double gainUnifiedRangeMin, double gainUnifiedRangeMax, double gain )
//-----------------------------------------------------------------------------
{
    VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
    boGUILocked_ = true;
    pSLGain_->SetRange( static_cast< int >( gainUnifiedRangeMin * SLIDER_GRANULARITY_ ), static_cast< int >( gainUnifiedRangeMax * SLIDER_GRANULARITY_ ) );
    pSCGain_->SetRange( gainUnifiedRangeMin, gainUnifiedRangeMax );
    pSCGain_->SetIncrement( 1 / SLIDER_GRANULARITY_ );
    pSCGain_->SetValue( gain );
    pSLGain_->SetValue( static_cast< int >( gain * SLIDER_GRANULARITY_ ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::DoSetupBlackLevelControls( double blackLevelUnifiedRangeMin, double blackLevelUnifiedRangeMax, double blackLevel )
//-----------------------------------------------------------------------------
{
    VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
    boGUILocked_ = true;
    pSLBlackLevel_->SetRange( static_cast< int >( blackLevelUnifiedRangeMin * SLIDER_GRANULARITY_ ), static_cast< int >( blackLevelUnifiedRangeMax * SLIDER_GRANULARITY_ ) );
    pSCBlackLevel_->SetRange( blackLevelUnifiedRangeMin, blackLevelUnifiedRangeMax );
    pSCBlackLevel_->SetIncrement( 1 / SLIDER_GRANULARITY_ );
    pSCBlackLevel_->SetValue( blackLevel );
    pSLBlackLevel_->SetValue( static_cast< int >( blackLevel * SLIDER_GRANULARITY_ ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::DoSetupWhiteBalanceControls( double whiteBalanceRMin, double whiteBalanceRMax, double whiteBalanceR, double whiteBalanceBMin, double whiteBalanceBMax, double whiteBalanceB )
//-----------------------------------------------------------------------------
{
    VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
    boGUILocked_ = true;
    pSLWhiteBalanceR_->SetRange( static_cast< int >( whiteBalanceRMin * SLIDER_GRANULARITY_ ), static_cast< int >( whiteBalanceRMax * SLIDER_GRANULARITY_ ) );
    pSCWhiteBalanceR_->SetRange( whiteBalanceRMin, whiteBalanceRMax );
    pSCWhiteBalanceR_->SetIncrement( 1 / SLIDER_GRANULARITY_ );
    pSCWhiteBalanceR_->SetValue( whiteBalanceR );
    pSLWhiteBalanceR_->SetValue( static_cast< int >( whiteBalanceR * SLIDER_GRANULARITY_ ) );

    pSLWhiteBalanceB_->SetRange( static_cast< int >( whiteBalanceBMin * SLIDER_GRANULARITY_ ), static_cast< int >( whiteBalanceBMax * SLIDER_GRANULARITY_ ) );
    pSCWhiteBalanceB_->SetRange( whiteBalanceBMin, whiteBalanceBMax );
    pSCWhiteBalanceB_->SetIncrement( 1 / SLIDER_GRANULARITY_ );
    pSCWhiteBalanceB_->SetValue( whiteBalanceB );
    pSLWhiteBalanceB_->SetValue( static_cast< int >( whiteBalanceB * SLIDER_GRANULARITY_ ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::DoSetupFrameRateControls( double frameRateRangeMin, double frameRateRangeMax, double frameRate )
//-----------------------------------------------------------------------------
{
    VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
    boGUILocked_ = true;
    pSLFrameRate_->SetRange( static_cast< int >( frameRateRangeMin * SLIDER_GRANULARITY_ ), static_cast< int >( frameRateRangeMax * SLIDER_GRANULARITY_ ) );
    pSCFrameRate_->SetRange( frameRateRangeMin, frameRateRangeMax );
    pSCFrameRate_->SetIncrement( 1 / SLIDER_GRANULARITY_ );
    pSCFrameRate_->SetValue( frameRate );
    pSLFrameRate_->SetValue( static_cast< int >( frameRate * SLIDER_GRANULARITY_ ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnCBShowOnUseDevice( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    pParentPropViewFrame_->SetAutoShowQSWOnUseDevice( e.IsChecked() );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnClose( wxCloseEvent& )
//-----------------------------------------------------------------------------
{
    ShutdownDlg( wxID_CANCEL );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnPresetColorCamera( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    currentSettings_.boColorEnabled = true;
    currentSettings_.boCameraBasedColorProcessingEnabled = true;
    SetupPresetButtons( false, true );
    HandlePresetChanges();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnPresetColorHost( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    currentSettings_.boColorEnabled = true;
    currentSettings_.boCameraBasedColorProcessingEnabled = false;
    SetupPresetButtons( true, false );
    HandlePresetChanges();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnPresetGray( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    currentSettings_.boColorEnabled = false;
    currentSettings_.boCameraBasedColorProcessingEnabled = false;
    SetupPresetButtons( false, false );
    HandlePresetChanges();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ApplyPreset( bool boColor, TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    SuspendAcquisitionScopeLock suspendAcquisitionLock( pParentPropViewFrame_ );
    if( boColor )
    {
        SetOptimalPixelFormatColor( optimizationLevel );
    }
    else
    {
        SetOptimalPixelFormatGray( optimizationLevel );
    }
    SetupDevice();
    PresetHiddenAdjustments( boColor, optimizationLevel );
    SetupControls();

    WriteQuickSetupWizardLogMessage( wxString::Format( wxT( "Using %s-optimized %s presets for device %s(%s)" ), ( ( optimizationLevel == olBestQuality ) ? "quality" : "speed" ),
                                     ( boColor ? "color" : "grayscale" ), ConvertedString( currentProductString_ ).c_str(), ConvertedString( currentDeviceSerial_ ).c_str() ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnPresetCustom( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    wxMessageBox( wxT( "Nothing implemented for the 'Custom' preset so far" ), wxT( "Under Construction!" ), wxOK | wxICON_INFORMATION, this );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnPresetFactory( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const bool boShallDoFactoryReset = ShowFactoryResetPopup();
    if( boShallDoFactoryReset )
    {
        SuspendAcquisitionScopeLock suspendAcquisitionLock( pParentPropViewFrame_ );

        try
        {
            // Auto-gain has to manually be turned off prior to reverting to default, as the whole unified-gain functionality in combination with
            // the regular updates for the GUI introduces problems with the invalidation. Particularly it has been observed, that after a loading
            // of UserSetDefault, the auto-gain remains on auto, even though it should revert to off
            ConfigureGainAuto( false );
            RestoreFactoryDefault();
            SetAcquisitionFrameRateLimitMode();
        }
        catch( const ImpactAcquireException& e )
        {
            wxMessageBox( wxString::Format( wxT( "Failed to restore factory settings(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ), wxT( "Error" ), wxOK | wxICON_INFORMATION, this );
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to restore factory settings(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }

        pID_->restoreDefault();
        pIP_->restoreDefault();
        QueryInitialDeviceSettings();
        if( featuresSupported_.boColorOptionsSupport )
        {
            currentSettings_.boColorEnabled = true;
        }
        SetupDriverSettings();
        SetupControls();
        SetupPresetButtons( false, false );
        SelectLUTDependingOnPixelFormat();
        WriteQuickSetupWizardLogMessage( wxString::Format( wxT( "Restored factory presets for device %s(%s)" ), ConvertedString( currentProductString_ ).c_str(), ConvertedString( currentDeviceSerial_ ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnLeftMouseButtonDown( wxMouseEvent& e )
//-----------------------------------------------------------------------------
{
    switch( e.GetId() )
    {
    case widStaticBitmapQuality:
        pSLOptimization_->SetValue( pSLOptimization_->GetMin() );
        break;
    case widStaticBitmapSpeed:
        pSLOptimization_->SetValue( pSLOptimization_->GetMax() );
        break;
    }
    SetupOptimization();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ConfigureExposureAuto( bool boActive )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ && ( featuresSupported_.boAutoExposureSupport ) )
    {
        try
        {
            SetAutoExposure( boActive );
            if( boActive )
            {
                currentSettings_.exposureTime = pSCExposure_->GetValue();
            }
            else
            {
                UpdateExposureControlsFromCamera();
            }
            currentSettings_.boAutoExposureEnabled = boActive;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to modify driver properties(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ConfigureGainAuto( bool boActive )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ && ( featuresSupported_.boAutoGainSupport ) )
    {
        try
        {
            SetAutoGain( boActive );
            if( boActive )
            {
                currentSettings_.unifiedGain = pSCGain_->GetValue();
                // DigitalGain has to be set to 0, or else it's value will stack with the auto-gain values!
                WriteUnifiedGainData( 0 );
            }
            else
            {
                UpdateGainControlsFromCamera();
            }
            currentSettings_.boAutoGainEnabled = boActive;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to modify driver properties(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ConfigureGamma( bool boActive )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        try
        {
            SetGamma( boActive );
            currentSettings_.boGammaEnabled = boActive;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to modify driver properties(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ConfigureWhiteBalanceAuto( bool boActive )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ && ( featuresSupported_.boAutoWhiteBalanceSupport ) )
    {
        try
        {
            SetAutoWhiteBalance( boActive );
            if( boActive )
            {
                currentSettings_.whiteBalanceRed = pSCWhiteBalanceR_->GetValue();
                currentSettings_.whiteBalanceBlue = pSCWhiteBalanceB_->GetValue();
            }
            else
            {
                UpdateWhiteBalanceControlsFromCamera();
            }
            currentSettings_.boAutoWhiteBalanceEnabled = boActive;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to modify driver properties(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ConfigureCCM( bool boActive )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        try
        {
            DoConfigureColorCorrection( boActive );
            currentSettings_.boCCMEnabled = boActive;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to modify driver properties(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ConfigureFrameRateMaximum( bool boActive )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ && ( featuresSupported_.boMaximumFrameRateSupport ) )
    {
        try
        {
            DoConfigureFrameRateMaximum( boActive, pSCFrameRate_->GetValue() );
            currentSettings_.boMaximumFrameRateEnabled = boActive;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to modify driver properties(Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
}

//-----------------------------------------------------------------------------
double WizardQuickSetup::DoReadDriverBasedSaturationData( void ) const
//-----------------------------------------------------------------------------
{
    double currentSaturation = 0.;
    try
    {
        if( featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing )
        {
            currentSaturation = ( ( pIP_->colorTwistRow0.read( 0 ) - 0.299 ) / 0.701 ) * 100.0;
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to read saturation value(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return currentSaturation;
}

//-----------------------------------------------------------------------------
string WizardQuickSetup::DoWriteDriverBasedSaturationData( double saturation )
//-----------------------------------------------------------------------------
{
    ostringstream oss;
    try
    {
        if( featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing )
        {
            if( pIP_->colorTwistEnable.readS() != string( "On" ) )
            {
                LoggedWriteProperty( pIP_->colorTwistEnable, string( "On" ) );
            }
            pIP_->setSaturation( saturation / 100.0 );
        }
        oss << "Method setSaturation() called with parameter " << ( saturation / 100.0 ) << endl;
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write saturation value(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return oss.str();
}

//-----------------------------------------------------------------------------
string WizardQuickSetup::GetFullPropertyName( Property prop ) const
//-----------------------------------------------------------------------------
{
    DeviceComponentLocator locator( pDev_, dltSetting );
    Component it( prop.hObj() );
    wxString propertyPath;
    while( it.isValid() && ( it.hObj() != locator.hObj() ) )
    {
        propertyPath.Prepend( wxString::Format( wxT( "%s/" ), ConvertedString( it.name() ).c_str() ) );
        it = it.parent();
    }
    if( propertyPath.IsEmpty() )
    {
        return prop.name();
    }

    static const wxString prefixToRemove( wxT( "Base/" ) );
    return string( propertyPath.mb_str() ).substr( prefixToRemove.Length() ) + prop.name();
}

//-----------------------------------------------------------------------------
double WizardQuickSetup::GetWhiteBalanceDriver( TWhiteBalanceChannel channel )
//-----------------------------------------------------------------------------
{
    WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
    return ( channel == wbcRed ) ? wbs.redGain.read() : wbs.blueGain.read();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleExposureSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLExposure_->SetValue( ExposureToSliderValue( pSCExposure_->GetValue() ) );
        }
        ApplyExposure();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleGainSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLGain_->SetValue( static_cast<int>( pSCGain_->GetValue()*SLIDER_GRANULARITY_ ) );
        }
        ApplyGain();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleBlackLevelSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLBlackLevel_->SetValue( static_cast<int>( pSCBlackLevel_->GetValue()*SLIDER_GRANULARITY_ ) );
        }
        ApplyBlackLevel();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleWhiteBalanceRSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLWhiteBalanceR_->SetValue( static_cast<int>( pSCWhiteBalanceR_->GetValue()*SLIDER_GRANULARITY_ ) );
        }
        ApplyWhiteBalance( wbcRed );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleWhiteBalanceBSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLWhiteBalanceB_->SetValue( static_cast<int>( pSCWhiteBalanceB_->GetValue()*SLIDER_GRANULARITY_ ) );
        }
        ApplyWhiteBalance( wbcBlue );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleSaturationSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLSaturation_->SetValue( static_cast<int>( pSCSaturation_->GetValue()*SLIDER_GRANULARITY_ ) );
        }
        ApplySaturation();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandleFrameRateSpinControlChanges( void )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLFrameRate_->SetValue( static_cast<int>( pSCFrameRate_->GetValue()*SLIDER_GRANULARITY_ ) );
        }
        ApplyFrameRate();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::HandlePresetChanges( void )
//-----------------------------------------------------------------------------
{
    const bool boColorEnabled = currentSettings_.boColorEnabled;
    const TOptimizationLevel optimizationLevel = currentSettings_.optimizationLevel;
    ApplyPreset( boColorEnabled, optimizationLevel );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::QueryInitialDeviceSettings( void )
//-----------------------------------------------------------------------------
{
    currentSettings_.boAutoExposureEnabled = false;
    currentSettings_.boAutoGainEnabled = false;
    currentSettings_.boGammaEnabled = false;
    currentSettings_.boMaximumFrameRateEnabled = false;
    currentSettings_.boColorEnabled = false;
    currentSettings_.boCCMEnabled = false;
    currentSettings_.boDeviceSpecificOverlappedExposure = false;
    currentSettings_.imageFormatControlPixelFormat = "NotInitialised";

    QueryInterfaceLayoutSpecificSettings( currentSettings_ );
    if( HasUnifiedGain() )
    {
        currentSettings_.unifiedGain = ReadUnifiedGainData();
    }
    if( HasColorFormat() )
    {
        currentSettings_.boColorEnabled = true;
    }
    currentSettings_.saturation = 0.;
    currentSettings_.boCCMEnabled = GetCCMState();

    TryToReadFrameRate( currentSettings_.frameRate );

    if( HasFrameRateEnable() )
    {
        currentSettings_.boMaximumFrameRateEnabled = !GetFrameRateEnable();
    }

    currentSettings_.boGammaEnabled = IsGammaActive();
    currentSettings_.analogGainMin = analogGainMin_;
    currentSettings_.analogGainMax = analogGainMax_;
    currentSettings_.digitalGainMin = digitalGainMin_;
    currentSettings_.digitalGainMax = digitalGainMax_;
    currentSettings_.analogBlackLevelMin = analogBlackLevelMin_;
    currentSettings_.analogBlackLevelMax = analogBlackLevelMax_;

    currentSettings_.imageFormatControlPixelFormat = GetPixelFormat();
    currentSettings_.autoExposureUpperLimit = GetAutoExposureUpperLimit();
    currentSettings_.pixelClock = GetPixelClock();
    currentSettings_.imageFormatControlPixelFormat = GetPixelFormat();
    currentSettings_.boDeviceSpecificOverlappedExposure = GetExposureOverlapped();
    currentSettings_.maxFrameRateAtCurrentAOI = DetermineMaxFrameRateAtCurrentAOI( true );
    currentSettings_.maxAllowedHQFrameRateAtCurrentAOI = DetermineOptimalHQFrameRateAtCurrentAOI( true );

    currentSettings_.imageDestinationPixelFormat = pID_->pixelFormat.read();
}

//-----------------------------------------------------------------------------
double WizardQuickSetup::ExposureFromSliderValue( void ) const
//-----------------------------------------------------------------------------
{
    const int value = pSLExposure_->GetValue();
    const int valueMax = pSLExposure_->GetMax();
    return pow( static_cast< double >( value ) / static_cast< double >( valueMax ), EXPOSURE_SLIDER_GAMMA_ ) * static_cast< double >( valueMax );
}

//-----------------------------------------------------------------------------
int WizardQuickSetup::ExposureToSliderValue( const double exposure ) const
//-----------------------------------------------------------------------------
{
    const double exposureMax = pSCExposure_->GetMax();
    return static_cast< int >( pow( exposure / exposureMax, 1. / EXPOSURE_SLIDER_GAMMA_ ) * exposureMax );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLExposure( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCExposure_->SetValue( static_cast< int >( ExposureFromSliderValue() ) );
        }
        ApplyExposure();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLGain( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCGain_->SetValue( static_cast< double >( pSLGain_->GetValue() ) / SLIDER_GRANULARITY_ );
        }
        ApplyGain();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLBlackLevel( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCBlackLevel_->SetValue( static_cast< double >( pSLBlackLevel_->GetValue() ) / SLIDER_GRANULARITY_ );
        }
        ApplyBlackLevel();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLWhiteBalanceR( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCWhiteBalanceR_->SetValue( static_cast< double >( pSLWhiteBalanceR_->GetValue() ) / SLIDER_GRANULARITY_ );
        }
        ApplyWhiteBalance( wbcRed );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLWhiteBalanceB( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCWhiteBalanceB_->SetValue( static_cast< double >( pSLWhiteBalanceB_->GetValue() ) / SLIDER_GRANULARITY_ );
        }
        ApplyWhiteBalance( wbcBlue );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLSaturation( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCSaturation_->SetValue( static_cast< double >( pSLSaturation_->GetValue() ) / SLIDER_GRANULARITY_ );
        }
        ApplySaturation();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnSLFrameRate( wxScrollEvent& )
//-----------------------------------------------------------------------------
{
    if( !boGUILocked_ )
    {
        {
            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSCFrameRate_->SetValue( static_cast< double >( pSLFrameRate_->GetValue() ) / SLIDER_GRANULARITY_ );
        }
        ApplyFrameRate();
    }
}

//-----------------------------------------------------------------------------
double WizardQuickSetup::ReadUnifiedGainData( void )
//-----------------------------------------------------------------------------
{
    double currentGain = 0.0;
    try
    {
        currentGain = DoReadUnifiedGain();
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to read combined analog and digital gain values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return currentGain;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::WriteUnifiedGainData( double unifiedGain )
//-----------------------------------------------------------------------------
{
    try
    {
        DoWriteUnifiedGain( unifiedGain );
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write combined analog and digital gain values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
double WizardQuickSetup::ReadUnifiedBlackLevelData()
//-----------------------------------------------------------------------------
{
    double currentBlackLevel = 0.0;
    try
    {
        currentBlackLevel = DoReadUnifiedBlackLevel();
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to read combined analog and digital blackLevel values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return currentBlackLevel;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::WriteUnifiedBlackLevelData( double unifiedBlackLevel )
//-----------------------------------------------------------------------------
{
    try
    {
        DoWriteUnifiedBlackLevelData( unifiedBlackLevel );
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write combined analog and digital blackLevel values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::PresetHiddenAdjustments( bool boColor, TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    if( ( optimizationLevel == olBestQuality ) || ( optimizationLevel == olCompromiseQualityAndSpeed ) )
    {
        SetFrameRateEnable( true );
        SetFrameRate( currentSettings_.maxAllowedHQFrameRateAtCurrentAOI );
        currentSettings_.boMaximumFrameRateEnabled = false;
    }
    else if( featuresSupported_.boMaximumFrameRateSupport )
    {
        SetAcquisitionFrameRateLimitMode();
        currentSettings_.boMaximumFrameRateEnabled = true;
    }
    else
    {
        // By entering a big number the maxValue of the Frame rate will be written
        SetFrameRate( 10000. );
        currentSettings_.boMaximumFrameRateEnabled = false;
    }

    ConfigureGamma( boColor );
    SelectOptimalPixelClock( optimizationLevel );
    SelectOptimalAutoExposureSettings( ( optimizationLevel == olBestQuality ) || ( optimizationLevel == olCompromiseQualityAndSpeed ) );
    if( boColor )
    {
        WriteSaturationData( 100. );
        LoggedWriteProperty( pID_->pixelFormat, idpfAuto );
    }
    else
    {
        LoggedWriteProperty( pID_->pixelFormat, idpfMono8 );
    }
    ConfigureCCM( boColor );
    SelectLUTDependingOnPixelFormat();
}

//-----------------------------------------------------------------------------
bool WizardQuickSetup::InitializeExposureParameters( void )
//-----------------------------------------------------------------------------
{
    try
    {
        if( featuresSupported_.boAutoExposureSupport )
        {
            SetAutoExposure( true );
            return true;
        }
        else
        {
            SetExposureTime( 25000 );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to initialize exposure parameters(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return false;
}

//-----------------------------------------------------------------------------
bool WizardQuickSetup::InitializeGainParameters( void )
//-----------------------------------------------------------------------------
{
    try
    {
        if( featuresSupported_.boAutoGainSupport )
        {
            SetAutoGain( true );
            return true;
        }
        else
        {
            DoWriteUnifiedGain( 0. );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to initialize gain parameters(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return false;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::InitializeWhiteBalanceParametersDriver( void )
//-----------------------------------------------------------------------------
{
    WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
    LoggedWriteProperty( wbs.WBAoiMode, string( "Full" ) );
    LoggedWriteProperty( pIP_->whiteBalance, wbpUser1 );
    LoggedWriteProperty( pIP_->whiteBalanceCalibration, wbcmNextFrame );
    currentSettings_.boAutoWhiteBalanceEnabled = false;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::InitializeWizardForSelectedDevice( Device* pDev )
//-----------------------------------------------------------------------------
{
    currentDeviceSerial_ = pDev->serial.readS();
    currentProductString_ = pDev->product.readS();
    currentDeviceFWVersion_ = static_cast< unsigned int >( pDev->firmwareVersion.read() );
    pCBShowDialogAtStartup_->SetValue( pParentPropViewFrame_->GetAutoShowQSWOnUseDevice() );

    CreateInterfaceLayoutSpecificControls( pDev );
    pID_ = new ImageDestination( pDev );

    SetupUnifiedData();
    QueryInitialDeviceSettings();
    InitializeGammaParameters();

    // 'optimizationLevel' is no camera property, but rather a 'QuickSetup setting', and has to be handled differently than everything else
    currentSettings_.optimizationLevel = olBestQuality;

    AnalyzeDeviceAndGatherInformation();
    SetupDriverSettings();

    if( GetAutoConfigureOnStart() )
    {
        ApplyPreset( featuresSupported_.boColorOptionsSupport, olBestQuality );
    }
    else
    {
        SetupControls();
    }

    SetTitle( wxString::Format( wxT( "Quick Setup [%s - %s] (%s)" ), ConvertedString( currentProductString_ ).c_str(), ConvertedString( currentDeviceSerial_ ).c_str(), ConvertedString( pDev->interfaceLayout.readS() ).c_str() ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::RefreshControls( void )
//-----------------------------------------------------------------------------
{
    const bool boAutoExposureSupported = featuresSupported_.boAutoExposureSupport;
    const bool boExposureAutoIsActive = currentSettings_.boAutoExposureEnabled;
    pBtnExposureAuto_->SetValue( boExposureAutoIsActive );
    pBtnExposureAuto_->Enable( boAutoExposureSupported );
    pSLExposure_->Enable( !boAutoExposureSupported || !boExposureAutoIsActive );
    pSCExposure_->Enable( !boAutoExposureSupported || !boExposureAutoIsActive );

    if( featuresSupported_.boGainSupport == true )
    {
        const bool boAutoGainSupported = featuresSupported_.boAutoGainSupport;
        const bool boGainAutoIsActive = currentSettings_.boAutoGainEnabled;
        pBtnGainAuto_->SetValue( boGainAutoIsActive );
        pBtnGainAuto_->Enable( boAutoGainSupported );
        pSLGain_->Enable( !boAutoGainSupported || !boGainAutoIsActive );
        pSCGain_->Enable( !boAutoGainSupported || !boGainAutoIsActive );
    }
    else
    {
        pBtnGainAuto_->SetValue( false );
        pBtnGainAuto_->Enable( false );
        pSLGain_->Enable( false );
        pSCGain_->Enable( false );
    }

    // Black level needs no refresh, the slider should always be visible since there is no auto-black-level functionality.
    pSLBlackLevel_->Enable( featuresSupported_.boBlackLevelSupport );
    pSCBlackLevel_->Enable( featuresSupported_.boBlackLevelSupport );

    pBtnGamma_->SetValue( currentSettings_.boGammaEnabled );

    if( currentSettings_.boColorEnabled )
    {
        const bool boAutoWhiteBalanceSupported = featuresSupported_.boAutoWhiteBalanceSupport;
        const bool boAutoWhiteBalanceIsActive = currentSettings_.boAutoWhiteBalanceEnabled;
        pBtnWhiteBalanceAuto_->SetValue( boAutoWhiteBalanceIsActive );
        pBtnWhiteBalanceAuto_->Enable( boAutoWhiteBalanceSupported );
        pSLWhiteBalanceR_->Enable( !boAutoWhiteBalanceSupported || !boAutoWhiteBalanceIsActive );
        pSCWhiteBalanceR_->Enable( !boAutoWhiteBalanceSupported || !boAutoWhiteBalanceIsActive );
        pSLWhiteBalanceB_->Enable( !boAutoWhiteBalanceSupported || !boAutoWhiteBalanceIsActive );
        pSCWhiteBalanceB_->Enable( !boAutoWhiteBalanceSupported || !boAutoWhiteBalanceIsActive );
    }
    else
    {
        pSLWhiteBalanceR_->Enable( false );
        pSCWhiteBalanceR_->Enable( false );
        pSLWhiteBalanceB_->Enable( false );
        pSCWhiteBalanceB_->Enable( false );
        pBtnWhiteBalanceAuto_->Enable( false );
        pBtnWhiteBalanceAuto_->SetValue( false );
    }

    if( currentSettings_.boColorEnabled && featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing )
    {
        pBtnCCM_->SetValue( currentSettings_.boCCMEnabled );
        pBtnCCM_->Enable( featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing );
        pSLSaturation_->Enable( true );
        pSCSaturation_->Enable( true );
    }
    else
    {
        pSLSaturation_->Enable( false );
        pSCSaturation_->Enable( false );
        pBtnCCM_->Enable( false );
        pBtnCCM_->SetValue( false );
    }

    const bool boMaximumFrameRateSupported = featuresSupported_.boMaximumFrameRateSupport;
    const bool boFrameRateMaximumIsActive = currentSettings_.boMaximumFrameRateEnabled;
    const bool boFrameRateRegulationSupported = featuresSupported_.boRegulateFrameRateSupport;
    pBtnFrameRateMaximum_->SetValue( boFrameRateMaximumIsActive );
    pBtnFrameRateMaximum_->Enable( boMaximumFrameRateSupported );
    pSLFrameRate_->Enable( boFrameRateRegulationSupported ? ( !boMaximumFrameRateSupported || !boFrameRateMaximumIsActive ) : false );
    pSCFrameRate_->Enable( boFrameRateRegulationSupported ? ( !boMaximumFrameRateSupported || !boFrameRateMaximumIsActive ) : false );
    pFrameRateControlStaticText_->Enable( boFrameRateRegulationSupported );
    pBtnPresetColorHost_->Enable( featuresSupported_.boColorOptionsSupport && featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing );
    pBtnPresetColorCamera_->Enable( featuresSupported_.boColorOptionsSupport && featuresSupported_.boDeviceSupportsColorCorrectionMatrixProcessing );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupPresetButtons( bool enableHost, bool enableCamera )
//-----------------------------------------------------------------------------
{
    if( enableHost )
    {
        pBtnPresetColorHost_->SetBitmap( PNGToIconImage( qsw_computer_png, sizeof( qsw_computer_png ), false ) );
    }
    else
    {
        pBtnPresetColorHost_->SetBitmap( PNGToIconImage( qsw_computer_disabled_png, sizeof( qsw_computer_disabled_png ), false ) );
    }

    if( enableCamera )
    {
        pBtnPresetColorCamera_->SetBitmap( PNGToIconImage( qsw_camera_png, sizeof( qsw_camera_png ), false ) );
    }
    else
    {
        pBtnPresetColorCamera_->SetBitmap( PNGToIconImage( qsw_camera_disabled_png, sizeof( qsw_camera_disabled_png ), false ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SelectLUTDependingOnPixelFormat( void )
//-----------------------------------------------------------------------------
{
    const string currentPixelFormat = currentSettings_.imageFormatControlPixelFormat;

    if( currentPixelFormat.find( "8", 0 ) != string::npos &&
        pIP_->LUTMappingSoftware.read() != LUTm8To8 )
    {
        LoggedWriteProperty( pIP_->LUTMappingSoftware, LUTm8To8 );
    }
    else if( currentPixelFormat.find( "10", 0 ) != string::npos &&
             pIP_->LUTMappingSoftware.read() != LUTm10To10 )
    {
        LoggedWriteProperty( pIP_->LUTMappingSoftware, LUTm10To10 );
    }
    else if( currentPixelFormat.find( "12", 0 ) != string::npos &&
             pIP_->LUTMappingSoftware.read() != LUTm12To12 )
    {
        LoggedWriteProperty( pIP_->LUTMappingSoftware, LUTm12To12 );
    }
    else if( currentPixelFormat.find( "14", 0 ) != string::npos &&
             pIP_->LUTMappingSoftware.read() != LUTm14To14 )
    {
        LoggedWriteProperty( pIP_->LUTMappingSoftware, LUTm14To14 );
    }
    else if( currentPixelFormat.find( "16", 0 ) != string::npos &&
             pIP_->LUTMappingSoftware.read() != LUTm16To16 )
    {
        LoggedWriteProperty( pIP_->LUTMappingSoftware, LUTm16To16 );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupControls( void )
//-----------------------------------------------------------------------------
{
    try
    {
        SetupExposureControls();
        SetupGainControls();
        SetupBlackLevelControls();
        SetupWhiteBalanceControls();

        if( featuresSupported_.boColorOptionsSupport )
        {
            const double saturationRangeMin = 0.;
            const double saturationRangeMax = 200.;
            const double saturation = ReadSaturationData();

            VarScopeMod<bool> scopeGUILocked( boGUILocked_, false ); // unfortunately some of the next lines emit messages which cause message handlers to be invoked. This needs to be blocked here
            boGUILocked_ = true;
            pSLSaturation_->SetRange( static_cast< int >( saturationRangeMin * SLIDER_GRANULARITY_ ), static_cast< int >( saturationRangeMax * SLIDER_GRANULARITY_ ) );
            pSCSaturation_->SetRange( saturationRangeMin, saturationRangeMax );
            pSCSaturation_->SetIncrement( 1 / SLIDER_GRANULARITY_ );
            pSCSaturation_->SetValue( saturation );
            pSLSaturation_->SetValue( static_cast< int >( saturation * SLIDER_GRANULARITY_ ) );
        }
        SetupFrameRateControls();
    }
    catch( const ImpactAcquireException& ) {}
    RefreshControls();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupDevice( void )
//-----------------------------------------------------------------------------
{
    try
    {
        currentSettings_.boAutoExposureEnabled = InitializeExposureParameters();
        currentSettings_.boAutoGainEnabled = InitializeGainParameters();
        InitializeGammaParameters();
        InitializeBlackLevelParameters();

        if( featuresSupported_.boColorOptionsSupport )
        {
            InitializeWhiteBalanceParameters();
        }
        currentSettings_.maxFrameRateAtCurrentAOI = DetermineMaxFrameRateAtCurrentAOI( false );
        currentSettings_.maxAllowedHQFrameRateAtCurrentAOI = DetermineOptimalHQFrameRateAtCurrentAOI( false );
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Error during device Setup (Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupDriverSettings( void )
//-----------------------------------------------------------------------------
{
    if( pIP_->LUTEnable.isValid() && pIP_->LUTEnable.isWriteable() && !featuresSupported_.boGammaSupport )
    {
        LoggedWriteProperty( pIP_->LUTMode, LUTmGamma );
        const unsigned int LUTCnt = pIP_->getLUTParameterCount();
        for( unsigned int i = 0; i < LUTCnt; i++ )
        {
            mvIMPACT::acquire::LUTParameters& lpm = pIP_->getLUTParameter( i );
            LoggedWriteProperty( lpm.gamma, GAMMA_CORRECTION_VALUE_ );
            LoggedWriteProperty( lpm.gammaMode, LUTgmLinearStart );
        }
        ConfigureGamma( false );
    }

    if( featuresSupported_.boColorOptionsSupport && featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing )
    {
        if( ( pIP_->colorTwistInputCorrectionMatrixEnable.isValid() && pIP_->colorTwistInputCorrectionMatrixEnable.isWriteable() ) &&
            ( pIP_->colorTwistOutputCorrectionMatrixEnable.isValid() && pIP_->colorTwistOutputCorrectionMatrixEnable.isWriteable() ) )
        {
            LoggedWriteProperty( pIP_->colorTwistInputCorrectionMatrixMode, cticmmDeviceSpecific );
            LoggedWriteProperty( pIP_->colorTwistOutputCorrectionMatrixMode, ctocmmXYZTosRGB_D50 );
            LoggedWriteProperty( pIP_->colorTwistInputCorrectionMatrixEnable, bFalse );
            LoggedWriteProperty( pIP_->colorTwistOutputCorrectionMatrixEnable, bFalse );
        }
    }

    ConfigurePolarizationDataExtraction();

    LoggedWriteProperty( pID_->pixelFormat, currentSettings_.imageDestinationPixelFormat );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupOptimization( void )
//-----------------------------------------------------------------------------
{
    currentSettings_.optimizationLevel = GetOptimizationLevel();
    HandlePresetChanges();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupUnifiedData( void )
//-----------------------------------------------------------------------------
{
    SetupUnifiedGainData();
    SetupUnifiedBlackLevelData();
    analogGainMin_ = currentSettings_.analogGainMin;
    analogGainMax_ = currentSettings_.analogGainMax;
    digitalGainMin_ = currentSettings_.digitalGainMin;
    digitalGainMax_ = currentSettings_.digitalGainMax;
    analogBlackLevelMin_ = currentSettings_.analogBlackLevelMin;
    analogBlackLevelMax_ = currentSettings_.analogBlackLevelMax;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetAcquisitionFrameRateLimitMode( void )
//-----------------------------------------------------------------------------
{
    DoSetAcquisitionFrameRateLimitMode();
}

//-----------------------------------------------------------------------------
bool WizardQuickSetup::ShowFactoryResetPopup( void )
//-----------------------------------------------------------------------------
{
    return wxMessageBox( wxT( "Are you sure you want to load the factory settings? All current settings will be overwritten!\n\nThis cannot be undone by pressing 'Cancel'." ), wxT( "About to load factory settings" ), wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION, this ) == wxYES;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ShutdownDlg( wxStandardID result )
//-----------------------------------------------------------------------------
{
    SuspendAcquisitionScopeLock suspendAcquisitionLock( pParentPropViewFrame_ );
    pParentPropViewFrame_->RemoveCaptureSettingFromStack( pDev_, pFI_, ( result == wxID_CANCEL ) );
    pParentPropViewFrame_->RestoreGUIStateAfterQuickSetupWizard( true );
    pParentPropViewFrame_->RestoreSelectorStateAfterQuickSetupWizard();
    pParentPropViewFrame_->ClearQuickSetupWizardPointer();
    CleanUp();
    Destroy();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ShowImageTimeoutPopup( void )
//-----------------------------------------------------------------------------
{
    wxMessageBox( wxT( "Image request timeout\n\nThe last Image Request returned with an Image Timeout. This means that the camera cannot stream data and indicates a problem with the current configuration.\n\nPlease press the 'Factory' button to load the Factory Settings and then continue with the setup." ), wxT( "Image Timeout!" ), wxOK | wxICON_INFORMATION, this );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::ToggleDetails( bool boShow )
//-----------------------------------------------------------------------------
{
    if( boShow )
    {
        pTCHistory_->Show();
        pBtnClearHistory_->Show();
        SetSize( GetSize().GetWidth(), GetSize().GetHeight() + pTCHistory_->GetSize().GetHeight() );
        pBtnHistory_->SetLabel( wxT( "&History <<<" ) );
    }
    else
    {
        pTCHistory_->Hide();
        pBtnClearHistory_->Hide();
        SetSize( GetSize().GetWidth(), GetSize().GetHeight() - pTCHistory_->GetSize().GetHeight() );
        pBtnHistory_->SetLabel( wxT( "&History >>>" ) );
    }
    pTopDownSizer_->Layout();
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::UpdateDetailsTextControl( void )
//-----------------------------------------------------------------------------
{
    if( propertyChangeHistory_.length() != 0 )
    {
        wxFont fixedPitchFont( 8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
        fixedPitchFont.SetWeight( wxFONTWEIGHT_BOLD );
        wxTextAttr fixedPitchStyle;
        fixedPitchStyle.SetFont( fixedPitchFont );
        WriteToTextCtrl( pTCHistory_, ConvertedString( propertyChangeHistory_ ), fixedPitchStyle );
        propertyChangeHistory_.clear();
        //Workaround for windows refresh issue
        pTCHistory_->ScrollLines( 1 );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::UpdateFrameRateWarningBitmap( bool boIsFrameRateMax )
//-----------------------------------------------------------------------------
{
    // Since wxWidgets in Linux has trouble rendering the bitmap correctly after Show( boIsFrameRateMax )
    // has been called, we are forced to use this workaround which provides the same functionality.
    pStaticBitmapWarning_->SetBitmap( *GlobalDataStorage::Instance()->GetBitmap( boIsFrameRateMax ? GlobalDataStorage::bIcon_Warning : GlobalDataStorage::bIcon_Empty ) );
    pStaticBitmapWarning_->SetToolTip( boIsFrameRateMax ? wxT( "Auto-exposure too long (scene is too dark)!\nFPS may be slower than anticipated!" ) : wxT( "" ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::UpdateExposureControlsFromCamera( void )
//-----------------------------------------------------------------------------
{
    double const exposureTime = GetExposureTime();
    pSCExposure_->SetValue( exposureTime );
    pSLExposure_->SetValue( ExposureToSliderValue( exposureTime ) );
    currentSettings_.exposureTime = exposureTime;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::UpdateGainControlsFromCamera( void )
//-----------------------------------------------------------------------------
{
    double const unifiedGain = ReadUnifiedGainData();
    pSCGain_->SetValue( unifiedGain );
    pSLGain_->SetValue( static_cast< int >( unifiedGain * SLIDER_GRANULARITY_ ) );
    currentSettings_.unifiedGain = unifiedGain;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::UpdateWhiteBalanceControlsFromCamera( void )
//-----------------------------------------------------------------------------
{
    double whiteBalanceRed;
    double whiteBalanceBlue;
    if( boLastWBChannelRead_ == wbcRed )
    {
        whiteBalanceRed = GetWhiteBalance( wbcRed ) * 100.;
        whiteBalanceBlue = GetWhiteBalance( wbcBlue ) * 100.;
        boLastWBChannelRead_ = wbcBlue;
    }
    else
    {
        whiteBalanceBlue = GetWhiteBalance( wbcBlue ) * 100.;
        whiteBalanceRed = GetWhiteBalance( wbcRed ) * 100.;
        boLastWBChannelRead_ = wbcRed;
    }

    pSCWhiteBalanceR_->SetValue( whiteBalanceRed );
    pSLWhiteBalanceR_->SetValue( static_cast< int >( whiteBalanceRed * SLIDER_GRANULARITY_ ) );
    currentSettings_.whiteBalanceRed = whiteBalanceRed;
    pSCWhiteBalanceB_->SetValue( whiteBalanceBlue );
    pSLWhiteBalanceB_->SetValue( static_cast< int >( whiteBalanceBlue * SLIDER_GRANULARITY_ ) );
    currentSettings_.whiteBalanceBlue = whiteBalanceBlue;
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::UpdateControlsData( void )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boAutoExposureSupport &&
        currentSettings_.boAutoExposureEnabled )
    {
        UpdateExposureControlsFromCamera();
    }
    if( featuresSupported_.boAutoGainSupport &&
        currentSettings_.boAutoGainEnabled )
    {
        UpdateGainControlsFromCamera();
    }
    if( ( featuresSupported_.boAutoWhiteBalanceSupport && currentSettings_.boAutoWhiteBalanceEnabled && currentSettings_.boColorEnabled ) ||
        ( pDev_->family.readS() == "mvBlueFOX" ) )
    {
        UpdateWhiteBalanceControlsFromCamera();
    }
    if( featuresSupported_.boMaximumFrameRateSupport )
    {
        SetupFrameRateControls();
        currentSettings_.frameRate = pSCFrameRate_->GetMax();
    }
    UpdateDetailsTextControl();
    RefreshControls();
}

//////////////////////////STANDARD DIALOG-BUTTONS//////////////////////////////
//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnCancel( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    ShutdownDlg( wxID_CANCEL );
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::OnBtnOk( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    ShutdownDlg( wxID_OK );
}

//=============================================================================
//================= Implementation WizardQuickSetupGenICam ====================
//=============================================================================
//-----------------------------------------------------------------------------
WizardQuickSetupGenICam::WizardQuickSetupGenICam( PropViewFrame* pParent, const wxString& title, Device* pDev, FunctionInterface* pFI, bool boAutoConfigureOnStart ) :
    WizardQuickSetup( pParent, title, pDev, pFI, boAutoConfigureOnStart ), pDeC_( 0 ), pAcC_( 0 ), pAnC_( 0 ), pIFC_( 0 ), pUSC_( 0 )
//-----------------------------------------------------------------------------
{
    InitializeWizardForSelectedDevice( pDev );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::ConfigurePolarizationDataExtraction( void )
//-----------------------------------------------------------------------------
{
    static const wxString s_polarizationSensorNamePostfix( wxT( "_POL" ) );
    if( !pIFC_->sensorName.isValid() )
    {
        return;
    }

    const wxString sensorName( ConvertedString( pIFC_->sensorName.read() ) );
    if( !sensorName.EndsWith( s_polarizationSensorNamePostfix ) )
    {
        return;
    }

    if( pIP_->polarizedDataExtractionEnable.isValid() &&
        pIP_->polarizedDataExtractionMode.isValid() &&
        supportsValue( pIP_->polarizedDataExtractionMode, prm2By2 ) )
    {
        pIP_->polarizedDataExtractionEnable.write( bTrue );
        pIP_->polarizedDataExtractionMode.write( prm2By2 );
        if( pIFC_->pixelColorFilter.isValid() &&
            pIP_->formatReinterpreterBayerMosaicParity.isValid() )
        {
            const string pixelFormat( pIFC_->pixelFormat.readS() );
            const string pixelColorFilter( pIFC_->pixelColorFilter.readS() );
            if( ( pixelFormat.find( "Mono" ) != string::npos ) &&
                ( pixelColorFilter.find( "Bayer" ) != string::npos ) )
            {
                pIP_->formatReinterpreterEnable.write( bTrue );
                if( pixelFormat == "Mono8" )
                {
                    pIP_->formatReinterpreterMode.write( ibfrmMono8_To_Mono8 );
                }
                else if( pixelFormat == "Mono10" )
                {
                    pIP_->formatReinterpreterMode.write( ibfrmMono10_To_Mono10 );
                }
                else if( pixelFormat == "Mono12" )
                {
                    pIP_->formatReinterpreterMode.write( ibfrmMono12_To_Mono12 );
                }
                else if( pixelFormat == "Mono14" )
                {
                    pIP_->formatReinterpreterMode.write( ibfrmMono14_To_Mono14 );
                }
                else if( pixelFormat == "Mono16" )
                {
                    pIP_->formatReinterpreterMode.write( ibfrmMono16_To_Mono16 );
                }

                if( pixelColorFilter == "BayerRG" )
                {
                    pIP_->formatReinterpreterBayerMosaicParity.write( bmpRG );
                }
                else if( pixelColorFilter == "BayerGB" )
                {
                    pIP_->formatReinterpreterBayerMosaicParity.write( bmpGB );
                }
                else if( pixelColorFilter == "BayerGR" )
                {
                    pIP_->formatReinterpreterBayerMosaicParity.write( bmpGR );
                }
                else if( pixelColorFilter == "BayerBG" )
                {
                    pIP_->formatReinterpreterBayerMosaicParity.write( bmpBG );
                }

                pIP_->colorProcessing.write( cpmBayer );
            }
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::CreateInterfaceLayoutSpecificControls( Device* pDev )
//-----------------------------------------------------------------------------
{
    pDeC_ = new GenICam::DeviceControl( pDev );
    pAcC_ = new GenICam::AcquisitionControl( pDev );
    pAnC_ = new GenICam::AnalogControl( pDev );
    pIFC_ = new GenICam::ImageFormatControl( pDev );
    pCTC_ = new GenICam::ColorTransformationControl( pDev );
    pUSC_ = new GenICam::UserSetControl( pDev );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::DeleteInterfaceLayoutSpecificControls( void )
//-----------------------------------------------------------------------------
{
    DeleteElement( pDeC_ );
    DeleteElement( pAcC_ );
    DeleteElement( pAnC_ );
    DeleteElement( pIFC_ );
    DeleteElement( pCTC_ );
    DeleteElement( pUSC_ );
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::DetermineMaxFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats )
//-----------------------------------------------------------------------------
{
    double maxFrameRateAtCurrentAOI = 5.;
    try
    {
        // this has to be done in order to get the original value of the mvResultingFramerate before changes are made.
        pAcC_->mvResultingFrameRate.read();
        const string originalPixelFormat = pIFC_->pixelFormat.readS();
        const string originalPixelClock = pDeC_->mvDeviceClockFrequency.readS();
        const double originalExposure = pAcC_->exposureTime.read();
        SetAutoExposure( false );
        if( checkAllSupportedPixelFormats )
        {
            if( HasColorFormat() )
            {
                SetOptimalPixelFormatColor( olBestQuality );
            }
            else
            {
                SetOptimalPixelFormatGray( olBestQuality );
            }
        }
        SelectOptimalPixelClock( olBestQuality );
        LoggedWriteProperty( pAcC_->exposureTime, 1000 );
        maxFrameRateAtCurrentAOI = pAcC_->mvResultingFrameRate.read();
        //Revert values after the max frame rate has been calculated
        LoggedWriteProperty( pIFC_->pixelFormat, string( originalPixelFormat ) );

        if( pDeC_->mvDeviceClockFrequency.readS() != originalPixelClock )
        {
            LoggedWriteProperty( pDeC_->mvDeviceClockFrequency, string( originalPixelClock ) );
        }
        LoggedWriteProperty( pAcC_->exposureTime, originalExposure );
        if( currentSettings_.boAutoExposureEnabled )
        {
            SetAutoExposure( true );
        }
        //This has to be done since SetOptimalPixelFormat overwrite the currentSettings internally
        currentSettings_.imageFormatControlPixelFormat = originalPixelFormat;
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Error while determining the maximum frame rate! (Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return maxFrameRateAtCurrentAOI;
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::DetermineOptimalHQFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats )
//-----------------------------------------------------------------------------
{
    double optimalHQFrameRateAtCurrentAOI = 5.;
    try
    {
        const string originalPixelFormat = pIFC_->pixelFormat.readS();
        if( checkAllSupportedPixelFormats )
        {
            if( HasColorFormat() )
            {
                SetOptimalPixelFormatColor( olBestQuality );
            }
            else
            {
                SetOptimalPixelFormatGray( olBestQuality );
            }
        }

        GenICam::TransportLayerControl tlc( pDev_ );
        // The factor which specifies the effective bandwidth usable by devices utilizing the specific interface (including some reserves)
        double interfaceTransmissionCorrectionValue = 1.;
        int64_type linkThroughput = 0;

        if( pDeC_->deviceTLType.isValid() )
        {
            if( pDeC_->deviceTLType.readS() == "USB3Vision" )
            {
                //U3V
                // Roughly half of the bandwidth specified by the USB3.0 standard
                interfaceTransmissionCorrectionValue = 0.5;
                linkThroughput = pDeC_->deviceLinkSpeed.read();
            }
            else if( pDeC_->deviceTLType.readS() == "GigEVision" )
            {
                //GEV
                // 80 percent of the bandwidth specified by the GigEVision standard
                if( tlc.mvGevSCBWControl.isValid() )
                {
                    // This should work for GigE devices with older firmware where the deviceLinkSpeed property was not implemented yet
                    tlc.mvGevSCBWControl.writeS( "mvGevSCBW" );
                    linkThroughput = tlc.mvGevSCBW.read() * 1000;
                }
                else
                {
                    linkThroughput = pDeC_->deviceLinkSpeed.read();
                }
                interfaceTransmissionCorrectionValue = 0.8;
            }
            else if( pDeC_->deviceTLType.readS() == "PCI" )
            {
                // PCIe
                interfaceTransmissionCorrectionValue = 1.0;
                linkThroughput = pDeC_->deviceLinkSpeed.read();
            }
        }
        else
        {
            // Other
            interfaceTransmissionCorrectionValue = 1.0;
            linkThroughput = pDeC_->deviceLinkSpeed.read();
        }

        const int64_type payLoadSize = tlc.payloadSize.read();
        optimalHQFrameRateAtCurrentAOI = ( static_cast< double >( linkThroughput ) / static_cast< double >( payLoadSize ) ) * interfaceTransmissionCorrectionValue;

        //Revert values after the max frame rate has been calculated
        LoggedWriteProperty( pIFC_->pixelFormat, string( originalPixelFormat ) );

        //This has to be done since SelectOptimalPixelFormat overwrite the currentSettings internally
        currentSettings_.imageFormatControlPixelFormat = originalPixelFormat;
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Error while determining the optimal HQ frame rate! (Error: %s(%s))" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return optimalHQFrameRateAtCurrentAOI;
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::DoConfigureColorCorrection( bool boActive )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boDeviceSupportsColorCorrectionMatrixProcessing && ( pIFC_->pixelFormat.readS().find( "RGB", 0 ) != string::npos ) && currentSettings_.boCameraBasedColorProcessingEnabled )
    {
        LoggedWriteProperty( pCTC_->colorTransformationEnable, boActive ? bTrue : bFalse );
        DoConfigureDriverBaseColorCorrection( bFalse );
    }
    else
    {
        if( pCTC_->colorTransformationEnable.isValid() )
        {
            LoggedWriteProperty( pCTC_->colorTransformationEnable, bFalse );
        }
        DoConfigureDriverBaseColorCorrection( boActive ? bTrue : bFalse );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::DoConfigureFrameRateMaximum( bool boActive, double frameRateValue )
//-----------------------------------------------------------------------------
{
    if( boActive )
    {
        currentSettings_.frameRate = frameRateValue;
        if( pAcC_->acquisitionFrameRate.hasMaxValue() )
        {
            LoggedWriteProperty( pAcC_->acquisitionFrameRate, pAcC_->acquisitionFrameRate.getMaxValue() );
        }
        SetFrameRateEnable( false );
    }
    else
    {
        SetFrameRateEnable( true );
        if( pAcC_->acquisitionFrameRate.hasMaxValue() )
        {
            // In the case of FrameRate, checks have to be done first, as due to changes in exposure, the desired value
            // may be out of range (e.g. writing the maximum frame rate value after exposure has increased dramatically)
            frameRateValue = currentSettings_.frameRate;
            double frameRateMin = pAcC_->acquisitionFrameRate.getMinValue();
            double frameRateMax = pAcC_->acquisitionFrameRate.getMaxValue();
            if( frameRateValue <= frameRateMin )
            {
                LoggedWriteProperty( pAcC_->acquisitionFrameRate, frameRateMin );
                currentSettings_.frameRate = frameRateMin;
            }
            else if( frameRateValue >= frameRateMax )
            {
                LoggedWriteProperty( pAcC_->acquisitionFrameRate, frameRateMax );
                currentSettings_.frameRate = frameRateMax;
            }
            else
            {
                LoggedWriteProperty( pAcC_->acquisitionFrameRate, frameRateValue );
            }
        }
        SetupFrameRateControls();
    }
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::DoReadUnifiedBlackLevel( void ) const
//-----------------------------------------------------------------------------
{
    double currentBlackLevel = pAnC_->blackLevel.read();
    pAnC_->blackLevelSelector.writeS( ( pAnC_->blackLevelSelector.readS() == "DigitalAll" ) ? "All" : "DigitalAll" );
    currentBlackLevel += pAnC_->blackLevel.read();
    return currentBlackLevel;
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::DoReadSaturationData( void ) const
//-----------------------------------------------------------------------------
{
    return featuresSupported_.boDeviceSupportsColorCorrectionMatrixProcessing ? 100.0 : DoReadDriverBasedSaturationData();
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::DoReadUnifiedGain( void ) const
//-----------------------------------------------------------------------------
{
    double currentGain = 0.;
    if( currentSettings_.boAutoGainEnabled == true )
    {
        // ReadUnifiedGain leads to stuttering. Since DigitalGain is set to 0 on pressing AutoGain Button,
        // one could rely on analog gain values alone to update the current gain when auto-gain is in use.
        if( featuresSupported_.boUnifiedGainSupport )
        {
            if( pAnC_->gainSelector.readS() == "DigitalAll" )
            {
                pAnC_->gainSelector.writeS( "AnalogAll" );
            }
        }
        currentGain = pAnC_->gain.read();
    }
    else
    {
        currentGain = pAnC_->gain.read();
        if( featuresSupported_.boUnifiedGainSupport )
        {
            pAnC_->gainSelector.writeS( ( pAnC_->gainSelector.readS() == "DigitalAll" ) ? "AnalogAll" : "DigitalAll" );
            currentGain += pAnC_->gain.read();
        }
    }
    return currentGain;
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::DoSetAcquisitionFrameRateLimitMode( void )
//-----------------------------------------------------------------------------
{
    if( pAcC_->mvAcquisitionFrameRateLimitMode.isValid() )
    {
        // there are cameras without mvAcquisitionFrameRateLimitMode but with a max. frame rate (e.g. mvBlueNAOS)
        LoggedWriteProperty( pAcC_->mvAcquisitionFrameRateLimitMode, string( "mvDeviceLinkThroughput" ) );
    }
    SetFrameRateEnable( true );
    if( pAcC_->acquisitionFrameRate.hasMaxValue() )
    {
        LoggedWriteProperty( pAcC_->acquisitionFrameRate, pAcC_->acquisitionFrameRate.getMaxValue() );
    }
    SetFrameRateEnable( false );
    currentSettings_.boMaximumFrameRateEnabled = true;
}

//-----------------------------------------------------------------------------
string WizardQuickSetupGenICam::DoWriteDeviceBasedSaturationData( double saturation )
//-----------------------------------------------------------------------------
{
    if( initialDeviceColorCorrectionMatrix_.empty() )
    {
        WriteQuickSetupWizardErrorMessage( wxT( "Failed to write saturation value as the initial device color correction matrix could not be read from the device!" ) );
    }

    ostringstream oss;
    double K = saturation / 100.0;
    LoggedWriteProperty( pCTC_->colorTransformationEnable, bTrue );

    try
    {
        // This is necessary to calculate the coefficients with a custom saturation value
        // once it is implemented this will be obsolete
        vector<vector<double> > saturationMatrix;
        vector<double> row;

        row.push_back( 0.299 + 0.701 * K );
        row.push_back( 0.587 * ( 1 - K ) );
        row.push_back( 0.114 * ( 1 - K ) );
        saturationMatrix.push_back( row );
        row.clear();

        row.push_back( 0.299 * ( 1 - K ) );
        row.push_back( 0.587 + 0.413 * K );
        row.push_back( 0.114 * ( 1 - K ) );
        saturationMatrix.push_back( row );
        row.clear();

        row.push_back( 0.299 * ( 1 - K ) );
        row.push_back( 0.587 * ( 1 - K ) );
        row.push_back( 0.114 + 0.886 * K );
        saturationMatrix.push_back( row );
        row.clear();

        const int ml = static_cast<int>( initialDeviceColorCorrectionMatrix_[0].size() - 1 );
        const int nl = static_cast<int>( initialDeviceColorCorrectionMatrix_[1].size() - 1 );
        const int pl = static_cast<int>( initialDeviceColorCorrectionMatrix_[2].size() - 1 );

        for( size_t i = 0; i < 3; i++ )
        {
            row.push_back( 0. );
        }
        vector<vector<double> > resultingColorCorrectionMatrix ( 3, row );

        resultingColorCorrectionMatrix[0][0] = 0.;
        resultingColorCorrectionMatrix[1][1] = 0.;
        resultingColorCorrectionMatrix[2][2] = 0.;

        vector<string> colorTransformationGainStrings;
        pCTC_->colorTransformationValueSelector.getTranslationDictStrings( colorTransformationGainStrings );

        const string colorTransformationValueSelectorStringPrefix = "Gain";
        for( int i = 0; i < ml; i++ )
        {
            for( int j = 0; j < nl; j++ )
            {
                for( int k = 0; k < pl; k++ )
                {
                    resultingColorCorrectionMatrix[i][j] = resultingColorCorrectionMatrix[i][j] + ( initialDeviceColorCorrectionMatrix_[i][k] * saturationMatrix[k][j] );
                }
            }
        }
        int index = 0;
        for( size_t i = 0; i < 3; i++ )
        {
            const size_t colorTransformationGainStringsSize = colorTransformationGainStrings[i].size();
            for( size_t j = 0; j < 3; j++ )
            {
                LoggedWriteProperty( pCTC_->colorTransformationValueSelector, colorTransformationValueSelectorStringPrefix + colorTransformationGainStrings[index].substr( colorTransformationGainStringsSize - 2, 1 ) + colorTransformationGainStrings[j].substr( colorTransformationGainStringsSize - 1, 1 ) );
                LoggedWriteProperty( pCTC_->colorTransformationValue, double( resultingColorCorrectionMatrix[i][j] ) );
                index++;
            }
        }
        oss << "Method setSaturation() called with parameter " << ( K ) << endl;
        if( pCTC_->colorTransformationEnable.read() != bTrue )
        {
            LoggedWriteProperty( pCTC_->colorTransformationEnable, ( K == 1 ) ?  bFalse : bTrue );
        }
        else if( K == 1 )
        {
            LoggedWriteProperty( pCTC_->colorTransformationEnable, bFalse );
        }

        ( pCTC_->colorTransformationEnable.read() != bTrue ) ? currentSettings_.boCCMEnabled = false : currentSettings_.boCCMEnabled = true;
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write saturation value(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return oss.str();
}

//-----------------------------------------------------------------------------
string WizardQuickSetupGenICam::DoWriteSaturationData( double saturation )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boDeviceSupportsColorCorrectionMatrixProcessing && featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing && ( ( pIFC_->pixelFormat.readS().find( "RGB" ) != string::npos ) || ( pIFC_->pixelFormat.readS().find( "BGR" ) != string::npos ) ) )
    {
        return DoWriteDeviceBasedSaturationData( saturation );
    }
    return DoWriteDriverBasedSaturationData( saturation );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::DoWriteUnifiedBlackLevelData( double value ) const
//-----------------------------------------------------------------------------
{
    try
    {
        if( pAnC_->blackLevelSelector.readS() == string( "All" ) )
        {
            if( value >= analogBlackLevelMin_ && value <= analogBlackLevelMax_ )
            {
                LoggedWriteProperty( pAnC_->blackLevel, value );
                pAnC_->blackLevelSelector.writeS( "DigitalAll" );
                pAnC_->blackLevel.write( 0. );
            }
            else if( value < analogBlackLevelMin_ )
            {
                LoggedWriteProperty( pAnC_->blackLevel, analogBlackLevelMin_ );
                LoggedWriteProperty( pAnC_->blackLevelSelector, string( "DigitalAll" ) );
                LoggedWriteProperty( pAnC_->blackLevel, value - analogBlackLevelMin_ );
            }
            else if( value > analogBlackLevelMax_ )
            {
                LoggedWriteProperty( pAnC_->blackLevel, analogBlackLevelMax_ );
                LoggedWriteProperty( pAnC_->blackLevelSelector, string( "DigitalAll" ) );
                LoggedWriteProperty( pAnC_->blackLevel, value - analogBlackLevelMax_ );
            }
        }
        else
        {
            if( value >= analogBlackLevelMin_ && value <= analogBlackLevelMax_ )
            {
                pAnC_->blackLevel.write( 0. );
                pAnC_->blackLevelSelector.writeS( "All" );
                LoggedWriteProperty( pAnC_->blackLevel, value );
            }
            else if( value < analogBlackLevelMin_ )
            {
                LoggedWriteProperty( pAnC_->blackLevel, value - analogBlackLevelMin_ );
                LoggedWriteProperty( pAnC_->blackLevelSelector, string( "All" ) );
                LoggedWriteProperty( pAnC_->blackLevel, analogBlackLevelMin_ );
            }
            else if( value > analogBlackLevelMax_ )
            {
                LoggedWriteProperty( pAnC_->blackLevel, analogBlackLevelMax_ );
                LoggedWriteProperty( pAnC_->blackLevelSelector, string( "All" ) );
                LoggedWriteProperty( pAnC_->blackLevel, value - analogBlackLevelMax_ );
            }
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write combined analog and digital blackLevel values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::DoWriteUnifiedGain( double value ) const
//-----------------------------------------------------------------------------
{
    try
    {
        if( ( pAnC_->gainSelector.readS() == string( "AnalogAll" ) ) )
        {
            if( ( value >= analogGainMin_ ) && ( value <= analogGainMax_ ) )
            {
                LoggedWriteProperty( pAnC_->gain, value );
                if( featuresSupported_.boUnifiedGainSupport )
                {
                    pAnC_->gainSelector.writeS( "DigitalAll" );
                    pAnC_->gain.write( 0. );
                }
            }
            else if( value < analogGainMin_ )
            {
                LoggedWriteProperty( pAnC_->gain, analogGainMin_ );
                LoggedWriteProperty( pAnC_->gainSelector, string( "DigitalAll" ) );
                LoggedWriteProperty( pAnC_->gain, value - analogGainMin_ );
            }
            else if( value > analogGainMax_ )
            {
                LoggedWriteProperty( pAnC_->gain, analogGainMax_ );
                LoggedWriteProperty( pAnC_->gainSelector, string( "DigitalAll" ) );
                LoggedWriteProperty( pAnC_->gain, value - analogGainMax_ );
            }
        }
        else
        {
            if( ( value >= analogGainMin_ ) && ( value <= analogGainMax_ ) )
            {
                pAnC_->gain.write( 0. );
                pAnC_->gainSelector.writeS( "AnalogAll" );
                LoggedWriteProperty( pAnC_->gain, value );
            }
            else if( value < analogGainMin_ )
            {
                LoggedWriteProperty( pAnC_->gain, value - analogGainMin_ );
                LoggedWriteProperty( pAnC_->gainSelector, string( "AnalogAll" ) );
                LoggedWriteProperty( pAnC_->gain, analogGainMin_ );
            }
            else if( value > analogGainMax_ )
            {
                LoggedWriteProperty( pAnC_->gain, value - analogGainMax_ );
                LoggedWriteProperty( pAnC_->gainSelector, string( "AnalogAll" ) );
                LoggedWriteProperty( pAnC_->gain, analogGainMax_ );
            }
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write combined analog and digital gain values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::GetAutoExposureUpperLimit( void ) const
//-----------------------------------------------------------------------------
{
    double result = 100000.;
    if( pAcC_->mvExposureAutoUpperLimit.isValid() )
    {
        if( pAcC_->mvExposureAutoUpperLimit.isVisible() )
        {
            result = pAcC_->mvExposureAutoUpperLimit.read();
        }
        else if( pAcC_->exposureAuto.isValid() )
        {
            pAcC_->exposureAuto.writeS( "Continuous" );
            result = pAcC_->mvExposureAutoUpperLimit.read();
            pAcC_->exposureAuto.writeS( "Off" );
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
vector<vector<double> > WizardQuickSetupGenICam::GetInitialDeviceColorCorrectionMatrix( void ) const
//-----------------------------------------------------------------------------
{
    vector<vector<double> > outputMatrix;
    if( pIP_->colorTwistInputCorrectionMatrixEnable.isValid() )
    {
        pIP_->colorTwistInputCorrectionMatrixEnable.write( bTrue );
        pIP_->colorTwistOutputCorrectionMatrixEnable.write( bTrue );
        vector<double> row( 4, 0. );
        pIP_->colorTwistResultingMatrixRow0.read( row );
        outputMatrix.push_back( row );
        pIP_->colorTwistResultingMatrixRow1.read( row );
        outputMatrix.push_back( row );
        pIP_->colorTwistResultingMatrixRow2.read( row );
        outputMatrix.push_back( row );

        pIP_->colorTwistInputCorrectionMatrixEnable.write( bFalse );
        pIP_->colorTwistOutputCorrectionMatrixEnable.write( bFalse );
    }
    return outputMatrix;
}

//-----------------------------------------------------------------------------
string WizardQuickSetupGenICam::GetPixelClock( void ) const
//-----------------------------------------------------------------------------
{
    return ( pDeC_->mvDeviceClockFrequency.isValid() ) ? pDeC_->mvDeviceClockFrequency.readS() : string( "NoClockFound" );
}

//-----------------------------------------------------------------------------
string WizardQuickSetupGenICam::GetPixelFormat( void ) const
//-----------------------------------------------------------------------------
{
    return ( pIFC_->pixelFormat.isValid() ) ? pIFC_->pixelFormat.readS() : string( "Mono8" );
}

//-----------------------------------------------------------------------------
double WizardQuickSetupGenICam::GetWhiteBalance( TWhiteBalanceChannel channel )
//-----------------------------------------------------------------------------
{
    if( pAnC_->balanceRatioSelector.isValid() )
    {
        pAnC_->balanceRatioSelector.writeS( ( channel == wbcRed ) ? "Red" : "Blue" );
        return pAnC_->balanceRatio.read();
    }
    else
    {
        return GetWhiteBalanceDriver( channel );
    }
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasAEC( void ) const
//-----------------------------------------------------------------------------
{
    if( pAcC_->exposureAuto.isValid() && pAcC_->exposureAuto.isWriteable() )
    {
        vector<string> validExposureAutoValues;
        pAcC_->exposureAuto.getTranslationDictStrings( validExposureAutoValues );
        const vector<string>::const_iterator it = validExposureAutoValues.begin();
        const vector<string>::const_iterator itEND = validExposureAutoValues.end();
        if( find( it, itEND, "Continuous" ) != itEND )
        {
            if( pAcC_->mvExposureAutoMode.isValid() )
            {
                vector<string> mvExposureAutoModes;
                pAcC_->mvExposureAutoMode.getTranslationDictStrings( mvExposureAutoModes );
                return find( mvExposureAutoModes.begin(), mvExposureAutoModes.end(), "mvDevice" ) != mvExposureAutoModes.end();
            }
            else
            {
                return true;
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasAGC( void )
//-----------------------------------------------------------------------------
{
    vector<string> gainSelectorStrings;
    pAnC_->gainSelector.getTranslationDictStrings( gainSelectorStrings );
    const vector<string>::const_iterator it = gainSelectorStrings.begin();
    const vector<string>::const_iterator itEND = gainSelectorStrings.end();
    const string originalSetting = pAnC_->gainSelector.readS();
    bool boResult = false;
    if( find( it, itEND, "AnalogAll" ) != itEND )
    {
        pAnC_->gainSelector.writeS( "AnalogAll" );
        if( pAnC_->gainAuto.isValid() && pAnC_->gainAuto.isWriteable() )
        {
            if( pAnC_->mvGainAutoMode.isValid() )
            {
                vector<string> mvGainAutoModes;
                pAnC_->mvGainAutoMode.getTranslationDictStrings( mvGainAutoModes );
                if( find( mvGainAutoModes.begin(), mvGainAutoModes.end(), "mvDevice" ) != mvGainAutoModes.end() )
                {
                    boResult = true;
                }
            }
            else
            {
                boResult = true;
            }
        }
    }
    else
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Device does not seem to have a AnalogAll Selector!" ) ) );
    }
    pAnC_->gainSelector.writeS( originalSetting );
    return boResult;
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasMaximumFrameRate( void ) const
//-----------------------------------------------------------------------------
{
    static const unsigned int s_FW_VERSION_WITH_NEW_RESULTING_FRAMERATE_BEHAVIOR = ( ( 1 << 24 ) | ( 6 << 16 ) | ( 450 << 4 ) | 0 ); // 1.6.450.0 in 8.8.12.4 notation
    if( currentDeviceFWVersion_ < s_FW_VERSION_WITH_NEW_RESULTING_FRAMERATE_BEHAVIOR )
    {
        // for (very) old firmware versions mvResultingFrameRate should not be used as its function was different then
        return pAcC_->mvAcquisitionFrameRateLimitMode.isValid() && pAcC_->mvAcquisitionFrameRateLimitMode.isWriteable();
    }

    // otherwise it is sufficient just to check if mvResultingFrameRate exists
    return pAcC_->mvResultingFrameRate.isValid();
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasAWB( void ) const
//-----------------------------------------------------------------------------
{
    if( pAnC_->balanceWhiteAuto.isValid() && pAnC_->balanceWhiteAuto.isWriteable() )
    {
        vector<string> validWhiteBalanceAutoValues;
        pAnC_->balanceWhiteAuto.getTranslationDictStrings( validWhiteBalanceAutoValues );
        return find( validWhiteBalanceAutoValues.begin(), validWhiteBalanceAutoValues.end(), "Continuous" ) != validWhiteBalanceAutoValues.end();
    }
    return false;
}

//-----------------------------------------------------------------------------
bool stringStartsWithBayer( const string& value )
//-----------------------------------------------------------------------------
{
    return value.find( "Bayer" ) == 0;
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasColorFormat( void ) const
//-----------------------------------------------------------------------------
{
    vector<string> pixelFormatStrings;
    pIFC_->pixelFormat.getTranslationDictStrings( pixelFormatStrings );
    bool hasColorOption = find_if( pixelFormatStrings.begin(), pixelFormatStrings.end(), stringStartsWithBayer ) != pixelFormatStrings.end();
    if( !hasColorOption && pIFC_->pixelColorFilter.isValid() )
    {
        // some devices might not report color formats even though they are fitted with color sensors (e.g. devices with a polarized sensor filter)
        hasColorOption = stringStartsWithBayer( pIFC_->pixelColorFilter.readS() );
    }
    return hasColorOption;
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasUnifiedGain( void ) const
//-----------------------------------------------------------------------------
{
    if( pAnC_->gainSelector.isValid() )
    {
        return false;
    }
    vector<string> gainSelectorStrings;
    pAnC_->gainSelector.getTranslationDictStrings( gainSelectorStrings );
    const vector<string>::const_iterator it = gainSelectorStrings.begin();
    const vector<string>::const_iterator itEND = gainSelectorStrings.end();
    return find( it, itEND, "DigitalAll" ) != itEND;
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::HasFactoryDefault( void ) const
//-----------------------------------------------------------------------------
{
    if( pUSC_->userSetSelector.isValid() && pUSC_->userSetLoad.isValid() )
    {
        vector<string> validUserSets;
        pUSC_->userSetSelector.getTranslationDictStrings( validUserSets );
        return find( validUserSets.begin(), validUserSets.end(), string( "Default" ) ) != validUserSets.end();
    }
    return false;
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::InitializeBlackLevelParameters( void )
//-----------------------------------------------------------------------------
{
    try
    {
        DoWriteUnifiedBlackLevelData( 0. );
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to initialize black level parameters(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::InitializeGammaParameters( void )
//-----------------------------------------------------------------------------
{
    try
    {
        if( pAnC_->mvGammaSelector.isValid() )
        {
            LoggedWriteProperty( pAnC_->mvGammaSelector, string( "mvUser" ) );
            LoggedWriteProperty<PropertyF, double>( pAnC_->gamma, 0.8 );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to initialize gamma parameters(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::InitializeWhiteBalanceParameters( void )
//-----------------------------------------------------------------------------
{
    if( pAnC_->balanceRatio.isValid() )
    {
        try
        {
            if( featuresSupported_.boAutoWhiteBalanceSupport )
            {
                SetAutoWhiteBalance( true );
            }
            else
            {
                SetWhiteBalance( wbcRed, 1.4 );
                SetWhiteBalance( wbcBlue, 2. );
            }
            currentSettings_.boAutoWhiteBalanceEnabled = featuresSupported_.boAutoWhiteBalanceSupport;
        }
        catch( const ImpactAcquireException& e )
        {
            WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to initialize white balance parameters(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
        }
    }
    else
    {
        InitializeWhiteBalanceParametersDriver();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::QueryInterfaceLayoutSpecificSettings( DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    if( pAcC_->exposureTime.isValid() )
    {
        devSettings.exposureTime = pAcC_->exposureTime.read();
    }
    devSettings.boAutoExposureEnabled = ( pAcC_->exposureAuto.isValid() ) ? ( pAcC_->exposureAuto.readS() != string( "Off" ) ) : false;
    devSettings.boAutoGainEnabled = ( pAnC_->gainAuto.isValid() ) ? pAnC_->gainAuto.readS() != string( "Off" ) : false;

    if( pAnC_->blackLevelSelector.isValid() )
    {
        devSettings.unifiedBlackLevel = DoReadUnifiedBlackLevel();
    }

    if( pAnC_->balanceRatioSelector.isValid() )
    {
        pAnC_->balanceRatioSelector.writeS( "Red" );
        devSettings.whiteBalanceRed = pAnC_->balanceRatio.read() * 100.;
        pAnC_->balanceRatioSelector.writeS( "Blue" );
        devSettings.whiteBalanceBlue = pAnC_->balanceRatio.read() * 100.;
    }
    devSettings.boAutoWhiteBalanceEnabled = ( pAnC_->balanceWhiteAuto.isValid() ) ? ( pAnC_->balanceWhiteAuto.readS() != string( "Off" ) ) : false;

    if( DoesDriverOfferColorCorrectionMatrixProcessing() && HasNativeColorCorrection() )
    {
        initialDeviceColorCorrectionMatrix_ = GetInitialDeviceColorCorrectionMatrix();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::RestoreFactoryDefault( void )
//-----------------------------------------------------------------------------
{
    pUSC_->userSetSelector.writeS( "Default" );
    pUSC_->userSetLoad.call();
    WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
    LoggedWriteProperty( wbs.redGain, 1.0 );
    LoggedWriteProperty( wbs.blueGain, 1.0 );
    DetermineMaxFrameRateAtCurrentAOI( false );
    DetermineOptimalHQFrameRateAtCurrentAOI( false );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetOptimalPixelFormatColor( TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    vector<string> pixelFormatStrings;
    pIFC_->pixelFormat.getTranslationDictStrings( pixelFormatStrings );
    const vector<string>::const_iterator itBEGIN = pixelFormatStrings.begin();
    const vector<string>::const_iterator itEND = pixelFormatStrings.end();
    vector<string>::const_iterator it = itEND;

    if( currentSettings_.boCameraBasedColorProcessingEnabled && featuresSupported_.boDeviceSupportsColorCorrectionMatrixProcessing )
    {
        string pixelFormat = "";
        if( ( optimizationLevel == olBestQuality ) ||
            ( optimizationLevel == olCompromiseQualityAndSpeed ) )
        {
            if( ( it = find( itBEGIN, itEND, "RGB10p32" ) ) == itEND )
            {
                if( ( it = find( itBEGIN, itEND, "BGR10V2Packed" ) ) == itEND )
                {
                    it = find( itBEGIN, itEND, "RGB10" );
                }
            }
        }

        if( it == itEND )
        {
            if( ( it = find( itBEGIN, itEND, "RGB8Packed" ) ) == itEND )
            {
                if( ( it = find( itBEGIN, itEND, "BGR8Packed" ) ) == itEND )
                {
                    it = find( itBEGIN, itEND, "RGB8" );
                }
            }
        }
        if( it != itEND )
        {
            LoggedWriteProperty( pIFC_->pixelFormat, *it );
            currentSettings_.imageFormatControlPixelFormat = *it;
        }
    }
    else
    {
        string pixelColorFilter;
        if( pIFC_->pixelColorFilter.isValid() )
        {
            pixelColorFilter = pIFC_->pixelColorFilter.readS();
        }
        else
        {
            for( size_t i = 0; i < pixelFormatStrings.size(); i++ )
            {
                if( pixelFormatStrings[i].find( "BayerRG" ) != string::npos )
                {
                    pixelColorFilter = "BayerRG";
                    break;
                }
                else if( pixelFormatStrings[i].find( "BayerGB" ) != string::npos )
                {
                    pixelColorFilter = "BayerGB";
                    break;
                }
                else if( pixelFormatStrings[i].find( "BayerGR" ) != string::npos )
                {
                    pixelColorFilter = "BayerGR";
                    break;
                }
                else if( pixelFormatStrings[i].find( "BayerBG" ) != string::npos )
                {
                    pixelColorFilter = "BayerBG";
                    break;
                }
            }
        }

        string pixelFormat;
        if( optimizationLevel == olBestQuality )
        {
            if( find( itBEGIN, itEND, pixelColorFilter + "12p" ) != itEND )
            {
                pixelFormat = pixelColorFilter + "12p";
            }
            else if( find( itBEGIN, itEND, pixelColorFilter + "12" ) != itEND )
            {
                pixelFormat = pixelColorFilter + "12";
            }
        }

        if( ( ( optimizationLevel == olCompromiseQualityAndSpeed ) || ( ( optimizationLevel == olBestQuality ) && pixelFormat.empty() ) ) &&
            ( find( itBEGIN, itEND, pixelColorFilter + "10" ) != itEND ) )
        {
            pixelFormat = pixelColorFilter + "10";
        }

        if( pixelFormat.empty() &&
            ( find( itBEGIN, itEND, pixelColorFilter + "8" ) != itEND ) )
        {
            pixelFormat = pixelColorFilter + "8";
        }

        if( !pixelFormat.empty() )
        {
            LoggedWriteProperty( pIFC_->pixelFormat, pixelFormat );
            currentSettings_.imageFormatControlPixelFormat = pixelFormat;
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetOptimalPixelFormatGray( TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    vector<string> pixelFormatStrings;
    pIFC_->pixelFormat.getTranslationDictStrings( pixelFormatStrings );
    const vector<string>::const_iterator itBEGIN = pixelFormatStrings.begin();
    const vector<string>::const_iterator itEND = pixelFormatStrings.end();
    vector<string>::const_iterator it = itEND;

    if( optimizationLevel == olBestQuality )
    {
        if( ( it = find( itBEGIN, itEND, "Mono12p" ) ) == itEND )
        {
            it = find( itBEGIN, itEND, "Mono12" );
        }
    }

    if( ( optimizationLevel == olCompromiseQualityAndSpeed ) || ( ( optimizationLevel == olBestQuality ) && ( it == itEND ) ) )
    {
        it = find( itBEGIN, itEND, "Mono10" );
    }

    if( it == itEND )
    {
        it = find( itBEGIN, itEND, "Mono8" );
    }

    if( it != itEND )
    {
        LoggedWriteProperty( pIFC_->pixelFormat, *it );
        currentSettings_.imageFormatControlPixelFormat = *it;
    }
    else
    {
        // In this case it is a color camera and grayscale preset has been pressed.
        SetOptimalPixelFormatColor( optimizationLevel );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SelectOptimalAutoExposureSettings( bool boHighQuality )
//-----------------------------------------------------------------------------
{
    if( boHighQuality )
    {
        SetAutoExposureUpperLimit( 200000. );
    }
    else
    {
        // We write the 95% of the target value in order to compensate for delays introduced by the readout time,
        // especially when the sensor is operating at the highest allowed auto-exposure limit.
        SetAutoExposureUpperLimit( ( 1000000. / currentSettings_.maxFrameRateAtCurrentAOI ) * 0.95 );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SelectOptimalPixelClock( TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{

    if( pDeC_->mvDeviceClockFrequency.isValid() )
    {
        vector<int64_type> pixelClocks;
        pDeC_->mvDeviceClockFrequency.getTranslationDictValues( pixelClocks );
        if( pixelClocks.size() > 1 )
        {
            sort( pixelClocks.begin(), pixelClocks.end() );
            const string sensorName( pIFC_->sensorName.read() );
            const int64_type optimalPixelClock = (  optimizationLevel != olHighestSpeed ) ? ( sensorName.find( "MT9" ) == 0 ? pixelClocks[1] : pixelClocks[0] ) : pixelClocks[pixelClocks.size() - 1];
            LoggedWriteProperty( pDeC_->mvDeviceClockFrequency, optimalPixelClock );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupExposureControls( void )
//-----------------------------------------------------------------------------
{
    if( pAcC_->exposureTime.isValid() )
    {
        const double exposureMin = pAcC_->exposureTime.hasMinValue() ? pAcC_->exposureTime.getMinValue() : 1.;
        const double exposureMax = ( pAcC_->exposureTime.hasMaxValue() && ( pAcC_->exposureTime.getMaxValue() < 200000. ) ) ? pAcC_->exposureTime.getMaxValue() : 200000.;
        const double exposure = pAcC_->exposureTime.read();
        const bool boHasStepWidth = pAcC_->exposureTime.hasStepWidth();
        double increment = boHasStepWidth ? pAcC_->exposureTime.getStepWidth() : 1.;
        DoSetupExposureControls( exposureMin, exposureMax, exposure, boHasStepWidth, increment );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupGainControls( void )
//-----------------------------------------------------------------------------
{
    if( pAnC_->gainSelector.isValid() )
    {
        DoSetupGainControls( analogGainMin_ + digitalGainMin_, analogGainMax_ + digitalGainMax_, DoReadUnifiedGain() );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupBlackLevelControls( void )
//-----------------------------------------------------------------------------
{
    if( pAnC_->blackLevelSelector.isValid() )
    {
        DoSetupBlackLevelControls( -24., 24., DoReadUnifiedBlackLevel() );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupWhiteBalanceControls( void )
//-----------------------------------------------------------------------------
{
    if( pAnC_->balanceRatioSelector.isValid() )
    {
        pAnC_->balanceRatioSelector.writeS( "Red" );
        const double whiteBalanceR = pAnC_->balanceRatio.read() * 100.0;
        pAnC_->balanceRatioSelector.writeS( "Blue" );
        const double whiteBalanceB = pAnC_->balanceRatio.read() * 100.0;
        DoSetupWhiteBalanceControls( 1., 500., whiteBalanceR, 1., 500., whiteBalanceB );
    }
    else
    {
        SetupWhiteBalanceControlsDriver();
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupFrameRateControls( void )
//-----------------------------------------------------------------------------
{
    if( pAcC_->mvAcquisitionFrameRateLimitMode.isValid() && ( pAcC_->mvAcquisitionFrameRateLimitMode.readS() != string( "mvDeviceLinkThroughput" ) ) )
    {
        // not in the correct FrameRateLimitMode
        return;
    }

    if( GetFrameRateEnable() )
    {
        // These settings are used when the frame rate is manually configured.
        // Depending on whether the old or the new frame rate behaviour is used we have to use a different property here. This did change in firmware version 1.6.450.0
        static const unsigned int s_FW_VERSION_WITH_NEW_RESULTING_FRAMERATE_BEHAVIOR = ( ( 1 << 24 ) | ( 6 << 16 ) | ( 450 << 4 ) | 0 ); // 1.6.450.0 in 8.8.12.4 notation
        PropertyF propFrameRate( ( currentDeviceFWVersion_ < s_FW_VERSION_WITH_NEW_RESULTING_FRAMERATE_BEHAVIOR ) ? pAcC_->acquisitionFrameRate.hObj() : pAcC_->mvResultingFrameRate.hObj() );

        const double frameRateRangeMin = propFrameRate.hasMinValue() ? ( propFrameRate.getMinValue() >= 5. ? propFrameRate.getMinValue() : 5. ) : 2.;
        const double frameRateRangeMax = propFrameRate.hasMaxValue() ? pAcC_->acquisitionFrameRate.getMaxValue() : 500.;
        const double frameRate = ( pAcC_->acquisitionFrameRate.read() > propFrameRate.read() ) ? propFrameRate.read() : pAcC_->acquisitionFrameRate.read();

        UpdateFrameRateWarningBitmap( ( 1000000. / pAcC_->exposureTime.read() ) < frameRate );
        DoSetupFrameRateControls( frameRateRangeMin, frameRateRangeMax, frameRate );
    }
    else
    {
        // These settings are used when FrameRate is set to Auto.
        //--------------------------------------
        // Hardcoding the frameRateRangeMin value is not elegant; however there is no other obvious minimum
        // that could be used instead. Number 5 also fits with the maximum exposure limit of the wizard (200ms).
        const double frameRateRangeMin = 5;
        const double frameRateRangeMax = ( pAcC_->mvResultingFrameRate.isValid() && pAcC_->mvResultingFrameRate.hasMaxValue() ) ? pAcC_->mvResultingFrameRate.getMaxValue() : 100.;
        const double frameRate = pAcC_->mvResultingFrameRate.read();

        // This may only happen in High-Quality modes, since in High-Speed modes the maximum auto-exposure value allowed is
        // limited according to the cameras maximum fps value in function DetermineMaxFrameRateAtCurrentAOI()
        UpdateFrameRateWarningBitmap( ( 1000000. / pAcC_->exposureTime.read() ) < pAcC_->mvResultingFrameRate.read() );
        DoSetupFrameRateControls( frameRateRangeMin, frameRateRangeMax, frameRate );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetWhiteBalance( TWhiteBalanceChannel channel, double value )
//-----------------------------------------------------------------------------
{
    if( pAnC_->balanceRatioSelector.isValid() )
    {
        if( pAnC_->balanceRatioSelector.readS() == string( channel == wbcRed ? "Blue" : "Red" ) )
        {
            LoggedWriteProperty( pAnC_->balanceRatioSelector, string( channel == wbcRed ? "Red" : "Blue" ) );
        }
        LoggedWriteProperty( pAnC_->balanceRatio, value );
    }
    else
    {
        SetWhiteBalanceDriver( channel, value );
    }
}

//-----------------------------------------------------------------------------
bool WizardQuickSetupGenICam::Supports12BitPixelFormats( void ) const
//-----------------------------------------------------------------------------
{
    if( pIFC_->sensorName.isValid() )
    {
        const string sensorName = pIFC_->sensorName.readS();
        if( sensorName.find( "IMX" ) != string::npos )
        {
            if( pIFC_->pixelFormat.isValid() && pIFC_->pixelFormat.hasDict() )
            {
                set<string> validFormats;
                validFormats.insert( "BayerRG12" );
                validFormats.insert( "BayerGR12" );
                validFormats.insert( "BayerGB12" );
                validFormats.insert( "BayerBG12" );
                validFormats.insert( "BGR10V2Packed" );
                validFormats.insert( "RGB10" );
                validFormats.insert( "RGB10p32" );
                validFormats.insert( "Mono12p" );
                validFormats.insert( "Mono12" );

                const set<string>::const_iterator itBEGIN = validFormats.begin();
                const set<string>::const_iterator itEND = validFormats.end();

                vector<string> pixelFormatStrings;
                pIFC_->pixelFormat.getTranslationDictStrings( pixelFormatStrings );
                const vector<string>::size_type cnt = pixelFormatStrings.size();
                for( vector<string>::size_type i = 0; i < cnt; i++ )
                {
                    if( find( itBEGIN, itEND, pixelFormatStrings[i] ) != itEND )
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetFrameRate( double value )
//-----------------------------------------------------------------------------
{
    if( pAcC_->acquisitionFrameRate.hasMaxValue() )
    {
        // Out-of-bounds check because of Frame rate controls' dynamic nature
        const double currentMaxFrameRate = pAcC_->acquisitionFrameRate.getMaxValue();
        LoggedWriteProperty( pAcC_->acquisitionFrameRate, ( value > currentMaxFrameRate ) ? currentMaxFrameRate : value );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetAutoExposure( bool boEnable )
//-----------------------------------------------------------------------------
{
    if( boEnable )
    {
        LoggedWriteProperty( pAcC_->exposureAuto, string( "Continuous" ) );
        LoggedWriteProperty( pAcC_->mvExposureAutoUpperLimit, 200000. );
        LoggedWriteProperty( pAcC_->mvExposureAutoAverageGrey, 50 );
        if( pAcC_->mvExposureAutoMode.isValid() )
        {
            LoggedWriteProperty( pAcC_->mvExposureAutoMode, string( "mvDevice" ) );
        }
    }
    else
    {
        LoggedWriteProperty( pAcC_->exposureAuto, string( "Off" ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetAutoExposureUpperLimit( double exposureMax )
//-----------------------------------------------------------------------------
{
    try
    {
        if( pAcC_->mvExposureAutoUpperLimit.isValid() )
        {
            bool boMustTurnOffAEC = false;
            if( HasAEC() && ( pAcC_->exposureAuto.readS() == "Off" ) )
            {
                SetAutoExposure( true );
                boMustTurnOffAEC = true;
            }
            // 'mvExposureAutoUpperLimit' is only writeable if 'ExposureAuto' is active('Continuous')
            LoggedWriteProperty( pAcC_->mvExposureAutoUpperLimit, exposureMax );
            if( boMustTurnOffAEC )
            {
                SetAutoExposure( false );
            }

            // This must be done here until the AEC/AGC algorithm on MV GenICam devices checks
            // the Exposure Upper Limit even if Gain is not zero.
            if( pAcC_->exposureTime.read() > pAcC_->mvExposureAutoUpperLimit.read() )
            {
                LoggedWriteProperty( pAcC_->exposureTime, pAcC_->mvExposureAutoUpperLimit.read() );
            }
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write 'mvExposureAutoUpperLimit' value(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetAutoGain( bool boEnable )
//-----------------------------------------------------------------------------
{
    if( boEnable )
    {
        if( currentSettings_.unifiedGain > currentSettings_.analogGainMax || currentSettings_.unifiedGain < currentSettings_.analogGainMin )
        {
            // We do this to eliminate any residual digital gain values, since we switch to auto-gain which is analog, and we
            // do not need any digital offset to carry with us.
            DoWriteUnifiedGain( 0. );
        }
    }
    pAnC_->gainSelector.writeS( "AnalogAll" );
    LoggedWriteProperty( pAnC_->gainAuto, string( boEnable ? "Continuous" : "Off" ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupUnifiedGainData( void )
//-----------------------------------------------------------------------------
{
    if( !HasGain() )
    {
        currentSettings_.analogGainMax = 1.;
        currentSettings_.analogGainMin = 0.;
        currentSettings_.digitalGainMax = 0.;
        currentSettings_.digitalGainMin = 0.;
        return;
    }
    vector<string> gainSelectorStrings;
    pAnC_->gainSelector.getTranslationDictStrings( gainSelectorStrings );
    const vector<string>::const_iterator it = gainSelectorStrings.begin();
    const vector<string>::const_iterator itEND = gainSelectorStrings.end();
    string originalSetting = pAnC_->gainSelector.readS();
    if( find( it, itEND, "AnalogAll" ) != itEND && find( it, itEND, "DigitalAll" ) != itEND )
    {
        pAnC_->gainSelector.writeS( "AnalogAll" );
        currentSettings_.analogGainMax = pAnC_->gain.getMaxValue();
        currentSettings_.analogGainMin = ( pAnC_->gain.getMinValue() < 0 ) ? ( pAnC_->gain.getMinValue() < -6.0 ) ? ( -6.0 ) : ( pAnC_->gain.getMinValue() ) : 0;
        pAnC_->gainSelector.writeS( "DigitalAll" );
        currentSettings_.digitalGainMax = pAnC_->gain.getMaxValue();
        currentSettings_.digitalGainMin = pAnC_->gain.getMinValue();
    }
    else if( find( it, itEND, "AnalogAll" ) != itEND && find( it, itEND, "DigitalAll" ) == itEND )
    {
        pAnC_->gainSelector.writeS( "AnalogAll" );
        currentSettings_.analogGainMax = pAnC_->gain.getMaxValue();
        currentSettings_.analogGainMin = ( pAnC_->gain.getMinValue() < 0 ) ? ( pAnC_->gain.getMinValue() < -6.0 ) ? ( -6.0 ) : ( pAnC_->gain.getMinValue() ) : 0;
        currentSettings_.digitalGainMax = 0.;
        currentSettings_.digitalGainMin = 0.;
    }
    else
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Device does not support an 'AnalogAll' Gain selector!" ) ) );
    }
    pAnC_->gainSelector.writeS( originalSetting );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::SetupUnifiedBlackLevelData( void )
//-----------------------------------------------------------------------------
{
    if( !HasBlackLevel() )
    {
        currentSettings_.analogBlackLevelMax = 1;
        currentSettings_.analogBlackLevelMin = 0;
        return;
    }
    vector<string> blackLevelSelectorStrings;
    pAnC_->blackLevelSelector.getTranslationDictStrings( blackLevelSelectorStrings );
    vector<string>::const_iterator it = blackLevelSelectorStrings.begin();
    const vector<string>::const_iterator itEND = blackLevelSelectorStrings.end();
    string originalSetting = pAnC_->blackLevelSelector.readS();
    if( ( find( it, itEND, "All" ) != itEND ) &&
        ( find( it, itEND, "DigitalAll" ) != itEND ) )
    {
        pAnC_->blackLevelSelector.writeS( "All" );
        currentSettings_.analogBlackLevelMax = pAnC_->blackLevel.getMaxValue();
        currentSettings_.analogBlackLevelMin = ( pAnC_->blackLevel.getMinValue() < 0 ) ? ( pAnC_->blackLevel.getMinValue() < -6.0 ) ? ( -6.0 ) : ( pAnC_->blackLevel.getMinValue() ) : 0;
    }
    else
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Device does not support an 'All' or a 'DigitalAll' BlackLevel selector!" ) ) );
    }
    pAnC_->blackLevelSelector.writeS( originalSetting );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::TryToReadFrameRate( double& value )
//-----------------------------------------------------------------------------
{
    if( pAcC_->acquisitionFrameRate.isValid() )
    {
        // Up to this point the device has not been analyzed and there is no information available about
        // what capabilities it supports, and crucially whether it supports Auto frame rate or not. Maybe there
        // is another way to do this, or the sequence of QueryInitialSettings and AnalyzeDeviceAndGatherInformation
        // should be reworked...? As of now it is not clear what will happen with third-party devices.
        if( pAcC_->mvAcquisitionFrameRateLimitMode.isValid() && ( pAcC_->mvAcquisitionFrameRateLimitMode.readS() != string( "mvDeviceLinkThroughput" ) ) )
        {
            LoggedWriteProperty( pAcC_->mvAcquisitionFrameRateLimitMode, string( "mvDeviceLinkThroughput" ) );
        }
        if( GetFrameRateEnable() == false )
        {
            SetFrameRateEnable( true );
            value = pAcC_->acquisitionFrameRate.read();
            SetFrameRateEnable( false );
        }
        else
        {
            value = pAcC_->acquisitionFrameRate.read();
        }
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::WriteExposureFeatures( const DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    LoggedWriteProperty( pAcC_->exposureTime, devSettings.exposureTime );
    if( featuresSupported_.boAutoExposureSupport )
    {
        LoggedWriteProperty( pAcC_->exposureAuto, string( devSettings.boAutoExposureEnabled ? "Continuous" : "Off" ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::WriteGainFeatures( const DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boAutoGainSupport )
    {
        if( pAnC_->gainSelector.readS() != string( "AnalogAll" ) )
        {
            pAnC_->gainSelector.writeS( "AnalogAll" );
        }
        LoggedWriteProperty( pAnC_->gainAuto, string( devSettings.boAutoGainEnabled ? "Continuous" : "Off" ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupGenICam::WriteWhiteBalanceFeatures( const DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    if( pAnC_->balanceRatioSelector.isValid() )
    {
        if( pAnC_->balanceRatioSelector.readS() == "Red" )
        {
            LoggedWriteProperty( pAnC_->balanceRatio, devSettings.whiteBalanceRed / 100. );
            LoggedWriteProperty( pAnC_->balanceRatioSelector, string( "Blue" ) );
            LoggedWriteProperty( pAnC_->balanceRatio, devSettings.whiteBalanceBlue / 100. );
        }
        else
        {
            LoggedWriteProperty( pAnC_->balanceRatio, devSettings.whiteBalanceBlue / 100. );
            LoggedWriteProperty( pAnC_->balanceRatioSelector, string( "Red" ) );
            LoggedWriteProperty( pAnC_->balanceRatio, devSettings.whiteBalanceRed / 100. );
        }
        if( featuresSupported_.boAutoWhiteBalanceSupport )
        {
            LoggedWriteProperty( pAnC_->balanceWhiteAuto, string( devSettings.boAutoWhiteBalanceEnabled ? "Continuous" : "Off" ) );
        }
    }
    else
    {
        WriteWhiteBalanceFeaturesDriver( devSettings );
    }
}


//=============================================================================
//============== Implementation WizardQuickSetupDeviceSpecific ================
//=============================================================================
//-----------------------------------------------------------------------------
WizardQuickSetupDeviceSpecific::WizardQuickSetupDeviceSpecific( PropViewFrame* pParent, const wxString& title, Device* pDev, FunctionInterface* pFI, bool boAutoConfigureOnStart ) :
    WizardQuickSetup( pParent, title, pDev, pFI, boAutoConfigureOnStart ), pCSBF_( 0 )
//-----------------------------------------------------------------------------
{
    InitializeWizardForSelectedDevice( pDev );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::CreateInterfaceLayoutSpecificControls( Device* pDev )
//-----------------------------------------------------------------------------
{
    pCSBF_ = new CameraSettingsBlueFOX( pDev );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::DeleteInterfaceLayoutSpecificControls( void )
//-----------------------------------------------------------------------------
{
    DeleteElement( pCSBF_ );
}

//-----------------------------------------------------------------------------
double WizardQuickSetupDeviceSpecific::DetermineMaxFrameRateAtCurrentAOI( bool /*checkAllSupportedPixelFormats*/ )
//-----------------------------------------------------------------------------
{
    // Devices supporting the DeviceSpecific Interface have no way of calculating the maximum
    // frame rate by themselves. This means we have to settle with a worst-case scenario approach (mvBF-220aC/G).
    // Iterating over the entire product-gamut with a switch-case construct could be another approach
    return 100.;
}

//-----------------------------------------------------------------------------
double WizardQuickSetupDeviceSpecific::DetermineOptimalHQFrameRateAtCurrentAOI( bool /*checkAllSupportedPixelFormats*/ )
//-----------------------------------------------------------------------------
{
    // Devices supporting the DeviceSpecific Interface have no way of calculating the maximum
    // frame rate by themselves. This means we have to settle with a frame rate that allows relatively
    // long exposures on one hand, and on the other also allows a stutter free live image.
    // Iterating over the entire product-gamut with a switch-case construct could be another approach
    return 25.;
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::DoConfigureColorCorrection( bool boActive )
//-----------------------------------------------------------------------------
{
    DoConfigureDriverBaseColorCorrection( boActive );
}

//-----------------------------------------------------------------------------
double WizardQuickSetupDeviceSpecific::DoReadSaturationData( void ) const
//-----------------------------------------------------------------------------
{
    double currentSaturation = 0.;
    try
    {
        if( featuresSupported_.boDriverSupportsColorCorrectionMatrixProcessing )
        {
            currentSaturation = ( ( pIP_->colorTwistRow0.read( 0 ) - 0.299 ) / 0.701 ) * 100.0;
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to read saturation value(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
    return currentSaturation;
}

//-----------------------------------------------------------------------------
double WizardQuickSetupDeviceSpecific::DoReadUnifiedBlackLevel( void ) const
//-----------------------------------------------------------------------------
{
    if( pIP_->gainOffsetKneeEnable.isValid() )
    {
        if( pIP_->gainOffsetKneeEnable.read() == bFalse )
        {
            LoggedWriteProperty( pIP_->gainOffsetKneeEnable, bTrue );
        }
        return pIP_->gainOffsetKneeMasterOffset_pc.read();
    }
    return 0.;
}

//-----------------------------------------------------------------------------
double WizardQuickSetupDeviceSpecific::DoReadUnifiedGain( void ) const
//-----------------------------------------------------------------------------
{
    return ( pCSBF_->gain_dB.isValid() ) ? pCSBF_->gain_dB.read() : 0.;
}

//-----------------------------------------------------------------------------
string WizardQuickSetupDeviceSpecific::DoWriteSaturationData( double saturation )
//-----------------------------------------------------------------------------
{
    return DoWriteDriverBasedSaturationData( saturation );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::DoWriteUnifiedBlackLevelData( double value ) const
//-----------------------------------------------------------------------------
{
    try
    {
        if( pIP_->gainOffsetKneeEnable.isValid() )
        {
            if( pIP_->gainOffsetKneeEnable.read() == bFalse )
            {
                LoggedWriteProperty( pIP_->gainOffsetKneeEnable, bTrue );
            }
            LoggedWriteProperty( pIP_->gainOffsetKneeMasterOffset_pc, value );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write blackLevel value:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::DoWriteUnifiedGain( double value ) const
//-----------------------------------------------------------------------------
{
    if( pCSBF_->gain_dB.isValid() && pCSBF_->gain_dB.isVisible() )
    {
        LoggedWriteProperty( pCSBF_->gain_dB, value );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetExposureOverlapped( bool boEnable )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->exposeMode.isValid() )
    {
        vector<TCameraExposeMode> exposeModes;
        pCSBF_->exposeMode.getTranslationDictValues( exposeModes );
        const vector<TCameraExposeMode>::const_iterator itBEGIN = exposeModes.begin();
        const vector<TCameraExposeMode>::const_iterator itEND = exposeModes.end();
        if( find( itBEGIN, itEND, cemOverlapped ) != itEND )
        {
            LoggedWriteProperty( pCSBF_->exposeMode, string( boEnable ? "Overlapped" : "Standard" ) );
        }
    }
}

//-----------------------------------------------------------------------------
double WizardQuickSetupDeviceSpecific::GetAutoExposureUpperLimit( void ) const
//-----------------------------------------------------------------------------
{
    return ( pCSBF_->getAutoControlParameters().exposeUpperLimit_us.isValid() ) ? pCSBF_->getAutoControlParameters().exposeUpperLimit_us.read() : 100000.;
}

//-----------------------------------------------------------------------------
string WizardQuickSetupDeviceSpecific::GetPixelFormat( void ) const
//-----------------------------------------------------------------------------
{
    return ( pCSBF_->pixelFormat.isValid() ) ? pCSBF_->pixelFormat.readS() : string( "Mono8" );
}

//-----------------------------------------------------------------------------
string WizardQuickSetupDeviceSpecific::GetPixelClock( void ) const
//-----------------------------------------------------------------------------
{
    return ( pCSBF_->pixelClock_KHz.isValid() ) ? pCSBF_->pixelClock_KHz.readS() : string( "NoClockFound" );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::InitializeBlackLevelParameters( void )
//-----------------------------------------------------------------------------
{
    LoggedWriteProperty( pCSBF_->offsetAutoCalibration, string( "On" ) );
    DoWriteUnifiedBlackLevelData( 0. );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::QueryInterfaceLayoutSpecificSettings( DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->expose_us.isValid() )
    {
        devSettings.exposureTime = pCSBF_->expose_us.read();
    }
    devSettings.boAutoExposureEnabled = ( pCSBF_->autoExposeControl.isValid() ) ? ( pCSBF_->autoExposeControl.read() != aecOff ) : false;
    devSettings.boAutoGainEnabled = ( pCSBF_->autoGainControl.isValid() ) ? ( pCSBF_->autoGainControl.read() != agcOff ) : false;

    if( pCSBF_->offset_pc.isValid() )
    {
        devSettings.unifiedBlackLevel = DoReadUnifiedBlackLevel();
    }

    WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
    devSettings.whiteBalanceRed = wbs.redGain.read() * 100.;
    devSettings.whiteBalanceBlue = wbs.blueGain.read() * 100.;

    // No 'auto white balance' when using the driver based filter!
    devSettings.boAutoWhiteBalanceEnabled = false;
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::RestoreFactoryDefault( void )
//-----------------------------------------------------------------------------
{
    pCSBF_->restoreDefault();
    WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
    LoggedWriteProperty( wbs.redGain, 1.0 );
    LoggedWriteProperty( wbs.blueGain, 1.0 );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetOptimalPixelFormatColor( TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    static const string s_m10 = string( "Mono10" );
    static const string s_m8 = string( "Mono8" );

    vector<string> pixelFormatStrings;
    pCSBF_->pixelFormat.getTranslationDictStrings( pixelFormatStrings );
    const vector<string>::const_iterator itBEGIN = pixelFormatStrings.begin();
    const vector<string>::const_iterator itEND = pixelFormatStrings.end();
    if( find( itBEGIN, itEND, s_m10 ) != itEND &&
        optimizationLevel != olHighestSpeed &&
        optimizationLevel != olCompromiseQualityAndSpeed )
    {
        LoggedWriteProperty( pCSBF_->pixelFormat, s_m10 );
        currentSettings_.imageFormatControlPixelFormat = s_m10;
    }
    else if( find( itBEGIN, itEND, s_m8 ) != itEND )
    {
        LoggedWriteProperty( pCSBF_->pixelFormat, s_m8 );
        currentSettings_.imageFormatControlPixelFormat = s_m8;
    }
    else
    {
        static const string s_defaultPixelFormat = string( "Auto" );
        LoggedWriteProperty( pCSBF_->pixelFormat, s_defaultPixelFormat );
        currentSettings_.imageFormatControlPixelFormat = s_defaultPixelFormat;
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetOptimalPixelFormatGray( TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    static const string s_m10 = string( "Mono10" );
    static const string s_m8 = string( "Mono8" );

    vector<string> pixelFormatStrings;
    pCSBF_->pixelFormat.getTranslationDictStrings( pixelFormatStrings );
    const vector<string>::const_iterator it = pixelFormatStrings.begin();
    const vector<string>::const_iterator itEND = pixelFormatStrings.end();
    if( ( find( it, itEND, s_m10 ) != itEND ) &&
        optimizationLevel != olHighestSpeed &&
        optimizationLevel != olCompromiseQualityAndSpeed )
    {
        LoggedWriteProperty( pCSBF_->pixelFormat, s_m10 );
        currentSettings_.imageFormatControlPixelFormat = s_m10;
    }
    else if( find( it, itEND, s_m8 ) != itEND )
    {
        LoggedWriteProperty( pCSBF_->pixelFormat, s_m8 );
        currentSettings_.imageFormatControlPixelFormat = s_m8;
    }
    else
    {
        static const string s_defaultPixelFormat = string( "Auto" );
        LoggedWriteProperty( pCSBF_->pixelFormat, s_defaultPixelFormat );
        currentSettings_.imageFormatControlPixelFormat = s_defaultPixelFormat;
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SelectOptimalAutoExposureSettings( bool boHighQuality )
//-----------------------------------------------------------------------------
{
    if( boHighQuality == true )
    {
        SetAutoExposureUpperLimit( 200000. );
    }
    else
    {
        // We write the 95% of the target value in order to compensate for delays introduced by the readout time,
        // especially when the sensor is operating at the highest allowed auto-exposure limit.
        SetAutoExposureUpperLimit( ( 1000000. / currentSettings_.maxFrameRateAtCurrentAOI ) * 0.95 );
    }

    // When speed is preferred, the ExposeMode overlapped might help, if available!
    SetExposureOverlapped( !boHighQuality );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SelectOptimalPixelClock( TOptimizationLevel optimizationLevel )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->pixelClock_KHz.isValid() )
    {
        vector<TCameraPixelClock> pixelClocks;
        pCSBF_->pixelClock_KHz.getTranslationDictValues( pixelClocks );
        sort( pixelClocks.begin(), pixelClocks.end() );
        const TCameraPixelClock optimalPixelClock = ( optimizationLevel != olHighestSpeed ) ? pixelClocks.front() : pixelClocks.back();
        LoggedWriteProperty( pCSBF_->pixelClock_KHz, optimalPixelClock );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetupExposureControls( void )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->expose_us.isValid() )
    {
        const double exposureMin = pCSBF_->expose_us.hasMinValue() ? pCSBF_->expose_us.getMinValue() : 1.;
        const double exposureMax = ( pCSBF_->expose_us.hasMaxValue() && ( pCSBF_->expose_us.getMaxValue() < 200000. ) ) ? pCSBF_->expose_us.getMaxValue() : 200000.;
        const double exposure = pCSBF_->expose_us.read();
        const bool boHasStepWidth = pCSBF_->expose_us.hasStepWidth();
        const double increment = boHasStepWidth ? static_cast< double >( pCSBF_->expose_us.getStepWidth() ) : 1.;
        DoSetupExposureControls( exposureMin, exposureMax, exposure, boHasStepWidth, increment );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetupGainControls( void )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->gain_dB.isValid() )
    {
        const double gainUnifiedRangeMin = analogGainMin_ + digitalGainMin_;
        const double gainUnifiedRangeMax = analogGainMax_ + digitalGainMax_;
        const double gain = DoReadUnifiedGain();
        DoSetupGainControls( gainUnifiedRangeMin, gainUnifiedRangeMax, gain );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetupBlackLevelControls( void )
//-----------------------------------------------------------------------------
{
    if( pIP_->gainOffsetKneeEnable.isValid() )
    {
        const double blackLevelUnifiedRangeMin = -24.;
        const double blackLevelUnifiedRangeMax = 24.;
        const double blackLevel = DoReadUnifiedBlackLevel();
        DoSetupBlackLevelControls( blackLevelUnifiedRangeMin, blackLevelUnifiedRangeMax, blackLevel );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetupWhiteBalanceControlsDriver( void )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boColorOptionsSupport )
    {
        WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
        const double whiteBalanceRMin = 10.;
        const double whiteBalanceRMax = 500.;
        const double whiteBalanceR = wbs.redGain.read() * 100.0;
        const double whiteBalanceBMin = 10.;
        const double whiteBalanceBMax = 500.;
        const double whiteBalanceB = wbs.blueGain.read() * 100.0;
        DoSetupWhiteBalanceControls( whiteBalanceRMin, whiteBalanceRMax, whiteBalanceR, whiteBalanceBMin, whiteBalanceBMax, whiteBalanceB );
    }
    else
    {
        // This has to be done for aesthetic reasons. If a grayscale camera is opened, the white balance controls are
        // of course grayed out, however the last values (e.g. from the previous color camera) are still being shown
        DoSetupWhiteBalanceControls( 1., 500., 100., 1., 500., 100. );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::SetWhiteBalanceDriver( TWhiteBalanceChannel channel, double value )
//-----------------------------------------------------------------------------
{
    // WhiteBalance for mvBF2 has to be done via driver Properties
    try
    {
        WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
        if( pIP_->whiteBalance.readS() != string( "User1" ) )
        {
            LoggedWriteProperty( pIP_->whiteBalance, string( "User1" ) );
        }
        if( channel == wbcRed )
        {
            LoggedWriteProperty( wbs.redGain, value );
        }
        else if( channel == wbcBlue )
        {
            LoggedWriteProperty( wbs.blueGain, value );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Failed to write white balance values:\n%s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetup::WriteWhiteBalanceFeaturesDriver( const DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    WhiteBalanceSettings& wbs = pIP_->getWBUserSetting( 0 );
    LoggedWriteProperty( wbs.redGain, devSettings.whiteBalanceRed / 100. );
    LoggedWriteProperty( wbs.blueGain, devSettings.whiteBalanceBlue / 100. );
    if( featuresSupported_.boAutoWhiteBalanceSupport )
    {
        // In case continuous auto white balancing is implemented:
        //pCSBF_->"balanceWhiteAuto-Property".writeS( string( devSettings.boAutoWhiteBalanceEnabled ? "Continuous" : "Off" ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetAutoExposure( bool boEnable )
//-----------------------------------------------------------------------------
{
    if( boEnable )
    {
        AutoControlParameters ACP = pCSBF_->getAutoControlParameters();
        LoggedWriteProperty( ACP.aoiMode, string( "Full" ) );
        LoggedWriteProperty( pCSBF_->autoExposeControl, string( "On" ) );
        LoggedWriteProperty( ACP.exposeUpperLimit_us, 200000 );
        LoggedWriteProperty( ACP.desiredAverageGreyValue, 70 );
    }
    else
    {
        LoggedWriteProperty( pCSBF_->autoExposeControl, string( "Off" ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetAutoExposureUpperLimit( double exposureMax )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->getAutoControlParameters().exposeUpperLimit_us.isValid() )
    {
        LoggedWriteProperty( pCSBF_->getAutoControlParameters().exposeUpperLimit_us, static_cast<int>( exposureMax ) );
    }
    LoggedWriteProperty( pCSBF_->expose_us, static_cast<int>( exposureMax / 2 ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetAutoGain( bool boEnable )
//-----------------------------------------------------------------------------
{
    LoggedWriteProperty( pCSBF_->autoGainControl, string( boEnable ? "On" : "Off" ) );
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetupUnifiedGainData( void )
//-----------------------------------------------------------------------------
{
    if( pCSBF_->gain_dB.isValid() )
    {
        currentSettings_.analogGainMax = pCSBF_->gain_dB.getMaxValue();
        currentSettings_.analogGainMin = pCSBF_->gain_dB.getMinValue();
        currentSettings_.digitalGainMax = 0;
        currentSettings_.digitalGainMin = 0;
    }
    else
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Device has no 'Gain_dB' Property!" ) ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::SetupUnifiedBlackLevelData( void )
//-----------------------------------------------------------------------------
{
    if( pIP_->gainOffsetKneeEnable.isValid() )
    {
        if( pIP_->gainOffsetKneeEnable.read() == bFalse )
        {
            LoggedWriteProperty( pIP_->gainOffsetKneeEnable, bTrue );
        }
        currentSettings_.analogBlackLevelMax = pIP_->gainOffsetKneeMasterOffset_pc.getMaxValue();
        currentSettings_.analogBlackLevelMin = pIP_->gainOffsetKneeMasterOffset_pc.getMinValue();
    }
    else
    {
        WriteQuickSetupWizardErrorMessage( wxString::Format( wxT( "Device has no 'gainOffsetKneeEnable' Property!" ) ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::WriteExposureFeatures( const DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    LoggedWriteProperty( pCSBF_->expose_us, static_cast<int>( devSettings.exposureTime ) );
    if( featuresSupported_.boAutoExposureSupport )
    {
        LoggedWriteProperty( pCSBF_->autoExposeControl, string( devSettings.boAutoExposureEnabled ? "On" : "Off" ) );
    }
}

//-----------------------------------------------------------------------------
void WizardQuickSetupDeviceSpecific::WriteGainFeatures( const DeviceSettings& devSettings )
//-----------------------------------------------------------------------------
{
    if( featuresSupported_.boAutoGainSupport )
    {
        LoggedWriteProperty( pCSBF_->autoGainControl, string( devSettings.boAutoGainEnabled ? "On" : "Off" ) );
    }
}
