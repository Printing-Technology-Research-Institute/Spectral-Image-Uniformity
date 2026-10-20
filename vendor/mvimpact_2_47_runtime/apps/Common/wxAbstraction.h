//-----------------------------------------------------------------------------
#ifndef wxAbstractionH
#define wxAbstractionH wxAbstractionH
//-----------------------------------------------------------------------------
#include <apps/Common/Info.h>
#include <sstream>
#include <string>
#include <vector>

#include "wxIncludePrologue.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/config.h>
#include <wx/dialog.h>
#include <wx/dynlib.h>
#include <wx/hyperlink.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/notebook.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/splash.h>
#include <wx/socket.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/window.h>
#include "wxIncludeEpilogue.h"

//-----------------------------------------------------------------------------
struct ConvertedString : wxString
//-----------------------------------------------------------------------------
{
#if wxUSE_UNICODE
    ConvertedString( const char* s ) :
        wxString( s, wxConvUTF8 ) {}
    ConvertedString( const std::string& s ) :
        wxString( s.c_str(), wxConvUTF8 ) {}
    ConvertedString( const wxString& s ) :
# if wxCHECK_VERSION(2,9,0)
        wxString( s.mb_str(), wxConvUTF8 ) {}
# else
        wxString( s.c_str(), wxConvUTF8 ) {}
# endif
#else
    ConvertedString( const char* s ) :
        wxString( s ) {}
    ConvertedString( const std::string& s ) :
        wxString( s.c_str() ) {}
    ConvertedString( const wxString& s ) :
# if wxCHECK_VERSION(2,9,0)
        wxString( s.mb_str() ) {}
# else
        wxString( s.c_str() ) {}
# endif
#endif
};

#if ( wxMINOR_VERSION > 6 ) && ( wxMAJOR_VERSION < 3 ) && !WXWIN_COMPATIBILITY_2_6
#include <wx/filedlg.h>
enum
{
    wxOPEN              = wxFD_OPEN,
    wxSAVE              = wxFD_SAVE,
    wxOVERWRITE_PROMPT  = wxFD_OVERWRITE_PROMPT,
    wxFILE_MUST_EXIST   = wxFD_FILE_MUST_EXIST,
    wxMULTIPLE          = wxFD_MULTIPLE,
    wxCHANGE_DIR        = wxFD_CHANGE_DIR
};
#endif

//-----------------------------------------------------------------------------
enum TApplicationColors
//-----------------------------------------------------------------------------
{
    acRedPastel = ( 200 << 16 ) | ( 200 << 8 ) | 255,
    acYellowPastel = ( 180 << 16 ) | ( 255 << 8 ) | 255,
    acBluePastel = ( 255 << 16 ) | ( 220 << 8 ) | 200,
    acDarkBluePastel = ( 255 << 16 ) | ( 100 << 8 ) | 80,
    acGreenPastel = ( 200 << 16 ) | ( 255 << 8 ) | 200,
    acGreyPastel = ( 200 << 16 ) | ( 200 << 8 ) | 200,
    acDarkGrey = ( 100 << 16 ) | ( 100 << 8 ) | 100
};

