#include <algorithm>
#include <apps/Common/wxAbstraction.h>
#include <common/STLHelper.h>
#include "GlobalDataStorage.h"
#include "PropData.h"
#include "SpinEditorDouble.h"
#include "WizardSequencerControl.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/combobox.h>
#include <wx/platinfo.h>
#include <wx/propgrid/advprops.h>
#include <wx/spinctrl.h>
#include <apps/Common/wxIncludeEpilogue.h>

using namespace std;

//=============================================================================
//============== Implementation WizardSequencerControl ========================
//=============================================================================
BEGIN_EVENT_TABLE( WizardSequencerControl, OkAndCancelDlg )
    EVT_BUTTON( widAddAnotherSetButton, WizardSequencerControl::OnBtnAddAnotherSet )
    EVT_BUTTON( widRemoveSelectedSetButton, WizardSequencerControl::OnBtnRemoveSelectedSet )
    EVT_BUTTON( widSetSelectedSetAsStartSetButton, WizardSequencerControl::OnBtnSetSelectedSetAsStartSet )
    EVT_BUTTON( widAutoAssignSetsToDisplaysButton, WizardSequencerControl::OnBtnAutoAssignSetsToDisplays )
    EVT_CHECKBOX( widSettingsCheckBox, WizardSequencerControl::OnSettingsCheckBoxChecked )
    EVT_NOTEBOOK_PAGE_CHANGED( widSequencerSetsNotebook, WizardSequencerControl::OnNBSequencerSetsPageChanged )
    EVT_SPINCTRL( widNextSetSpinControl, WizardSequencerControl::OnNextSetSpinControlChanged )
    EVT_TEXT( widSequencerTriggerSourceComboBox, WizardSequencerControl::OnTriggerSourceTextChanged )
    EVT_TEXT( widSequencerTriggerActivationComboBox, WizardSequencerControl::OnTriggerActivationTextChanged )
END_EVENT_TABLE()

const wxString WizardSequencerControl::s_displayStringPrefix_( wxT( "Display " ) );
const wxString WizardSequencerControl::s_sequencerSetStringPrefix_( wxT( "Set " ) );

//-----------------------------------------------------------------------------
WizardSequencerControl::WizardSequencerControl( PropViewFrame* pParent, const wxString& title, mvIMPACT::acquire::Device* pDev, size_t displayCount, SequencerSetToDisplayMap& setToDisplayTable )
    : OkAndCancelDlg( pParent, widMainFrame, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMINIMIZE_BOX ),
      pParent_( pParent ), sc_( pDev ), setToDisplayTable_( setToDisplayTable ), currentErrors_(), sequencerFeatures_(),
      sequencerSetControlsMap_(), startingSequencerSet_( 0 ),
      maxSequencerSetNumber_( 0 ), displayCount_( displayCount ), boCounterDurationCapability_( false ), boUseSpecialCounterDurationBehaviour_( false ),
      pSetSettingsNotebook_( 0 ), pStaticBitmapWarning_( 0 ), pInfoText_( 0 ), pStartingSetText_( 0 )
