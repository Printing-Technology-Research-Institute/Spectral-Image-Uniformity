//-----------------------------------------------------------------------------
#ifndef PropGridFrameBaseH
#define PropGridFrameBaseH PropGridFrameBaseH
//-----------------------------------------------------------------------------
#include <apps/Common/wxAbstraction.h>
#include <map>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include "PropTree.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/wx.h>
#include <wx/stopwatch.h>
#include <apps/Common/wxIncludeEpilogue.h>

class MethodObject;
class PropData;
class wxPropertyGrid;
class wxPropertyGridEvent;

//-----------------------------------------------------------------------------
TImageFileFormat GetImageFileFormatFromFileFilterIndex( const int filterIndex );
TImageDestinationPixelFormat GetPixelFormatFromFileFilterIndex( const int filterIndex );
TVideoCodec GetVideoCodecFromFileFilterIndex( const int filterIndex );
int GetImageFileFormatIndexFromFileFormat( const TImageFileFormat filterFormat );
wxString GetStringRepresentationFromImageFileFormat( TImageFileFormat fileFormat );
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
class MethodExecutionData
//-----------------------------------------------------------------------------
{
    wxPGProperty* pGridProp_;
    MethodObject* pMethod_;
    int callResult_;
    wxString errorString_;
    wxString callResultMessage_;
public:
    explicit MethodExecutionData( wxPGProperty* pGridProp, MethodObject* pMeth );
    void operator()( void );
    int GetCallResult( void ) const
    {
        return callResult_;
    }
    wxString GetErrorString( void ) const
    {
        return errorString_;
    }
    wxString GetResultMessage( void ) const
    {
        return callResultMessage_;
    }
    MethodObject* GetMethod( void ) const
    {
        return pMethod_;
    }
};