//=============================================================================
//================= Implementation FramePositionStorage =======================
//=============================================================================
//-----------------------------------------------------------------------------
class FramePositionStorage
//-----------------------------------------------------------------------------
{
    wxWindow* m_pWin;
public:
    FramePositionStorage( wxWindow* pWin ) : m_pWin( pWin ) {}
    void Save( void ) const
    {
        Save( m_pWin );
    }
    static void Save( wxWindow* pWin, const wxString& windowName = wxT( "MainFrame" ) )
    {
        wxConfigBase* pConfig( wxConfigBase::Get() );
        int height, width, XPos, YPos;
        pWin->GetSize( &width, &height );
        pWin->GetPosition( &XPos, &YPos );
        // when we e.g. try to write config stuff on a read-only file system the result can
        // be an annoying message box. Therefore we switch off logging during the storage operation.
        wxLogNull logSuspendScope;
        // For some reasons on some systems negative values might be reported from time to time. This has
        // been observed on multi-screen setups so far and when this happens the next time an application
        // is stared the window then is just visible in the taskbar on Windows. In order to prevent this
        // we don't store negative values.
        if( height > 0 )
        {
            pConfig->Write( wxString::Format( wxT( "/%s/h" ), windowName.c_str() ), height );
        }
        if( width > 0 )
        {
            pConfig->Write( wxString::Format( wxT( "/%s/w" ), windowName.c_str() ), width );
        }
        pConfig->Write( wxString::Format( wxT( "/%s/x" ), windowName.c_str() ), XPos );
        pConfig->Write( wxString::Format( wxT( "/%s/y" ), windowName.c_str() ), YPos );
        if( dynamic_cast<wxTopLevelWindow*>( pWin ) )
        {
            pConfig->Write( wxString::Format( wxT( "/%s/maximized" ), windowName.c_str() ), dynamic_cast<wxTopLevelWindow*>( pWin )->IsMaximized() );
        }
        pConfig->Flush();
    }
    static wxRect Load( const wxRect& defaultDimensions, bool& boMaximized, const wxString& windowName = wxT( "MainFrame" ) )
    {
        wxConfigBase* pConfig( wxConfigBase::Get() );
        wxRect rect;
        rect.height = pConfig->Read( wxString::Format( wxT( "/%s/h" ), windowName.c_str() ), defaultDimensions.height );
        rect.width = pConfig->Read( wxString::Format( wxT( "/%s/w" ), windowName.c_str() ), defaultDimensions.width );
        rect.x = pConfig->Read( wxString::Format( wxT( "/%s/x" ), windowName.c_str() ), defaultDimensions.x );
        rect.y = pConfig->Read( wxString::Format( wxT( "/%s/y" ), windowName.c_str() ), defaultDimensions.y );
        boMaximized = pConfig->Read( wxString::Format( wxT( "/%s/maximized" ), windowName.c_str() ), 1l ) != 0;
        int displayWidth = 0;
        int displayHeight = 0;
        wxDisplaySize( &displayWidth, &displayHeight );
        if( ( rect.x >= displayWidth ) || ( ( rect.x + rect.width ) < 0 ) )
        {
            rect.x = 0;
        }
        if( ( rect.y >= displayHeight ) || ( ( rect.y + rect.height ) < 0 ) )
        {
            rect.y = 0;
        }
        return rect;
    }
};

//=============================================================================
//================= Implementation SplashScreenScope ==========================
//=============================================================================
//-----------------------------------------------------------------------------
class SplashScreenScope
//-----------------------------------------------------------------------------
{
    wxSplashScreen* pSplash_;
    wxStopWatch stopWatch_;
public:
    explicit SplashScreenScope( const wxBitmap& bmp, const wxBitmap& icon ) : pSplash_( 0 ), stopWatch_()
    {
        pSplash_ = new wxSplashScreen( ( wxSystemSettings::GetScreenType() <= wxSYS_SCREEN_SMALL ) ? icon : bmp, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_NO_TIMEOUT, 0, NULL, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSIMPLE_BORDER );
        pSplash_->Update();
    }
    virtual ~SplashScreenScope()
    {
        const unsigned long startupTime_ms = static_cast<unsigned long>( stopWatch_.Time() );
        const unsigned long minSplashDisplayTime_ms = 3000;
        if( startupTime_ms < minSplashDisplayTime_ms )
        {
            wxMilliSleep( minSplashDisplayTime_ms - startupTime_ms );
        }
        delete pSplash_;
    }
};

//=============================================================================
//================= Implementation SocketInitializeScope ======================
//=============================================================================
//-----------------------------------------------------------------------------
class SocketInitializeScope
//-----------------------------------------------------------------------------
{
public:
    explicit SocketInitializeScope()
    {
        // This has to be called in order to be able to initialize sockets outside of
        // the main thread ( http://www.litwindow.com/Knowhow/wxSocket/wxsocket.html )
        wxSocketBase::Initialize();
    }
    ~SocketInitializeScope()
    {
        // Must apparently be done since we called wxSocketBase::Initialize(),
        // otherwise memory leaks occur when closing the application.
        wxSocketBase::Shutdown();
    }
};

