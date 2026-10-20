//-----------------------------------------------------------------------------
#ifndef WizardQuickSetupH
#define WizardQuickSetupH WizardQuickSetupH
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include "ValuesFromUserDlg.h"
#include "spinctld.h"
#include "PropViewFrame.h"

class wxCheckBox;
class wxSlider;
class wxTextCtrl;
class wxToggleButton;

//-----------------------------------------------------------------------------
class WizardQuickSetup : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
public:
    explicit                            WizardQuickSetup( PropViewFrame* pParent, const wxString& title, Device* pDev, FunctionInterface* pFI, bool boAutoConfigureOnStart );
    void                                ShowImageTimeoutPopup( void );
    void                                UpdateControlsData( void );

protected:
    //-----------------------------------------------------------------------------
    enum TWhiteBalanceChannel
    //-----------------------------------------------------------------------------
    {
        wbcRed,
        wbcBlue
    };
    //-----------------------------------------------------------------------------
    enum TOptimizationLevel
    //-----------------------------------------------------------------------------
    {
        olBestQuality,
        olCompromiseQualityAndSpeed,
        olHighestSpeed
    };
    void                                UpdateFrameRateWarningBitmap( bool boIsFrameRateMax );

    //-----------------------------------------------------------------------------
    struct DeviceSettings
    //-----------------------------------------------------------------------------
    {
        double                          exposureTime;
        bool                            boAutoExposureEnabled;
        double                          unifiedGain;
        bool                            boAutoGainEnabled;
        double                          unifiedBlackLevel;
        bool                            boGammaEnabled;
        double                          whiteBalanceRed;
        double                          whiteBalanceBlue;
        bool                            boAutoWhiteBalanceEnabled;
        double                          saturation;
        bool                            boCCMEnabled;
        double                          frameRate;
        double                          maxFrameRateAtCurrentAOI;
        double                          maxAllowedHQFrameRateAtCurrentAOI;
        double                          autoExposureUpperLimit;
        bool                            boMaximumFrameRateEnabled;

        double                          analogGainMin;
        double                          analogGainMax;
        double                          digitalGainMin;
        double                          digitalGainMax;
        double                          analogBlackLevelMin;
        double                          analogBlackLevelMax;

        bool                            boColorEnabled;
        bool                            boDeviceSpecificOverlappedExposure;
        bool                            boCameraBasedColorProcessingEnabled;
        TOptimizationLevel              optimizationLevel;
        std::string                     imageFormatControlPixelFormat;
        TImageDestinationPixelFormat    imageDestinationPixelFormat;
        std::string                     pixelClock;
    };

    //-----------------------------------------------------------------------------
    struct SupportedWizardFeatures
    //-----------------------------------------------------------------------------
    {
        bool                            boGainSupport;
        bool                            boBlackLevelSupport;
        bool                            boAutoExposureSupport;
        bool                            boAutoGainSupport;
        bool                            boUnifiedGainSupport;
        bool                            boAutoWhiteBalanceSupport;
        bool                            boMaximumFrameRateSupport;
        bool                            boRegulateFrameRateSupport;
        bool                            boColorOptionsSupport;
        bool                            boDriverSupportsColorCorrectionMatrixProcessing;
        bool                            boGammaSupport;
        bool                            boDeviceSupportsColorCorrectionMatrixProcessing;
    };

    std::string                         currentProductString_;
    std::string                         currentDeviceSerial_;
    unsigned int                        currentDeviceFWVersion_;
    DeviceSettings                      currentSettings_;
    SupportedWizardFeatures             featuresSupported_;

    virtual void                        ConfigurePolarizationDataExtraction( void ) {}
    virtual void                        CreateInterfaceLayoutSpecificControls( Device* pDev ) = 0;
    virtual void                        DeleteInterfaceLayoutSpecificControls( void ) = 0;
    virtual double                      DetermineMaxFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats ) = 0;
    virtual double                      DetermineOptimalHQFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats ) = 0;
    virtual void                        DoConfigureColorCorrection( bool boActive ) = 0;
    void                                DoConfigureDriverBaseColorCorrection( bool boActive );
    virtual void                        DoConfigureFrameRateMaximum( bool /*boActive*/, double /*frameRateValue*/ ) {}
    double                              DoReadDriverBasedSaturationData( void ) const;
    virtual double                      DoReadUnifiedBlackLevel( void ) const = 0;
    virtual double                      DoReadUnifiedGain( void ) const = 0;
    virtual void                        DoSetAcquisitionFrameRateLimitMode( void ) {}
    virtual std::string                 DoWriteDriverBasedSaturationData( double saturation );
    virtual void                        DoWriteUnifiedGain( double value ) const = 0;
    virtual void                        DoWriteUnifiedBlackLevelData( double value ) const = 0;
    void                                DoSetupFrameRateControls( double frameRateRangeMin, double frameRateRangeMax, double frameRate );
    void                                DoSetupExposureControls( double exposureMin, double exposureMax, double exposure, bool boHasStepWidth, double increment );
    void                                DoSetupGainControls( double gainUnifiedRangeMin, double gainUnifiedRangeMax, double gain );
    void                                DoSetupBlackLevelControls( double blackLevelUnifiedRangeMin, double blackLevelUnifiedRangeMax, double blackLevel );
    void                                DoSetupWhiteBalanceControls( double whiteBalanceRMin, double whiteBalanceRMax, double whiteBalanceR, double whiteBalanceBMin, double whiteBalanceBMax, double whiteBalanceB );
    TOptimizationLevel                  GetOptimizationLevel( void ) const
    {
        if( pSLOptimization_->GetValue() == pSLOptimization_->GetMax() )
        {
            return olHighestSpeed;
        }
        return static_cast< TOptimizationLevel >( pSLOptimization_->GetValue() );
    }
    virtual double                      GetAutoExposureUpperLimit( void ) const = 0;
    virtual bool                        GetAutoConfigureOnStart( void ) const
    {
        return boAutoConfigureOnStart_;
    }
    virtual bool                        GetCCMState( void ) const = 0;
    virtual bool                        GetDriverCCMState( void ) const
    {
        return ( DoesDriverOfferColorCorrectionMatrixProcessing() &&
                 ( pIP_->colorTwistInputCorrectionMatrixEnable.read() == bTrue ) &&
                 ( pIP_->colorTwistEnable.read() == bTrue ) &&
                 ( pIP_->colorTwistOutputCorrectionMatrixEnable.read() == bTrue ) );
    }
    virtual double                      GetExposureTime( void ) const = 0;
    virtual bool                        GetExposureOverlapped() const = 0;
    virtual bool                        GetFrameRateEnable( void ) const
    {
        return false;
    }
    std::string                         GetFullPropertyName( Property prop ) const;
    virtual std::string                 GetPixelClock( void ) const = 0;
    virtual std::string                 GetPixelFormat( void ) const = 0;
    virtual double                      GetWhiteBalance( TWhiteBalanceChannel channel )
    {
        return GetWhiteBalanceDriver( channel );
    }
    double                              GetWhiteBalanceDriver( TWhiteBalanceChannel channel );
    virtual bool                        HasAEC( void ) const = 0;
    virtual bool                        HasAGC( void ) = 0;
    virtual bool                        HasAWB( void ) const = 0;
    virtual bool                        HasMaximumFrameRate( void ) const = 0;
    virtual bool                        HasBlackLevel( void ) const = 0;
    virtual bool                        HasColorFormat( void ) const = 0;
    virtual bool                        HasFactoryDefault( void ) const = 0;
    virtual bool                        HasFrameRateEnable( void ) const
    {
        return false;
    }
    virtual bool                        HasGain( void ) const = 0;
    virtual bool                        HasUnifiedGain( void ) const
    {
        return false;
    }
    virtual bool                        HasNativeColorCorrection( void ) const = 0;
    virtual bool                        HasNativeGamma( void ) const = 0;
    bool                                DoesDriverOfferColorCorrectionMatrixProcessing( void ) const
    {
        return pIP_->colorTwistEnable.isValid();
    }

    bool                                InitializeExposureParameters( void );
    bool                                InitializeGainParameters( void );
    virtual void                        InitializeBlackLevelParameters( void ) = 0;
    virtual void                        InitializeGammaParameters( void ) {}
    virtual void                        InitializeWhiteBalanceParameters( void )
    {
        InitializeWhiteBalanceParametersDriver();
    }
    void                                InitializeWhiteBalanceParametersDriver( void );
    virtual bool                        IsDriverBasedGammaActive( void )
    {
        return pIP_->LUTEnable.read() == bTrue;
    }
    virtual bool                        IsGammaActive( void ) = 0;
    virtual void                        QueryInterfaceLayoutSpecificSettings( DeviceSettings& devSettings ) = 0;
    virtual void                        RestoreFactoryDefault( void ) = 0;
    virtual void                        SetOptimalPixelFormatColor(  TOptimizationLevel optimizationLevel ) = 0;
    virtual void                        SetOptimalPixelFormatGray(  TOptimizationLevel optimizationLevel ) = 0;
    virtual void                        SelectOptimalAutoExposureSettings( bool boHighQuality ) = 0;
    virtual void                        SelectOptimalPixelClock( TOptimizationLevel optimizationLevel ) = 0;
    virtual void                        SetExposureTime( double value ) = 0;
    virtual void                        SetExposureOverlapped( bool /* boEnable */ ) {}
    virtual void                        SetAutoExposure( bool boEnable ) = 0;
    virtual void                        SetAutoExposureUpperLimit( double exposureMax ) = 0;
    virtual void                        SetAutoGain( bool boEnable ) = 0;
    virtual void                        SetAutoWhiteBalance( bool /* boEnable */ ) {}
    virtual void                        SetupExposureControls( void ) = 0;
    virtual void                        SetupGainControls( void ) = 0;
    virtual void                        SetupBlackLevelControls( void ) = 0;
    virtual void                        SetupWhiteBalanceControls( void )
    {
        SetupWhiteBalanceControlsDriver();
    }
    void SetupWhiteBalanceControlsDriver( void );
    virtual void                        SetupUnifiedGainData( void ) = 0;
    virtual void                        SetupUnifiedBlackLevelData( void ) = 0;
    virtual void                        SetupFrameRateControls( void ) {}
    virtual void                        SetFrameRateEnable( bool /* boOn */ ) {}
    virtual void                        SetFrameRate( double /* value */ ) {}
    virtual void                        SetGamma(  bool /* boOn */ ) = 0;
    virtual void                        SetPixelClock( const std::string& frequency ) = 0;
    virtual void                        SetPixelFormat( const std::string& format ) = 0;
    virtual void                        SetWhiteBalance( TWhiteBalanceChannel channel, double value )
    {
        SetWhiteBalanceDriver( channel, value );
    }
    void                                SetWhiteBalanceDriver( TWhiteBalanceChannel channel, double value );
    virtual bool                        Supports12BitPixelFormats( void ) const = 0;
    virtual void                        TryToReadFrameRate( double& /*value*/ ) = 0;
    virtual void                        WriteExposureFeatures( const DeviceSettings& devSettings ) = 0;
    virtual void                        WriteGainFeatures( const DeviceSettings& devSettings ) = 0;
    virtual void                        WriteWhiteBalanceFeatures( const DeviceSettings& devSettings )
    {
        WriteWhiteBalanceFeaturesDriver( devSettings );
    }
    void                                WriteWhiteBalanceFeaturesDriver( const DeviceSettings& devSettings );
    void                                WriteQuickSetupWizardErrorMessage( const wxString& msg ) const
    {
        pParentPropViewFrame_->WriteErrorMessage( wxString( wxT( "Quick Setup Wizard: " ) + msg + wxT( "\n" ) ) );
    }
    void                                WriteQuickSetupWizardLogMessage( const wxString& msg ) const
    {
        pParentPropViewFrame_->WriteLogMessage( wxString( wxT( "Quick Setup Wizard: " ) + msg + wxT( "\n" ) ) );
    }