//-----------------------------------------------------------------------------
class PropGridFrameBase : public wxFrame
//-----------------------------------------------------------------------------
{
    DECLARE_EVENT_TABLE()
private:
    wxTimer                             m_PropertyGridUpdateTimer;
protected:
    wxPropertyGridManager*              m_pPropGridManager;
    wxStopWatch                         m_stopWatch;

    //-----------------------------------------------------------------------------
    enum TTimerEvent
    //-----------------------------------------------------------------------------
    {
        teListUpdate,
        teCUSTOM_ID
    };
    //-----------------------------------------------------------------------------
    // IDs for the controls and the menu commands
    enum TMenuItemBase
    //-----------------------------------------------------------------------------
    {
        widPropertyGridManager = 1,
        miPopUpMethExec,
        miPopUpMethChangeParameterDelimiter,
        miPopUpPropForceRefresh,
        miPopUpPropCopyToClipboard,
        miPopUpPropSetFromClipboard,
        miPopUpPropRestoreDefault,
        miPopUpPropReadFromFile,
        miPopUpPropWriteToFile,
        miPopUpPropAttachCallback,
        miPopUpPropDetachCallback,
        miPopUpPropPlotInFeatureValueVsTimePlot,
        miPopUpPropAppendValue,
        miPopUpPropDeleteValue,
        miPopUpPropSetMultiple,
        miPopUpPropSetMultiple_FixedValue,
        miPopUpPropSetMultiple_FromToRange,
        miPopUpDetailedFeatureInfo,
        miPopUpWizard,
        mibLAST
    };
    //-----------------------------------------------------------------------------
    enum
    //-----------------------------------------------------------------------------
    {
        DEFAULT_PROP_GRID_UPDATE_PERIOD = 750
    };
    virtual void            AppendCustomPropGridExecutionErrorMessage( wxString& /*msg*/ ) const {}
    void                    ConfigureToolTipsForPropertyGrids( const bool boEnable );
    virtual void            DoExecuteMethod( wxPGProperty* pProp, MethodObject* pMethod )
    {
        ExecuteMethodSynchronously( pProp, pMethod );
    }
    void                    ExecuteMethodSynchronously( wxPGProperty* pProp, MethodObject* pMethod );
    void                    ExpandPropertyRecursively( wxPGProperty* id );
    void                    ExpandPropertyTreeToDeviceSettings( void );
    virtual bool            FeatureChangedCallbacksSupported( void ) const
    {
        return false;
    }
    virtual bool            FeatureVersusTimePlotSupported( void ) const
    {
        return false;
    }
    virtual bool            FeatureHasChangedCallback( mvIMPACT::acquire::Component ) const
    {
        return false;
    }
    virtual bool            FeatureSupportsWizard( mvIMPACT::acquire::Component ) const
    {
        return false;
    }
    wxPropertyGrid*         GetPropertyGrid( void )
    {
        return m_pPropGridManager->GetGrid();
    }
    wxPropertyGridPage*     GetSelectedPropertyGridPage( void )
    {
        return m_pPropGridManager->GetPage( m_pPropGridManager->GetSelectedPage() );
    }
    virtual EDisplayFlags   GetDisplayFlags( void ) const
    {
        return dfNone;
    }
    void                    OnChangePropGridMethodParameterDelimiter( wxCommandEvent& e );
    void                    OnExecutePropGridMethod( wxCommandEvent& e );
    void                    OnPopUpDetailedFeatureInfo( wxCommandEvent& e );
    void                    OnPopUpPropForceRefresh( wxCommandEvent& e );
    void                    OnPopUpPropCopyToClipboard( wxCommandEvent& e );
    void                    OnPopUpPropSetFromClipboard( wxCommandEvent& e );
    void                    OnPopUpPropRestoreDefault( wxCommandEvent& e );
    virtual void            OnPopUpPropReadFromFile( wxCommandEvent& ) {}
    virtual void            OnPopUpPropWriteToFile( wxCommandEvent& ) {}
    virtual void            OnPopUpPropAttachCallback( wxCommandEvent& ) {}
    virtual void            OnPopUpPropDetachCallback( wxCommandEvent& ) {}
    virtual void            OnPopUpPropPlotInFeatureValueVsTimePlot( wxCommandEvent& ) {}
    void                    OnPopUpPropAppendValue( wxCommandEvent& e );
    void                    OnPopUpPropDeleteValue( wxCommandEvent& e );
    void                    OnPopUpPropSetMultiple_FixedValue( wxCommandEvent& e );
    void                    OnPopUpPropSetMultiple_FromToRange( wxCommandEvent& e );
    virtual void            OnPopUpWizard( wxCommandEvent& /*e*/ ) {};
    void                    OnPropertyChanged( wxPropertyGridEvent& e );
    virtual void            OnPropertyChangedCustom( wxPropertyGridEvent& ) {}
    virtual void            OnPropertyGridTimer( void ) = 0;
    void                    OnPropertyRightClicked( wxPropertyGridEvent& e );
    virtual void            OnPropertyRightClickedCustom( wxPropertyGridEvent& ) {}
    void                    OnPropertyGridItemExpandedOrCollapsed( wxPropertyGridEvent& e );
    void                    OnPropertySelected( wxPropertyGridEvent& e )
    {
        m_pPropGridManager->GetGrid()->SetFocus();
        OnPropertySelectedCustom( e );
    }
    virtual void            OnPropertySelectedCustom( wxPropertyGridEvent& ) {}
    void                    OnTimer( wxTimerEvent& e );
    virtual void            OnTimerCustom( wxTimerEvent& e );
    void                    ProcessMethodExecutionResults( const MethodExecutionData& data );
    virtual bool            ShowFeatureChangeTimeConsumption( void ) const
    {
        return false;
    }
    virtual bool            ShowPropGridMethodExecutionErrors( void ) const
    {
        return true;
    }
    virtual bool            ShowStatusBarTooltip( void ) const
    {
        return false;
    }
public:
    explicit                PropGridFrameBase( wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size );
    virtual                ~PropGridFrameBase();
    bool                    SelectPropertyInPropertyGrid( PropData* pPropData );
    void                    StartPropertyGridUpdateTimer( void )
    {
        m_PropertyGridUpdateTimer.Start( DEFAULT_PROP_GRID_UPDATE_PERIOD );
    }
    void                    StopPropertyGridUpdateTimer( void )
    {
        StopTimer( m_PropertyGridUpdateTimer );
    }
    virtual bool            WizardAOISupported( void ) const
    {
        return false;
    }
    virtual void            WriteErrorMessage( const wxString& msg ) = 0;
    virtual void            WriteLogMessage( const wxString& msg, const wxTextAttr& style = wxTextAttr( *wxBLACK ) ) = 0;
};

//-----------------------------------------------------------------------------
class PropGridUpdateTimerSuspendScope
//-----------------------------------------------------------------------------
{
    PropGridFrameBase* p_;
public:
    explicit PropGridUpdateTimerSuspendScope( PropGridFrameBase* p ) : p_( p )
    {
        p_->StopPropertyGridUpdateTimer();
    }
    ~PropGridUpdateTimerSuspendScope()
    {
        p_->StartPropertyGridUpdateTimer();
    }
};

#endif // PropGridFrameBaseH