//=============================================================================
//================= Implementation miscellaneous helper functions =============
//=============================================================================
//-----------------------------------------------------------------------------
inline wxString LoadGenTLProducer( wxDynamicLibrary& lib )
//-----------------------------------------------------------------------------
{
#if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    const wxString libName( wxT( "mvGenTLProducer.cti" ) );
#else
    const wxString libName( wxT( "libmvGenTLProducer.so" ) );
#endif // #if defined(_WIN32) || defined(WIN32) || defined(__WIN32__)

    const wxString GenTLPathVariable( ( sizeof( void* ) == 8 ) ? wxT( "GENICAM_GENTL64_PATH" ) : wxT( "GENICAM_GENTL32_PATH" ) );
#if defined(linux) || defined(__linux) || defined(__linux__)
    const wxChar PATH_SEPARATOR( wxT( ':' ) );
#elif defined(_WIN32) || defined(WIN32) || defined(__WIN32__)
    const wxChar PATH_SEPARATOR( wxT( ';' ) );
#else
#   error Unsupported target platform
#endif
    wxString GenTLPath;
    wxArrayString potentialLocations;
    if( ::wxGetEnv( GenTLPathVariable, &GenTLPath ) )
    {
        potentialLocations = wxSplit( GenTLPath, PATH_SEPARATOR );
    }

    wxString message;
    // when we e.g. trying to load a shared library that cannot be found the result can
    // be an annoying message box. Therefore we switch off logging during the load attempts.
    wxLogNull logSuspendScope;
    const size_t potentialLocationCount = potentialLocations.Count();
    for( size_t i = 0; i < potentialLocationCount; i++ )
    {
        lib.Load( potentialLocations[i] + wxT( "/" ) + libName, wxDL_VERBATIM );
        if( lib.IsLoaded() )
        {
            break;
        }
    }

    if( !lib.IsLoaded() )
    {
        lib.Load( libName, wxDL_VERBATIM );
    }
    if( !lib.IsLoaded() )
    {
        message = wxString::Format( wxT( "Could not connect to '%s'. Check your installation.\n\n" ), libName.c_str() );
    }
    return message;
}

//-----------------------------------------------------------------------------
inline void AddSourceInfo( wxWindow* pParent, wxSizer* pParentSizer )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pSizer = new wxBoxSizer( wxHORIZONTAL );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( "The complete source code belonging to this application can be obtained by contacting " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, COMPANY_NAME, COMPANY_WEBSITE ) );
    pParentSizer->Add( pSizer, 0, wxALL | wxALIGN_CENTER, 5 );
}

//-----------------------------------------------------------------------------
inline void AddSupportInfo( wxWindow* pParent, wxSizer* pParentSizer )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pSizer = new wxBoxSizer( wxHORIZONTAL );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( "Support contact: " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, COMPANY_SUPPORT_SERVICEDESK, COMPANY_SUPPORT_SERVICEDESK ) );
    pParentSizer->Add( pSizer, 0, wxALL | wxALIGN_CENTER, 5 );
}

//-----------------------------------------------------------------------------
inline void AddwxWidgetsInfo( wxWindow* pParent, wxSizer* pParentSizer )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pSizer = new wxBoxSizer( wxHORIZONTAL );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( "This tool has been written using " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, wxT( "wxWidgets" ), wxT( "http://www.wxwidgets.org" ) ) );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxString::Format( wxT( " %d.%d.%d." ), wxMAJOR_VERSION, wxMINOR_VERSION, wxRELEASE_NUMBER ) ) );
    pParentSizer->Add( pSizer, 0, wxALL | wxALIGN_CENTER, 5 );
}

//-----------------------------------------------------------------------------
inline void AddIconInfo( wxWindow* pParent, wxSizer* pParentSizer )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pSizer = new wxBoxSizer( wxHORIZONTAL );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( "This tool uses modified icons downloaded from here " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, wxT( "icons8" ), wxT( "https://icons8.com/" ) ) );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( " and here " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, wxT( "Freepik" ), wxT( "http://www.freepik.com" ) ) );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( ". The " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, wxT( "Material Design" ), wxT( "https://material.io/resources/icons/?style=outline" ) ) );
    pSizer->Add( new wxStaticText( pParent, wxID_ANY, wxT( " icons are published under " ) ) );
    pSizer->Add( new wxHyperlinkCtrl( pParent, wxID_ANY, wxT( "Apache 2.0 license." ), wxT( "https://www.apache.org/licenses/LICENSE-2.0.html" ) ) );
    pParentSizer->Add( pSizer, 0, wxALL | wxALIGN_CENTER, 5 );
}

