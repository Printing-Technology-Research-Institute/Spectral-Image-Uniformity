//-----------------------------------------------------------------------------
#ifndef WizardMultiCoreAcquisitionH
#define WizardMultiCoreAcquisitionH WizardMultiCoreAcquisitionH
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include "ValuesFromUserDlg.h"

class PropViewFrame;
class wxBitmapToggleButton;
class wxSpinControl;

//-----------------------------------------------------------------------------
class WizardMultiCoreAcquisition : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
private:
    mvIMPACT::acquire::Device* pDev_;
    mvIMPACT::acquire::FunctionInterface* pFI_;
    mvIMPACT::acquire::Info info_;
    mvIMPACT::acquire::GenICam::DataStreamModule dsm_;
    PropViewFrame* pParent_;
    TBoolean boMultiCoreAcquisitionEnableStateOnEnter_;
    bool boAcquisitionStateOnCancel_;

    wxBitmap* pToggleButtonUndefined_;
    wxBitmap* pToggleButtonChecked_;
    wxBitmap* pToggleButtonUnchecked_;
    wxBitmap* pToggleButtonUnavailable_;
    wxBitmap* pToggleButtonCheckedUndefined_;
    wxBitmap* pToggleButtonUncheckedUndefined_;

    std::vector<wxBitmapToggleButton*> CPUCores_;
    wxSpinCtrl* pSCFirstCoreIndex_;
    wxSpinCtrl* pSCCoreCount_;
    wxSpinCtrl* pSCCoreSwitchInterval_;
    wxTextCtrl* pTCBaseCore_;

    //-----------------------------------------------------------------------------
    enum TWidgetIDs_MultiCoreAcquisition
    //-----------------------------------------------------------------------------
    {
        widMainFrame = widFirst,
        widSCFirstCoreIndex,
        widSCCoreCount,
        widSCCoreSwitchInterval
    };

    void ApplyCurrentSettings( void );
    void CloseDlg( int result );
    void CreateGUI( void );
    virtual void OnBtnCancel( wxCommandEvent& )
    {
        CloseDlg( wxID_CANCEL );
        Hide();
    }
    virtual void OnBtnOk( wxCommandEvent& )
    {
        CloseDlg( wxID_OK );
        Hide();
    }
    void OnClose( wxCloseEvent& e );
    void OnSCChanged( wxSpinEvent& )
    {
        ApplyCurrentSettings();
    }
    void OnSCTextChanged( wxCommandEvent& )
    {
        ApplyCurrentSettings();
    }
    void SetupDlgControls( void );
public:
    explicit WizardMultiCoreAcquisition( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, mvIMPACT::acquire::FunctionInterface* pFI );
    ~WizardMultiCoreAcquisition();
    void RefreshControls( void );
    void SetAcquisitionStateOnCancel( bool boAcquisitionStateOnCancel )
    {
        boAcquisitionStateOnCancel_ = boAcquisitionStateOnCancel;
    }
    void SetMultiCoreAcquisitionEnableStateOnEnter( TBoolean boActive )
    {
        boMultiCoreAcquisitionEnableStateOnEnter_ = boActive;
    }
};

#endif // WizardMultiCoreAcquisitionH