private:
    //-----------------------------------------------------------------------------
    enum TWidgetIDs_QuickSetup
    //-----------------------------------------------------------------------------
    {
        widMainFrame = wxID_HIGHEST,
        widBtnPresetColorHost,
        widBtnPresetColorCamera,
        widBtnPresetGray,
        widStaticBitmapQuality,
        widStaticBitmapSpeed,
        widStaticBitmapWarning,
        widSLOptimization,
        widBtnPresetFactory,
        widSLExposure,
        widSCExposure,
        widBtnExposureAuto,
        widSLGain,
        widSCGain,
        widBtnGainAuto,
        widSLBlackLevel,
        widSCBlackLevel,
        widBtnGamma,
        widSLSaturation,
        widSCSaturation,
        widBtnCCM,
        widSLWhiteBalanceR,
        widSCWhiteBalanceR,
        widSLWhiteBalanceB,
        widSCWhiteBalanceB,
        widBtnWhiteBalanceAuto,
        widSLFrameRate,
        widSCFrameRate,
        widBtnFrameRateMaximum,
        widCBShowDialogAtStartup,
        widBtnHistory,
        widBtnClearHistory,
        widTCHistory

    };

    //----------------------------------STATICS------------------------------------
    static const double                 EXPOSURE_SLIDER_GAMMA_;
    static const double                 SLIDER_GRANULARITY_;
    static const double                 GAMMA_CORRECTION_VALUE_;

    //----------------------------------GUI ELEMENTS-------------------------------
    wxBoxSizer*                         pTopDownSizer_;
    wxBitmapButton*                     pBtnPresetColorHost_;
    wxBitmapButton*                     pBtnPresetColorCamera_;
    wxBitmapButton*                     pBtnPresetFactory_;
    wxBitmapButton*                     pBtnPresetGray_;
    wxSlider*                           pSLOptimization_;
    wxStaticBitmap*                     pStaticBitmapQuality_;
    wxStaticBitmap*                     pStaticBitmapSpeed_;
    wxStaticBitmap*                     pStaticBitmapWarning_;
    wxSlider*                           pSLExposure_;
    wxSpinCtrlDbl*                      pSCExposure_;
    wxToggleButton*                     pBtnExposureAuto_;
    wxSlider*                           pSLGain_;
    wxSpinCtrlDbl*                      pSCGain_;
    wxToggleButton*                     pBtnGainAuto_;
    wxSlider*                           pSLBlackLevel_;
    wxSpinCtrlDbl*                      pSCBlackLevel_;
    wxToggleButton*                     pBtnGamma_;
    wxSlider*                           pSLWhiteBalanceR_;
    wxSpinCtrlDbl*                      pSCWhiteBalanceR_;
    wxSlider*                           pSLWhiteBalanceB_;
    wxSpinCtrlDbl*                      pSCWhiteBalanceB_;
    wxToggleButton*                     pBtnWhiteBalanceAuto_;
    wxSlider*                           pSLSaturation_;
    wxSpinCtrlDbl*                      pSCSaturation_;
    wxToggleButton*                     pBtnCCM_;
    wxSlider*                           pSLFrameRate_;
    wxSpinCtrlDbl*                      pSCFrameRate_;
    wxToggleButton*                     pBtnFrameRateMaximum_;
    wxStaticText*                       pFrameRateControlStaticText_;
    wxCheckBox*                         pCBShowDialogAtStartup_;
    wxToggleButton*                     pBtnHistory_;
    wxButton*                           pBtnClearHistory_;
    wxTextCtrl*                         pTCHistory_;
    wxBoxSizer*                         pInfoSizer_;
    bool                                boGUILocked_;
    bool                                boAutoConfigureOnStart_;