//-----------------------------------------------------------------------------
{
    /*
        |-------------------------------------|
        | pTopDownSizer                       |
        |                spacer               |
        | |---------------------------------| |
        | | pSettingsSizer                  | |
        | | |--------| |------------------| | |
        | | | Global | |   Set-Specific   | | |
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

    AnalyzeSequencerSetFeatures( pMainPanel );

    // add a check box for each "sequenceable" property
    const int minSpace = ( ( wxPlatformInfo().GetOperatingSystemId() & wxOS_WINDOWS ) != 0 ) ? 2 : 1;
    wxStaticBoxSizer* pFeaturesSizer = new wxStaticBoxSizer( wxVERTICAL, pMainPanel, wxT( "Sequencer Properties" ) );
    pFeaturesSizer->AddSpacer( 4 * minSpace );
    const SequencerFeatureContainer::size_type sequencerFeatureCount = sequencerFeatures_.size();
    for( SequencerFeatureContainer::size_type i = 0; i < sequencerFeatureCount; i++ )
    {
        try
        {
            if( sc_.sequencerFeatureSelector.isValid() )
            {
                sc_.sequencerFeatureSelector.writeS( string( sequencerFeatures_[i].second->GetLabel().mb_str() ) );
            }
            wxCheckBox* pCB = sequencerFeatures_[i].second;
            pCB->SetValue( sc_.sequencerFeatureEnable.read() == bTrue );
            pCB->Enable( sc_.sequencerFeatureEnable.isWriteable() );
            // special handling of the CounterDuration Feature, extra Information by adding a tool tip
            if( pCB->GetLabel().IsSameAs( "CounterDuration" ) )
            {
                pCB->SetToolTip( wxT( "The number of images to be taken with each sequencer set.\nThe counter end events actually using this feature can be found in the list of valid trigger sources of each sequencer path." ) );
            }
            pFeaturesSizer->AddSpacer( 2 * minSpace );
            pFeaturesSizer->Add( pCB );
        }
        catch( const ImpactAcquireException& e )
        {
            wxMessageBox( wxString::Format( wxT( "An exception was raised: %s(%s)" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ), wxT( "Internal Problem" ), wxICON_EXCLAMATION );
        }
    }

    pSetSettingsNotebook_ = new wxNotebook( pMainPanel, widSequencerSetsNotebook, wxDefaultPosition, wxDefaultSize, wxNB_TOP );
    int64_type setCount = 0;
    set<int64_type> activeSequencerSets;
    InquireCurrentlyActiveSequencerSets( activeSequencerSets );
    for( set<int64_type>::iterator it = activeSequencerSets.begin(); it != activeSequencerSets.end(); it++ )
    {
        ComposeSetPanel( displayCount, setCount, *it, false );
        sc_.sequencerSetSelector.write( sc_.sequencerSetNext.read() );
        ++setCount;
    }

    pStaticBitmapWarning_ = new wxStaticBitmap( pMainPanel, widStaticBitmapWarning, *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Empty ), wxDefaultPosition, wxDefaultSize, 0, wxT( "" ) );
    pInfoText_ = new wxStaticText( pMainPanel, wxID_ANY, wxT( "" ), wxDefaultPosition, wxSize( 360, WXGRID_DEFAULT_ROW_HEIGHT ), wxST_ELLIPSIZE_MIDDLE );
    pStartingSetText_ = new wxStaticText( pMainPanel, wxID_ANY, wxString::Format( wxT( "Current Starting Set: %d" ), static_cast<int>( startingSequencerSet_ ) ) );
    pBtnOk_ = new wxButton( pMainPanel, widBtnOk, wxT( "&Save and Run" ) );
    pBtnApply_ = new wxButton( pMainPanel, widBtnApply, wxT( "&Save" ) );
    pBtnCancel_ = new wxButton( pMainPanel, widBtnCancel, wxT( "&Cancel" ) );

    // putting it all together
    wxStaticBoxSizer* pSetManagementSizer = new wxStaticBoxSizer( wxVERTICAL, pMainPanel, wxT( "Set Management" ) );
    pSetManagementSizer->Add( new wxButton( pMainPanel, widAddAnotherSetButton, wxT( "Add Another Set" ) ), wxSizerFlags().Expand().Border( wxALL, 2 ) );
    pSetManagementSizer->Add( new wxButton( pMainPanel, widRemoveSelectedSetButton, wxT( "Remove Selected Set" ) ), wxSizerFlags().Expand().Border( wxALL, 2 ) );
    pSetManagementSizer->Add( new wxButton( pMainPanel, widSetSelectedSetAsStartSetButton, wxT( "Set Selected Set As Starting Set" ) ), wxSizerFlags().Expand().Border( wxALL, 2 ) );
    pSetManagementSizer->Add( pStartingSetText_, wxSizerFlags().Align( wxALIGN_CENTER_HORIZONTAL ) );

    wxStaticBoxSizer* pDisplayManagementSizer = new wxStaticBoxSizer( wxVERTICAL, pMainPanel, wxT( "Display Management" ) );
    wxButton* pAutoAssignDisplaysToSetsButton = new wxButton( pMainPanel, widAutoAssignSetsToDisplaysButton, wxT( "Auto-Assign Displays To Sets" ) );
    pAutoAssignDisplaysToSetsButton->SetToolTip( wxT( "Automatically Creates A Display Matrix\nBig Enough To Accommodate All Sets\nAnd Assigns Each Set To A Display" ) );
    pDisplayManagementSizer->Add( pAutoAssignDisplaysToSetsButton, wxSizerFlags().Expand().Border( wxALL, 2 ) );

    wxBoxSizer* pGlobalSettingsSizer = new wxBoxSizer( wxVERTICAL );
    pGlobalSettingsSizer->Add( pFeaturesSizer, wxSizerFlags( 6 ).Expand() );
    pGlobalSettingsSizer->Add( pSetManagementSizer, wxSizerFlags( 3 ).Expand() );
    pGlobalSettingsSizer->Add( pDisplayManagementSizer, wxSizerFlags( 2 ).Expand() );

    wxBoxSizer* pSettingsSizer = new wxBoxSizer( wxHORIZONTAL );
    pSettingsSizer->AddSpacer( 5 );
    pSettingsSizer->Add( pGlobalSettingsSizer, wxSizerFlags( 2 ).Expand() );
    pSettingsSizer->AddSpacer( 2 * minSpace );
    pSettingsSizer->Add( pSetSettingsNotebook_, wxSizerFlags( 5 ).Expand() );
    pSettingsSizer->AddSpacer( 2 * minSpace );

    // customizing the last line of buttons
    wxBoxSizer* pButtonSizer = new wxBoxSizer( wxHORIZONTAL );
    pButtonSizer->Add( pStaticBitmapWarning_, wxSizerFlags().Border( wxALL, 7 ) );
    wxBoxSizer* pSmallInfoTextAlignmentSizer = new wxBoxSizer( wxVERTICAL );
    pSmallInfoTextAlignmentSizer->AddSpacer( 15 );
    wxFont font = pInfoText_->GetFont();
    font.SetWeight( wxFONTWEIGHT_BOLD );
    pInfoText_->SetFont( font );
    pInfoText_->SetForegroundColour( wxColour( 255, 0, 0 ) );
    pSmallInfoTextAlignmentSizer->Add( pInfoText_ );
    pButtonSizer->Add( pSmallInfoTextAlignmentSizer );
    pButtonSizer->AddStretchSpacer( 10 );
    pButtonSizer->Add( pBtnOk_, wxSizerFlags().Border( wxALL, 7 ) );
    pButtonSizer->Add( pBtnApply_, wxSizerFlags().Border( wxALL, 7 ) );
    pButtonSizer->Add( pBtnCancel_, wxSizerFlags().Border( wxALL, 7 ) );

    wxBoxSizer* pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pSettingsSizer, wxSizerFlags( 10 ).Expand() );
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pButtonSizer, wxSizerFlags().Expand() );

    pMainPanel->SetSizer( pTopDownSizer );
    pTopDownSizer->SetSizeHints( this );
    SetClientSize( pTopDownSizer->GetMinSize() );
    SetSizeHints( GetSize() );
    Center();

    LoadSequencerSets();
    SelectSequencerSetAndCallMethod( GetSelectedSequencerSetNr(), sc_.sequencerSetLoad );
    RefreshCurrentPropertyGrid();
}

//-----------------------------------------------------------------------------
WizardSequencerControl::~WizardSequencerControl()
//-----------------------------------------------------------------------------
{
    for_each( sequencerSetControlsMap_.begin(), sequencerSetControlsMap_.end(), ptr_fun( DeleteSecond<const int64_type, SequencerSetControls*> ) );
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::AddError( const SequencerErrorInfo& errorInfo )
//-----------------------------------------------------------------------------
{
    RemoveError( errorInfo );
    currentErrors_.push_back( errorInfo );
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::AnalyzeSequencerSetFeatures( wxPanel* pMainPanel )
//-----------------------------------------------------------------------------
{
    // check for "sequenceable" properties supported by the device
    vector<string> selectableFeaturesForSequencer;
    if( sc_.sequencerFeatureSelector.isValid() && sc_.sequencerSetLoad.isValid() )
    {
        sc_.sequencerFeatureSelector.getTranslationDictStrings( selectableFeaturesForSequencer );
        const vector<string>::size_type selectableFeaturesForSequencerCount = selectableFeaturesForSequencer.size();
        ComponentLocator locator( sc_.sequencerSetSelector.parent().parent() );
        int64_type originalSelectorPosition = sc_.sequencerFeatureSelector.read();
        for( vector<string>::size_type i = 0; i < selectableFeaturesForSequencerCount; i++ )
        {
            sc_.sequencerFeatureSelector.writeS( selectableFeaturesForSequencer[i] );
            const HOBJ hObj = locator.findComponent( selectableFeaturesForSequencer[i] );
            pair<HOBJ, wxCheckBox*> entry( hObj, new wxCheckBox( pMainPanel, widSettingsCheckBox, ConvertedString( selectableFeaturesForSequencer[i] ).c_str() ) );
            sequencerFeatures_.push_back( entry );
        }
        sc_.sequencerFeatureSelector.write( originalSelectorPosition );
    }

    if( sc_.sequencerTriggerSource.isValid() && sc_.sequencerTriggerSource.isWriteable() )
    {
        vector<string> sequencerTriggerSourceValidValues;
        sc_.sequencerTriggerSource.getTranslationDictStrings( sequencerTriggerSourceValidValues );
        // Check for the 'CounterDuration' feature (relevant for the implementation for mv cameras)
        if( find( sequencerTriggerSourceValidValues.begin(), sequencerTriggerSourceValidValues.end(), "Counter1End" ) != sequencerTriggerSourceValidValues.end() &&
            find( sequencerTriggerSourceValidValues.begin(), sequencerTriggerSourceValidValues.end(), "ExposureEnd" ) != sequencerTriggerSourceValidValues.end() )
        {
            boCounterDurationCapability_ = true;
        }
        // The special handling of 'CounterDuration' only makes sense if there are either exactly 2 trigger sources 'Counter1End' and 'ExposureEnd'
        boUseSpecialCounterDurationBehaviour_ = ( sequencerTriggerSourceValidValues.size() == 2 ) && boCounterDurationCapability_;
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::CheckIfActiveSetsAreReferencedAtLeastOnce( void )
//-----------------------------------------------------------------------------
{
    const SetNrToControlsMap::const_iterator itSetsEND = sequencerSetControlsMap_.end();
    for( SetNrToControlsMap::iterator itActiveSet = sequencerSetControlsMap_.begin(); itActiveSet != itSetsEND; itActiveSet++ )
    {
        const int64_type currentSet = itActiveSet->first;
        bool boSetIsBeingReferenced = false;
        vector<int64_type> inactiveSetReferences;
        for( SetNrToControlsMap::iterator itReferencingSet = sequencerSetControlsMap_.begin(); ( itReferencingSet != itSetsEND ) && !boSetIsBeingReferenced; itReferencingSet++ )
        {
            const vector<wxSpinCtrl*>::size_type sequencerPathCount = itReferencingSet->second->pSequencerSetNextSCs.size();
            for( vector<wxSpinCtrl*>::size_type i = 0; ( i < sequencerPathCount ) && !boSetIsBeingReferenced; i++ )
            {
                const int64_type referencingSetNextSetSetting = itReferencingSet->second->pSequencerSetNextSCs[i]->GetValue();
                if( currentSet == referencingSetNextSetSetting )
                {
                    if( sc_.sequencerTriggerSource.isValid() && ( itReferencingSet->second->pSequencerTriggerSourceCBs.size() > i ) )
                    {
                        if( itReferencingSet->second->pSequencerTriggerSourceCBs[i]->GetValue() == wxT( "Off" ) )
                        {
                            inactiveSetReferences.push_back( itReferencingSet->first );
                        }
                        else
                        {
                            boSetIsBeingReferenced = true;
                        }
                    }
                    else
                    {
                        boSetIsBeingReferenced = true;
                    }
                }
            }
        }
        if( boSetIsBeingReferenced )
        {
            itActiveSet->second->pSetPanel->SetBackgroundColour( *wxWHITE );
            itActiveSet->second->pSetPanel->SetToolTip( 0 );
            RemoveError( SequencerErrorInfo( currentSet, sweSetsNotBeingReferenced, -1LL ) );
        }
        else if( !inactiveSetReferences.empty() )
        {
            itActiveSet->second->pSetPanel->SetBackgroundColour( wxColour( acRedPastel ) );
            wxString inactiveSetReferencesString;
            if( inactiveSetReferences.size() == 1 )
            {
                inactiveSetReferencesString.Append( wxString::Format( wxT( "another set(%d)" ), static_cast<int>( inactiveSetReferences[0] ) ) );
            }
            else
            {
                inactiveSetReferencesString.Append( wxT( "other sets(" ) );
                const vector<int64_type>::size_type inactiveSetReferenceCount = inactiveSetReferences.size();
                for( vector<int64_type>::size_type i = 0; i < inactiveSetReferenceCount; i++ )
                {
                    inactiveSetReferencesString.Append( wxString::Format( wxT( "%s%d" ), ( i == 0 ) ? wxT( "" ) : wxT( ", " ), static_cast<int>( inactiveSetReferences[i] ) ) );
                }
                inactiveSetReferencesString.Append( wxT( ")" ) );
            }
            itActiveSet->second->pSetPanel->SetToolTip( wxString::Format( wxT( "This set is set as 'NextSet' by %s however all references have 'SequencerTriggerSource' set to 'Off'!\nPlease review your sequencer sets configuration!" ), inactiveSetReferencesString.c_str() ) );
            AddError( SequencerErrorInfo( currentSet, sweSetsNotBeingReferenced, -1LL ) );
        }
        else
        {
            itActiveSet->second->pSetPanel->SetBackgroundColour( wxColour( acRedPastel ) );
            itActiveSet->second->pSetPanel->SetToolTip( wxT( "This set is not set as 'NextSet' by any other set!\nPlease review your sequencer sets configuration!" ) );
            AddError( SequencerErrorInfo( currentSet, sweSetsNotBeingReferenced, -1LL ) );
        }
    }
    UpdateErrors();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::CheckIfReferencedSetsAreActive( void )
//-----------------------------------------------------------------------------
{
    SetNrToControlsMap::iterator it = sequencerSetControlsMap_.begin();
    const SetNrToControlsMap::const_iterator itEND = sequencerSetControlsMap_.end();
    while( it != itEND )
    {
        const vector<wxSpinCtrl*>::size_type sequencerPathCount = it->second->pSequencerSetNextSCs.size();
        for( vector<wxSpinCtrl*>::size_type i = 0; i < sequencerPathCount; i++ )
        {
            const int64_type referencedSet = it->second->pSequencerSetNextSCs[i]->GetValue();
            if( sequencerSetControlsMap_.find( referencedSet ) != sequencerSetControlsMap_.end() )
            {
                it->second->pSequencerSetNextSCs[i]->SetBackgroundColour( *wxWHITE );
                it->second->pSequencerSetNextSCs[i]->SetToolTip( 0 );
                RemoveError( SequencerErrorInfo( it->first, sweReferenceToInactiveSet, referencedSet ) );
            }
            else
            {
                it->second->pSequencerSetNextSCs[i]->SetBackgroundColour( wxColour( acRedPastel ) );
                it->second->pSequencerSetNextSCs[i]->SetToolTip( wxT( "This set not active!\nPlease correct this value so\nthat it points to an active set!" ) );
                AddError( SequencerErrorInfo( it->first, sweReferenceToInactiveSet, referencedSet ) );
            }
        }
        ++it;
    }
    UpdateErrors();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::CheckIfStartingSetIsAnActiveSequencerSet( const int64_type setNumber )
//-----------------------------------------------------------------------------
{
    if( sequencerSetControlsMap_.find( setNumber ) != sequencerSetControlsMap_.end() )
    {
        pStartingSetText_->SetBackgroundColour( pInfoText_->GetBackgroundColour() );
        pStartingSetText_->SetToolTip( 0 );
        RemoveError( SequencerErrorInfo( setNumber, sweStartingSetNotActive, -1LL ) );
    }
    else
    {
        pStartingSetText_->SetBackgroundColour( wxColour( acRedPastel ) );
        pStartingSetText_->SetToolTip( wxT( "Starting set is not an active set!\nPlease review your sequencer sets configuration!" ) );
        AddError( SequencerErrorInfo( setNumber, sweStartingSetNotActive, -1LL ) );
    }
    UpdateErrors();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::ComposeSetPanel( size_t displayCount, int64_type setCount, int64_type setNumber, bool boInsertOnFirstFreePosition )
//-----------------------------------------------------------------------------
{
    sc_.sequencerSetSelector.write( ( setCount == 0 ) ? sc_.sequencerSetStart.read() : setNumber );

    SequencerSetControls* pSequencerSet = new SequencerSetControls();
    pSequencerSet->pSetPanel = new wxPanel( pSetSettingsNotebook_, static_cast<wxWindowID>( widStaticBitmapWarning + setNumber + 1 ), wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, wxString::Format( wxT( "%s%d" ), s_sequencerSetStringPrefix_.c_str(), static_cast< int >( setNumber ) ) );

    // workaround for correct notebook background colors on all platforms
    const wxColour defaultCol = pSetSettingsNotebook_->GetThemeBackgroundColour();
    if( defaultCol.IsOk() )
    {
        pSequencerSet->pSetPanel->SetBackgroundColour( defaultCol );
    }

    pSequencerSet->pSetPropertyGrid = new wxPropertyGrid( pSequencerSet->pSetPanel, static_cast<wxWindowID>( pSequencerSet->pSetPanel->GetId() + maxSequencerSetNumber_ ), wxDefaultPosition, wxDefaultSize, wxPG_BOLD_MODIFIED | wxPG_STATIC_SPLITTER | wxPG_SPLITTER_AUTO_CENTER, pSequencerSet->pSetPanel->GetName() );
    Bind( wxEVT_PG_CHANGED, &WizardSequencerControl::OnPropertyChanged, this, static_cast<wxWindowID>( pSequencerSet->pSetPanel->GetId() + maxSequencerSetNumber_ ) );
    UpdatePropertyGrid( pSequencerSet );

    pSequencerSet->pDisplayToUseCB = new wxComboBox( pSequencerSet->pSetPanel, wxID_ANY );
    for( size_t i = 0; i < displayCount; i++ )
    {
        pSequencerSet->pDisplayToUseCB->Append( wxString::Format( wxT( "%s%d" ), s_displayStringPrefix_.c_str(), static_cast<int>( i ) ) );
    }
    if( setToDisplayTable_.empty() ||
        setToDisplayTable_.find( setNumber ) == setToDisplayTable_.end() )
    {
        assert( displayCount > 0 );
        pSequencerSet->pDisplayToUseCB->Select( static_cast<int>( setCount % displayCount ) );
    }
    else
    {
        SequencerSetToDisplayMap::const_iterator displayIt = setToDisplayTable_.find( setNumber );
        pSequencerSet->pDisplayToUseCB->Select( displayIt->second );
    }

    wxBoxSizer* pOptionsSizer = new wxBoxSizer( wxHORIZONTAL );
    pOptionsSizer->Add( new wxStaticText( pSequencerSet->pSetPanel, wxID_ANY, wxT( "Show On Display:" ) ), wxSizerFlags().Border( wxALL, 3 ) );
    pOptionsSizer->AddSpacer( 2 );
    pOptionsSizer->Add( pSequencerSet->pDisplayToUseCB );

    wxFlexGridSizer* pSequencerPathControlsSizer = new wxFlexGridSizer( sc_.sequencerTriggerActivation.isValid() ? 6 : 4, 2, 2 );
    pSequencerPathControlsSizer->AddGrowableCol( 3, 2 );
    if( sc_.sequencerTriggerActivation.isValid() )
    {
        pSequencerPathControlsSizer->AddGrowableCol( 5, 2 );
    }
    if( sc_.sequencerPathSelector.isValid() && sc_.sequencerPathSelector.isWriteable() &&
        sc_.sequencerTriggerSource.isValid() )
    {

        const int64_type sequencerPathSelector_Max = sc_.sequencerPathSelector.getMaxValue();
        for( int64_type i = 0; i <= sequencerPathSelector_Max; i++ )
        {
            sc_.sequencerPathSelector.write( i );
            CreateGUIElementsForPathData( pSequencerSet, pSequencerPathControlsSizer, i );
        }
    }
    else
    {
        CreateGUIElementsForPathData( pSequencerSet, pSequencerPathControlsSizer, 0 );
    }
    wxBoxSizer* pSequencerPathControlsBoxSizer = new wxStaticBoxSizer( wxVERTICAL, pSequencerSet->pSetPanel, wxT( "Sequencer Path Configuration " ) );
    pSequencerPathControlsBoxSizer->Add( pSequencerPathControlsSizer, wxSizerFlags().Expand().Border( wxALL, 2 ) );

    wxBoxSizer* pSetVerticalSizer = new wxBoxSizer( wxVERTICAL );
    pSetVerticalSizer->AddSpacer( 2 );
    pSetVerticalSizer->Add( pSequencerSet->pSetPropertyGrid, wxSizerFlags( 12 ).Expand() );
    pSetVerticalSizer->AddSpacer( 2 );
    pSetVerticalSizer->Add( pSequencerPathControlsBoxSizer, wxSizerFlags( 4 ).Expand() );
    pSetVerticalSizer->AddSpacer( 2 );
    pSetVerticalSizer->Add( pOptionsSizer, wxSizerFlags( 1 ).Border( wxALL, 2 ) );
    pSetVerticalSizer->AddSpacer( 2 );

    wxBoxSizer* pSetHorizontalSizer = new wxBoxSizer( wxHORIZONTAL );
    pSetHorizontalSizer->AddSpacer( 2 );
    pSetHorizontalSizer->Add( pSetVerticalSizer, wxSizerFlags( 10 ).Expand() );
    pSetHorizontalSizer->AddSpacer( 2 );

    pSequencerSet->pSetPanel->SetSizer( pSetHorizontalSizer );

    sequencerSetControlsMap_.insert( make_pair( setNumber, pSequencerSet ) );

    if( boInsertOnFirstFreePosition )
    {
        // determine the position in which the new Tab will be inserted.
        int64_type position = -1;
        for( SetNrToControlsMap::iterator it = sequencerSetControlsMap_.begin(); it != sequencerSetControlsMap_.end(); it++ )
        {
            position++;
            if( it->first == setNumber )
            {
                break;
            }
        }
        LoadSequencerSet( setNumber );
        SelectSequencerSetAndCallMethod( GetSelectedSequencerSetNr(), sc_.sequencerSetLoad );
        pSetSettingsNotebook_->InsertPage( static_cast<size_t>( position ), pSequencerSet->pSetPanel, pSequencerSet->pSetPanel->GetName(), true );
        HandleAutomaticNextSetSettingsOnInsert( position, setNumber );
    }
    else
    {
        pSetSettingsNotebook_->AddPage( pSequencerSet->pSetPanel, pSequencerSet->pSetPanel->GetName(), false );
    }
}

//-----------------------------------------------------------------------------
wxComboBox* WizardSequencerControl::CreateComboBoxFromProperty( SequencerSetControls* pSequencerSet, vector<wxComboBox*>& container, wxWindowID id, PropertyI64 prop )
//-----------------------------------------------------------------------------
{
    wxComboBox* pCB = 0;
    if( prop.isValid() && prop.hasDict() )
    {
        pCB = new wxComboBox( pSequencerSet->pSetPanel, id );
        RefreshComboBoxFromProperty( pCB, prop );
        container.push_back( pCB );
    }
    return pCB;
}

//-----------------------------------------------------------------------------
wxSpinCtrl* WizardSequencerControl::CreateSequencerSetNextControl( SequencerSetControls* pSequencerSet )
//-----------------------------------------------------------------------------
{
    wxSpinCtrl* pSC = new wxSpinCtrl( pSequencerSet->pSetPanel, widNextSetSpinControl );
    pSC->SetRange( 0, static_cast<int>( sc_.sequencerSetSelector.getMaxValue() ) );
    pSC->SetValue( static_cast<int>( sc_.sequencerSetNext.read() ) );
    pSequencerSet->pSequencerSetNextSCs.push_back( pSC );
    return pSC;
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::CreateGUIElementsForPathData( SequencerSetControls* pSequencerSet, wxFlexGridSizer* pSequencerPathControlsSizer, int64_type pathIndex )
//-----------------------------------------------------------------------------
{
    pSequencerPathControlsSizer->Add( new wxStaticText( pSequencerSet->pSetPanel, wxID_ANY, wxString::Format( wxT( "Path %d, Next Set:" ), static_cast<int>( pathIndex ) ) ), wxSizerFlags().Border( wxALL, 3 ) );
    pSequencerPathControlsSizer->Add( CreateSequencerSetNextControl( pSequencerSet ) );
    pSequencerPathControlsSizer->Add( new wxStaticText( pSequencerSet->pSetPanel, wxID_ANY, wxT( "Trigger Source:" ) ), wxSizerFlags().Expand().Border( wxALL, 3 ) );
    pSequencerPathControlsSizer->Add( CreateComboBoxFromProperty( pSequencerSet, pSequencerSet->pSequencerTriggerSourceCBs, widSequencerTriggerSourceComboBox, sc_.sequencerTriggerSource ) );
    if( sc_.sequencerTriggerActivation.isValid() )
    {
        pSequencerPathControlsSizer->Add( new wxStaticText( pSequencerSet->pSetPanel, wxID_ANY, wxT( "Trigger Activation:" ) ), wxSizerFlags().Expand().Border( wxALL, 3 ) );
        pSequencerPathControlsSizer->Add( CreateComboBoxFromProperty( pSequencerSet, pSequencerSet->pSequencerTriggerActivationCBs, widSequencerTriggerActivationComboBox, sc_.sequencerTriggerActivation ) );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::DoConsistencyChecks( void )
//-----------------------------------------------------------------------------
{
    currentErrors_.clear();
    CheckIfActiveSetsAreReferencedAtLeastOnce();
    CheckIfReferencedSetsAreActive();
    CheckIfStartingSetIsAnActiveSequencerSet( startingSequencerSet_ );
}

//-----------------------------------------------------------------------------
WizardSequencerControl::SetNrToControlsMap::const_iterator WizardSequencerControl::GetSequencerSetControlsFromPageIndex( size_t pageNr ) const
//-----------------------------------------------------------------------------
{
    wxWindow* pPage = pSetSettingsNotebook_->GetPage( pageNr );
    SetNrToControlsMap::const_iterator itEND = sequencerSetControlsMap_.end();
    for( SetNrToControlsMap::const_iterator it = sequencerSetControlsMap_.begin(); it != itEND; it++ )
    {
        if( it->second->pSetPanel == pPage )
        {
            return it;
        }
    }
    return itEND;
}

//-----------------------------------------------------------------------------
int64_type WizardSequencerControl::GetSequencerSetNrFromPageIndex( size_t pageNr ) const
//-----------------------------------------------------------------------------
{
    SetNrToControlsMap::const_iterator it = GetSequencerSetControlsFromPageIndex( pageNr );
    return ( it == sequencerSetControlsMap_.end() ) ? -1LL : it->first;
}

//-----------------------------------------------------------------------------
const SequencerSetToDisplayMap& WizardSequencerControl::GetSetToDisplayTable( void )
//-----------------------------------------------------------------------------
{
    setToDisplayTable_.clear();
    for( SetNrToControlsMap::iterator it = sequencerSetControlsMap_.begin(); it != sequencerSetControlsMap_.end(); it++ )
    {
        setToDisplayTable_.insert( make_pair( static_cast<long>( it->first ), wxAtoi( it->second->pDisplayToUseCB->GetValue().substr( s_displayStringPrefix_.length() ) ) ) );
    }
    return setToDisplayTable_;
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::HandleAutomaticNextSetSettingsOnInsert( int64_type tabPageNumber, int64_type setNumber )
//-----------------------------------------------------------------------------
{
    // if another page exists after the inserted one, overwrite the nextSet setting with the number of the set of the next page.
    // if not then overwrite  the nextSet setting with the number of the set of the first page.
    const int nextTab = wxAtoi( pSetSettingsNotebook_->GetPage( ( static_cast<size_t>( tabPageNumber ) < pSetSettingsNotebook_->GetPageCount() - 1 ) ? static_cast<size_t>( tabPageNumber ) + 1 : 0 )->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
    sequencerSetControlsMap_.find( setNumber )->second->pSequencerSetNextSCs[0]->SetValue( nextTab );

    // if another page exists before the inserted one, overwrite the nextSet setting of that page with the currently inserted one.
    if( tabPageNumber > 0 )
    {
        const int previousTab = wxAtoi( pSetSettingsNotebook_->GetPage( static_cast<size_t>( tabPageNumber ) - 1 )->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
        SetNrToControlsMap::const_iterator it = sequencerSetControlsMap_.find( previousTab );
        it->second->pSequencerSetNextSCs[0]->SetValue( static_cast<int>( setNumber ) );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::HandleAutomaticNextSetSettingsOnRemove( int64_type tabPageNumber, int64_type setNumber )
//-----------------------------------------------------------------------------
{
    // if another page exists before the removed one, overwrite the nextSet setting of that page with the one which replaced the removed.
    if( tabPageNumber > 0 )
    {
        const int previousTab = wxAtoi( pSetSettingsNotebook_->GetPage( static_cast<size_t>( tabPageNumber ) - 1 )->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
        SetNrToControlsMap::const_iterator it = sequencerSetControlsMap_.find( previousTab );
        it->second->pSequencerSetNextSCs[0]->SetValue( static_cast<int>( setNumber ) );
    }

    // if another page exists after the removed one, overwrite the nextSet setting with the number of the set of the next page.
    // if not then overwrite  the nextSet setting with the number of the set of the first page.
    const int nextTab = wxAtoi( pSetSettingsNotebook_->GetPage( ( static_cast<size_t>( tabPageNumber ) < pSetSettingsNotebook_->GetPageCount() - 1 ) ? static_cast<size_t>( tabPageNumber ) + 1 : 0 )->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
    sequencerSetControlsMap_.find( setNumber )->second->pSequencerSetNextSCs[0]->SetValue( nextTab );
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::InquireCurrentlyActiveSequencerSets( set<int64_type>& activeSequencerSets )
//-----------------------------------------------------------------------------
{
    activeSequencerSets.clear();
    if( sc_.sequencerSetStart.isValid() && sc_.sequencerSetSelector.isValid() )
    {
        set<int64_type> sequencerSetsStillToProcess;
        startingSequencerSet_ = sc_.sequencerSetStart.read();
        sequencerSetsStillToProcess.insert( startingSequencerSet_ );
        sc_.sequencerSetSelector.write( startingSequencerSet_ );
        maxSequencerSetNumber_ = sc_.sequencerSetSelector.getMaxValue();
        while( !sequencerSetsStillToProcess.empty() )
        {
            sc_.sequencerSetSelector.write( *sequencerSetsStillToProcess.begin() );
            activeSequencerSets.insert( *sequencerSetsStillToProcess.begin() );
            sequencerSetsStillToProcess.erase( sequencerSetsStillToProcess.begin() );
            if( sc_.sequencerPathSelector.isValid() )
            {
                const int64_type sequencerPathSelector_Max = sc_.sequencerPathSelector.getMaxValue();
                for( int64_type i = 0; i <= sequencerPathSelector_Max; i++ )
                {
                    sc_.sequencerPathSelector.write( i );
                    if( activeSequencerSets.find( sc_.sequencerSetNext.read() ) == activeSequencerSets.end() )
                    {
                        if( !sc_.sequencerTriggerSource.isValid() || ( sc_.sequencerTriggerSource.readS() != "Off" ) )
                        {
                            sequencerSetsStillToProcess.insert( sc_.sequencerSetNext.read() );
                        }
                    }
                }
            }
            else
            {
                sequencerSetsStillToProcess.insert( sc_.sequencerSetNext.read() );
            }

        }
        sc_.sequencerSetSelector.write( startingSequencerSet_ );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::LoadSequencerSet( int64_type setNumber )
//-----------------------------------------------------------------------------
{
    if( sc_.sequencerSetSelector.isValid() && sc_.sequencerSetLoad.isValid() )
    {
        int64_type originalSetting = sc_.sequencerSetSelector.read();
        SetNrToControlsMap::iterator it = sequencerSetControlsMap_.find( setNumber );
        ReadPropertiesOfSequencerSet( it );
        sc_.sequencerSetSelector.write( originalSetting );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::LoadSequencerSets( void )
//-----------------------------------------------------------------------------
{
    if( sc_.sequencerSetSelector.isValid() && sc_.sequencerSetLoad.isValid() )
    {
        int64_type originalSetting = sc_.sequencerSetSelector.read();
        for( SetNrToControlsMap::iterator it = sequencerSetControlsMap_.begin(); it != sequencerSetControlsMap_.end(); it++ )
        {
            ReadPropertiesOfSequencerSet( it );
        }
        sc_.sequencerSetSelector.write( originalSetting );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnBtnAddAnotherSet( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const int64_type setCount = sequencerSetControlsMap_.size();
    if( setCount > maxSequencerSetNumber_ )
    {
        wxMessageBox( wxT( "Maximum Number Of Supported Sequencer Sets Reached!" ), wxT( "Cannot Add Another Set!" ), wxICON_EXCLAMATION );
        return;
    }
    for( int64_type newSetCandidate = 0; newSetCandidate <= maxSequencerSetNumber_; newSetCandidate++ )
    {
        SetNrToControlsMap::iterator it = sequencerSetControlsMap_.find( newSetCandidate );
        if( it == sequencerSetControlsMap_.end() )
        {
            SelectSequencerSetAndCallMethod( GetSelectedSequencerSetNr(), sc_.sequencerSetSave );
            ComposeSetPanel( displayCount_, setCount, newSetCandidate, true );
            break;
        }
    }
    DoConsistencyChecks();
    Refresh();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnBtnAutoAssignSetsToDisplays( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    const DisplayWindowContainer::size_type displayCount = pParent_->GetDisplayCount();
    if( displayCount > 0 )
    {
        unsigned int xCount = 1;
        unsigned int yCount = 1;
        const unsigned int setCount = static_cast<unsigned int>( sequencerSetControlsMap_.size() );
        while( ( xCount * yCount ) < setCount )
        {
            if( xCount <= yCount )
            {
                ++xCount;
            }
            else
            {
                ++yCount;
            }
        }
        pParent_->SetDisplayWindowCount( xCount, yCount );

        wxArrayString displayStrings;
        for( DisplayWindowContainer::size_type i = 0; i < displayCount; i++ )
        {
            displayStrings.Add( wxString::Format( wxT( "%s%d" ), s_displayStringPrefix_.c_str(), static_cast<int>( i ) ) );
        }

        SetNrToControlsMap::const_iterator it = sequencerSetControlsMap_.begin();
        const SetNrToControlsMap::const_iterator itEND = sequencerSetControlsMap_.end();
        while( it != itEND )
        {
            it->second->pDisplayToUseCB->Set( displayStrings );
            it->second->pDisplayToUseCB->Select( static_cast<int>( it->first % displayCount ) );
            ++it;
        }
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnBtnRemoveSelectedSet( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    if( sequencerSetControlsMap_.size() == 1 )
    {
        wxMessageBox( wxT( "The last set cannot be removed!" ), wxT( "Cannot Remove Set!" ), wxICON_EXCLAMATION );
        return;
    }
    RemoveSetPanel();
    DoConsistencyChecks();
    Refresh();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnBtnSetSelectedSetAsStartSet( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    startingSequencerSet_ = wxAtoi( pSetSettingsNotebook_->GetCurrentPage()->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
    pStartingSetText_->SetLabel( wxString::Format( wxT( "Current Starting Set: %d" ), static_cast<int>( startingSequencerSet_ ) ) );
    DoConsistencyChecks();
    Refresh();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnNBSequencerSetsPageChanged( wxNotebookEvent& e )
//-----------------------------------------------------------------------------
{
    try
    {
        if( e.GetOldSelection() >= 0 )
        {
            SelectSequencerSetAndCallMethod( GetSequencerSetNrFromPageIndex( static_cast<size_t>( e.GetOldSelection() ) ), sc_.sequencerSetSave );
        }
        SelectSequencerSetAndCallMethod( GetSequencerSetNrFromPageIndex( static_cast<size_t>( e.GetSelection() ) ), sc_.sequencerSetLoad );
        RefreshCurrentPropertyGrid();
    }
    catch( const ImpactAcquireException& e )
    {
        wxMessageDialog errorDlg( NULL, wxString::Format( wxT( "Internal error while switching sequencer sets: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        errorDlg.ShowModal();
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnPropertyChanged( wxPropertyGridEvent& e )
//-----------------------------------------------------------------------------
{
    wxString errorString;
    try
    {
        const string valueANSI( e.GetProperty()->GetValueAsString().mb_str() );
        PropertyObject::WritePropVal( valueANSI, Component( static_cast<HOBJ>( reinterpret_cast<intptr_t>( e.GetProperty()->GetClientData() ) ) ) );
    }
    catch( const ImpactAcquireException& e )
    {
        wxMessageDialog errorDlg( NULL,
                                  wxString::Format( wxT( "Can't set value(Error: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ),
                                  wxT( "Error" ),
                                  wxOK | wxICON_INFORMATION );
        errorDlg.ShowModal();
    }
    RefreshCurrentPropertyGrid();
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnSettingsCheckBoxChecked( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    wxCheckBox* pCheckBox = dynamic_cast<wxCheckBox*>( e.GetEventObject() );
    try
    {
        // first send the new information to the device
        if( sc_.sequencerFeatureSelector.isValid() )
        {
            const string featureName( pCheckBox->GetLabel().mb_str() );
            sc_.sequencerFeatureSelector.writeS( featureName );
            sc_.sequencerFeatureEnable.write( e.IsChecked() ? bTrue : bFalse );
        }
        // then update ALL 'feature enable' check boxes as enabling/disabling one feature may affect the state of others
        const SequencerFeatureContainer::const_iterator itEND = sequencerFeatures_.end();
        for( SequencerFeatureContainer::iterator it = sequencerFeatures_.begin(); it != itEND; it++ )
        {
            const string featureName( it->second->GetLabel().mb_str() );
            sc_.sequencerFeatureSelector.writeS( featureName );
            it->second->SetValue( ( sc_.sequencerFeatureEnable.read() == bTrue ) ? true : false );
        }
    }
    catch( const ImpactAcquireException& ex )
    {
        wxMessageDialog errorDlg( NULL, wxString::Format( wxT( "Internal error while handling check box event: %s(%s)!" ), ConvertedString( ex.getErrorString() ).c_str(), ConvertedString( ex.getErrorCodeAsString() ).c_str() ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        errorDlg.ShowModal();
    }
    for( SetNrToControlsMap::iterator it = sequencerSetControlsMap_.begin() ; it != sequencerSetControlsMap_.end(); it++ )
    {
        UpdatePropertyGrid( it->second );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnTriggerSourceTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    SetNrToControlsMap::const_iterator it = GetSelectedSequencerSetControls();
    if( it != sequencerSetControlsMap_.end() )
    {
        try
        {
            SequencerSetControls* pControls = it->second;
            sc_.sequencerSetSelector.write( it->first );
            const size_t sequencerPathSelector_Max = static_cast<size_t>( sc_.sequencerPathSelector.getMaxValue() );
            for( size_t i = 0; i <= sequencerPathSelector_Max; i++ )
            {
                sc_.sequencerPathSelector.write( static_cast<int64_type>( i ) );
                sc_.sequencerTriggerSource.writeS( string( pControls->pSequencerTriggerSourceCBs[i]->GetValue().mb_str() ) );
                if( sc_.sequencerTriggerActivation.isValid() )
                {
                    RefreshComboBoxFromProperty( pControls->pSequencerTriggerActivationCBs[i], sc_.sequencerTriggerActivation );
                }
            }
        }
        catch( const ImpactAcquireException& ex )
        {
            wxMessageDialog errorDlg( NULL, wxString::Format( wxT( "Internal error while handling combo box event for set %d: %s(%s)!" ), static_cast<int>( it->first ), ConvertedString( ex.getErrorString() ).c_str(), ConvertedString( ex.getErrorCodeAsString() ).c_str() ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
            errorDlg.ShowModal();
        }
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::OnTriggerActivationTextChanged( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    SetNrToControlsMap::const_iterator it = GetSelectedSequencerSetControls();
    if( it != sequencerSetControlsMap_.end() )
    {
        try
        {
            SequencerSetControls* pControls = it->second;
            sc_.sequencerSetSelector.write( it->first );
            const size_t sequencerPathSelector_Max = static_cast<size_t>( sc_.sequencerPathSelector.getMaxValue() );
            for( size_t i = 0; i <= sequencerPathSelector_Max; i++ )
            {
                if( !pControls->pSequencerTriggerActivationCBs[i]->IsTextEmpty() )
                {
                    sc_.sequencerPathSelector.write( static_cast<int64_type>( i ) );
                    if( sc_.sequencerTriggerActivation.isValid() && sc_.sequencerTriggerActivation.isWriteable() )
                    {
                        sc_.sequencerTriggerActivation.writeS( string( pControls->pSequencerTriggerActivationCBs[i]->GetValue().mb_str() ) );
                    }
                }
            }
        }
        catch( const ImpactAcquireException& ex )
        {
            wxMessageDialog errorDlg( NULL, wxString::Format( wxT( "Internal error while handling combo box event for set %d: %s(%s)!" ), static_cast<int>( it->first ), ConvertedString( ex.getErrorString() ).c_str(), ConvertedString( ex.getErrorCodeAsString() ).c_str() ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
            errorDlg.ShowModal();
        }
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::ReadPropertiesOfSequencerSet( SetNrToControlsMap::iterator& it )
//-----------------------------------------------------------------------------
{
    SelectSequencerSetAndCallMethod( it->first, sc_.sequencerSetLoad );
    const SequencerFeatureContainer::const_iterator compItEND = sequencerFeatures_.end();
    for( SequencerFeatureContainer::iterator compIt = sequencerFeatures_.begin(); compIt != compItEND; compIt++ )
    {
        if( compIt->first != INVALID_ID )
        {
            const Component comp( compIt->first );
            wxPGProperty* pGridProperty = 0;
            if( ( compIt->second->IsChecked() ) && ( comp.type() & ctProp ) )
            {
                const Property prop( compIt->first );
                pGridProperty = it->second->pSetPropertyGrid->GetProperty( prop.name() );
                try
                {
                    pGridProperty->SetValue( PropertyObject::GetCurrentValueAsString( prop ) );
                }
                catch( const ImpactAcquireException& ) {}
            }
        }
    }
    if( sc_.sequencerPathSelector.isValid() && sc_.sequencerPathSelector.isWriteable() )
    {
        const int64_type sequencerPathSelector_Max = sc_.sequencerPathSelector.getMaxValue();
        for( int64_type i = 0; i <= sequencerPathSelector_Max; i++ )
        {
            sc_.sequencerPathSelector.write( i );
            ReadSequencerPathDataFromDeviceToGUI( it->second, i );
        }
    }
    else
    {
        ReadSequencerPathDataFromDeviceToGUI( it->second, 0 );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::ReadSequencerPathDataFromDeviceToGUI( SequencerSetControls* pControls, int64_type pathIndex )
//-----------------------------------------------------------------------------
{
    pControls->pSequencerSetNextSCs[static_cast<size_t>( pathIndex )]->SetValue( static_cast<int>( sc_.sequencerSetNext.read() ) );
    if( sc_.sequencerTriggerSource.isValid() )
    {
        pControls->pSequencerTriggerSourceCBs[static_cast<size_t>( pathIndex )]->ChangeValue( ConvertedString( sc_.sequencerTriggerSource.readS() ) );
    }
    if( sc_.sequencerTriggerActivation.isValid() )
    {
        pControls->pSequencerTriggerActivationCBs[static_cast<size_t>( pathIndex )]->ChangeValue( ConvertedString( sc_.sequencerTriggerActivation.readS() ) );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::RefreshComboBoxFromProperty( wxComboBox* pCB, PropertyI64 prop )
//-----------------------------------------------------------------------------
{
    vector<string> validValues;
    prop.getTranslationDictStrings( validValues );
    const vector<string>::size_type validValueCount = validValues.size();
    wxArrayString items;
    for( vector<string>::size_type i = 0; i < validValueCount; i++ )
    {
        items.Add( ConvertedString( validValues[i] ) );
    }

    if( pCB->GetStrings() != items )
    {
        pCB->Set( items );
    }
    const int currentValueIndex = pCB->FindString( ConvertedString( prop.readS() ) );
    if( ( currentValueIndex != wxNOT_FOUND ) && ( currentValueIndex != pCB->GetSelection() ) )
    {
        pCB->Select( currentValueIndex );
    }
    pCB->Enable( prop.isWriteable() );
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::RefreshCurrentPropertyGrid( void )
//-----------------------------------------------------------------------------
{
    SetNrToControlsMap::const_iterator itControls = GetSelectedSequencerSetControls();
    if( ( itControls == sequencerSetControlsMap_.end() ) ||
        ( itControls->second == 0 ) )
    {
        return;
    }

    wxPropertyGridIterator it = itControls->second->pSetPropertyGrid->GetIterator();
    while( !it.AtEnd() )
    {
        try
        {
            Property prop( static_cast<HOBJ>( reinterpret_cast<intptr_t>( ( *it )->GetClientData() ) ) );
            ( *it )->SetValueFromString( PropertyObject::GetCurrentValueAsString( prop ) );
        }
        catch( const ImpactAcquireException& ) {}
        ++it;
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::RemoveError( const SequencerErrorInfo& errorInfo )
//-----------------------------------------------------------------------------
{
    const vector<SequencerErrorInfo>::iterator it = find( currentErrors_.begin(), currentErrors_.end(), errorInfo );
    if( it != currentErrors_.end() )
    {
        currentErrors_.erase( it );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::RemoveSetPanel( void )
//-----------------------------------------------------------------------------
{
    const int64_type currentPosition = pSetSettingsNotebook_->FindPage( pSetSettingsNotebook_->GetCurrentPage() );
    const int64_type setNumber = wxAtoi( pSetSettingsNotebook_->GetCurrentPage()->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
    const SetNrToControlsMap::iterator it = sequencerSetControlsMap_.find( setNumber );
    Unbind( wxEVT_PG_CHANGED, &WizardSequencerControl::OnPropertyChanged, this, it->second->pSetPanel->GetId() + wxWindowID( maxSequencerSetNumber_ ) );
    pSetSettingsNotebook_->DeletePage( static_cast<size_t>( currentPosition ) );
    sequencerSetControlsMap_.erase( it );
    if( pSetSettingsNotebook_->GetCurrentPage() != NULL )
    {
        const int64_type newSetNumber = wxAtoi( pSetSettingsNotebook_->GetCurrentPage()->GetName().substr( s_sequencerSetStringPrefix_.length() ) );
        HandleAutomaticNextSetSettingsOnRemove( static_cast<unsigned int>( currentPosition ), static_cast<unsigned int>( newSetNumber ) );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::UpdateSequencerTriggerSource( bool boUseCounter1EndRatherThanExposureEnd )
//-----------------------------------------------------------------------------
{
    // Implicitly change 'SequencerTriggerSource' for all sets from 'ExposureEnd' to 'Counter1End'
    // when the 'CounterDuration' feature check-box is enabled. Otherwise the property has no meaning.
    // The reverse must, of course, be done when the 'CounterDuration' feature check-box is unchecked.
    // This however must only be done, when there are actually just the 2 possible trigger sources required for this
    // special handling. If there are more possibilities the user needs to decide what is right!
    if( boCounterDurationCapability_ && boUseSpecialCounterDurationBehaviour_ && sc_.sequencerTriggerSource.isWriteable() )
    {
        sc_.sequencerTriggerSource.writeS( ( boUseCounter1EndRatherThanExposureEnd == true ? "Counter1End" : "ExposureEnd" ) );
    }
}

//-----------------------------------------------------------------------------
bool WizardSequencerControl::SaveSequencerSets( void )
//-----------------------------------------------------------------------------
{
    if( sequencerSetControlsMap_.empty() )
    {
        wxMessageBox( wxT( "At Least 1 Sequencer Set Has To Be Configured!" ), wxT( "Nothing To Save!" ), wxICON_EXCLAMATION );
        return false;
    }

    if( sc_.sequencerFeatureSelector.isValid() && sc_.sequencerFeatureEnable.isValid() && sc_.sequencerSetSave.isValid() )
    {
        const int64_type originalSetting = sc_.sequencerSetSelector.read();
        // all sets have to be saved
        {
            for( SetNrToControlsMap::iterator it = sequencerSetControlsMap_.begin(); it != sequencerSetControlsMap_.end(); it++ )
            {
                bool boUseCounter1EndRatherThanExposureEnd = false;
                SelectSequencerSetAndCallMethod( it->first, sc_.sequencerSetLoad );
                SequencerFeatureContainer::iterator compItEND = sequencerFeatures_.end();
                for( SequencerFeatureContainer::iterator compIt = sequencerFeatures_.begin(); compIt != compItEND; compIt++ )
                {
                    if( compIt->first != INVALID_ID )
                    {
                        const Component comp( compIt->first );
                        if( ( compIt->second->IsChecked() ) &&
                            ( comp.type() & ctProp ) &&
                            ( wxString( comp.name() ).IsSameAs( wxString( wxT( "CounterDuration" ) ) ) ) )
                        {
                            boUseCounter1EndRatherThanExposureEnd = true;
                            break;
                        }
                    }
                }
                if( sc_.sequencerPathSelector.isValid() && sc_.sequencerPathSelector.isWriteable() )
                {
                    const int64_type sequencerPathSelector_Max = sc_.sequencerPathSelector.getMaxValue();
                    for( int64_type i = 0; i <= sequencerPathSelector_Max; i++ )
                    {
                        sc_.sequencerPathSelector.write( i );
                        UpdateSequencerTriggerSource( boUseCounter1EndRatherThanExposureEnd );
                        WriteSequencerPathDataFromGUIToDevice( it->second, i );
                    }
                }
                else
                {
                    UpdateSequencerTriggerSource( boUseCounter1EndRatherThanExposureEnd );
                    WriteSequencerPathDataFromGUIToDevice( it->second, 0 );
                }
                SelectSequencerSetAndCallMethod( it->first, sc_.sequencerSetSave );
            }
        }
        sc_.sequencerSetStart.write( startingSequencerSet_ );
        sc_.sequencerSetSelector.write( originalSetting );
    }
    return true;
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::SaveSequencerSetsAndEnd( int returnCode )
//-----------------------------------------------------------------------------
{
    SelectSequencerSetAndCallMethod( GetSelectedSequencerSetNr(), sc_.sequencerSetSave );
    if( SaveSequencerSets() )
    {
        EndModal( returnCode );
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::SelectSequencerSetAndCallMethod( int64_type setNr, Method& meth )
//-----------------------------------------------------------------------------
{
    try
    {
        sc_.sequencerSetSelector.write( setNr );
        meth.call();
    }
    catch( const ImpactAcquireException& e )
    {
        wxMessageDialog errorDlg( NULL, wxString::Format( wxT( "Internal error while switching sequencer sets: %s(%s))!" ), ConvertedString( e.getErrorString() ).c_str(), ConvertedString( e.getErrorCodeAsString() ).c_str() ), wxT( "Error" ), wxOK | wxICON_INFORMATION );
        errorDlg.ShowModal();
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::UpdateError( const wxString& msg, const wxBitmap& icon ) const
//-----------------------------------------------------------------------------
{
    pStaticBitmapWarning_->SetBitmap( icon );
    pInfoText_->SetLabel( msg );
    pInfoText_->SetToolTip( msg );
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::UpdateErrors( void ) const
//-----------------------------------------------------------------------------
{
    if( currentErrors_.empty() )
    {
        UpdateError( wxString::Format( wxT( "" ) ), *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Empty ) );
    }
    else
    {
        const SequencerErrorInfo& lastError = currentErrors_.back();
        const int setNumber = static_cast<int>( lastError.setNumber );
        switch( lastError.errorType )
        {
        case sweReferenceToInactiveSet:
            UpdateError( wxString::Format( wxT( "Set %d: 'Next Set' Points To An Invalid Set (Set %d)!" ), setNumber, static_cast<int>( lastError.nextSet ) ), *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Warning ) );
            break;
        case sweSetsNotBeingReferenced:
            UpdateError( wxString::Format( wxT( "Set %d: No Other Set Has Set %d As 'Next Set'!" ), setNumber, setNumber ), *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Warning ) );
            break;
        case sweStartingSetNotActive:
            UpdateError( wxString::Format( wxT( "Set %d: Is Configured As Starting Set But It Is Missing!" ), setNumber ), *GlobalDataStorage::Instance()->GetBitmap( GlobalDataStorage::bIcon_Warning ) );
            break;
        }
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::UpdatePropertyGrid( SequencerSetControls* pSelectedSequencerSet )
//-----------------------------------------------------------------------------
{
    SequencerFeatureContainer::iterator itEND = sequencerFeatures_.end();
    for( SequencerFeatureContainer::iterator it = sequencerFeatures_.begin(); it != itEND; it++ )
    {
        if( it->first != INVALID_ID )
        {
            const Property prop( it->first );
            wxPGProperty* pGridProperty = pSelectedSequencerSet->pSetPropertyGrid->GetProperty( prop.name() );
            if( !pGridProperty )
            {
                const TComponentType type = prop.type();
                wxPropertyGrid* pGrid = pSelectedSequencerSet->pSetPropertyGrid;
                const wxString elementName( prop.name() );
                switch( PropertyObject::GetEditorType( prop, elementName ) )
                {
                case PropData::_ctrlBoolean:
                    pGridProperty = pGrid->Append( new wxBoolProperty( elementName, wxPG_LABEL ) );
                    pGrid->SetPropertyAttribute( pGridProperty, wxPG_BOOL_USE_CHECKBOX, wxVariant( 1 ) );
                    break;
                case PropData::_ctrlSpinner:
                    if( ( type == ctPropInt ) || ( type == ctPropInt64 ) || ( type == ctPropFloat ) )
                    {
                        pGridProperty = pGrid->Append( new wxStringProperty( elementName, wxPG_LABEL ) );
                        pGridProperty->SetEditor( wxPGCustomSpinCtrlEditor_HOBJ::Instance()->GetEditor() );
                    }
                    else
                    {
                        wxASSERT( !"invalid component type for spinner control" );
                    }
                    break;
                case PropData::_ctrlEdit:
                    pGridProperty = pGrid->Append( new wxLongStringProperty( elementName, wxPG_LABEL ) );
                    break;
                case PropData::_ctrlCombo:
                    {
                        wxPGChoices soc;
                        PropertyObject::GetTransformedDict( prop, soc );
                        pGridProperty = pGrid->Append( new wxEnumProperty( elementName, wxPG_LABEL, soc ) );
                    }
                    break;
                case PropData::_ctrlMultiChoiceSelector:
                    {
                        wxPGChoices soc;
                        PropertyObject::GetTransformedDict( prop, soc );
                        wxPGProperty* p = new wxMultiChoiceProperty( elementName, wxPG_LABEL, soc );
                        pGridProperty = pGrid->Append( p );
                    }
                    break;
                case PropData::_ctrlFileSelector:
                    pGridProperty = pGrid->Append( new wxFileProperty( elementName, wxPG_LABEL ) );
                    break;
                case PropData::_ctrlDirSelector:
                    pGridProperty = pGrid->Append( new wxDirProperty( elementName, wxPG_LABEL, ::wxGetUserHome() ) );
                    break;
                case PropData::_ctrlBinaryDataEditor:
                    pGridProperty = pGrid->Append( new wxBinaryDataProperty( elementName, wxPG_LABEL ) );
                    pGrid->SetPropertyValidator( pGridProperty, *wxBinaryDataProperty::GetClassValidator() );
                    break;
                default:
                    break;
                }
                if( pGridProperty )
                {
                    pGridProperty->SetClientData( reinterpret_cast<void*>( static_cast<wxIntPtr>( static_cast<unsigned int>( prop.hObj() ) ) ) );
                    try
                    {
                        pGridProperty->SetValue( PropertyObject::GetCurrentValueAsString( prop ) );
                    }
                    catch( const ImpactAcquireException& ) {}
                }
            }
            if( pGridProperty )
            {
                pGridProperty->Hide( !it->second->IsChecked() );
            }
        }
    }
}

//-----------------------------------------------------------------------------
void WizardSequencerControl::WriteSequencerPathDataFromGUIToDevice( SequencerSetControls* pControls, int64_type pathIndex )
//-----------------------------------------------------------------------------
{
    sc_.sequencerSetNext.write( pControls->pSequencerSetNextSCs[static_cast<size_t>( pathIndex )]->GetValue() );
    if( sc_.sequencerTriggerSource.isValid() && sc_.sequencerTriggerSource.isWriteable() )
    {
        sc_.sequencerTriggerSource.writeS( string( pControls->pSequencerTriggerSourceCBs[static_cast<size_t>( pathIndex )]->GetValue().mb_str() ) );
    }
    if( sc_.sequencerTriggerActivation.isValid() && sc_.sequencerTriggerActivation.isWriteable() )
    {
        sc_.sequencerTriggerActivation.writeS( string( pControls->pSequencerTriggerActivationCBs[static_cast<size_t>( pathIndex )]->GetValue().mb_str() ) );
    }
}
