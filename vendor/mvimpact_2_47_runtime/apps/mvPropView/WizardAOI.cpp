#include <algorithm>
#include <apps/Common/exampleHelper.h>
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "DataConversion.h"
#include "GlobalDataStorage.h"
#include "ImageCanvas.h"
#include "spinctld.h"
#include "WizardAOI.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/platinfo.h>
#include <wx/spinctrl.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;

//=============================================================================
//============== Implementation StoppedAcquisitionScope =======================
//=============================================================================
//-----------------------------------------------------------------------------
class StoppedAcquisitionScope
//-----------------------------------------------------------------------------
{
    PropViewFrame* pWindow_;
public:
    explicit StoppedAcquisitionScope( PropViewFrame* pWindow ) : pWindow_( pWindow )
    {
        pWindow_->EnsureAcquisitionState( false );
    }
    ~StoppedAcquisitionScope()
    {
        pWindow_->EnsureAcquisitionState( true );
    }
};

//=============================================================================
//============== Implementation WizardAOI =====================================
//=============================================================================
BEGIN_EVENT_TABLE( WizardAOI, OkAndCancelDlg )
    EVT_BUTTON( widCalibrateButton, WizardAOI::OnBtnCalibrate )
    EVT_CHECKBOX( widEnable, WizardAOI::OnCheckBoxStateChanged )
    EVT_NOTEBOOK_PAGE_CHANGED( widNotebookAOI, WizardAOI::OnNotebookPageChanged )
    EVT_CLOSE( WizardAOI::OnClose )
END_EVENT_TABLE()
//-----------------------------------------------------------------------------
WizardAOI::WizardAOI( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, ImageCanvas* pImageCanvas, bool boAcquisitionStateOnCancel, FunctionInterface* pFI )
    : OkAndCancelDlg( pParent, widMainFrame, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX ),
      pParent_( pParent ), pDev_( pDev ), pFI_( pFI ), pImageCanvas_( pImageCanvas ), currentErrors_(), configuredAreas_(), boGUICreationComplete_( false ), boAcquisitionStateOnCancel_( boAcquisitionStateOnCancel ),
      boHandleChangedMessages_( true ), pAOIForOverlay_( 0 ), previousSensorAOI_(), rectSensor_(), offsetXIncSensor_( -1LL ), offsetYIncSensor_( -1LL ), widthIncSensor_( -1LL ), heightIncSensor_( -1LL ),
      widthMinSensor_( -1LL ), heightMinSensor_( -1LL ), pCBShowAOILabel_( 0 ), pAOISettingsNotebook_( 0 ), pStaticBitmapWarning_( 0 ), pInfoText_( 0 ), imageProcessing_( pDev_ ), imageDestination_( pDev_ )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pSettingsSizer                  | |
        | | |--------| |------------------| | |
        | | | Global | |   Area-Specific  | | |
        | | |        | |                  | | |
        | | |Settings| |     Settings     | | |
        | | |--------| |------------------| | |
        | |---------------------------------| |
        |                spacer               |
        | |---------------------------------| |
        | | pButtonSizer                    | |
        | |---------------------------------| |
        |-------------------------------------|
        */

    wxPanel* pMainPanel = new wxPanel( this );
    pStaticBitmapWarning_ = new wxStaticBitmap( pMainPanel, widStaticBitmapWarning, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Empty ), wxDefaultPosition, wxDefaultSize, 0, wxT( "" ) );
    pInfoText_ = new wxStaticText( pMainPanel, wxID_ANY, wxT( "" ), wxDefaultPosition, wxSize( 360, WXGRID_DEFAULT_ROW_HEIGHT ), wxST_ELLIPSIZE_MIDDLE );

    pImageCanvas_->DisableDoubleClickAndPrunePopupMenu( true );

    pAOISettingsNotebook_ = new wxNotebook( pMainPanel, widNotebookAOI, wxDefaultPosition, wxDefaultSize, wxNB_TOP );

    int aoiNumber = 0;

    ComposeAreaPanelAOI( aoictAOI, true, aoiNumber );
    aoiNumber++;

    const bool boDeviceSpecific = ( ( ( !pDev_->interfaceLayout.isValid() ) || ( pDev_->interfaceLayout.read() == dilDeviceSpecific ) ) && ( ( pDev_->family.read() == "mvBlueFOX" ) || ( pDev_->family.read() == "mvVirtualDevice" ) ) );
    if( boDeviceSpecific )
    {
        if( pDev_->family.read() == "mvBlueFOX" )
        {
            CameraSettingsBlueFOX bds( pDev_ );
            if( bds.autoExposeControl.isValid() )
            {
                AutoControlParameters acp( bds.getAutoControlParameters() );
                if( bds.autoExposeControl.isValid() && acp.aoiMode.isWriteable() && supportsEnumStringValue( acp.aoiMode, "User" ) )
                {
                    ComposeAreaPanelAEC( aoictAEC, false, aoiNumber );
                    aoiNumber++;
                }
                if( bds.autoGainControl.isValid() && acp.aoiMode.isWriteable() && supportsEnumStringValue( acp.aoiMode, "User" ) )
                {
                    ComposeAreaPanelAGC( aoictAGC, false, aoiNumber );
                    aoiNumber++;
                }
                const string product( pDev_->product.read() );
                if( product[product.length() - 1] == 'C' )
                {
                    ComposeAreaPanelAWB( aoictAWBHost, false, aoiNumber );
                    aoiNumber++;
                }
            }
        }
    }
    else
    {
        GenICam::AcquisitionControl acq( pDev_ );
        GenICam::AnalogControl anc( pDev_ );
        GenICam::DeviceControl dc( pDev_ );
        GenICam::ImageFormatControl ifc( pDev_ );
        if( acq.exposureAuto.isValid() && acq.exposureAuto.isWriteable() && supportsEnumStringValue( acq.exposureAuto, "Continuous" ) )
        {
            ComposeAreaPanelAEC( aoictAEC, false, aoiNumber );
            aoiNumber++;
        }
        if( anc.gainAuto.isValid() && anc.gainAuto.isWriteable() && supportsEnumStringValue( anc.gainAuto, "Continuous" ) )
        {
            ComposeAreaPanelAGC( aoictAGC, false, aoiNumber );
            aoiNumber++;
        }
        if( anc.balanceWhiteAuto.isValid() && anc.balanceWhiteAuto.isWriteable() && supportsEnumStringValue( anc.balanceWhiteAuto, "Continuous" ) )
        {
            ComposeAreaPanelAWBCamera( aoictAWB, false, aoiNumber );
            aoiNumber++;
        }
        if( dc.mvDeviceSensorColorMode.isValid() && ( dc.mvDeviceSensorColorMode.readS() == "BayerMosaic" ) &&
            ifc.pixelFormat.isValid() && ( ifc.pixelFormat.readS().find( "Bayer" ) != string::npos ) )
        {
            const AOIControl* pControl = ComposeAreaPanelAWB( aoictAWBHost, false, aoiNumber );
            aoiNumber++;
            if( anc.balanceWhiteAuto.isValid() && ( anc.balanceWhiteAuto.readS() == "Continuous" ) )
            {
                pControl->pCalibrate_->Enable( false );
                pControl->pCalibrate_->SetLabel( wxT( "Host based AWB is only possible if camera based AWB is switched off!" ) );
            }
        }
    }

    ComposeAreaPanelFFCCalibration( aoictFFCCalibrate, false, aoiNumber );
    aoiNumber++;

    ComposeAreaPanelFFC( aoictFFC, false, aoiNumber );
    aoiNumber++;

    ComposeAreaPanelScaler( aoictScaler, false, aoiNumber );
    aoiNumber++;

    pBtnOk_ = new wxButton( pMainPanel, widBtnOk, wxT( "&Ok" ) );
    pBtnOk_->SetToolTip( wxT( "Closes this dialog, applies all the AOI settings and starts the acquisition" ) );
    pBtnCancel_ = new wxButton( pMainPanel, widBtnCancel, wxT( "&Cancel" ) );
    pBtnCancel_->SetToolTip( wxT( "Just closes this dialog. AOI settings will be discarded" ) );

    // putting it all together
    const int minSpace = ( ( wxPlatformInfo().GetOperatingSystemId() & wxOS_WINDOWS ) != 0 ) ? 2 : 1;

    wxBoxSizer* pSettingsSizer = new wxBoxSizer( wxHORIZONTAL );
    pSettingsSizer->AddSpacer( 2 * minSpace );
    pSettingsSizer->Add( pAOISettingsNotebook_, wxSizerFlags( 5 ).Expand() );
    pSettingsSizer->AddSpacer( 2 * minSpace );

    // customizing the last line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonSizer->Add( pStaticBitmapWarning_, wxSizerFlags().Border( wxALL, 7 ) );
    wxBoxSizer* pSmallInfoTextAlignmentSizer = new wxBoxSizer( wxVERTICAL );
    pSmallInfoTextAlignmentSizer->AddSpacer( 15 );
    wxFont font = pInfoText_->GetFont();
    font.SetWeight( wxFONTWEIGHT_BOLD );
    pInfoText_->SetFont( font );
    pSmallInfoTextAlignmentSizer->Add( pInfoText_ );
    pButtonSizer->Add( pSmallInfoTextAlignmentSizer );
    pButtonSizer->AddStretchSpacer( 10 );
    pButtonSizer->Add( pBtnOk_, wxSizerFlags().Border( wxALL, 7 ) );
    pButtonSizer->Add( pBtnCancel_, wxSizerFlags().Border( wxALL, 7 ) );

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 15 );
    wxStaticText* pAOIDragNote = new wxStaticText( pMainPanel, wxID_ANY, wxT( "You can either drag and resize the AOI on top of the live image or use the slider below!\n" ) );
    pAOIDragNote->SetFont( font );
    pTopDownSizer->Add( pAOIDragNote, wxSizerFlags().Align( wxALIGN_CENTER_HORIZONTAL ) );
    pTopDownSizer->AddSpacer( 15 );
    pTopDownSizer->Add( pSettingsSizer, wxSizerFlags( 10 ).Expand() );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pButtonSizer, wxSizerFlags().Expand() );

    pMainPanel->SetSizer( pTopDownSizer );
    pTopDownSizer->SetSizeHints( this );
    SetClientSize( pTopDownSizer->GetMinSize() );
    SetSizeHints( GetSize() );
    Center();

    // first collect all AOIs configured with the Wizards notebook pages
    const wxRect rect1( previousSensorAOI_.GetLeft(), previousSensorAOI_.GetTop(), previousSensorAOI_.GetWidth(), previousSensorAOI_.GetHeight() );
    pAOIForOverlay_ = new AOI( rect1, wxPen( wxColour( 0, 255, 0 ), 2 ), true, true,
                               wxString::Format( wxT( "%s" ), wxT( "AOI" ) ),
                               static_cast<int>( offsetXIncSensor_ ), static_cast<int>( offsetYIncSensor_ ), static_cast<int>( widthIncSensor_ ), static_cast<int>( heightIncSensor_ ),
                               static_cast<int>( widthMinSensor_ ), static_cast<int>( heightMinSensor_ ) );

    AOIAccessScope aoiAccessScope( *pImageCanvas_ );
    pImageCanvas_->RegisterAOI( pAOIForOverlay_ );

    boGUICreationComplete_ = true;
    pImageCanvas_->SetAOIWizardActive( true );
}