//-----------------------------------------------------------------------------
inline void AddListControlToAboutNotebook( wxNotebook* pNotebook, const wxString& pageTitle, bool boSelectPage, const wxString& col0, const wxString& col1, const std::vector<std::pair<wxString, wxString> >& v )
//-----------------------------------------------------------------------------
{
    wxListCtrl* pListCtrl = new wxListCtrl( pNotebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE );
    pListCtrl->InsertColumn( 0, col0 );
    pListCtrl->InsertColumn( 1, col1 );
    const unsigned long cnt = static_cast<unsigned long>( v.size() );
    for( unsigned long i = 0; i < cnt; i++ )
    {
        long index = pListCtrl->InsertItem( i, v[i].first, i );
        pListCtrl->SetItem( index, 1, v[i].second );
    }
    for( unsigned int i = 0; i < 2; i++ )
    {
        pListCtrl->SetColumnWidth( i, wxLIST_AUTOSIZE );
    }
    pNotebook->AddPage( pListCtrl, pageTitle, boSelectPage );
}

//-----------------------------------------------------------------------------
inline void AppendPathSeparatorIfNeeded( wxString& path )
//-----------------------------------------------------------------------------
{
    if( !path.EndsWith( wxT( "/" ) ) && !path.EndsWith( wxT( "\\" ) ) )
    {
        path.append( wxT( "/" ) );
    }
}

//-----------------------------------------------------------------------------
inline void WriteToTextCtrl( wxTextCtrl* pTextCtrl, const wxString& msg, const wxTextAttr& style = wxTextAttr( *wxBLACK ) )
//-----------------------------------------------------------------------------
{
    if( pTextCtrl )
    {
        // If you want the control to show the last line of text at the bottom, you can add "ScrollLines(1)"
        // right after the AppendText call. AppendText will ensure the new line is visible, and ScrollLines
        // will ensure the scroll bar is at the real end of the range, not further.
        long posBefore = pTextCtrl->GetLastPosition();
        pTextCtrl->AppendText( msg );
        long posAfter = pTextCtrl->GetLastPosition();
        pTextCtrl->SetStyle( posBefore, posAfter, style );
        pTextCtrl->ScrollLines( 1 );
        pTextCtrl->ShowPosition( pTextCtrl->GetLastPosition() ); // ensure that this position is really visible
    }
}

//-----------------------------------------------------------------------------
struct AboutDialogInformation
//-----------------------------------------------------------------------------
{
    wxIcon icon_;
    wxWindow* pParent_;
    wxString applicationName_;
    wxString briefDescription_;
    unsigned int yearOfInitialRelease_;
    std::vector<std::pair<wxString, wxTextAttr> > usageHints_;
    std::vector<std::pair<wxString, wxString> > keyboardShortcuts_;
    std::vector<std::pair<wxString, wxString> > availableCommandLineOptions_;
    bool boAddIconInfo_;
    bool boAddExpatInfo_;
    bool boAddFFmpegInfo_;
    explicit AboutDialogInformation( const wxIcon& icon, wxWindow* pParent = 0 ) : icon_( icon ), pParent_( pParent ), applicationName_(),
        briefDescription_(), yearOfInitialRelease_( 2005 ), usageHints_(), keyboardShortcuts_(),
        availableCommandLineOptions_(), boAddIconInfo_( false ), boAddExpatInfo_( false ), boAddFFmpegInfo_( false ) {}
};