protected:
    Device*                             pDev_;
    FunctionInterface*                  pFI_;
    ImageProcessing*                    pIP_;
    double                              analogGainMin_;
    double                              analogGainMax_;
    double                              digitalGainMin_;
    double                              digitalGainMax_;
    double                              analogBlackLevelMin_;
    double                              analogBlackLevelMax_;
    double                              digitalBlackLevelMin_;
    double                              digitalBlackLevelMax_;
    TWhiteBalanceChannel                boLastWBChannelRead_;

    void                                InitializeWizardForSelectedDevice( Device* pDev );
    template<class _Ty, typename _Tx>
    void                                LoggedWriteProperty( _Ty& prop, const _Tx& value ) const
    {
        prop.write( value );
        std::ostringstream oss;
        oss << "Property " << GetFullPropertyName( prop ) << " set to " << prop.readS() << std::endl;
        propertyChangeHistory_ += oss.str();
    }
    template<class _Ty>
    void                                LoggedWriteProperty( _Ty& prop, const std::string& value ) const
    {
        prop.writeS( value );
        std::ostringstream oss;
        oss << "Property " << GetFullPropertyName( prop ) << " set to " << prop.readS() << std::endl;
        propertyChangeHistory_ += oss.str();
    }

private:
    PropViewFrame*                      pParentPropViewFrame_;
    mutable std::string                 propertyChangeHistory_;
    ImageDestination*                   pID_;

    //----------------------------------GENERAL------------------------------------
    void                                AnalyzeDeviceAndGatherInformation( void );
    void                                ApplyPreset( bool boColor, TOptimizationLevel optimizationLevel );
    void                                CleanUp( void );
    void                                CloseDlg( void );
    void                                HandlePresetChanges( void );
    void                                OnCBShowOnUseDevice( wxCommandEvent& e );
    virtual void                        OnClose( wxCloseEvent& );
    void                                QueryInitialDeviceSettings( void );
    void                                PresetHiddenAdjustments( bool boColor, TOptimizationLevel optimizationLevel );
    void                                RefreshControls( void );
    void                                SelectLUTDependingOnPixelFormat( void );
    void                                SetAcquisitionFrameRateLimitMode( void );
    void                                SetupControls( void );
    void                                SetupDevice( void );
    void                                SetupDriverSettings( void );
    void                                SetupOptimization( void );
    void                                SetupPresetButtons( bool enableHost, bool enableCamera );
    void                                SetupUnifiedData( void );
    bool                                ShowFactoryResetPopup( void );
    void                                ShutdownDlg( wxStandardID result );
    void                                ToggleDetails( bool boShow );
    void                                UpdateDetailsTextControl( void );
    void                                UpdateExposureControlsFromCamera( void );
    void                                UpdateGainControlsFromCamera( void );
    void                                UpdateWhiteBalanceControlsFromCamera( void );

    //----------------------------------EXPOSURE-----------------------------------
    void                                ApplyExposure( void );
    void                                ConfigureExposureAuto( bool boActive );
    double                              ExposureFromSliderValue( void ) const;
    int                                 ExposureToSliderValue( const double exposure ) const;
    void                                HandleExposureSpinControlChanges( void );

    //----------------------------------GAIN---------------------------------------
    void                                ApplyGain( void );
    void                                ConfigureGainAuto( bool boActive );
    void                                HandleGainSpinControlChanges( void );
    double                              ReadUnifiedGainData( void );
    void                                WriteUnifiedGainData( double unifiedGain );

    //----------------------------------BLACKLEVEL---------------------------------
    void                                ApplyBlackLevel( void );
    void                                ConfigureGamma( bool boActive );
    void                                HandleBlackLevelSpinControlChanges( void );
    double                              ReadUnifiedBlackLevelData( void );
    void                                WriteUnifiedBlackLevelData( double unifiedBlackLevel );

    //----------------------------------WHITEBALANCE-------------------------------
    void                                ApplyWhiteBalance( TWhiteBalanceChannel channel );
    void                                ConfigureWhiteBalanceAuto( bool boActive );
    void                                HandleWhiteBalanceRSpinControlChanges( void );
    void                                HandleWhiteBalanceBSpinControlChanges( void );

    //----------------------------------SATURATION--------------------------------
    void                                ApplySaturation( void );
    void                                ConfigureCCM( bool boActive );
    virtual double                      DoReadSaturationData( void ) const = 0;
    virtual std::string                 DoWriteSaturationData( double saturation ) = 0;
    void                                HandleSaturationSpinControlChanges( void );
    double                              ReadSaturationData( void ) const
    {
        return DoReadSaturationData();
    }
    void                                WriteSaturationData( double saturation )
    {
        propertyChangeHistory_ += DoWriteSaturationData( saturation );
    }

    //----------------------------------FRAMERATE---------------------------------
    void                                ApplyFrameRate( void );
    void                                ConfigureFrameRateMaximum( bool boActive );
    void                                HandleFrameRateSpinControlChanges( void );

    //----------------------------------BUTTONS------------------------------------
    void                                OnBtnPresetColorHost( wxCommandEvent& e );
    void                                OnBtnPresetColorCamera( wxCommandEvent& e );
    void                                OnBtnPresetGray( wxCommandEvent& e );
    void                                OnBtnPresetCustom( wxCommandEvent& e );
    void                                OnBtnPresetFactory( wxCommandEvent& e );

    void                                OnBtnExposureAuto( wxCommandEvent& e )
    {
        ConfigureExposureAuto( e.IsChecked() );
        RefreshControls();
    }
    void                                OnBtnGainAuto( wxCommandEvent& e )
    {
        ConfigureGainAuto( e.IsChecked() );
        RefreshControls();
    }
    void                                OnBtnGamma( wxCommandEvent& e )
    {
        ConfigureGamma( e.IsChecked() );
        RefreshControls();
    }
    void                                OnBtnCCM( wxCommandEvent& e )
    {
        ConfigureCCM( e.IsChecked() );
        RefreshControls();
    }
    void                                OnBtnWhiteBalanceAuto( wxCommandEvent& e )
    {
        ConfigureWhiteBalanceAuto( e.IsChecked() );
        RefreshControls();
    }
    void                                OnBtnFrameRateMaximum( wxCommandEvent& e )
    {
        ConfigureFrameRateMaximum( e.IsChecked() );
        RefreshControls();
    }
    void                                OnBtnHistory( wxCommandEvent& e )
    {
        ToggleDetails( e.IsChecked() );
    }
    void                                OnBtnClearHistory( wxCommandEvent& )
    {
        pTCHistory_->Clear();
    }

    virtual void                        OnBtnCancel( wxCommandEvent& );
    virtual void                        OnBtnOk( wxCommandEvent& );

    //----------------------------------BITMAPS-------------------------------------
    void                                OnLeftMouseButtonDown( wxMouseEvent& e );

    //----------------------------------SPINCONTROLS--------------------------------
    void                                OnSCExposureChanged( wxSpinEvent& )
    {
        HandleExposureSpinControlChanges();
    }
    void                                OnSCGainChanged( wxSpinEvent& )
    {
        HandleGainSpinControlChanges();
    }
    void                                OnSCBlackLevelChanged( wxSpinEvent& )
    {
        HandleBlackLevelSpinControlChanges();
    }
    void                                OnSCWhiteBalanceRChanged( wxSpinEvent& )
    {
        HandleWhiteBalanceRSpinControlChanges();
    }
    void                                OnSCWhiteBalanceBChanged( wxSpinEvent& )
    {
        HandleWhiteBalanceBSpinControlChanges();
    }
    void                                OnSCSaturationChanged( wxSpinEvent& )
    {
        HandleSaturationSpinControlChanges();
    }
    void                                OnSCFrameRateChanged( wxSpinEvent& )
    {
        HandleFrameRateSpinControlChanges();
    }

    //----------------------------------TEXTCONTROLS--------------------------------
    void                                OnSCExposureTextChanged( wxCommandEvent& )
    {
        HandleExposureSpinControlChanges();
    }
    void                                OnSCGainTextChanged( wxCommandEvent& )
    {
        HandleGainSpinControlChanges();
    }
    void                                OnSCBlackLevelTextChanged( wxCommandEvent& )
    {
        HandleBlackLevelSpinControlChanges();
    }
    void                                OnSCWhiteBalanceRTextChanged( wxCommandEvent& )
    {
        HandleWhiteBalanceRSpinControlChanges();
    }
    void                                OnSCWhiteBalanceBTextChanged( wxCommandEvent& )
    {
        HandleWhiteBalanceBSpinControlChanges();
    }
    void                                OnSCSaturationTextChanged( wxCommandEvent& )
    {
        HandleSaturationSpinControlChanges();
    }
    void                                OnSCFrameRateTextChanged( wxCommandEvent& )
    {
        HandleFrameRateSpinControlChanges();
    }

    //----------------------------------SLIDERS-------------------------------------
    void                                OnSLExposure( wxScrollEvent& e );
    void                                OnSLGain( wxScrollEvent& e );
    void                                OnSLBlackLevel( wxScrollEvent& e );
    void                                OnSLWhiteBalanceR( wxScrollEvent& e );
    void                                OnSLWhiteBalanceB( wxScrollEvent& e );
    void                                OnSLSaturation( wxScrollEvent& e );
    void                                OnSLFrameRate( wxScrollEvent& e );
    void                                OnSLOptimization( wxScrollEvent& )
    {
        SetupOptimization();
    }
    DECLARE_EVENT_TABLE()
};

