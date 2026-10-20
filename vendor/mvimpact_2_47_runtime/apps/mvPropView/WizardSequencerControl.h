//-----------------------------------------------------------------------------
#ifndef WizardSequencerControlH
#define WizardSequencerControlH WizardSequencerControlH
//-----------------------------------------------------------------------------
#include <map>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include "PropViewFrame.h"
#include <set>
#include "ValuesFromUserDlg.h"

//-----------------------------------------------------------------------------
class WizardSequencerControl : public OkAndCancelDlg
//-----------------------------------------------------------------------------
{
public:
    explicit WizardSequencerControl( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, size_t displayCount, SequencerSetToDisplayMap& setToDisplayTable );
    virtual ~WizardSequencerControl();

    const SequencerSetToDisplayMap& GetSetToDisplayTable( void );

private:
    //-----------------------------------------------------------------------------
    /// \brief GUI elements for a single sequencer set
    struct SequencerSetControls
    //-----------------------------------------------------------------------------
    {
        wxPanel* pSetPanel;
        wxPropertyGrid* pSetPropertyGrid;
        std::vector<wxSpinCtrl*> pSequencerSetNextSCs;
        std::vector<wxComboBox*> pSequencerTriggerSourceCBs;
        std::vector<wxComboBox*> pSequencerTriggerActivationCBs;
        wxComboBox* pDisplayToUseCB;
    };
    typedef std::map<int64_type, SequencerSetControls*> SetNrToControlsMap;
    typedef std::vector<std::pair<HOBJ, wxCheckBox*> > SequencerFeatureContainer;

    //-----------------------------------------------------------------------------
    enum TWidgetIDs_SequencerControl
    //-----------------------------------------------------------------------------
    {
        widMainFrame = wxID_HIGHEST,
        widDisplayComboBox,
        widNextSetSpinControl,
        widSequencerTriggerSourceComboBox,
        widSequencerTriggerActivationComboBox,
        widSequencerSetsNotebook,
        widSettingsCheckBox,
        widAddAnotherSetButton,
        widRemoveSelectedSetButton,
        widSetSelectedSetAsStartSetButton,
        widAutoAssignSetsToDisplaysButton,
        widStaticBitmapWarning
    };

    //-----------------------------------------------------------------------------
    enum TSequencerWizardError
    //-----------------------------------------------------------------------------
    {
        sweReferenceToInactiveSet,
        sweSetsNotBeingReferenced,
        sweStartingSetNotActive
    };

    //-----------------------------------------------------------------------------
    /// \brief Necessary Information for Sequencer Set Management Errors
    struct SequencerErrorInfo
    //-----------------------------------------------------------------------------
    {
        int64_type setNumber;
        TSequencerWizardError errorType;
        int64_type nextSet;
        explicit SequencerErrorInfo( int64_type nr, TSequencerWizardError error, int64_type nextNr ) : setNumber( nr ), errorType( error ), nextSet( nextNr ) {}
        bool operator==( const SequencerErrorInfo& rhs )
        {
            return ( setNumber == rhs.setNumber ) &&
                   ( errorType == rhs.errorType ) &&
                   ( nextSet == rhs.nextSet );
        }
    };

    PropViewFrame*                      pParent_;
    GenICam::SequencerControl           sc_;
    mutable SequencerSetToDisplayMap    setToDisplayTable_;
    std::vector<SequencerErrorInfo>     currentErrors_;
    SequencerFeatureContainer           sequencerFeatures_;
    SetNrToControlsMap                  sequencerSetControlsMap_;
    int64_type                          startingSequencerSet_;
    int64_type                          maxSequencerSetNumber_;
    size_t                              displayCount_;
    bool                                boCounterDurationCapability_;
    bool                                boUseSpecialCounterDurationBehaviour_;

    wxNotebook*                         pSetSettingsNotebook_;
    wxStaticBitmap*                     pStaticBitmapWarning_;
    wxStaticText*                       pInfoText_;
    wxStaticText*                       pStartingSetText_;

    static const wxString               s_displayStringPrefix_;
    static const wxString               s_sequencerSetStringPrefix_;

    void                                AnalyzeSequencerSetFeatures( wxPanel* pMainPanel );
    void                                AddError( const SequencerErrorInfo& errorInfo );
    void                                CheckIfActiveSetsAreReferencedAtLeastOnce( void );
    void                                CheckIfReferencedSetsAreActive( void );
    void                                CheckIfStartingSetIsAnActiveSequencerSet( int64_type setNumber );
    void                                ComposeSetPanel( size_t displayCount, int64_type setCount, int64_type setNumber, bool boInsertOnFirstFreePosition );
    wxComboBox*                         CreateComboBoxFromProperty( SequencerSetControls* pSequencerSet, std::vector<wxComboBox*>& container, wxWindowID id, PropertyI64 prop );
    void                                CreateGUIElementsForPathData( SequencerSetControls* pSequencerSet, wxFlexGridSizer* pSequencerPathControlsSizer, int64_type pathIndex );
    wxSpinCtrl*                         CreateSequencerSetNextControl( SequencerSetControls* pSequencerSet );
    void                                DoConsistencyChecks( void );
    SetNrToControlsMap::const_iterator  GetSelectedSequencerSetControls( void ) const
    {
        return GetSequencerSetControlsFromPageIndex( pSetSettingsNotebook_->GetSelection() );
    }
    int64_type                          GetSelectedSequencerSetNr( void ) const
    {
        SetNrToControlsMap::const_iterator it = GetSelectedSequencerSetControls();
        return ( it == sequencerSetControlsMap_.end() ) ? -1LL : it->first;
    }
    SetNrToControlsMap::const_iterator  GetSequencerSetControlsFromPageIndex( size_t pageNr ) const;
    int64_type                          GetSequencerSetNrFromPageIndex( size_t pageNr ) const;
    void                                HandleAutomaticNextSetSettingsOnInsert( int64_type tabPageNumber, int64_type setNumber );
    void                                HandleAutomaticNextSetSettingsOnRemove( int64_type tabPageNumber, int64_type setNumber );
    void                                InquireCurrentlyActiveSequencerSets( std::set<int64_type>& activeSequencerSets );
    void                                LoadSequencerSet( int64_type setNumber );
    void                                LoadSequencerSets( void );
    void                                RemoveError( const SequencerErrorInfo& errorInfo );
    void                                RemoveSetPanel( void );
    void                                ReadPropertiesOfSequencerSet( SetNrToControlsMap::iterator& it );
    void                                ReadSequencerPathDataFromDeviceToGUI( SequencerSetControls* pControls, int64_type pathIndex );
    void                                RefreshComboBoxFromProperty( wxComboBox* pCB, PropertyI64 prop );
    void                                RefreshCurrentPropertyGrid( void );
    bool                                SaveSequencerSets( void );
    void                                SaveSequencerSetsAndEnd( int returnCode );
    void                                SelectSequencerSetAndCallMethod( int64_type setNr, Method& meth );
    void                                UpdateError( const wxString& msg, const wxBitmap& icon ) const;
    void                                UpdateErrors( void ) const;
    void                                UpdatePropertyGrid( SequencerSetControls* pSelectedSequencerSet );
    void                                UpdateSequencerTriggerSource( bool boUseCounter1EndRatherThanExposureEnd );
    void                                WriteSequencerPathDataFromGUIToDevice( SequencerSetControls* pControls, int64_type pathIndex );

    //----------------------------------GUI Functions----------------------------------
    void                                OnBtnApply( wxCommandEvent& )
    {
        SaveSequencerSetsAndEnd( wxID_APPLY );
    }
    void                                OnBtnCancel( wxCommandEvent& )
    {
        EndModal( wxID_CANCEL );
    }
    void                                OnBtnOk( wxCommandEvent& )
    {
        SaveSequencerSetsAndEnd( wxID_OK );
    }
    void                                OnBtnAddAnotherSet( wxCommandEvent& e );
    void                                OnBtnRemoveSelectedSet( wxCommandEvent& e );
    void                                OnBtnSetSelectedSetAsStartSet( wxCommandEvent& e );
    void                                OnBtnAutoAssignSetsToDisplays( wxCommandEvent& e );
    void                                OnNBSequencerSetsPageChanged( wxNotebookEvent& e );
    void                                OnNextSetSpinControlChanged( wxSpinEvent& )
    {
        DoConsistencyChecks();
    }
    void                                OnPropertyChanged( wxPropertyGridEvent& e );
    void                                OnSettingsCheckBoxChecked( wxCommandEvent& e );
    void                                OnTriggerSourceTextChanged( wxCommandEvent& e );
    void                                OnTriggerActivationTextChanged( wxCommandEvent& e );

    DECLARE_EVENT_TABLE()
};

#endif // WizardSequencerControlH