//-----------------------------------------------------------------------------
inline void DisplayCommandLineProcessingInformation( wxTextCtrl* pTextCtrl, const wxString& processedCommandLineParameters, const wxString& parserErrors, const wxTextAttr& style )
//-----------------------------------------------------------------------------
{
    WriteToTextCtrl( pTextCtrl, wxT( "\n" ) );
    WriteToTextCtrl( pTextCtrl, wxT( "Press 'F1' for help.\n" ), style );
    WriteToTextCtrl( pTextCtrl, wxT( "\n" ) );
    const wxString none( wxT( "none" ) );
    WriteToTextCtrl( pTextCtrl, wxString::Format( wxT( "Processed command line parameters: %s\n" ), ( processedCommandLineParameters.length() > 0 ) ? processedCommandLineParameters.c_str() : none.c_str() ), style );
    WriteToTextCtrl( pTextCtrl, wxT( "\n" ) );
    if( !parserErrors.IsEmpty() )
    {
        WriteToTextCtrl( pTextCtrl, parserErrors, wxTextAttr( *wxRED ) );
        WriteToTextCtrl( pTextCtrl, wxT( "\n" ) );
    }
}

//-----------------------------------------------------------------------------
inline void DisplayCommonAboutDialog( const AboutDialogInformation& info )
//-----------------------------------------------------------------------------
{
    wxBoxSizer* pTopDownSizer;
    wxDialog dlg( info.pParent_, wxID_ANY, wxString( wxString::Format( wxT( "About %s" ), info.applicationName_.c_str() ) ), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX );
    dlg.SetIcon( info.icon_ );
    pTopDownSizer = new wxBoxSizer( wxVERTICAL );
    wxStaticText* pText = new wxStaticText( &dlg, wxID_ANY, info.briefDescription_ );
    pTopDownSizer->Add( pText, 0, wxALL | wxALIGN_CENTER, 5 );
    pText = new wxStaticText( &dlg, wxID_ANY, wxString::Format( wxT( "(C) %u - %s by %s" ), info.yearOfInitialRelease_, CURRENT_YEAR, COMPANY_NAME ) );
    pTopDownSizer->Add( pText, 0, wxALL | wxALIGN_CENTER, 5 );
    pText = new wxStaticText( &dlg, wxID_ANY, wxString::Format( wxT( "Version %s" ), VERSION_STRING ) );
    pTopDownSizer->Add( pText, 0, wxALL | wxALIGN_CENTER, 5 );
    AddSupportInfo( &dlg, pTopDownSizer );
    AddwxWidgetsInfo( &dlg, pTopDownSizer );
    if( info.boAddIconInfo_ )
    {
        AddIconInfo( &dlg, pTopDownSizer );
    }
    if( info.boAddExpatInfo_ )
    {
        pText = new wxStaticText( &dlg, wxID_ANY, wxT( "The expat wrapper class used internally has been written by Descartes Systems Sciences, Inc." ) );
        pTopDownSizer->Add( pText, 0, wxALL | wxALIGN_CENTER, 5 );
    }
    if( info.boAddFFmpegInfo_ )
    {
        wxBoxSizer* pSizer = new wxBoxSizer( wxHORIZONTAL );
        pSizer->Add( new wxStaticText( &dlg, wxID_ANY, wxT( "The video recording functionality of this application was implemented using libraries from the " ) ) );
        pSizer->Add( new wxHyperlinkCtrl( &dlg, wxID_ANY, wxT( "FFmpeg" ), wxT( "https://www.ffmpeg.org/" ) ) );
        pSizer->Add( new wxStaticText( &dlg, wxID_ANY, wxT( " project under the LGPLv2.1. These must be installed separately" ) ) );
        pTopDownSizer->Add( pSizer, 0, wxALL | wxALIGN_CENTER, 5 );
    }
    AddSourceInfo( &dlg, pTopDownSizer );
    wxNotebook* pNotebook = new wxNotebook( &dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize );
    wxTextCtrl* pUsageHints = new wxTextCtrl( pNotebook, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxBORDER_NONE | wxTE_RICH | wxTE_READONLY );
    pNotebook->AddPage( pUsageHints, wxT( "Usage Hints" ), true );
    std::vector<std::pair<wxTextAttr, wxString> >::size_type usageHintStringCount = info.usageHints_.size();
    for( std::vector<std::pair<wxString, wxTextAttr> >::size_type i = 0; i < usageHintStringCount; i++ )
    {
        WriteToTextCtrl( pUsageHints, info.usageHints_[i].first, info.usageHints_[i].second );
    }
    pUsageHints->ScrollLines( -( 256 * 256 ) ); // make sure the text control always shows the beginning of the help text
    if( !info.keyboardShortcuts_.empty() )
    {
        AddListControlToAboutNotebook( pNotebook, wxT( "Keyboard Shortcuts" ), false, wxT( "Shortcut" ), wxT( "Command" ), info.keyboardShortcuts_ );
    }
    if( !info.availableCommandLineOptions_.empty() )
    {
        AddListControlToAboutNotebook( pNotebook, wxT( "Available Command Line Options" ), false, wxT( "Command" ), wxT( "Description" ), info.availableCommandLineOptions_ );
    }
    pTopDownSizer->AddSpacer( 10 );
    pTopDownSizer->Add( pNotebook, wxSizerFlags( 5 ).Expand() );
    pTopDownSizer->AddSpacer( 10 );
    wxButton* pBtnOK = new wxButton( &dlg, wxID_OK, wxT( "OK" ) );
    pBtnOK->SetDefault();
    pTopDownSizer->Add( pBtnOK, 0, wxALL | wxALIGN_RIGHT, 15 );
    dlg.SetSizer( pTopDownSizer );
    dlg.SetSizeHints( 720, 500 );
    dlg.SetSize( -1, -1, 800, 500 );
    dlg.ShowModal();
}