//-----------------------------------------------------------------------------
class WizardQuickSetupGenICam : public WizardQuickSetup
//-----------------------------------------------------------------------------
{
public:
    explicit                            WizardQuickSetupGenICam( PropViewFrame* pParent, const wxString& title, Device* pDev, FunctionInterface* pFI, bool boAutoConfigureOnStart );
protected:
    virtual void                        ConfigurePolarizationDataExtraction( void );
    virtual void                        CreateInterfaceLayoutSpecificControls( Device* pDev );
    virtual void                        DeleteInterfaceLayoutSpecificControls( void );
    virtual double                      DetermineMaxFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats );
    virtual double                      DetermineOptimalHQFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats );
    virtual void                        DoConfigureColorCorrection( bool boActive );
    virtual void                        DoConfigureFrameRateMaximum( bool boActive, double frameRateValue );
    virtual double                      DoReadSaturationData( void ) const;
    virtual double                      DoReadUnifiedBlackLevel( void ) const;
    virtual double                      DoReadUnifiedGain( void ) const;
    virtual void                        DoSetAcquisitionFrameRateLimitMode( void );
    virtual std::string                 DoWriteSaturationData( double saturation );
    virtual std::string                 DoWriteDeviceBasedSaturationData( double saturation );
    virtual void                        DoWriteUnifiedGain( double value ) const;
    virtual void                        DoWriteUnifiedBlackLevelData( double value ) const;
    virtual double                      GetAutoExposureUpperLimit( void ) const;
    virtual bool                        GetCCMState( void ) const
    {
        return HasNativeColorCorrection() ? ( pCTC_->colorTransformationEnable.read() == bTrue ) : GetDriverCCMState();
    }
    virtual bool                        GetExposureOverlapped() const
    {
        return false;
    }
    std::vector<std::vector<double> >   GetInitialDeviceColorCorrectionMatrix( void ) const;
    virtual std::string                 GetPixelClock( void ) const;
    virtual std::string                 GetPixelFormat( void ) const;
    virtual double                      GetExposureTime( void ) const
    {
        return pAcC_->exposureTime.read();
    }
    virtual bool                        GetFrameRateEnable( void ) const
    {
        return pAcC_->mvAcquisitionFrameRateEnable.read() == bTrue;
    }
    virtual double                      GetWhiteBalance( TWhiteBalanceChannel channel );
    virtual bool                        HasAEC( void ) const;
    virtual bool                        HasAGC( void );
    virtual bool                        HasAWB( void ) const;
    virtual bool                        HasMaximumFrameRate( void ) const;
    virtual bool                        HasBlackLevel( void ) const
    {
        return pAnC_->blackLevelSelector.isValid();
    }
    virtual bool                        HasColorFormat( void ) const;
    virtual bool                        HasNativeColorCorrection( void ) const
    {
        return pCTC_->colorTransformationSelector.isValid();
    }
    virtual bool                        HasFactoryDefault( void ) const;
    virtual bool                        HasFrameRateEnable( void ) const
    {
        return pAcC_->mvAcquisitionFrameRateEnable.isValid();
    }
    virtual bool                        HasGain( void ) const
    {
        return pAnC_->gainSelector.isValid();
    }
    virtual bool                        HasUnifiedGain( void ) const;
    virtual bool                        HasNativeGamma( void ) const
    {
        return pAnC_->mvGammaEnable.isValid();
    }
    virtual void                        InitializeBlackLevelParameters( void );
    virtual void                        InitializeGammaParameters( void );
    virtual void                        InitializeWhiteBalanceParameters( void );
    virtual bool                        IsGammaActive( void )
    {
        return HasNativeGamma() ? ( pAnC_->gamma.read() == bTrue ) : IsDriverBasedGammaActive();
    }
    virtual void                        QueryInterfaceLayoutSpecificSettings( DeviceSettings& devSettings );
    virtual void                        RestoreFactoryDefault( void );
    virtual void                        SetOptimalPixelFormatColor( TOptimizationLevel optimizationLevel );
    virtual void                        SetOptimalPixelFormatGray(  TOptimizationLevel optimizationLevel );
    virtual void                        SelectOptimalAutoExposureSettings( bool boHighQuality );
    virtual void                        SelectOptimalPixelClock( TOptimizationLevel optimizationLevel );
    virtual void                        SetExposureTime( double value )
    {
        LoggedWriteProperty( pAcC_->exposureTime, value );
    }
    virtual void                        SetAutoExposure( bool boEnable );
    virtual void                        SetAutoExposureUpperLimit( double exposureMax );
    virtual void                        SetAutoGain( bool boEnable );
    virtual void                        SetGamma(  bool  boOn  )
    {
        HasNativeGamma() ? LoggedWriteProperty( pAnC_->mvGammaEnable, boOn ? bTrue : bFalse ) : LoggedWriteProperty( pIP_->LUTEnable, boOn ? bTrue : bFalse );
    }
    virtual void                        SetAutoWhiteBalance( bool boEnable )
    {
        LoggedWriteProperty( pAnC_->balanceWhiteAuto, std::string( boEnable ? "Continuous" : "Off" ) );
    }
    virtual void                        SetFrameRateEnable( bool boOn )
    {
        LoggedWriteProperty( pAcC_->mvAcquisitionFrameRateEnable, std::string( boOn ? "On" : "Off" ) );
    }
    virtual void                        SetPixelClock( const std::string& frequency )
    {
        LoggedWriteProperty( pDeC_->mvDeviceClockFrequency, frequency );
    }
    virtual void                        SetPixelFormat( const std::string& format )
    {
        LoggedWriteProperty( pIFC_->pixelFormat, format );
    }
    virtual void                        SetupExposureControls( void );
    virtual void                        SetupGainControls( void );
    virtual void                        SetupBlackLevelControls( void );
    virtual void                        SetupWhiteBalanceControls( void );
    virtual void                        SetupFrameRateControls( void );
    virtual void                        SetupUnifiedGainData( void );
    virtual void                        SetupUnifiedBlackLevelData( void );
    virtual void                        SetWhiteBalance( TWhiteBalanceChannel channel, double value );
    virtual bool                        Supports12BitPixelFormats( void ) const;
    virtual void                        TryToReadFrameRate( double& value );
    virtual void                        WriteExposureFeatures( const DeviceSettings& devSettings );
    virtual void                        WriteGainFeatures( const DeviceSettings& devSettings );
    virtual void                        WriteWhiteBalanceFeatures( const DeviceSettings& devSettings );
    virtual void                        SetFrameRate( double value );
