//-----------------------------------------------------------------------------
#ifndef WizardAOIH
#define WizardAOIH WizardAOIH
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include "PropViewFrame.h"
#include "ValuesFromUserDlg.h"

class ImageCanvas;
class wxSpinCtrlDbl;

//-----------------------------------------------------------------------------
class WizardAOI : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
public:
    explicit WizardAOI( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, ImageCanvas* pImageCanvas, bool boAcquisitionStateOnCancel, mvIMPACT::acquire::FunctionInterface* pFI );
    virtual ~WizardAOI();

    void ShowImageTimeoutPopup( void );
    void UpdateControlsFromAOI( const AOI* p );
protected:
    enum TAOIControlType
    {
        aoictUNDEFINED = -1,
        aoictAOI = 0,
        aoictAEC,
        aoictAGC,
        aoictAWB,
        aoictAWBHost,
        aoictFFC,
        aoictFFCCalibrate,
        aoictScaler
    };

    //-----------------------------------------------------------------------------
    /// \brief GUI elements for a single sequencer set
    struct AOIControl
    //-----------------------------------------------------------------------------
    {
        TAOIControlType type_;
        wxPanel* pSetPanel_;
        wxString areaString_;
        wxString areaTooltipString_;
        wxFlexGridSizer* pGridSizer_;
        wxCheckBox* pCBAOIEnable_;
        wxButton* pCalibrate_;
        wxSpinCtrlDbl* pSCAOIx_;
        wxSpinCtrlDbl* pSCAOIy_;
        wxSpinCtrlDbl* pSCAOIw_;
        wxSpinCtrlDbl* pSCAOIh_;
        wxSpinCtrlDbl* pSCAOIwDest_;
        wxSpinCtrlDbl* pSCAOIhDest_;
        explicit AOIControl() : type_( aoictUNDEFINED ), pSetPanel_( 0 ), areaString_(), areaTooltipString_(), pGridSizer_( 0 ), pCBAOIEnable_( 0 ),
            pCalibrate_( 0 ), pSCAOIx_( 0 ), pSCAOIy_( 0 ), pSCAOIw_( 0 ), pSCAOIh_( 0 ), pSCAOIwDest_( 0 ), pSCAOIhDest_( 0 ) {}
    };
    typedef std::map<int64_type, AOIControl*> AOINrToControlsMap;
    typedef std::map<const AOI*, AOIControl*> AOIToControlsMap;

    //-----------------------------------------------------------------------------
    /// \brief GUI elements for a single sequencer set
    struct SpinControlWindowIDs
    //-----------------------------------------------------------------------------
    {
        wxWindowID x_id;
        wxWindowID y_id;
        wxWindowID w_id;
        wxWindowID h_id;
        explicit SpinControlWindowIDs( wxWindowID x, wxWindowID y, wxWindowID w, wxWindowID h ) :
            x_id( x ), y_id( y ), w_id( w ), h_id( h ) {}
    };

    //-----------------------------------------------------------------------------
    enum TWidgetIDs_AOIWizard
    //-----------------------------------------------------------------------------
    {
        widMainFrame = wxID_HIGHEST,
        widCBShowAOILabels,
        widStaticBitmapWarning,
        widNotebookAOI,
        widEnable,
        widCalibrateButton,
        widSCAOIx,
        widSCAOIy,
        widSCAOIw,
        widSCAOIh,
        widSCAOIwDest,
        widSCAOIhDest,
        widLAST
    };

    //-----------------------------------------------------------------------------
    enum TAOIWizardError
    //-----------------------------------------------------------------------------
    {
        mrweAOIOutOfBounds
    };

    //-----------------------------------------------------------------------------
    /// \brief Necessary Information for AOI Management Errors
    struct AOIErrorInfo
    //-----------------------------------------------------------------------------
    {
        int64_type AOINumber;
        TAOIWizardError errorType;
        explicit AOIErrorInfo( int64_type nr, TAOIWizardError type ) : AOINumber( nr ), errorType( type )
        {
        }
        bool operator==( const AOIErrorInfo& theOther )
        {
            return ( AOINumber == theOther.AOINumber ) &&
                   ( errorType == theOther.errorType );
        }
    };

    PropViewFrame*                      pParent_;
    Device*                             pDev_;
    FunctionInterface*                  pFI_;
    ImageCanvas*                        pImageCanvas_;
    std::vector<AOIErrorInfo>           currentErrors_;
    AOINrToControlsMap                  configuredAreas_;
    bool                                boGUICreationComplete_;
    bool                                boAcquisitionStateOnCancel_;
    bool                                boHandleChangedMessages_;
    AOI*                                pAOIForOverlay_;
    wxRect                              previousSensorAOI_;
    wxRect                              rectSensor_;
    int64_type                          offsetXIncSensor_;
    int64_type                          offsetYIncSensor_;
    int64_type                          widthIncSensor_;
    int64_type                          heightIncSensor_;
    int64_type                          widthMinSensor_;
    int64_type                          heightMinSensor_;

    wxCheckBox*                         pCBShowAOILabel_;
    wxNotebook*                         pAOISettingsNotebook_;
    wxStaticBitmap*                     pStaticBitmapWarning_;
    wxStaticText*                       pInfoText_;

    ImageProcessing                     imageProcessing_;
    ImageDestination                    imageDestination_;

    void                                AddError( const AOIErrorInfo& errorInfo );
    void                                AddPageToAOISettingNotebook( int64_type value, AOIControl* pAOIControl, bool boInsertOnFirstFreePosition )
    {
        configuredAreas_.insert( std::make_pair( value, pAOIControl ) );
        if( boInsertOnFirstFreePosition )
        {
            // determine the position in which the new Tab will be inserted.
            int64_type position = -1;
            for( AOINrToControlsMap::iterator it = configuredAreas_.begin(); it != configuredAreas_.end(); it++ )
            {
                position++;
                if( it->first == value )
                {
                    break;
                }
            }
            pAOISettingsNotebook_->InsertPage( static_cast<size_t>( position ), pAOIControl->pSetPanel_, pAOIControl->pSetPanel_->GetName(), true );
        }
        else
        {
            pAOISettingsNotebook_->AddPage( pAOIControl->pSetPanel_, pAOIControl->pSetPanel_->GetName(), false );
        }

    }
    virtual void                        ApplyAOISettings( void ) = 0;
    virtual void                        ApplyAOISettingsAEC( TAOIControlType aoiType ) = 0;
    virtual void                        ApplyAOISettingsAGC( void ) = 0;
    void                                ApplyAOISettingsAWB( void );
    virtual void                        ApplyAOISettingsAWBCamera( void ) = 0;
    void                                ApplyAOISettingsFFC( void );
    void                                ApplyAOISettingsScaler( void );

    void                                BindSpinCtrlEventHandler( wxWindowID x_id, wxWindowID y_id, wxWindowID w_id, wxWindowID h_id )
    {
        Bind( wxEVT_SPINCTRL, &WizardAOI::OnAOITextChanged, this, x_id );
        Bind( wxEVT_SPINCTRL, &WizardAOI::OnAOITextChanged, this, y_id );
        Bind( wxEVT_SPINCTRL, &WizardAOI::OnAOITextChanged, this, w_id );
        Bind( wxEVT_SPINCTRL, &WizardAOI::OnAOITextChanged, this, h_id );
#ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL // Unfortunately on Linux wxWidgets 2.6.x - ??? handling these messages will cause problems, while on Windows not doing so will not always update the GUI as desired :-(
        Bind( wxEVT_TEXT, &WizardAOI::OnAOITextChanged, this, x_id );
        Bind( wxEVT_TEXT, &WizardAOI::OnAOITextChanged, this, y_id );
        Bind( wxEVT_TEXT, &WizardAOI::OnAOITextChanged, this, w_id );
        Bind( wxEVT_TEXT, &WizardAOI::OnAOITextChanged, this, h_id );
#endif // #ifdef BUILD_WITH_TEXT_EVENTS_FOR_SPINCTRL
    }

    void                                CalibrateWhiteBalance( void )
    {
        conditionalSetEnumPropertyByString( imageProcessing_.whiteBalanceCalibration, "Calibrate Next Frame", true );
    }
    void                                CalibrateFlatFieldCorrection()
    {
        conditionalSetEnumPropertyByString( imageProcessing_.flatFieldFilterCalibrationAoiMode, "UseAoi", true );
        conditionalSetProperty( imageProcessing_.flatFieldFilterCalibrationImageCount, 5, true );
        conditionalSetEnumPropertyByString( imageProcessing_.flatFieldFilterMode, "Calibrate", true );
    }
    void                                CloseDlg( int result );
    void                                ComposeAreaPanelAEC( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    void                                ComposeAreaPanelAGC( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    void                                ComposeAreaPanelAOI( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    AOIControl*                         ComposeAreaPanelAWB( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    void                                ComposeAreaPanelAWBCamera( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    void                                ComposeAreaPanelFFC( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    void                                ComposeAreaPanelFFCCalibration( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    void                                ComposeAreaPanelScaler( TAOIControlType aoiType, bool boInsertOnFirstFreePosition, int aoiCount );
    wxSpinCtrlDbl*                      CallCreateAOISpinControl( wxWindow* pParent, wxWindowID winId, int64_type min, int64_type max, int64_type value, int64_type inc, bool boEnabled )
    {
        wxSpinCtrlDbl* pSpinCtrl = CreateAOISpinControl( pParent, winId, min, max, value, inc, boEnabled );
        return pSpinCtrl;
    }
    wxSpinCtrlDbl*                      CreateAOISpinControl( wxWindow* pParent, wxWindowID id, int64_type min, int64_type max, int64_type value, int64_type inc, bool boEnabled );
    void                                CreateCalibrateButton( AOIControl* pAOIControl, wxString name )
    {
        pAOIControl->pGridSizer_->Add( new wxStaticText( pAOIControl->pSetPanel_, wxID_ANY, wxEmptyString ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
        pAOIControl->pCalibrate_ = new wxButton( pAOIControl->pSetPanel_, widCalibrateButton, name, wxDefaultPosition );
        pAOIControl->pGridSizer_->Add( pAOIControl->pCalibrate_, wxSizerFlags().Expand() );
    }
    void                                CreateCheckBoxEnableEntry( AOIControl* pAOIControl, wxString name, bool boEnabled, wxString checkBoxTooltip )
    {
        pAOIControl->pGridSizer_->Add( new wxStaticText( pAOIControl->pSetPanel_, wxID_ANY, name ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
        pAOIControl->pCBAOIEnable_ = new wxCheckBox( pAOIControl->pSetPanel_, widEnable, wxEmptyString, wxDefaultPosition );
        pAOIControl->pCBAOIEnable_->SetToolTip( checkBoxTooltip );
        pAOIControl->pCBAOIEnable_->SetValue( boEnabled );
        pAOIControl->pGridSizer_->Add( pAOIControl->pCBAOIEnable_, wxSizerFlags().Expand() );
    }
    void                                CreateSpinControlEntry( AOIControl* pAOIControl, wxSpinCtrlDbl*& pSCAOI, wxWindowID winId, wxString name, int64_type min, int64_type max, int64_type value, int64_type inc, bool boEnabled )
    {
        pAOIControl->pGridSizer_->Add( new wxStaticText( pAOIControl->pSetPanel_, wxID_ANY, name ), wxSizerFlags().Align( wxALIGN_CENTER_VERTICAL ) );
        pSCAOI = CallCreateAOISpinControl( pAOIControl->pSetPanel_, winId, min, max, value, inc, boEnabled );
        pAOIControl->pGridSizer_->Add( reinterpret_cast<wxWindow*>( pSCAOI ), wxSizerFlags().Expand() );
    }
    void                                ChangePageControlState( AOIControl* pAOIControl, bool boEnabled );
    void                                EnableOrDisableHostAWB( bool boEnable );
    void                                GetDeviceAOILimits( const int incX, const int incY, int& maxOffsetX, int& maxOffsetY, int& maxWidth, int& maxHeight ) const;
    AOIControl*                         GetAOIControl( TAOIControlType aoiType ) const
    {
        AOINrToControlsMap::const_iterator it = configuredAreas_.begin();
        const AOINrToControlsMap::const_iterator itEND = configuredAreas_.end();
        while( it != itEND )
        {
            if( it->second->type_ == aoiType )
            {
                return it->second;
            }
            it++;
        }
        return 0;
    }
    bool                                HasCurrentConfigurationCriticalError( void ) const;
    virtual void                        PopulateControls( void ) = 0;
    virtual void                        ReadCurrentAOISettings( void ) = 0;
    virtual void                        ReadMaxSensorResolution( void ) = 0;
    void                                RemoveError( const AOIErrorInfo& errorInfo );
    SpinControlWindowIDs                SetupAreaPanel( AOIControl* pAOIControl, TAOIControlType PageAOIType, int aoiNumber, wxString pageAOIName, wxString pageAOIToolTip )
    {
        pAOIControl->pSetPanel_ = new wxPanel( pAOISettingsNotebook_, static_cast<wxWindowID>( widStaticBitmapWarning + aoiNumber + 1 ), wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, pageAOIName );
        pAOIControl->type_ = static_cast<TAOIControlType>( PageAOIType );
        pAOIControl->areaString_ = pageAOIName;
        pAOIControl->areaTooltipString_ = pageAOIToolTip;
        pAOIControl->pSetPanel_->SetToolTip( pageAOIToolTip );

        // workaround for correct notebook background colors on all platforms
        const wxColour defaultCol = pAOISettingsNotebook_->GetThemeBackgroundColour();
        if( defaultCol.IsOk() )
        {
            pAOIControl->pSetPanel_->SetBackgroundColour( defaultCol );
        }

        pAOIControl->pGridSizer_ = new wxFlexGridSizer( 2 );
        pAOIControl->pGridSizer_->AddGrowableCol( 1, 2 );

        return SpinControlWindowIDs( widLAST + ( static_cast<wxWindowID>( aoiNumber * 0x100 ) | widSCAOIx ),
                                     widLAST + ( static_cast<wxWindowID>( aoiNumber * 0x100 ) | widSCAOIy ),
                                     widLAST + ( static_cast<wxWindowID>( aoiNumber * 0x100 ) | widSCAOIw ),
                                     widLAST + ( static_cast<wxWindowID>( aoiNumber * 0x100 ) | widSCAOIh ) );
    }
    virtual void                        SetupAutoGain( bool boContinuous ) = 0;
    virtual void                        SetupAutoExposure( bool boContinuous ) = 0;
    void                                SetupAutoWhiteBalance( void )
    {
        conditionalSetProperty( imageProcessing_.whiteBalance, wbpUser1, true );
        conditionalSetEnumPropertyByString( imageProcessing_.getWBUserSetting( 0 ).WBAoiMode, "UseAoi", true );
    }
    virtual void                        SetupAutoWhiteBalanceCamera( bool /*boContinuous*/ ) {}
    void                                SetupFlatFieldCorrection( bool boOn )
    {
        conditionalSetEnumPropertyByString( imageProcessing_.flatFieldFilterMode, boOn ? "On" : "Off", true );
        if( boOn )
        {
            conditionalSetEnumPropertyByString( imageProcessing_.flatFieldFilterCorrectionAoiMode, "UseAoi", true );
        }
    }
    void                                SetupScaler( bool boOn )
    {
        conditionalSetEnumPropertyByString( imageDestination_.scalerMode, boOn ? "On" : "Off", true );
        conditionalSetEnumPropertyByString( imageDestination_.scalerAoiEnable, boOn ? "On" : "Off", true );
    }
    virtual void                        SwitchToFullAOI( void ) = 0;
    virtual void                        UpdateAOIValues( TAOIControlType aoiType, TAOIControlType previousAoiType ) = 0;
    void                                UpdateAOIFromControl( void );
    void                                UpdateError( const wxString& msg, const wxBitmap& icon ) const;
    void                                UpdateErrors( void ) const;

    //----------------------------------GUI Functions----------------------------------
    void                                OnAOITextChanged( wxCommandEvent& )
    {
        if( boHandleChangedMessages_ )
        {
            UpdateAOIFromControl();
        }
    }
    virtual void                        OnBtnCancel( wxCommandEvent& )
    {
        CloseDlg( wxID_CANCEL );
    }
    virtual void                        OnBtnOk( wxCommandEvent& );
    void                                OnBtnCalibrate( wxCommandEvent& )
    {
        int page = pAOISettingsNotebook_->GetSelection();
        switch( configuredAreas_[page]->type_ )
        {
        case WizardAOI::aoictAOI:
        case WizardAOI::aoictAEC:
        case WizardAOI::aoictAGC:
        case WizardAOI::aoictAWB:
        case WizardAOI::aoictFFC:
            break;
        case WizardAOI::aoictAWBHost:
            CalibrateWhiteBalance();
            break;
        case WizardAOI::aoictFFCCalibrate:
            CalibrateFlatFieldCorrection();
            break;
        default:
            break;
        }
    }
    void                                OnCheckBoxStateChanged( wxCommandEvent& e )
    {
        ChangePageControlState( configuredAreas_[pAOISettingsNotebook_->GetSelection()], e.IsChecked() );
    }
    virtual void                        OnClose( wxCloseEvent& );
    virtual void                        OnNotebookPageChanged( wxBookCtrlEvent& e )
    {
        if( boGUICreationComplete_ && boHandleChangedMessages_ )
        {
            const int currentSelection = pAOISettingsNotebook_->GetSelection();
            AOIAccessScope aoiAccessScope( *pImageCanvas_ );
            pAOIForOverlay_->m_description = configuredAreas_[currentSelection]->areaString_;
            UpdateAOIValues( configuredAreas_[e.GetSelection()]->type_, configuredAreas_[e.GetOldSelection()]->type_ );
            UpdateAOIFromControl();
        }
    }
    DECLARE_EVENT_TABLE()
};

//-----------------------------------------------------------------------------
class WizardAOIGenICam : public WizardAOI
//-----------------------------------------------------------------------------
{
public:
    explicit                                    WizardAOIGenICam( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, ImageCanvas* pImageCanvas, bool boAcquisitionStateOnCancel, FunctionInterface* pFI );
protected:
    virtual void                                ApplyAOISettings( void );
    virtual void                                ApplyAOISettingsAEC( TAOIControlType aoiType );
    virtual void                                ApplyAOISettingsAGC( void );
    virtual void                                ApplyAOISettingsAWBCamera( void );
    virtual void                                PopulateControls( void )
    {
        ReadCurrentAOISettings();
        ReadMaxSensorResolution();
    }
    virtual void                                ReadCurrentAOISettings( void )
    {
        conditionalSetEnumPropertyByString( ifc_.mvMultiAreaMode, "mvOff", true );
        previousSensorAOI_.SetLeft( ifc_.offsetX.isValid() ? static_cast<int>( ifc_.offsetX.read() ) : 0 );
        previousSensorAOI_.SetTop( ifc_.offsetY.isValid() ? static_cast<int>( ifc_.offsetY.read() ) : 0 );
        previousSensorAOI_.SetWidth( ifc_.width.isValid() ? static_cast<int>( ifc_.width.read() ) : 0 );
        previousSensorAOI_.SetHeight( ifc_.height.isValid() ? static_cast<int>( ifc_.height.read() ) : 0 );
        conditionalSetProperty( ifc_.offsetX, int64_type( 0 ), true );
        conditionalSetProperty( ifc_.offsetY, int64_type( 0 ), true );
        conditionalSetProperty( ifc_.width, ifc_.width.getMaxValue(), true );
        conditionalSetProperty( ifc_.height, ifc_.height.getMaxValue(), true );
    }
    virtual void                                ReadMaxSensorResolution( void )
    {
        rectSensor_ = wxRect( 0, 0, static_cast<int>( ifc_.widthMax.read() ), static_cast<int>( ifc_.heightMax.read() ) );
        offsetXIncSensor_ = ifc_.offsetX.getStepWidth();
        offsetYIncSensor_ = ifc_.offsetY.getStepWidth();
        widthIncSensor_ = ifc_.width.getStepWidth();
        heightIncSensor_ = ifc_.height.getStepWidth();
        widthMinSensor_ = ifc_.width.getMinValue();
        heightMinSensor_ = ifc_.height.getMinValue();
    }
    virtual void                                SetupAutoGain( bool boContinuous )
    {
        if( boContinuous )
        {
            conditionalSetEnumPropertyByString( anc_.gainAuto, "Continuous", true );
            conditionalSetEnumPropertyByString( anc_.mvGainAutoAOIMode, "mvUser", true );
        }
        else
        {
            conditionalSetEnumPropertyByString( anc_.mvGainAutoAOIMode, "mvFull", true );
            conditionalSetEnumPropertyByString( anc_.gainAuto, "Off", true );
        }
    }
    virtual void                                SetupAutoExposure( bool boContinuous )
    {
        if( boContinuous )
        {
            conditionalSetEnumPropertyByString( acq_.exposureAuto, "Continuous", true );
            conditionalSetEnumPropertyByString( acq_.mvExposureAutoAOIMode, "mvUser", true );
            conditionalSetProperty( acq_.mvExposureAutoUpperLimit, ( acq_.mvExposureAutoUpperLimit.hasMaxValue() ? acq_.mvExposureAutoUpperLimit.getMaxValue() : 100000 ), true );
            conditionalSetProperty( acq_.mvExposureAutoLowerLimit, ( acq_.mvExposureAutoLowerLimit.hasMinValue() ? acq_.mvExposureAutoUpperLimit.getMinValue() : 1000 ), true );
        }
        else
        {
            conditionalSetEnumPropertyByString( acq_.mvExposureAutoAOIMode, "mvFull", true );
            conditionalSetEnumPropertyByString( acq_.exposureAuto, "Off", true );
        }
    }
    virtual void                                SetupAutoWhiteBalanceCamera( bool boContinuous )
    {
        conditionalSetEnumPropertyByString( anc_.mvBalanceWhiteAutoAOIMode, boContinuous ? "mvUser" : "mvFull", true );
        conditionalSetEnumPropertyByString( anc_.balanceWhiteAuto, boContinuous ? "On" : "Off", true );
    }
    virtual void                                SwitchToFullAOI( void )
    {
        conditionalSetProperty( ifc_.offsetX, int64_type( 0 ), true );
        conditionalSetProperty( ifc_.offsetY, int64_type( 0 ), true );
        conditionalSetProperty( ifc_.width, ifc_.widthMax.read(), true );
        conditionalSetProperty( ifc_.height, ifc_.heightMax.read(), true );
    }
    virtual void                                UpdateAOIValues( TAOIControlType aoiType, TAOIControlType previousAoiType );
private:
    GenICam::AcquisitionControl                 acq_;
    GenICam::ImageFormatControl                 ifc_;
    GenICam::AnalogControl                      anc_;
};

//-----------------------------------------------------------------------------
class WizardAOIDeviceSpecific : public WizardAOI
//-----------------------------------------------------------------------------
{
public:
    WizardAOIDeviceSpecific( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, ImageCanvas* pImageCanvas, bool boAcquisitionStateOnCancel, FunctionInterface* pFI );
protected:
    virtual void                                ApplyAOISettings( void );
    virtual void                                ApplyAOISettingsAEC( TAOIControlType aoiType );
    virtual void                                ApplyAOISettingsAGC( void ) {};
    virtual void                                ApplyAOISettingsAWBCamera( void )
    {
    };
    virtual void                                PopulateControls( void )
    {
        ReadCurrentAOISettings();
        ReadMaxSensorResolution();
    }
    virtual void                                ReadCurrentAOISettings( void )
    {
        previousSensorAOI_.SetLeft( csbd_.aoiStartX.isValid() ? static_cast<int>( csbd_.aoiStartX.read() ) : 0 );
        previousSensorAOI_.SetTop( csbd_.aoiStartY.isValid() ? static_cast<int>( csbd_.aoiStartY.read() ) : 0 );
        previousSensorAOI_.SetWidth( csbd_.aoiWidth.isValid() ? static_cast<int>( csbd_.aoiWidth.read() ) : 0 );
        previousSensorAOI_.SetHeight( csbd_.aoiHeight.isValid() ? static_cast<int>( csbd_.aoiHeight.read() ) : 0 );
        conditionalSetProperty( csbd_.aoiStartX, int( 0 ), true );
        conditionalSetProperty( csbd_.aoiStartY, int( 0 ), true );
        conditionalSetProperty( csbd_.aoiWidth, csbd_.aoiWidth.getMaxValue(), true );
        conditionalSetProperty( csbd_.aoiHeight, csbd_.aoiHeight.getMaxValue(), true );
    }
    virtual void                                ReadMaxSensorResolution( void )
    {
        rectSensor_ = wxRect( 0, 0, static_cast<int>( csbd_.aoiWidth.getMaxValue() ), static_cast<int>( csbd_.aoiHeight.getMaxValue() ) );
        offsetXIncSensor_ = csbd_.aoiStartX.getStepWidth();
        offsetYIncSensor_ = csbd_.aoiStartY.getStepWidth();
        widthIncSensor_ = csbd_.aoiWidth.getStepWidth();
        heightIncSensor_ = csbd_.aoiHeight.getStepWidth();
        widthMinSensor_ = csbd_.aoiWidth.getMinValue();
        heightMinSensor_ = csbd_.aoiHeight.getMinValue();
    }
    virtual void                                SetupAutoGain( bool boContinuous )
    {
        conditionalSetEnumPropertyByString( csbd_.autoGainControl, boContinuous ? "On" : "Off", true );
        if( acp_.gainUpperLimit_dB.hasMaxValue() )
        {
            conditionalSetProperty( acp_.gainUpperLimit_dB, acp_.gainUpperLimit_dB.getMaxValue(), true );
        }
        if( acp_.gainLowerLimit_dB.hasMaxValue() )
        {
            conditionalSetProperty( acp_.gainLowerLimit_dB, acp_.gainLowerLimit_dB.getMinValue(), true );
        }
    }
    virtual void                                SetupAutoExposure( bool boContinuous )
    {
        conditionalSetEnumPropertyByString( csbd_.autoExposeControl, boContinuous ? "On" : "Off", true );
        if( acp_.exposeUpperLimit_us.hasMaxValue() )
        {
            conditionalSetProperty( acp_.exposeUpperLimit_us, acp_.exposeUpperLimit_us.getMaxValue(), true );
        }
        if( acp_.exposeLowerLimit_us.hasMaxValue() )
        {
            conditionalSetProperty( acp_.exposeLowerLimit_us, acp_.exposeLowerLimit_us.getMinValue(), true );
        }
    }
    virtual void                                SwitchToFullAOI( void )
    {
        conditionalSetProperty( csbd_.aoiStartX, int( 0 ), true );
        conditionalSetProperty( csbd_.aoiStartY, int( 0 ), true );
        conditionalSetProperty( csbd_.aoiWidth, csbd_.aoiWidth.getMaxValue(), true );
        conditionalSetProperty( csbd_.aoiHeight, csbd_.aoiHeight.getMaxValue(), true );
    }
    virtual void                                UpdateAOIValues( TAOIControlType aoiType, TAOIControlType previousAoiType );
private:
    CameraSettingsBlueDevice                    csbd_;
    AutoControlParameters                       acp_;
};

#endif // WizardAOIH