//-----------------------------------------------------------------------------
inline wxTextAttr GetCommonTextStyle( const bool boBold, const bool boUnderlined, wxTextCtrl*
#if wxCHECK_VERSION(2, 9, 5)
#else
                                      pParent
#endif // #if wxCHECK_VERSION(2, 9, 5)
                                    )
//-----------------------------------------------------------------------------
{
    wxTextAttr theStyle;
#if wxCHECK_VERSION(2, 9, 5)
    wxFontInfo fontInfo( 10 );
    if( boBold )
    {
        fontInfo.Bold();
    }
    if( boUnderlined )
    {
        fontInfo.Underlined();
    }
    wxFont theFont( fontInfo );
#else
    pParent->GetStyle( pParent->GetLastPosition(), theStyle );
    wxFont theFont( theStyle.GetFont() );
    if( boBold )
    {
        theFont.SetWeight( wxFONTWEIGHT_BOLD );
    }
    theFont.SetPointSize( 10 );
    if( boUnderlined )
    {
        theFont.SetUnderlined( true );
    }
#endif // #if wxCHECK_VERSION(2, 9, 5)
    theStyle.SetFont( theFont );
    return theStyle;
}


//-----------------------------------------------------------------------------
inline wxTextAttr GetBoldStyle( wxTextCtrl* pParent )
//-----------------------------------------------------------------------------
{
    return GetCommonTextStyle( true, false, pParent );
}

//-----------------------------------------------------------------------------
inline wxTextAttr GetBoldUnderlinedStyle( wxTextCtrl* pParent )
//-----------------------------------------------------------------------------
{
    return GetCommonTextStyle( true, true, pParent );
}

//-----------------------------------------------------------------------------
inline wxTextAttr GetDefaultStyle( wxTextCtrl* pParent )
//-----------------------------------------------------------------------------
{
    return GetCommonTextStyle( false, false, pParent );
}

//-----------------------------------------------------------------------------
inline wxTextAttr GetUnderlinedStyle( wxTextCtrl* pParent )
//-----------------------------------------------------------------------------
{
    return GetCommonTextStyle( false, true, pParent );
}

//-----------------------------------------------------------------------------
inline bool IsListOfChoicesEmpty( wxComboBox* pCB )
//-----------------------------------------------------------------------------
{
#if wxCHECK_VERSION(2, 9, 3)
    return pCB->IsListEmpty();
#else
    return pCB->IsEmpty();
#endif // #if wxCHECK_VERSION(2, 9, 3)
}

//-----------------------------------------------------------------------------
inline void StopTimer( wxTimer& timer )
//-----------------------------------------------------------------------------
{
    if( timer.IsRunning() )
    {
        timer.Stop();
    }
}