//-----------------------------------------------------------------------------
WizardAOI::~WizardAOI()
//-----------------------------------------------------------------------------
{
    pImageCanvas_->SetAOIWizardActive( false );
    DeleteElement( pAOIForOverlay_ );
    for_each( configuredAreas_.begin(), configuredAreas_.end(), ptr_fun( DeleteSecond<const int64_type, AOIControl*> ) );
}

//-----------------------------------------------------------------------------
void WizardAOI::AddError( const AOIErrorInfo& errorInfo )
//-----------------------------------------------------------------------------
{
    RemoveError( errorInfo );
    currentErrors_.push_back( errorInfo );
}

//-----------------------------------------------------------------------------
void WizardAOI::ApplyAOISettingsAWB( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAWBHost );
    if( pControl )
    {
        const WhiteBalanceSettings& wbs( imageProcessing_.getWBUserSetting( 0 ) );
        conditionalSetProperty( wbs.aoiStartX, wbs.aoiStartX.getMinValue(), true );
        conditionalSetProperty( wbs.aoiStartY, wbs.aoiStartY.getMinValue(), true );
        conditionalSetProperty( wbs.aoiWidth, static_cast<int>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( wbs.aoiHeight, static_cast<int>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( wbs.aoiStartX, static_cast<int>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( wbs.aoiStartY, static_cast<int>( pControl->pSCAOIy_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::ApplyAOISettingsFFC( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictFFC );
    if( pControl )
    {
        conditionalSetProperty( imageProcessing_.flatFieldFilterCorrectionAoiOffsetX, imageProcessing_.flatFieldFilterCorrectionAoiOffsetX.getMinValue(), true );
        conditionalSetProperty( imageProcessing_.flatFieldFilterCorrectionAoiOffsetY, imageProcessing_.flatFieldFilterCorrectionAoiOffsetY.getMinValue(), true );
        conditionalSetProperty( imageProcessing_.flatFieldFilterCorrectionAoiWidth, static_cast<int>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( imageProcessing_.flatFieldFilterCorrectionAoiHeight, static_cast<int>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( imageProcessing_.flatFieldFilterCorrectionAoiOffsetX, static_cast<int>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( imageProcessing_.flatFieldFilterCorrectionAoiOffsetY, static_cast<int>( pControl->pSCAOIy_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::ApplyAOISettingsScaler( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictScaler );
    if( pControl )
    {
        conditionalSetProperty( imageDestination_.scalerAoiStartX, imageDestination_.scalerAoiStartX.getMinValue(), true );
        conditionalSetProperty( imageDestination_.scalerAoiStartY, imageDestination_.scalerAoiStartY.getMinValue(), true );
        conditionalSetProperty( imageDestination_.scalerAoiWidth, static_cast<int>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( imageDestination_.scalerAoiHeight, static_cast<int>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( imageDestination_.scalerAoiStartX, static_cast<int>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( imageDestination_.scalerAoiStartY, static_cast<int>( pControl->pSCAOIy_->GetValue() ), true );
        conditionalSetProperty( imageDestination_.imageWidth, static_cast<int>( pControl->pSCAOIwDest_->GetValue() ), true );
        conditionalSetProperty( imageDestination_.imageHeight, static_cast<int>( pControl->pSCAOIhDest_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::ChangePageControlState( AOIControl* pAOIControl, bool boEnable )
//-----------------------------------------------------------------------------
{
    if( !boGUICreationComplete_ )
    {
        return;
    }
    pAOIControl->pCBAOIEnable_->SetValue( boEnable );
    pAOIControl->pSCAOIx_->Enable( boEnable );
    pAOIControl->pSCAOIy_->Enable( boEnable );
    pAOIControl->pSCAOIw_->Enable( boEnable );
    pAOIControl->pSCAOIh_->Enable( boEnable );

    switch( pAOIControl->type_ )
    {
    case aoictAEC:
        SetupAutoExposure( boEnable );
        break;
    case aoictAGC:
        SetupAutoGain( boEnable );
        break;
    case aoictAWB:
        SetupAutoWhiteBalanceCamera( boEnable );
        EnableOrDisableHostAWB( !boEnable );
        break;
    case aoictFFC:
        SetupFlatFieldCorrection( boEnable );
        break;
    case aoictScaler:
        SetupScaler( boEnable );
        pAOIControl->pSCAOIwDest_->Enable( boEnable );
        pAOIControl->pSCAOIhDest_->Enable( boEnable );
        UpdateAOIFromControl();
        break;
    default:
        break;
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::CloseDlg( int result )
//-----------------------------------------------------------------------------
{
    if( result != wxID_OK )
    {
        pParent_->EnsureAcquisitionState( false );
    }
    pParent_->RemoveCaptureSettingFromStack( pDev_, pFI_, result != wxID_OK );
    pImageCanvas_->UnregisterAllAOIs();
    pImageCanvas_->DisableDoubleClickAndPrunePopupMenu( false );
    DeleteElement( pAOIForOverlay_ );
    pParent_->RestoreGUIStateAfterWizard( true, true );
    pParent_->EnsureAcquisitionState( ( result == wxID_OK ) ? true : boAcquisitionStateOnCancel_ );
    pParent_->OnBeforeAOIDialogDestruction();
    Destroy();
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelAEC( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "AEC AOI" ), wxT( "Configures the AOI used by the Automatic Exposure Controller (AEC). The controller will adjust the exposure time until the configured AOI will reach the defined average grey value." ) );
    const bool boDeviceSpecific = ( ( !pDev_->interfaceLayout.isValid() || pDev_->interfaceLayout.read() == dilDeviceSpecific ) && pDev_->family.read() == "mvBlueFOX" );
    if( boDeviceSpecific )
    {
        CameraSettingsBlueFOX bds( pDev_ );
        if( bds.autoExposeControl.isValid() )
        {
            const bool boEnable = bds.autoExposeControl.readS() == "On" ;
            AutoControlParameters acp( bds.getAutoControlParameters() );
            bds.getAutoControlParameters().aoiMode.writeS( "User" );
            CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable AEC: " ), boEnable, wxT( "Enables or disables the Automatic Exposure Controller." ) );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, acp.aoiStartX.getMaxValue(), acp.aoiStartX.read(), acp.aoiStartX.getStepWidth(), boEnable );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, acp.aoiStartY.getMaxValue(), acp.aoiStartY.read(), acp.aoiStartY.getStepWidth(), boEnable );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), bds.getAutoControlParameters().aoiWidth.getStepWidth(), acp.aoiWidth.getMaxValue(), acp.aoiWidth.read(), bds.getAutoControlParameters().aoiWidth.getStepWidth(), boEnable );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), acp.aoiHeight.getStepWidth(), acp.aoiHeight.getMaxValue(), acp.aoiHeight.read(), acp.aoiHeight.getStepWidth(), boEnable );
        }
    }
    else
    {
        GenICam::AcquisitionControl acq( pDev_ );
        GenICam::ImageFormatControl ifc( pDev_ );
        if( acq.exposureAuto.isValid() )
        {
            const string exposureMode = acq.exposureAuto.readS();
            const string exposureAOIMode = acq.mvExposureAutoAOIMode.readS();
            string exposureTimeSelector = "";

            if( acq.exposureTimeSelector.isValid() )
            {
                exposureTimeSelector = acq.exposureTimeSelector.readS();
            }

            if( ( acq.mvExposureAutoAOIMode.isValid() && acq.mvExposureAutoAOIMode.isWriteable() ) )
            {
                acq.exposureAuto.writeS( "Continuous" );
                acq.mvExposureAutoAOIMode.writeS( "mvUser" );
            }
            const int64_type offsetX = acq.mvExposureAutoOffsetX.read();
            int64_type offsetXMax = acq.mvExposureAutoOffsetX.getMaxValue();
            const int64_type offsetXInc = acq.mvExposureAutoOffsetX.getStepWidth();
            const int64_type offsetY = acq.mvExposureAutoOffsetY.read();
            int64_type offsetYMax = acq.mvExposureAutoOffsetY.getMaxValue();
            const int64_type offsetYInc = acq.mvExposureAutoOffsetY.getStepWidth();
            int64_type width = acq.mvExposureAutoWidth.read();
            int64_type widthMax = acq.mvExposureAutoWidth.getMaxValue();
            int64_type widthInc = acq.mvExposureAutoWidth.getStepWidth();
            int64_type height = acq.mvExposureAutoHeight.read();
            int64_type heightMax = acq.mvExposureAutoHeight.getMaxValue();
            int64_type heightInc = acq.mvExposureAutoHeight.getStepWidth();

            // workaround for a bug for cameras supporting multi exposure
            if( ( ( height == 0 ) || ( width == 0 ) ) || ( heightMax > ifc.heightMax.read() || ( widthMax > ifc.widthMax.read() ) ) )
            {
                widthInc = ifc.width.getStepWidth();
                heightInc = ifc.height.getStepWidth();

                offsetXMax = ifc.widthMax.read() - widthInc;
                offsetYMax = ifc.widthMax.read() - heightInc;
                width = ifc.width.read();
                widthMax = ifc.widthMax.read();
                height = ifc.height.read();
                heightMax = ifc.heightMax.read();
            }

            if( ( acq.mvExposureAutoAOIMode.isValid() && acq.mvExposureAutoAOIMode.isWriteable() ) && ( ( acq.exposureAuto.readS() != exposureMode ) || ( acq.mvExposureAutoAOIMode.readS() != exposureAOIMode ) ) )
            {
                if( acq.exposureTimeSelector.isValid() )
                {
                    acq.exposureTimeSelector.writeS( exposureTimeSelector );
                }
                acq.exposureAuto.writeS( exposureMode );
                acq.mvExposureAutoAOIMode.writeS( exposureAOIMode );
            }
            const bool isContinuous = exposureMode == "Continuous";
            CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable AEC: " ), isContinuous, wxT( "Enables or disables the Automatic Exposure Controller." ) );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, offsetXMax, offsetX, offsetXInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, offsetYMax, offsetY, offsetYInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), widthInc, widthMax, width, widthInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), heightInc, heightMax, height, heightInc, isContinuous );
        }
    }
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelAGC( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "AGC AOI" ), wxT( "Configures the AOI used by the Automatic Gain Controller (AGC). The controller will adjust the signal amplification until the configured AOI will reach the defined average grey value." ) );
    const bool boDeviceSpecific = ( ( !pDev_->interfaceLayout.isValid() || pDev_->interfaceLayout.read() == dilDeviceSpecific ) && pDev_->family.read() == "mvBlueFOX" );
    if( boDeviceSpecific )
    {
        CameraSettingsBlueFOX bds( pDev_ );
        if( bds.autoGainControl.isValid() )
        {
            AutoControlParameters acp( bds.getAutoControlParameters() );
            bds.getAutoControlParameters().aoiMode.writeS( "User" );

            const bool boEnable = bds.autoGainControl.readS() == "On";
            CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable AGC: " ), boEnable, wxT( "Enables or disables the Automatic Gain Controller." ) );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, acp.aoiStartX.getMaxValue(), acp.aoiStartX.read(), acp.aoiStartX.getStepWidth(), boEnable );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, acp.aoiStartY.getMaxValue(), acp.aoiStartY.read(), acp.aoiStartY.getStepWidth(), boEnable );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), bds.getAutoControlParameters().aoiWidth.getStepWidth(), acp.aoiWidth.getMaxValue(), acp.aoiWidth.read(), bds.getAutoControlParameters().aoiWidth.getStepWidth(), boEnable );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), acp.aoiHeight.getStepWidth(), acp.aoiHeight.getMaxValue(), acp.aoiHeight.read(), acp.aoiHeight.getStepWidth(), boEnable );
            ChangePageControlState( pAOIControl, bds.autoExposeControl.readS() == "On" );
        }
    }
    else
    {
        GenICam::AnalogControl anc( pDev_ );
        GenICam::ImageFormatControl ifc( pDev_ );
        if( anc.gainAuto.isValid() )
        {
            const string gainMode = anc.gainAuto.readS();
            const string gainAOIMode = anc.mvGainAutoAOIMode.readS();
            if( anc.mvGainAutoAOIMode.isValid() && anc.mvGainAutoAOIMode.isWriteable() )
            {
                anc.gainAuto.writeS( "Continuous" );
                anc.mvGainAutoAOIMode.writeS( "mvUser" );
            }
            const int64_type offsetX = anc.mvGainAutoOffsetX.read();
            int64_type offsetXMax = anc.mvGainAutoOffsetX.getMaxValue();
            const int64_type offsetXInc = anc.mvGainAutoOffsetX.getStepWidth();
            const int64_type offsetY = anc.mvGainAutoOffsetY.read();
            int64_type offsetYMax = anc.mvGainAutoOffsetY.getMaxValue();
            const int64_type offsetYInc = anc.mvGainAutoOffsetY.getStepWidth();
            int64_type width = anc.mvGainAutoWidth.read();
            int64_type widthMax = anc.mvGainAutoWidth.getMaxValue();
            int64_type widthInc = anc.mvGainAutoWidth.getStepWidth();
            int64_type height = anc.mvGainAutoHeight.read();
            int64_type heightMax = anc.mvGainAutoHeight.getMaxValue();
            int64_type heightInc = anc.mvGainAutoHeight.getStepWidth();

            // workaround for a bug for cameras supporting multi exposure
            if( ( ( height == 0 ) || ( width == 0 ) ) || ( heightMax > ifc.heightMax.read() || ( widthMax > ifc.widthMax.read() ) ) )
            {
                widthInc = ifc.width.getStepWidth();
                heightInc = ifc.height.getStepWidth();

                offsetXMax = ifc.widthMax.read() - widthInc;
                offsetYMax = ifc.widthMax.read() - heightInc;
                width = ifc.width.read();
                widthMax = ifc.widthMax.read();
                height = ifc.height.read();
                heightMax = ifc.heightMax.read();
            }

            if( anc.mvGainAutoAOIMode.isValid() && anc.mvGainAutoAOIMode.isWriteable() && ( anc.gainAuto.readS() != gainMode || anc.mvGainAutoAOIMode.readS() != gainAOIMode ) )
            {
                anc.gainAuto.writeS( gainMode );
                anc.mvGainAutoAOIMode.writeS( gainAOIMode );
            }
            const bool isContinuous = gainMode == "Continuous";
            CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable AGC: " ), isContinuous, wxT( "Enables or disables the Automatic Gain Controller." ) );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, offsetXMax, offsetX, offsetXInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, offsetYMax, offsetY, offsetYInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), widthInc, widthMax, width, widthInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), heightInc, heightMax, height, heightInc, isContinuous );
        }
    }
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelAOI( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "AOI" ),  wxT( "Configures the sensor AOI." ) );
    const bool boDeviceSpecific = ( !pDev_->interfaceLayout.isValid() || pDev_->interfaceLayout.read() == dilDeviceSpecific );
    if( boDeviceSpecific )
    {
        BasicDeviceSettingsWithAOI bds( pDev_ );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, bds.aoiStartX.getMaxValue(), bds.aoiStartX.read(), bds.aoiStartX.getStepWidth(), true );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, bds.aoiStartY.getMaxValue(), bds.aoiStartY.read(), bds.aoiStartY.getStepWidth(), true );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), bds.aoiWidth.getStepWidth(), bds.aoiWidth.getMaxValue(), bds.aoiWidth.read(), bds.aoiWidth.getStepWidth(), true );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), bds.aoiHeight.getStepWidth(), bds.aoiHeight.getMaxValue(), bds.aoiHeight.read(), bds.aoiHeight.getStepWidth(), true );
    }
    else
    {
        GenICam::ImageFormatControl ifc( pDev_ );
        const int64_type incX = ifc.offsetX.getStepWidth();
        const int64_type incY = ifc.offsetY.getStepWidth();
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, ( ifc.sensorWidth.read() - incX ), ifc.offsetX.read(), incX, true );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, ( ifc.sensorHeight.read() - incY ), ifc.offsetY.read(), incY, true );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), incX, ifc.width.getMaxValue(), ifc.width.read(), ifc.width.getStepWidth(), true );
        CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), incY, ifc.height.getMaxValue(), ifc.height.read(), ifc.height.getStepWidth(), true );
    }
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelAWBCamera( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "AWB AOI" ),  wxT( "Configures the AOI used for the Automatic White Balance (AWB) on camera side. Areas outside of this AOI are not used to calculate the correction factors for the white balance calibration." ) );
    const bool boDeviceSpecific = ( !pDev_->interfaceLayout.isValid() || pDev_->interfaceLayout.read() == dilGenICam );
    if( boDeviceSpecific )
    {
        GenICam::AnalogControl anc( pDev_ );
        GenICam::ImageFormatControl ifc( pDev_ );

        if( anc.balanceWhiteAuto.isValid() && anc.balanceWhiteAuto.isWriteable() && ( supportsEnumStringValue( anc.balanceWhiteAuto, "Continuous" ) || supportsEnumStringValue( anc.balanceWhiteAuto, "Once" ) ) )
        {
            const string balanceWhiteAutoMode = anc.balanceWhiteAuto.readS();
            const string balanceWhiteAutoAOIMode = anc.mvBalanceWhiteAutoAOIMode.readS();
            if( ( anc.mvBalanceWhiteAutoAOIMode.isValid() && anc.mvBalanceWhiteAutoAOIMode.isWriteable() ) )
            {
                anc.balanceWhiteAuto.writeS( "Continuous" );
                anc.mvBalanceWhiteAutoAOIMode.writeS( "mvUser" );
            }
            const int64_type offsetX = anc.mvBalanceWhiteAutoOffsetX.read();
            int64_type offsetXMax = anc.mvBalanceWhiteAutoOffsetX.getMaxValue();
            const int64_type offsetXInc = anc.mvBalanceWhiteAutoOffsetX.getStepWidth();
            const int64_type offsetY = anc.mvBalanceWhiteAutoOffsetY.read();
            int64_type offsetYMax = anc.mvBalanceWhiteAutoOffsetY.getMaxValue();
            const int64_type offsetYInc = anc.mvBalanceWhiteAutoOffsetY.getStepWidth();
            int64_type width = anc.mvBalanceWhiteAutoWidth.read();
            int64_type widthMax = anc.mvBalanceWhiteAutoWidth.getMaxValue();
            int64_type widthInc = anc.mvBalanceWhiteAutoWidth.getStepWidth();
            int64_type height = anc.mvBalanceWhiteAutoHeight.read();
            int64_type heightMax = anc.mvBalanceWhiteAutoHeight.getMaxValue();
            int64_type heightInc = anc.mvBalanceWhiteAutoHeight.getStepWidth();

            // workaround for a bug for cameras supporting multi exposure
            if( ( ( height == 0 ) || ( width == 0 ) ) || ( heightMax > ifc.heightMax.read() || ( widthMax > ifc.widthMax.read() ) ) )
            {
                widthInc = ifc.width.getStepWidth();
                heightInc = ifc.height.getStepWidth();

                offsetXMax = ifc.widthMax.read() - widthInc;
                offsetYMax = ifc.widthMax.read() - heightInc;
                width = ifc.width.read();
                widthMax = ifc.widthMax.read();
                height = ifc.height.read();
                heightMax = ifc.heightMax.read();
            }

            if( ( anc.mvBalanceWhiteAutoAOIMode.isValid() && anc.mvBalanceWhiteAutoAOIMode.isWriteable() ) && ( anc.balanceWhiteAuto.readS() != balanceWhiteAutoMode || anc.mvBalanceWhiteAutoAOIMode.readS() != balanceWhiteAutoAOIMode ) )
            {
                anc.balanceWhiteAuto.writeS( balanceWhiteAutoMode );
                anc.mvBalanceWhiteAutoAOIMode.writeS( balanceWhiteAutoAOIMode );
            }
            const bool isContinuous = balanceWhiteAutoMode == "Continuous";
            CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable AWB: " ), isContinuous, wxT( "Enables or disables the device based Automatic White Balance." ) );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, offsetXMax, offsetX, offsetXInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, offsetYMax, offsetY, offsetYInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), widthInc, widthMax, width, widthInc, isContinuous );
            CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), heightInc, heightMax, height, heightInc, isContinuous );
        }
    }
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
WizardAOI::AOIControl* WizardAOI::ComposeAreaPanelAWB( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "AWB AOI Host" ), wxT( "Configures the AOI used for the Automatic White Balance (AWB) on host side. Areas outside of this AOI are not used to calculate the correction factors for the white balance calibration." ) );
    conditionalSetEnumPropertyByString( imageProcessing_.whiteBalance, "User1", true );
    SetupAutoWhiteBalance();

    const WhiteBalanceSettings& wbs( imageProcessing_.getWBUserSetting( 0 ) );
    const int incX = wbs.aoiStartX.hasStepWidth() ? wbs.aoiStartX.getMaxValue() : 1;
    const int incY = wbs.aoiStartY.hasStepWidth() ? wbs.aoiStartY.getMaxValue() : 1;
    const int incW = wbs.aoiWidth.hasStepWidth() ? wbs.aoiWidth.getMaxValue() : 1;
    const int incH = wbs.aoiHeight.hasStepWidth() ? wbs.aoiHeight.getMaxValue() : 1;

    int maxOffsetX = 0;
    int maxOffsetY = 0;
    int maxWidth = 640;
    int maxHeight = 480;
    GetDeviceAOILimits( incX, incY, maxOffsetX, maxOffsetY, maxWidth, maxHeight );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, maxOffsetX, wbs.aoiStartX.read(), incX, true );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, maxOffsetY, wbs.aoiStartY.read(), incY, true );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), incW, maxWidth, wbs.aoiWidth.read(), incW, true );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), incH, maxHeight, wbs.aoiHeight.read(), incH, true );
    CreateCalibrateButton( pAOIControl, wxT( "Calibrate" ) );
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
    return pAOIControl;
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelFFC( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "FFC AOI" ), wxT( "Configures the AOI used for the Flat Field Correction (FFC). Regions outside this AOI will not be changed." ) );
    const int incX = ( imageProcessing_.flatFieldFilterCorrectionAoiOffsetX.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCorrectionAoiOffsetX.getMaxValue() : 1 );
    const int incY = ( imageProcessing_.flatFieldFilterCorrectionAoiOffsetY.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCorrectionAoiOffsetY.getMaxValue() : 1 );
    const int incW = ( imageProcessing_.flatFieldFilterCorrectionAoiWidth.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCorrectionAoiWidth.getMaxValue() : 1 );
    const int incH = ( imageProcessing_.flatFieldFilterCorrectionAoiHeight.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCorrectionAoiHeight.getMaxValue() : 1 );

    int maxOffsetX = 0;
    int maxOffsetY = 0;
    int maxWidth = 640;
    int maxHeight = 480;
    GetDeviceAOILimits( incX, incY, maxOffsetX, maxOffsetY, maxWidth, maxHeight );
    const bool isFlatFieldFilterModeActive = imageProcessing_.flatFieldFilterMode.readS() == "On";
    CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable Flat Field Filter AOI: " ), isFlatFieldFilterModeActive, wxT( "Enables or disables the Flat Field Filter." ) );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, maxOffsetX, imageProcessing_.flatFieldFilterCorrectionAoiOffsetX.read(), incX, isFlatFieldFilterModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, maxOffsetY, imageProcessing_.flatFieldFilterCorrectionAoiOffsetY.read(), incY, isFlatFieldFilterModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), incW, maxWidth, imageProcessing_.flatFieldFilterCorrectionAoiWidth.read(), incW, isFlatFieldFilterModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), incH, maxHeight, imageProcessing_.flatFieldFilterCorrectionAoiHeight.read(), incH, isFlatFieldFilterModeActive );

    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelFFCCalibration( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "Flat Field Filter Calibration AOI" ), wxT( "Configures the AOI used for the calibration of the Flat Field Filter (FFC). Regions outside this AOI are not taken into account." ) );
    const int incX = ( imageProcessing_.flatFieldFilterCalibrationAoiOffsetX.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCalibrationAoiOffsetX.getMaxValue() : 1 );
    const int incY = ( imageProcessing_.flatFieldFilterCalibrationAoiOffsetY.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCalibrationAoiOffsetY.getMaxValue() : 1 );
    const int incW = ( imageProcessing_.flatFieldFilterCalibrationAoiWidth.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCalibrationAoiWidth.getMaxValue() : 1 );
    const int incH = ( imageProcessing_.flatFieldFilterCalibrationAoiHeight.hasStepWidth() == true ? imageProcessing_.flatFieldFilterCalibrationAoiHeight.getMaxValue() : 1 );

    int maxOffsetX = 0;
    int maxOffsetY = 0;
    int maxWidth = 640;
    int maxHeight = 480;
    GetDeviceAOILimits( incX, incY, maxOffsetX, maxOffsetY, maxWidth, maxHeight );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, maxOffsetX, imageProcessing_.flatFieldFilterCalibrationAoiOffsetX.read(), incX, true );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, maxOffsetY, imageProcessing_.flatFieldFilterCalibrationAoiOffsetY.read(), incY, true );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), incW, maxWidth, imageProcessing_.flatFieldFilterCalibrationAoiWidth.read(), incW, true );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), incH, maxHeight, imageProcessing_.flatFieldFilterCalibrationAoiHeight.read(), incH, true );
    CreateCalibrateButton( pAOIControl, wxT( "Calibrate" ) );
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
void WizardAOI::ComposeAreaPanelScaler( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiNumber )
//-----------------------------------------------------------------------------
{
    AOIControl* pAOIControl = new AOIControl();
    SpinControlWindowIDs spinIDControls = SetupAreaPanel( pAOIControl, aoiType, aoiNumber, wxT( "Scaler AOI" ), wxT( "Configures the resolution of the resulting image after up- or downscaling." ) );

    const wxWindowID wDest_id = widLAST + ( static_cast<wxWindowID>( aoiNumber * 0x100 ) | widSCAOIwDest );
    const wxWindowID hDest_id = widLAST + ( static_cast<wxWindowID>( aoiNumber * 0x100 ) | widSCAOIhDest );

    const int incX = ( imageDestination_.scalerAoiStartX.hasStepWidth() == true ? imageDestination_.scalerAoiStartX.getMaxValue() : 1 );
    const int incY = ( imageDestination_.scalerAoiStartY.hasStepWidth() == true ? imageDestination_.scalerAoiStartY.getMaxValue() : 1 );
    const int incW = ( imageDestination_.scalerAoiWidth.hasStepWidth() == true ? imageDestination_.scalerAoiWidth.getMaxValue() : 1 );
    const int incH = ( imageDestination_.scalerAoiHeight.hasStepWidth() == true ? imageDestination_.scalerAoiHeight.getMaxValue() : 1 );

    int maxOffsetX = 0;
    int maxOffsetY = 0;
    int maxWidth = 640;
    int maxHeight = 480;
    GetDeviceAOILimits( incX, incY, maxOffsetX, maxOffsetY, maxWidth, maxHeight );
    const bool isScalerModeActive = imageDestination_.scalerMode.readS() == "On";
    CreateCheckBoxEnableEntry( pAOIControl, wxT( "Enable Scaler: " ), isScalerModeActive, wxT( "Enables or disables the scaler." ) );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIx_, spinIDControls.x_id, wxT( "X-Offset: " ), 0, maxOffsetX, imageDestination_.scalerAoiStartX.read(), incX, isScalerModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIy_, spinIDControls.y_id, wxT( "Y-Offset: " ), 0, maxOffsetY, imageDestination_.scalerAoiStartY.read(), incY, isScalerModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIw_, spinIDControls.w_id, wxT( "Width: " ), incW, maxWidth, imageDestination_.scalerAoiWidth.read(), incW, isScalerModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIh_, spinIDControls.h_id, wxT( "Height: " ), incH, maxHeight, imageDestination_.scalerAoiHeight.read(), incH, isScalerModeActive );
    // Destination size
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIwDest_, wDest_id, wxT( "Destination Width: " ), incW, maxWidth, imageDestination_.imageWidth.read(), incW, isScalerModeActive );
    CreateSpinControlEntry( pAOIControl, pAOIControl->pSCAOIhDest_, hDest_id, wxT( "Destination Height: " ), incH, maxHeight, imageDestination_.imageHeight.read(), incH, isScalerModeActive );
    pAOIControl->pSetPanel_->SetSizer( pAOIControl->pGridSizer_ );
    BindSpinCtrlEventHandler( spinIDControls.x_id, spinIDControls.y_id, spinIDControls.w_id, spinIDControls.h_id );
    Bind( wxEVT_TEXT, &WizardAOI::OnAOITextChanged, this, wDest_id );
    Bind( wxEVT_TEXT, &WizardAOI::OnAOITextChanged, this, hDest_id );
    AddPageToAOISettingNotebook( aoiNumber, pAOIControl, boInsertOnFirstFreePosition );
}