private:
    GenICam::DeviceControl*                 pDeC_;
    GenICam::AcquisitionControl*            pAcC_;
    GenICam::AnalogControl*                 pAnC_;
    GenICam::ImageFormatControl*            pIFC_;
    GenICam::ColorTransformationControl*    pCTC_;
    GenICam::UserSetControl*                pUSC_;

    std::vector<std::vector<double> >       initialDeviceColorCorrectionMatrix_;
};

//-----------------------------------------------------------------------------
class WizardQuickSetupDeviceSpecific : public WizardQuickSetup
//-----------------------------------------------------------------------------
{
public:
    explicit                            WizardQuickSetupDeviceSpecific( PropViewFrame* pParent, const wxString& title, Device* pDev, FunctionInterface* pFI, bool boAutoConfigureOnStart );
protected:
    virtual void                        CreateInterfaceLayoutSpecificControls( Device* pDev );
    virtual void                        DeleteInterfaceLayoutSpecificControls( void );
    virtual double                      DetermineMaxFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats );
    virtual double                      DetermineOptimalHQFrameRateAtCurrentAOI( bool checkAllSupportedPixelFormats );
    virtual void                        DoConfigureColorCorrection( bool boActive );
    virtual double                      DoReadSaturationData( void ) const;
    virtual double                      DoReadUnifiedBlackLevel( void ) const;
    virtual double                      DoReadUnifiedGain( void ) const;
    virtual std::string                 DoWriteSaturationData( double saturation );
    virtual void                        DoWriteUnifiedGain( double value ) const;
    virtual void                        DoWriteUnifiedBlackLevelData( double value ) const;
    virtual double                      GetAutoExposureUpperLimit( void ) const;
    virtual bool                        GetCCMState( void ) const
    {
        return GetDriverCCMState();
    }
    virtual bool                        GetExposureOverlapped() const
    {
        return pCSBF_->exposeMode.read() == cemOverlapped;
    }
    virtual std::string                 GetPixelClock( void ) const;
    virtual std::string                 GetPixelFormat( void ) const;
    virtual double                      GetExposureTime( void ) const
    {
        return pCSBF_->expose_us.read();
    }
    virtual bool                        HasAEC( void ) const
    {
        return pCSBF_->autoExposeControl.isValid();
    }
    virtual bool                        HasAGC( void )
    {
        return pCSBF_->autoGainControl.isValid();
    }
    virtual bool                        HasAWB( void ) const
    {
        return false;
    }
    virtual bool                        HasMaximumFrameRate( void ) const
    {
        return false;
    }
    virtual bool                        HasBlackLevel( void ) const
    {
        return pIP_->gainOffsetKneeEnable.isValid();
    }
    virtual bool                        HasColorFormat( void ) const
    {
        return currentProductString_[currentProductString_.length() - 1] == 'C';
    }
    virtual bool                        HasFactoryDefault( void ) const
    {
        return true;
    }
    virtual bool                        HasFrameRateEnable( void ) const
    {
        return false;
    }
    virtual bool                        HasGain( void ) const
    {
        return pCSBF_->gain_dB.isValid();
    }
    virtual bool                        HasUnifiedGain( void ) const
    {
        return false;
    }
    virtual bool                        HasNativeGamma( void ) const
    {
        return false;
    }
    virtual bool                        HasNativeColorCorrection( void ) const
    {
        return false;
    }
    virtual void                        InitializeBlackLevelParameters( void );
    virtual bool                        IsGammaActive( void )
    {
        return IsDriverBasedGammaActive();
    }
    virtual void                        QueryInterfaceLayoutSpecificSettings( DeviceSettings& devSettings );
    virtual void                        RestoreFactoryDefault( void );
    virtual void                        SetOptimalPixelFormatColor( TOptimizationLevel optimizationLevel );
    virtual void                        SetOptimalPixelFormatGray( TOptimizationLevel optimizationLevely );
    virtual void                        SelectOptimalAutoExposureSettings( bool boHighQuality );
    virtual void                        SelectOptimalPixelClock( TOptimizationLevel optimizationLevel );
    virtual void                        SetExposureTime( double value )
    {
        LoggedWriteProperty( pCSBF_->expose_us, static_cast<int>( value ) );
    }
    virtual void                        SetAutoExposure( bool boEnable );
    virtual void                        SetAutoExposureUpperLimit( double exposureMax );
    virtual void                        SetAutoGain( bool boEnable );
    virtual void                        SetPixelClock( const std::string& frequency )
    {
        LoggedWriteProperty( pCSBF_->pixelClock_KHz, frequency );
    }
    virtual void                        SetPixelFormat( const std::string& format )
    {
        LoggedWriteProperty( pCSBF_->pixelFormat, format );
    }
    virtual void                        SetupExposureControls( void );
    virtual void                        SetExposureOverlapped( bool boEnable );
    virtual void                        SetupGainControls( void );
    virtual void                        SetGamma(  bool  boOn  )
    {
        LoggedWriteProperty( pIP_->LUTEnable, boOn ? bTrue : bFalse );
    }
    virtual void                        SetupBlackLevelControls( void );
    virtual void                        SetupUnifiedGainData( void );
    virtual void                        SetupUnifiedBlackLevelData( void );
    virtual bool                        Supports12BitPixelFormats( void ) const
    {
        return false;
    }
    virtual void                        TryToReadFrameRate( double& /*value*/ )
    {
        //No auto-frame rate for devices with deviceSpecific interface!
        UpdateFrameRateWarningBitmap( false );
    }
    virtual void                        WriteExposureFeatures( const DeviceSettings& devSettings );
    virtual void                        WriteGainFeatures( const DeviceSettings& devSettings );
private:
    CameraSettingsBlueFOX*              pCSBF_;
};

#endif // WizardQuickSetupH