//=============================================================================
//===================== Implementation Version helper functions ===============
//=============================================================================
//-----------------------------------------------------------------------------
struct Version
//-----------------------------------------------------------------------------
{
    long major_;
    long minor_;
    long subMinor_;
    long release_;
    explicit Version() : major_( -1 ), minor_( -1 ), subMinor_( -1 ), release_( 0 ) {}
    explicit Version( long ma, long mi, long smi, long re ) : major_( ma ), minor_( mi ), subMinor_( smi ), release_( re ) {}
};

//-----------------------------------------------------------------------------
inline bool operator<( const Version& a, const Version& b )
//-----------------------------------------------------------------------------
{
    if( a.major_ < b.major_ )
    {
        return true;
    }
    else if( a.major_ == b.major_ )
    {
        if( a.minor_ < b.minor_ )
        {
            return true;
        }
        else if( a.minor_ == b.minor_ )
        {
            if( a.subMinor_ < b.subMinor_ )
            {
                return true;
            }
            else if( a.subMinor_ == b.subMinor_ )
            {
                if( a.release_ < b.release_ )
                {
                    return true;
                }
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
inline bool operator==( const Version& a, const Version& b )
//-----------------------------------------------------------------------------
{
    return ( ( a.major_ == b.major_ ) && ( a.minor_ == b.minor_ ) && ( a.subMinor_ == b.subMinor_ ) && ( a.release_ == b.release_ ) );
}

//-----------------------------------------------------------------------------
inline bool operator!=( const Version& a, const Version& b )
//-----------------------------------------------------------------------------
{
    return ( ( a.major_ != b.major_ ) || ( a.minor_ != b.minor_ ) || ( a.subMinor_ != b.subMinor_ ) || ( a.release_ != b.release_ ) );
}

//-----------------------------------------------------------------------------
inline bool operator>( const Version& a, const Version& b )
//-----------------------------------------------------------------------------
{
    return ( !( a < b ) && !( a == b ) );
}

//-----------------------------------------------------------------------------
inline bool operator>=( const Version& a, const Version& b )
//-----------------------------------------------------------------------------
{
    return ( a > b ) || ( a == b );
}

//-----------------------------------------------------------------------------
inline bool operator<=( const Version& a, const Version& b )
//-----------------------------------------------------------------------------
{
    return ( a < b ) || ( a == b );
}

//-----------------------------------------------------------------------------
inline bool GetNextVersionNumber( wxString& str, long& number )
//-----------------------------------------------------------------------------
{
    wxString numberString = str.BeforeFirst( wxT( '.' ) );
    str = str.AfterFirst( wxT( '.' ) );
    return numberString.ToLong( &number );
}

//-----------------------------------------------------------------------------
inline Version VersionFromString( const wxString& versionAsString )
//-----------------------------------------------------------------------------
{
    Version version;
    wxString tmp( versionAsString );
    GetNextVersionNumber( tmp, version.major_ );
    if( !tmp.empty() )
    {
        GetNextVersionNumber( tmp, version.minor_ );
        if( !tmp.empty() )
        {
            GetNextVersionNumber( tmp, version.subMinor_ );
            if( !tmp.empty() )
            {
                GetNextVersionNumber( tmp, version.release_ );
            }
        }
    }
    return version;
}

//-----------------------------------------------------------------------------
inline wxString VersionToString( const Version& version )
//-----------------------------------------------------------------------------
{
    return wxString::Format( wxT( "%d.%d.%d.%d" ), version.major_, version.minor_, version.subMinor_, version.release_ );
}

//-----------------------------------------------------------------------------
inline bool IsVersionWithinRange( const Version& version, const Version& minVersion, const Version& maxVersion )
//-----------------------------------------------------------------------------
{
    return ( version >= minVersion ) && ( version <= maxVersion );
}

//=============================================================================
//============= Implementation IP Address conversion functions ================
//=============================================================================
//-----------------------------------------------------------------------------
inline std::string GetIPv4AddressAsString( unsigned int ip )
//-----------------------------------------------------------------------------
{
    std::ostringstream output;
    output << ( ( ip >> 24 ) & 0xFF ) << "." << ( ( ip >> 16 ) & 0xFF ) << "." << ( ( ip >> 8 ) & 0xFF ) << "." << ( ip & 0xFF );
    return output.str();
}

#endif // wxAbstractionH