//-----------------------------------------------------------------------------
wxSpinCtrlDbl* WizardAOI::CreateAOISpinControl( wxWindow* pParent, wxWindowID id, int64_type min, int64_type max, int64_type value, int64_type inc, bool boEnabled )
//-----------------------------------------------------------------------------
{
    wxSpinCtrlDbl* pSpinCtrl = new wxSpinCtrlDbl();
    pSpinCtrl->SetMode( mInt64 );
    pSpinCtrl->Create( pParent, id, wxEmptyString, wxDefaultPosition, wxSize( 80, -1 ), wxSP_ARROW_KEYS, double( min ), double( max ), double( value ), double( inc ), wxSPINCTRLDBL_AUTODIGITS, wxT( MY_FMT_I64 ), wxT( "wxSpinCtrlDbl" ), true );
    pSpinCtrl->Enable( boEnabled );
    return pSpinCtrl;
}

//-----------------------------------------------------------------------------
void WizardAOI::EnableOrDisableHostAWB( bool boEnable )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAWBHost );
    if( pControl )
    {
        pControl->pCalibrate_->Enable( boEnable );
        if( boEnable )
        {
            pControl->pCalibrate_->SetLabel( wxT( "Calibrate" ) );
            pControl->pCalibrate_->SetToolTip( wxT( "Calibrate optimal correction values for the color channels." ) );
        }
        else
        {
            pControl->pCalibrate_->SetLabel( wxT( "Host based AWB is only possible if no camera based AWB is active!" ) );
            pControl->pCalibrate_->SetToolTip( wxT( "The correction values on driver side will be set to 1 to avoid wrong colors because of multiple correction attempts." ) );
            const WhiteBalanceSettings& wbs( imageProcessing_.getWBUserSetting( 0 ) );
            conditionalSetProperty( wbs.redGain, 1.00, true );
            conditionalSetProperty( wbs.greenGain, 1.00, true );
            conditionalSetProperty( wbs.blueGain, 1.00, true );
        }
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::GetDeviceAOILimits( const int incX, const int incY, int& maxOffsetX, int& maxOffsetY, int& maxWidth, int& maxHeight ) const
//-----------------------------------------------------------------------------
{
    const bool boDeviceSpecific = ( !pDev_->interfaceLayout.isValid() || pDev_->interfaceLayout.read() == dilDeviceSpecific );
    if( !boDeviceSpecific )
    {
        GenICam::ImageFormatControl ifc( pDev_ );
        maxOffsetX = static_cast<int>( ifc.width.getMaxValue() ) - incX;
        maxOffsetY = static_cast<int>( ifc.height.getMaxValue() ) - incY;
        maxWidth = static_cast<int>( ifc.width.getMaxValue() );
        maxHeight = static_cast<int>( ifc.height.getMaxValue() );
    }
    else
    {
        BasicDeviceSettingsWithAOI bds( pDev_ );
        maxOffsetX = bds.aoiWidth.getMaxValue() - incX;
        maxOffsetY = bds.aoiHeight.getMaxValue() - incY;
        maxWidth = bds.aoiWidth.getMaxValue();
        maxHeight = bds.aoiHeight.getMaxValue();
    }
}

//-----------------------------------------------------------------------------
bool WizardAOI::HasCurrentConfigurationCriticalError( void ) const
//-----------------------------------------------------------------------------
{
    const vector<AOIErrorInfo>::size_type errorCnt = currentErrors_.size();
    bool boError = false;
    for( vector<AOIErrorInfo>::size_type i = 0; i < errorCnt; i++ )
    {
        boError = ( currentErrors_[i].errorType == mrweAOIOutOfBounds ) ? true : false;
    }
    return boError;
}

//-----------------------------------------------------------------------------
void WizardAOI::OnBtnOk( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    try
    {
        pParent_->EnsureAcquisitionState( false );
        // restore the previous AOI settings as otherwise these would be lost when later switching off the
        // AOI again as this wizard will override the current AOI with the maximum AOI in order to present
        // a full image to the user allowing him to move around the AOIs.
        ApplyAOISettings();
    }
    catch( const ImpactAcquireException& e )
    {
        const wxString msg = wxString::Format( wxT( "Failed to set up AOI for device %s(%s)!\n\nAPI error reported: %s(%s)" ),
                                               ConvertedString( pDev_->serial.read() ).c_str(),
                                               ConvertedString( pDev_->product.read() ).c_str(),
                                               ConvertedString( e.getErrorString() ).c_str(),
                                               ConvertedString( e.getErrorCodeAsString() ).c_str() );
        wxMessageBox( msg, wxT( "AOI Setup Failed" ), wxOK | wxICON_EXCLAMATION, this );
    }
    CloseDlg( wxID_OK );
}

//-----------------------------------------------------------------------------
void WizardAOI::OnClose( wxCloseEvent& e )
//-----------------------------------------------------------------------------
{
    CloseDlg( wxID_CLOSE );
    if( e.CanVeto() )
    {
        e.Veto();
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::RemoveError( const AOIErrorInfo& errorInfo )
//-----------------------------------------------------------------------------
{
    const vector<AOIErrorInfo>::iterator it = find( currentErrors_.begin(), currentErrors_.end(), errorInfo );
    if( it != currentErrors_.end() )
    {
        currentErrors_.erase( it );
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::ShowImageTimeoutPopup( void )
//-----------------------------------------------------------------------------
{
    wxMessageBox( wxT( "Image request timeout\n\nThe last Image Request returned with an Image Timeout. This means that the camera cannot stream data and indicates a problem with the current configuration.\n\nPlease close the AOI setup, fix the configuration and then continue with the setup of AOIs." ), wxT( "Image Timeout!" ), wxOK | wxICON_INFORMATION, this );
}

//-----------------------------------------------------------------------------
void WizardAOI::UpdateControlsFromAOI( const AOI* p )
//-----------------------------------------------------------------------------
{
    if( boGUICreationComplete_ )
    {
        const int currentSelection = pAOISettingsNotebook_->GetSelection();
        configuredAreas_[currentSelection]->pSCAOIx_->SetValue( ( p->m_rect.GetX() / p->m_incX ) * p->m_incX );
        configuredAreas_[currentSelection]->pSCAOIy_->SetValue( ( p->m_rect.GetY() / p->m_incY ) * p->m_incY );
        configuredAreas_[currentSelection]->pSCAOIw_->SetValue( ( p->m_rect.GetWidth() / p->m_incW ) * p->m_incW );
        configuredAreas_[currentSelection]->pSCAOIh_->SetValue( ( p->m_rect.GetHeight() / p->m_incH ) * p->m_incH );
    }
}

//-----------------------------------------------------------------------------
void WizardAOI::UpdateAOIFromControl( void )
//-----------------------------------------------------------------------------
{
    if( !boGUICreationComplete_ )
    {
        return;
    }
    boHandleChangedMessages_ = false;
    int currentSelection = pAOISettingsNotebook_->GetSelection();
    const wxRect rect1( static_cast<int>( configuredAreas_[currentSelection]->pSCAOIx_->GetValue() ),
                        static_cast<int>( configuredAreas_[currentSelection]->pSCAOIy_->GetValue() ),
                        static_cast<int>( configuredAreas_[currentSelection]->pSCAOIw_->GetValue() ),
                        static_cast<int>( configuredAreas_[currentSelection]->pSCAOIh_->GetValue() ) );

    AOIAccessScope aoiAccessScope( *pImageCanvas_ );
    if( imageDestination_.scalerMode.readS() != "On" )
    {
        pAOIForOverlay_->m_rect = rect1;
    }
    else
    {
        pAOIForOverlay_->m_rect = rectSensor_;
    }
    if( rectSensor_.Contains( rect1 ) )
    {
        switch( configuredAreas_[currentSelection]->type_ )
        {
        case aoictAEC:
            ApplyAOISettingsAEC( configuredAreas_[currentSelection]->type_ );
            break;
        case aoictAGC:
            ApplyAOISettingsAGC();
            break;
        case aoictAWB:
            ApplyAOISettingsAWBCamera();
            break;
        case aoictAWBHost:
            ApplyAOISettingsAWB();
            break;
        case aoictFFC:
            ApplyAOISettingsFFC();
            break;
        case aoictScaler:
            ApplyAOISettingsScaler();
            break;
        default:
            break;
        }
        RemoveError( AOIErrorInfo( currentSelection, mrweAOIOutOfBounds ) );
        pAOIForOverlay_->m_pen = wxPen( wxColour( wxT( "Green" ) ), 2 );
    }
    else
    {
        AddError( AOIErrorInfo( currentSelection, mrweAOIOutOfBounds ) );
        pAOIForOverlay_->m_pen = wxPen( wxColour( wxT( "Red" ) ), 2 );
    }
    UpdateErrors();
    boHandleChangedMessages_ = true;
}

//-----------------------------------------------------------------------------
void WizardAOI::UpdateError( const wxString& msg, const wxBitmap& icon ) const
//-----------------------------------------------------------------------------
{
    pStaticBitmapWarning_->SetBitmap( icon );
    pInfoText_->SetForegroundColour( HasCurrentConfigurationCriticalError() ? wxColour( 255, 0, 0 ) : wxColour( 0, 0, 0 ) );
    pInfoText_->SetLabel( msg );
    pInfoText_->SetToolTip( msg );
}

//-----------------------------------------------------------------------------
void WizardAOI::UpdateErrors( void ) const
//-----------------------------------------------------------------------------
{
    if( currentErrors_.empty() )
    {
        UpdateError( wxString::Format( wxT( "" ) ), *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Empty ) );
    }
    else
    {
        const AOIErrorInfo& lastError = currentErrors_.back();
        switch( lastError.errorType )
        {
        case mrweAOIOutOfBounds:
            UpdateError( wxString::Format( wxT( "The AOI does not fit into the sensor area!" ), static_cast<int>( lastError.AOINumber ) ), *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Warning ) );
            break;
        }
    }
}

//=============================================================================
//===================== Implementation WizardAOIGenICam =======================
//=============================================================================
//-----------------------------------------------------------------------------
WizardAOIGenICam::WizardAOIGenICam( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, ImageCanvas* pImageCanvas, bool boAcquisitionStateOnCancel, FunctionInterface* pFI ) : WizardAOI( pParent, title, pDev, pImageCanvas, boAcquisitionStateOnCancel, pFI ), acq_( pDev ), ifc_( pDev ), anc_( pDev )
//-----------------------------------------------------------------------------
{
    ReadCurrentAOISettings();
    ReadMaxSensorResolution();
    UpdateAOIFromControl();
    SwitchToFullAOI();
    pParent->EnsureAcquisitionState( true );
}

//-----------------------------------------------------------------------------
void WizardAOIGenICam::ApplyAOISettings( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAOI );
    if( pControl )
    {
        conditionalSetProperty( ifc_.offsetX, ifc_.offsetX.getMinValue(), true );
        conditionalSetProperty( ifc_.offsetY, ifc_.offsetY.getMinValue(), true );
        conditionalSetProperty( ifc_.width, static_cast<int64_type>( previousSensorAOI_.GetWidth() ), true );
        conditionalSetProperty( ifc_.height, static_cast<int64_type>( previousSensorAOI_.GetHeight() ), true );
        conditionalSetProperty( ifc_.offsetX, static_cast<int64_type>( previousSensorAOI_.GetLeft() ), true );
        conditionalSetProperty( ifc_.offsetY, static_cast<int64_type>( previousSensorAOI_.GetTop() ), true );

        // first switch on the areas which are configured and apply their values...
        conditionalSetProperty( ifc_.offsetX, ifc_.offsetX.getMinValue(), true );
        conditionalSetProperty( ifc_.offsetY, ifc_.offsetY.getMinValue(), true );
        conditionalSetProperty( ifc_.width, static_cast<int64_type>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( ifc_.height, static_cast<int64_type>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( ifc_.offsetX, static_cast<int64_type>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( ifc_.offsetY, static_cast<int64_type>( pControl->pSCAOIy_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOIGenICam::ApplyAOISettingsAEC(  TAOIControlType /*aoiType*/ )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAEC );
    if( pControl )
    {
        conditionalSetProperty( acq_.mvExposureAutoOffsetX, acq_.mvExposureAutoOffsetX.getMinValue(), true );
        conditionalSetProperty( acq_.mvExposureAutoOffsetY, acq_.mvExposureAutoOffsetY.getMinValue(), true );
        conditionalSetProperty( acq_.mvExposureAutoWidth, static_cast<int64_type>( previousSensorAOI_.GetWidth() ), true );
        conditionalSetProperty( acq_.mvExposureAutoHeight, static_cast<int64_type>( previousSensorAOI_.GetHeight() ), true );
        conditionalSetProperty( acq_.mvExposureAutoOffsetX, static_cast<int64_type>( previousSensorAOI_.GetLeft() ), true );
        conditionalSetProperty( acq_.mvExposureAutoOffsetY, static_cast<int64_type>( previousSensorAOI_.GetTop() ), true );

        // first switch on the areas which are configured and apply their values...
        conditionalSetProperty( acq_.mvExposureAutoOffsetX, acq_.mvExposureAutoOffsetX.getMinValue(), true );
        conditionalSetProperty( acq_.mvExposureAutoOffsetY, acq_.mvExposureAutoOffsetY.getMinValue(), true );
        conditionalSetProperty( acq_.mvExposureAutoWidth, static_cast<int64_type>( pControl->pSCAOIw_->GetValue() ) );
        conditionalSetProperty( acq_.mvExposureAutoHeight, static_cast<int64_type>( pControl->pSCAOIh_->GetValue() ) );
        conditionalSetProperty( acq_.mvExposureAutoOffsetX, static_cast<int64_type>( pControl->pSCAOIx_->GetValue() ) );
        conditionalSetProperty( acq_.mvExposureAutoOffsetY, static_cast<int64_type>( pControl->pSCAOIy_->GetValue() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardAOIGenICam::ApplyAOISettingsAGC( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAGC );
    if( pControl )
    {
        conditionalSetProperty( anc_.mvGainAutoOffsetX, anc_.mvGainAutoOffsetX.getMinValue(), true );
        conditionalSetProperty( anc_.mvGainAutoOffsetY, anc_.mvGainAutoOffsetY.getMinValue(), true );
        conditionalSetProperty( anc_.mvGainAutoWidth, static_cast<int64_type>( previousSensorAOI_.GetWidth() ), true );
        conditionalSetProperty( anc_.mvGainAutoHeight, static_cast<int64_type>( previousSensorAOI_.GetHeight() ), true );
        conditionalSetProperty( anc_.mvGainAutoOffsetX, static_cast<int64_type>( previousSensorAOI_.GetLeft() ), true );
        conditionalSetProperty( anc_.mvGainAutoOffsetY, static_cast<int64_type>( previousSensorAOI_.GetTop() ), true );

        // first switch on the areas which are configured and apply their values...
        conditionalSetProperty( anc_.mvGainAutoOffsetX, static_cast<int64_type>( anc_.mvGainAutoOffsetX.getMinValue() ), true );
        conditionalSetProperty( anc_.mvGainAutoOffsetY, static_cast<int64_type>( anc_.mvGainAutoOffsetY.getMinValue() ), true );
        conditionalSetProperty( anc_.mvGainAutoWidth, static_cast<int64_type>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( anc_.mvGainAutoHeight, static_cast<int64_type>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( anc_.mvGainAutoOffsetX, static_cast<int64_type>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( anc_.mvGainAutoOffsetY, static_cast<int64_type>( pControl->pSCAOIy_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOIGenICam::ApplyAOISettingsAWBCamera( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAWB );
    if( pControl )
    {
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetX, anc_.mvBalanceWhiteAutoOffsetX.getMinValue(), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetY, anc_.mvBalanceWhiteAutoOffsetY.getMinValue(), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoWidth, static_cast<int64_type>( previousSensorAOI_.GetWidth() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoHeight, static_cast<int64_type>( previousSensorAOI_.GetHeight() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetX, static_cast<int64_type>( previousSensorAOI_.GetLeft() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetY, static_cast<int64_type>( previousSensorAOI_.GetTop() ), true );

        // first switch on the areas which are configured and apply their values...
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetX, static_cast<int64_type>( anc_.mvBalanceWhiteAutoOffsetX.getMinValue() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetY, static_cast<int64_type>( anc_.mvBalanceWhiteAutoOffsetY.getMinValue() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoWidth, static_cast<int64_type>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoHeight, static_cast<int64_type>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetX, static_cast<int64_type>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( anc_.mvBalanceWhiteAutoOffsetY, static_cast<int64_type>( pControl->pSCAOIy_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOIGenICam::UpdateAOIValues( TAOIControlType aoiType, TAOIControlType /*previousAoiType*/ )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = 0;
    switch( aoiType )
    {
    case aoictAEC:
        pControl = GetAOIControl( aoictAEC );
        if( pControl )
        {
            if( pControl->pCBAOIEnable_->GetValue() )
            {
                pControl->pSCAOIh_->SetValue( static_cast<int>( acq_.mvExposureAutoHeight.read() ) );
                pControl->pSCAOIw_->SetValue( static_cast<int>( acq_.mvExposureAutoWidth.read() ) );
                pControl->pSCAOIx_->SetValue( static_cast<int>( acq_.mvExposureAutoOffsetX.read() ) );
                pControl->pSCAOIy_->SetValue( static_cast<int>( acq_.mvExposureAutoOffsetY.read() ) );
            }
        }
        break;
    case aoictAGC:
        pControl = GetAOIControl( aoictAGC );
        if( pControl )
        {
            if( pControl->pCBAOIEnable_->GetValue() )
            {
                pControl->pSCAOIh_->SetValue( static_cast<int>( anc_.mvGainAutoHeight.read() ) );
                pControl->pSCAOIw_->SetValue( static_cast<int>( anc_.mvGainAutoWidth.read() ) );
                pControl->pSCAOIx_->SetValue( static_cast<int>( anc_.mvGainAutoOffsetX.read() ) );
                pControl->pSCAOIy_->SetValue( static_cast<int>( anc_.mvGainAutoOffsetY.read() ) );
            }
        }
        break;
    case aoictAWB:
        pControl = GetAOIControl( aoictAWB );
        if( pControl )
        {
            if( pControl->pCBAOIEnable_->GetValue() )
            {
                pControl->pSCAOIh_->SetValue( static_cast<int>( anc_.mvBalanceWhiteAutoHeight.read() ) );
                pControl->pSCAOIw_->SetValue( static_cast<int>( anc_.mvBalanceWhiteAutoWidth.read() ) );
                pControl->pSCAOIx_->SetValue( static_cast<int>( anc_.mvBalanceWhiteAutoOffsetX.read() ) );
                pControl->pSCAOIy_->SetValue( static_cast<int>( anc_.mvBalanceWhiteAutoOffsetY.read() ) );
            }
        }
        break;
    default:
        break;
    }
}

//=============================================================================
//================= Implementation WizardAOIDeviceSpecific ====================
//=============================================================================
//-----------------------------------------------------------------------------
WizardAOIDeviceSpecific::WizardAOIDeviceSpecific( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, ImageCanvas* pImageCanvas, bool boAcquisitionStateOnCancel, FunctionInterface* pFI ) : WizardAOI( pParent, title, pDev, pImageCanvas, boAcquisitionStateOnCancel, pFI ), csbd_( pDev ), acp_( csbd_.getAutoControlParameters() )
//-----------------------------------------------------------------------------
{
    ReadCurrentAOISettings();
    ReadMaxSensorResolution();
    UpdateAOIFromControl();
    SwitchToFullAOI();
    pParent->EnsureAcquisitionState( true );
}

//-----------------------------------------------------------------------------
void WizardAOIDeviceSpecific::ApplyAOISettings( void )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoictAOI );
    if( pControl )
    {
        conditionalSetProperty( csbd_.aoiStartX, csbd_.aoiStartX.getMinValue(), true );
        conditionalSetProperty( csbd_.aoiStartY, csbd_.aoiStartY.getMinValue(), true );
        conditionalSetProperty( csbd_.aoiWidth, previousSensorAOI_.GetWidth(), true );
        conditionalSetProperty( csbd_.aoiHeight, previousSensorAOI_.GetHeight(), true );
        conditionalSetProperty( csbd_.aoiStartX, previousSensorAOI_.GetLeft(), true );
        conditionalSetProperty( csbd_.aoiStartY, previousSensorAOI_.GetTop(), true );

        // first switch on the areas which are configured and apply their values...
        conditionalSetProperty( csbd_.aoiStartX, csbd_.aoiStartX.getMinValue(), true );
        conditionalSetProperty( csbd_.aoiStartY, csbd_.aoiStartY.getMinValue(), true );
        conditionalSetProperty( csbd_.aoiWidth, static_cast<int>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( csbd_.aoiHeight, static_cast<int>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( csbd_.aoiStartX, static_cast<int>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( csbd_.aoiStartY, static_cast<int>( pControl->pSCAOIy_->GetValue() ), true );
    }
}

//-----------------------------------------------------------------------------
void WizardAOIDeviceSpecific::ApplyAOISettingsAEC( TAOIControlType aoiType )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = GetAOIControl( aoiType );
    if( pControl )
    {
        conditionalSetProperty( acp_.aoiStartX, acp_.aoiStartX.getMinValue(), true );
        conditionalSetProperty( acp_.aoiStartY, acp_.aoiStartY.getMinValue(), true );
        conditionalSetProperty( acp_.aoiWidth, static_cast<int>( pControl->pSCAOIw_->GetValue() ), true );
        conditionalSetProperty( acp_.aoiHeight, static_cast<int>( pControl->pSCAOIh_->GetValue() ), true );
        conditionalSetProperty( acp_.aoiStartX, static_cast<int>( pControl->pSCAOIx_->GetValue() ), true );
        conditionalSetProperty( acp_.aoiStartY, static_cast<int>( pControl->pSCAOIy_->GetValue() ), true );
    }
}
//-----------------------------------------------------------------------------
void WizardAOIDeviceSpecific::UpdateAOIValues( TAOIControlType aoiType, TAOIControlType previousAoiType )
//-----------------------------------------------------------------------------
{
    const AOIControl* pControl = 0;
    boHandleChangedMessages_ = false;
    if( aoiType && previousAoiType != aoictAOI )
    {
        switch( aoiType )
        {
        case aoictAEC:
            pControl = GetAOIControl( aoictAEC );
            if( pControl )
            {
                if( pControl->pCBAOIEnable_->IsChecked() && configuredAreas_[previousAoiType]->pCBAOIEnable_->IsChecked() )
                {
                    pControl->pSCAOIh_->SetValue( static_cast<int>( acp_.aoiHeight.read() ) );
                    pControl->pSCAOIw_->SetValue( static_cast<int>( acp_.aoiWidth.read() ) );
                    pControl->pSCAOIx_->SetValue( static_cast<int>( acp_.aoiStartX.read() ) );
                    pControl->pSCAOIy_->SetValue( static_cast<int>( acp_.aoiStartY.read() ) );
                }
            }
            break;
        case aoictAGC:
            pControl = GetAOIControl( aoictAGC );
            if( pControl )
            {
                if( pControl->pCBAOIEnable_->GetValue() == true && configuredAreas_[previousAoiType]->pCBAOIEnable_->GetValue() == true )
                {
                    pControl->pSCAOIh_->SetValue( static_cast<int>( acp_.aoiHeight.read() ) );
                    pControl->pSCAOIw_->SetValue( static_cast<int>( acp_.aoiWidth.read() ) );
                    pControl->pSCAOIx_->SetValue( static_cast<int>( acp_.aoiStartX.read() ) );
                    pControl->pSCAOIy_->SetValue( static_cast<int>( acp_.aoiStartY.read() ) );
                }
            }
            break;
        default:
            break;
        }
    }
    boHandleChangedMessages_ = true;
}
