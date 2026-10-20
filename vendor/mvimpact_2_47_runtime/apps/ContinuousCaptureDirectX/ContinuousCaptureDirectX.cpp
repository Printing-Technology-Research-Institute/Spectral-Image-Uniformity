#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdarg.h>
#include "resource.h"
#include <apps/Common/exampleHelper.h>
#include "CaptureParameters.h"

using namespace std;
using namespace mvIMPACT::acquire;

//-----------------------------------------------------------------------------
// Local definitions
//-----------------------------------------------------------------------------
#define NAME                          "ContinuousCaptureDirectX"
#define TITLE                         "mvIMPACT Acquire Direct Draw Example"
#define TIMER_ID                      1
#define TIMER_RATE                    1000
#define TIMER_ID_DEVICE_AUTO_SWITCH   2

typedef std::vector<CaptureParameters*> CaptureParamContainer;

//-----------------------------------------------------------------------------
// Global data
//-----------------------------------------------------------------------------
LPDIRECTDRAW7                    g_pDD = NULL;        // DirectDraw object
LPDIRECTDRAWSURFACE7             g_pDDSPrimary = NULL;// DirectDraw primary surface
LPDIRECTDRAWSURFACE7             g_pDDSBack = NULL;   // DirectDraw back surface
LPDIRECTDRAWSURFACE7             g_pDDSOne = NULL;    // Offscreen surface 1
LPDIRECTDRAWSURFACE7             g_pDDSTwo = NULL;    // Offscreen surface 2
BOOL                             g_bActive = FALSE;   // Is application active?
HWND                             g_hWnd;
CaptureParamContainer            g_paramSets;
CaptureParamContainer::size_type g_currentParamset = 0;
static bool                      g_TextOutOn = false;
static int                       g_wScr = GetSystemMetrics( SM_CXSCREEN );
static int                       g_hScr = GetSystemMetrics( SM_CYSCREEN );
static ImageBufferDesc           g_NullImage( ibpfRGB888Packed, g_wScr, g_hScr );
std::string                      g_info;
static BOOL                      g_bHelpOverlayActive = true;
std::vector<std::string>         g_onScreenHelp;
static BOOL                      g_bDeviceAutoSwitchTimerActive = false;
unsigned int                     g_deviceAutoSwitchRate_ms = 10000;
const unsigned int               g_deviceAutoSwitchRateInc_ms = 500;
static BOOL                      g_bShuttingDown = FALSE;
int                              g_fullOffset = 0;

//-----------------------------------------------------------------------------
class MyCaptureDisplay : public CaptureDisplay
//-----------------------------------------------------------------------------
{
    HBRUSH blackBrush_;
    //-----------------------------------------------------------------------------
    long WidthInBytes( int bits )
    //-----------------------------------------------------------------------------
    {
        return static_cast<long>( ( ( bits ) + 31 ) / 32 * 4 );
    }
    //-----------------------------------------------------------------------------
    HANDLE CreateEmptyDIB( int dx, int dy, int bipp,  unsigned char** ppData )
    //-----------------------------------------------------------------------------
    {
        BOOL boHicolor = ( bipp == 15 || bipp == 16 );
        int bitCount = ( boHicolor ) ? 16 : bipp;

        // get length of DIB in bytes for use when reading
        unsigned int colors = 0;
        unsigned int HeadSize = sizeof( BITMAPINFOHEADER );
        if( bipp <= 8 )
        {
            colors = 1 << bipp;
        }
        else if( boHicolor )
        {
            colors = 3 * sizeof( DWORD );
        }

        HeadSize += colors * sizeof( RGBQUAD );
        DWORD DIBsize = HeadSize;
        if( ppData )
        {
            DIBsize += WidthInBytes( dx * bitCount ) * ( long )dy;
        }

        // Allocate memory for DIB
        HANDLE hDIB = GlobalAlloc( GMEM_MOVEABLE | GMEM_ZEROINIT, DIBsize );

        if( !hDIB )
        {
            return 0;
        }

        BITMAPINFOHEADER* pBitmapInfo = ( LPBITMAPINFOHEADER )GlobalLock( hDIB );

        if( ppData )
        {
            *ppData = ( ( unsigned char* )pBitmapInfo ) + HeadSize;
        }

        // setup DIB parameters
        memset( pBitmapInfo, 0, sizeof( BITMAPINFOHEADER ) );
        pBitmapInfo->biSize     = sizeof( BITMAPINFOHEADER ); //  + colors * sizeof( RGBQUAD )
        pBitmapInfo->biWidth    = dx;
        pBitmapInfo->biHeight   = -dy;  // bitmap is vertically ok
        pBitmapInfo->biPlanes   = 1;
        pBitmapInfo->biBitCount = ( unsigned char ) bitCount;
        pBitmapInfo->biCompression = ( boHicolor ) ? BI_BITFIELDS : BI_RGB;
        pBitmapInfo->biSizeImage     = 0;
        pBitmapInfo->biXPelsPerMeter   = 0;
        pBitmapInfo->biYPelsPerMeter   = 0;

        // set gray palette
        pBitmapInfo->biClrUsed      = 0;
        pBitmapInfo->biClrImportant = 0; //all are important
        GlobalUnlock( hDIB );
        return hDIB;
    }
    //-----------------------------------------------------------------------------
    HRESULT DDCopyImage( IDirectDrawSurface7* pdds, const ImageBuffer* pIB, int /*x*/, int /*y*/, int dx, int dy )
    //-----------------------------------------------------------------------------
    {
        HDC                     hdcImage;
        HDC                     hdc;
        DDSURFACEDESC2          ddsd;
        HRESULT                 hr;

        if( pdds == NULL )
        {
            return E_FAIL;
        }

        pdds->Restore();
        hdcImage = CreateCompatibleDC( NULL );
        if( !hdcImage )
        {
            OutputDebugString( "CreateCompatibleDC failed\n" );
        }
        ddsd.dwSize = sizeof( ddsd );
        ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH;
        pdds->GetSurfaceDesc( &ddsd );
        if( ( hr = pdds->GetDC( &hdc ) ) == DD_OK )
        {
            HANDLE m_hDIB = CreateEmptyDIB( pIB->iWidth, pIB->iHeight, 24, NULL );
            LPBITMAPINFOHEADER m_lpBMPHdr = reinterpret_cast<LPBITMAPINFOHEADER>( GlobalLock( m_hDIB ) );
            int startX = ( g_wScr > pIB->iWidth ) ? ( g_wScr - pIB->iWidth ) / 2 : 0;
            int startY = ( g_hScr > pIB->iHeight ) ? ( g_hScr - pIB->iHeight ) / 2 : 0;
            if( startY > 0 )
            {
                RECT rect;
                rect.left = 0;
                rect.top = 0;
                rect.right = ( startY > 0 ) ? g_wScr : startX - 1;
                rect.bottom = startY - 1;
                FillRect( hdc, &rect, blackBrush_ );
                rect.top = pIB->iHeight + startY - 1;
                rect.bottom = g_hScr;
                FillRect( hdc, &rect, blackBrush_ );
            }
            if( startX > 0 )
            {
                RECT rect;
                rect.left = 0;
                rect.top = startY;
                rect.right = startX - 1;
                rect.bottom = g_hScr - startY;
                FillRect( hdc, &rect, blackBrush_ );
                rect.left = startX + pIB->iWidth;
                rect.right = g_wScr;
                FillRect( hdc, &rect, blackBrush_ );
            }
            SetDIBitsToDevice( hdc,
                               startX, startY, // destination upper left corner
                               dx, dy,         // source width and height
                               0, 0,           // source LOWER left corner
                               0,              // first scan line in array
                               pIB->iHeight,   // number of scan lines
                               pIB->vpData,
                               reinterpret_cast<LPBITMAPINFO>( m_lpBMPHdr ),
                               DIB_RGB_COLORS ); // RGB or palette indexes

            SetBkMode( hdc, TRANSPARENT );
            SetTextColor( hdc, RGB( 0, 255, 0 ) );
            if( g_TextOutOn )
            {
                TextOut( hdc, 0, g_hScr - 20, g_info.c_str(), static_cast<int>( g_info.size() ) );
            }
            if( g_bHelpOverlayActive && ( g_currentParamset < g_paramSets.size() ) )
            {
                g_onScreenHelp.clear();
                g_onScreenHelp.push_back( BuildFormattedString( "F1: Switch to the next device(Current device: %s(%s, index: %d))", g_paramSets[g_currentParamset]->device()->serial.readS().c_str(), g_paramSets[g_currentParamset]->device()->product.readS().c_str(), g_currentParamset ) );
                g_onScreenHelp.push_back( BuildFormattedString( "F2: Start/stop live capture from the current device(Currently %sactive)", g_paramSets[g_currentParamset]->isThreadRunning() ? "" : "NOT " ) );
                g_onScreenHelp.push_back( BuildFormattedString( "F3: Switch on/off on screen help(currently %sactive)", g_bHelpOverlayActive ? "" : "NOT " ) );
                g_onScreenHelp.push_back( BuildFormattedString( "F4: Switch on/off on screen information about the images(Currently %sactive)", g_TextOutOn ? "" : "NOT " ) );
                g_onScreenHelp.push_back( BuildFormattedString( "F5: Switch to the next channel of the device(Current channel: %d, supported channels: %d)", g_paramSets[g_currentParamset]->currentVideoChannel(), g_paramSets[g_currentParamset]->availableVideoChannels() ) );
                g_onScreenHelp.push_back( BuildFormattedString( "F9: Switch on/off automatic change of the capturing device every %d ms(Currently %sactive)", g_deviceAutoSwitchRate_ms, g_bDeviceAutoSwitchTimerActive ? "" : "NOT " ) );
                g_onScreenHelp.push_back( BuildFormattedString( "+(Numpad): Increase the period for the automatic change of the capturing device by %d ms", g_deviceAutoSwitchRateInc_ms ) );
                g_onScreenHelp.push_back( BuildFormattedString( "-(Numpad): Decrease the period for the automatic change of the capturing device by %d ms", g_deviceAutoSwitchRateInc_ms ) );
                const int cnt = static_cast<int>( g_onScreenHelp.size() );
                for( int i = 0; i < cnt; i++ )
                {
                    TextOut( hdc, 10, i * 20, g_onScreenHelp[i].c_str(), static_cast<int>( g_onScreenHelp[i].size() ) );
                }
            }

            FreeDIB( &m_hDIB, &m_lpBMPHdr );
            pdds->ReleaseDC( hdc );
        }
        DeleteDC( hdcImage );
        return hr;
    }
    //-----------------------------------------------------------------------------
    void FreeDIB( HANDLE* hDIB, LPBITMAPINFOHEADER* lpDIBHdr )
    //-----------------------------------------------------------------------------
    {
        if( *hDIB )
        {
            GlobalFree( *hDIB );
            *hDIB = 0;
            *lpDIBHdr = 0;
        }
    }
    //-----------------------------------------------------------------------------
    HRESULT RestoreAll( void )
    //-----------------------------------------------------------------------------
    {
        HRESULT                     hRet;

        hRet = g_pDDSPrimary->Restore();
        if( hRet == DD_OK )
        {
            hRet = g_pDDSOne->Restore();
            if( hRet == DD_OK )
            {
                hRet = g_pDDSTwo->Restore();
            }
        }
        return hRet;
    }
    //-----------------------------------------------------------------------------
    void UpdateFrame( HWND /*hWnd*/, const ImageBuffer* pIB )
    //-----------------------------------------------------------------------------
    {
        static BYTE                 phase = 0;
        HRESULT                     hRet;
        LPDIRECTDRAWSURFACE7        pdds;
        RECT                        rect;

        rect.left = 0;
        rect.top = 0;
        rect.right = g_wScr;
        rect.bottom = g_hScr;
        if( phase )
        {
            pdds = g_pDDSTwo;
            phase = 0;
        }
        else
        {
            pdds = g_pDDSOne;
            phase = 1;
        }
        bool boRun = true;
        while( boRun )
        {
            DDCopyImage( pdds, pIB, 0, 0, pIB->iWidth, pIB->iHeight );
            hRet = g_pDDSBack->BltFast( 0, 0, pdds, &rect, FALSE );
            if( hRet == DD_OK )
            {
                boRun = false;
                continue;
            }
            if( hRet == DDERR_SURFACELOST )
            {
                hRet = RestoreAll();
                if( hRet != DD_OK )
                {
                    boRun = false;
                    continue;
                }
            }
            if( hRet != DDERR_WASSTILLDRAWING )
            {
                boRun = false;
                continue;
            }
        }
    }
public:
    explicit MyCaptureDisplay() : blackBrush_( CreateSolidBrush( RGB( 0, 0, 0 ) ) ) {}
    ~MyCaptureDisplay()
    {
        DeleteObject( blackBrush_ );
    }
    virtual void SetImage( const ImageBuffer* pIB )
    {
        // Update and flip surfaces
        if( g_bActive && !g_bShuttingDown && pIB )
        {
            UpdateFrame( g_hWnd, pIB );

            bool boRun = true;
            while( boRun )
            {
                HRESULT hRet = g_pDDSPrimary->Flip( NULL, 0 );
                if( hRet == DD_OK )
                {
                    boRun = false;
                    continue;
                }
                if( hRet == DDERR_SURFACELOST )
                {
                    hRet = RestoreAll();
                    if( hRet != DD_OK )
                    {
                        boRun = false;
                        continue;
                    }
                }
                if( hRet != DDERR_WASSTILLDRAWING )
                {
                    boRun = false;
                    continue;
                }
            }
        }
    }
    virtual void RemoveImage( void )
    {
        // nothing to be done here!
    }
};

//-----------------------------------------------------------------------------
extern "C" IDirectDrawSurface7* DDCreateSurface( IDirectDraw7* pdd, int surfaceWidth, int surfaceHeight )
//-----------------------------------------------------------------------------
{
    DDSURFACEDESC2          ddsd;
    IDirectDrawSurface7*    pdds;

    ZeroMemory( &ddsd, sizeof( ddsd ) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth = surfaceWidth;
    ddsd.dwHeight = surfaceHeight;
    if( pdd->CreateSurface( &ddsd, &pdds, NULL ) != DD_OK )
    {
        return NULL;
    }
    return pdds;
}

//-----------------------------------------------------------------------------
static void ReleaseAllObjects( void )
//-----------------------------------------------------------------------------
{
    if( g_pDD != NULL )
    {
        if( g_pDDSPrimary != NULL )
        {
            g_pDDSPrimary->Release();
            g_pDDSPrimary = NULL;
        }
        if( g_pDDSOne != NULL )
        {
            g_pDDSOne->Release();
            g_pDDSOne = NULL;
        }
        if( g_pDDSTwo != NULL )
        {
            g_pDDSTwo->Release();
            g_pDDSTwo = NULL;
        }
        g_pDD->Release();
        g_pDD = NULL;
    }
}

//-----------------------------------------------------------------------------
HRESULT InitFail( HWND hWnd, HRESULT hRet, LPCTSTR szError, ... )
//-----------------------------------------------------------------------------
{
    const size_t BUF_SIZE = 128;
    char szBuff[BUF_SIZE];
    va_list vl;

    va_start( vl, szError );
#if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
    vsprintf_s( szBuff, BUF_SIZE, szError, vl );
#else
    _vsnprintf( szBuff, BUF_SIZE, szError, vl );
#endif // #if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
    ReleaseAllObjects();
    MessageBox( hWnd, szBuff, TITLE, MB_OK );
    DestroyWindow( hWnd );
    va_end( vl );
    return hRet;
}

//-----------------------------------------------------------------------------
void Cleanup( CaptureParamContainer& paramSets )
//-----------------------------------------------------------------------------
{
    const CaptureParamContainer::size_type cnt = paramSets.size();
    for( CaptureParamContainer::size_type i = 0; i < cnt; i++ )
    {
        delete paramSets[i];
    }
}

//-----------------------------------------------------------------------------
void ConfigureDeviceAutoSwitchTimer( HWND hWnd, BOOL boShouldBeActive )
//-----------------------------------------------------------------------------
{
    if( g_bDeviceAutoSwitchTimerActive )
    {
        KillTimer( hWnd, TIMER_ID_DEVICE_AUTO_SWITCH );
    }

    if( boShouldBeActive )
    {
        SetTimer( hWnd, TIMER_ID_DEVICE_AUTO_SWITCH, g_deviceAutoSwitchRate_ms, NULL );
    }
    g_bDeviceAutoSwitchTimerActive = boShouldBeActive;
}

//-----------------------------------------------------------------------------
void SelectDevice( unsigned int index )
//-----------------------------------------------------------------------------
{
    if( g_paramSets.empty() )
    {
        return;
    }

    if( index >= g_paramSets.size() )
    {
        DbOutput( "SelectDevice: invalid device selection. Defaulting to 0.\n" );
        index = 0;
    }

    if( g_paramSets[g_currentParamset]->isThreadRunning() )
    {
        DbOutput( "SelectDevice(%d): De-selecting device %s\n", __LINE__, g_paramSets[g_currentParamset]->device()->serial.read().c_str() );
        g_paramSets[g_currentParamset]->stopThread();
    }

    // clear both display buffers
    if( g_paramSets[index]->getDisplay() )
    {
        g_paramSets[index]->getDisplay()->SetImage( g_NullImage.getBuffer() );
        g_paramSets[index]->getDisplay()->SetImage( g_NullImage.getBuffer() );
    }

    if( index < g_paramSets.size() )
    {
        DbOutput( "SelectDevice(%d): Selecting device %s\n", __LINE__, g_paramSets[index]->device()->serial.read().c_str() );
        g_paramSets[index]->startThread();
        g_currentParamset = index;
    }
}

//-----------------------------------------------------------------------------
void SelectNextDevice( void )
//-----------------------------------------------------------------------------
{
    if( g_paramSets.size() > 1 )
    {
        SelectDevice( static_cast<unsigned int>( ( g_currentParamset + 1 ) % g_paramSets.size() ) );
    }
}

//-----------------------------------------------------------------------------
long FAR PASCAL WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
//-----------------------------------------------------------------------------
{
    switch( message )
    {
    case WM_ACTIVATE:
        // Pause if minimized
        g_bActive = !( ( BOOL )HIWORD( wParam ) );
        return 0L;
    case WM_DESTROY:
        // Clean up and close the application
        ReleaseAllObjects();
        PostQuitMessage( 0 );
        return 0L;
    case WM_KEYDOWN:
        // Handle any non-accelerated key commands
        switch( wParam )
        {
        case VK_F1:
            SelectNextDevice();
            return 0L;
        case VK_F2:
            if( g_currentParamset < g_paramSets.size() )
            {
                g_paramSets[g_currentParamset]->isThreadRunning() ? g_paramSets[g_currentParamset]->stopThread() : g_paramSets[g_currentParamset]->startThread();
            }
            return 0L;
        case VK_F3:
            g_bHelpOverlayActive = !g_bHelpOverlayActive;
            return 0L;
        case VK_F4:
            g_TextOutOn = !g_TextOutOn;
            return 0L;
        case VK_F5:
            if( g_currentParamset < g_paramSets.size() )
            {
                g_paramSets[g_currentParamset]->selectNextVideoChannel();
            }
            return 0L;
        //case VK_F6:
        //  return 0L;
        //case VK_F7:
        //  return 0L;
        //case VK_F8:
        //  return 0L;
        case VK_F9:
            ConfigureDeviceAutoSwitchTimer( hWnd, !g_bDeviceAutoSwitchTimerActive );
            return 0L;
        case VK_ADD:
            if( g_deviceAutoSwitchRate_ms < ( UINT_MAX - g_deviceAutoSwitchRateInc_ms ) )
            {
                g_deviceAutoSwitchRate_ms += g_deviceAutoSwitchRateInc_ms;
                ConfigureDeviceAutoSwitchTimer( hWnd, g_bDeviceAutoSwitchTimerActive );
            }
            return 0L;
        case VK_SUBTRACT:
            if( g_deviceAutoSwitchRate_ms > g_deviceAutoSwitchRateInc_ms )
            {
                g_deviceAutoSwitchRate_ms -= g_deviceAutoSwitchRateInc_ms;
                ConfigureDeviceAutoSwitchTimer( hWnd, g_bDeviceAutoSwitchTimerActive );
            }
            return 0L;
        case VK_ESCAPE:
            g_bShuttingDown = TRUE;
            KillTimer( hWnd, TIMER_ID );
            if( g_bDeviceAutoSwitchTimerActive )
            {
                KillTimer( hWnd, TIMER_ID_DEVICE_AUTO_SWITCH );
            }
            if( ( g_currentParamset < g_paramSets.size() ) &&
                ( g_paramSets[g_currentParamset]->isThreadRunning() ) )
            {
                g_paramSets[g_currentParamset]->stopThread();
            }
            PostMessage( hWnd, WM_CLOSE, 0, 0 );
            return 0L;
        }
        break;
    case WM_SETCURSOR:
        // Turn off the cursor since this is a full-screen application
        SetCursor( NULL );
        return TRUE;
    case WM_TIMER:
        if( g_bActive && !g_bShuttingDown )
        {
            if( TIMER_ID == wParam )
            {
                if( ( g_currentParamset < g_paramSets.size() ) &&
                    ( g_paramSets[g_currentParamset]->isThreadRunning() ) )
                {
                    const Statistics& s = g_paramSets[g_currentParamset]->statistics();
                    g_info = BuildFormattedString( "current frame rate: %.2f Hz, overall images captured: %d", s.framesPerSecond.read(), s.frameCount.read() );
                }
                else
                {
                    g_info = string( "" );
                }
            }
            else if( TIMER_ID_DEVICE_AUTO_SWITCH == wParam )
            {
                SelectNextDevice();
            }
        }
        break;
    }
    return static_cast<long>( DefWindowProc( hWnd, message, wParam, lParam ) );
}

//-----------------------------------------------------------------------------
static HRESULT InitApp( HINSTANCE hInstance, int nCmdShow )
//-----------------------------------------------------------------------------
{
    WNDCLASS                    wc;
    DDSURFACEDESC2              ddsd;
    DDSCAPS2                    ddscaps;
    HRESULT                     hRet;

    // Set up and register window class
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>( WindowProc );
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_MAIN_ICON ) );
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = ( HBRUSH )GetStockObject( BLACK_BRUSH );
    wc.lpszMenuName = NAME;
    wc.lpszClassName = NAME;
    RegisterClass( &wc );

    // Create a window
    g_hWnd = CreateWindowEx( WS_EX_TOPMOST,
                             NAME,
                             TITLE,
                             WS_POPUP,
                             0,
                             0,
                             g_wScr,
                             g_hScr,
                             NULL,
                             NULL,
                             hInstance,
                             NULL );
    if( !g_hWnd )
    {
        return FALSE;
    }
    ShowWindow( g_hWnd, nCmdShow );
    UpdateWindow( g_hWnd );
    SetFocus( g_hWnd );

    ///////////////////////////////////////////////////////////////////////////
    // Create the main DirectDraw object
    ///////////////////////////////////////////////////////////////////////////
    hRet = DirectDrawCreateEx( NULL, ( VOID** )&g_pDD, IID_IDirectDraw7, NULL );
    if( hRet != DD_OK )
    {
        return InitFail( g_hWnd, hRet, "DirectDrawCreateEx FAILED" );
    }

    // Get exclusive mode
    hRet = g_pDD->SetCooperativeLevel( g_hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
    if( hRet != DD_OK )
    {
        return InitFail( g_hWnd, hRet, "SetCooperativeLevel FAILED" );
    }

    // Set the video mode to g_wScr*g_hScr*32
    hRet = g_pDD->SetDisplayMode( g_wScr, g_hScr, 32, 0, 0 );
    if( hRet != DD_OK )
    {
        return InitFail( g_hWnd, hRet, "SetDisplayMode FAILED" );
    }

    // Create the primary surface with 1 back buffer
    ZeroMemory( &ddsd, sizeof( ddsd ) );
    ddsd.dwSize = sizeof( ddsd );
    ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE |
                          DDSCAPS_FLIP |
                          DDSCAPS_COMPLEX;
    ddsd.dwBackBufferCount = 1;
    hRet = g_pDD->CreateSurface( &ddsd, &g_pDDSPrimary, NULL );
    if( hRet != DD_OK )
    {
        return InitFail( g_hWnd, hRet, "CreateSurface FAILED" );
    }

    // Get a pointer to the back buffer
    ZeroMemory( &ddscaps, sizeof( ddscaps ) );
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
    hRet = g_pDDSPrimary->GetAttachedSurface( &ddscaps, &g_pDDSBack );
    if( hRet != DD_OK )
    {
        return InitFail( g_hWnd, hRet, "GetAttachedSurface FAILED" );
    }

    // Create an offscreen bitmap.
    g_pDDSOne = DDCreateSurface( g_pDD, g_wScr, g_hScr );
    if( g_pDDSOne == NULL )
    {
        return InitFail( g_hWnd, hRet, "Creation of surface 1 failed" );
    }
    g_pDDSTwo = DDCreateSurface( g_pDD, g_wScr, g_hScr );
    if( g_pDDSTwo == NULL )
    {
        return InitFail( g_hWnd, hRet, "Creation of surface 2 failed" );
    }

    if( TIMER_ID != SetTimer( g_hWnd, TIMER_ID, TIMER_RATE, NULL ) )
    {
        return InitFail( g_hWnd, hRet, "SetTimer FAILED" );
    }

    return DD_OK;
}

//-----------------------------------------------------------------------------
int PASCAL WinMain( HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int nCmdShow )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;

    DbOutput( "This sample is meant for devices that support multiple video channels only. Other devices might be installed.\n" );
    DbOutput( "but won't be recognized by the application.\n" );
    DbOutput( "\n" );

    vector<Device*> validDevices;
    getValidDevices( devMgr, validDevices );
    if( validDevices.empty() )
    {
        DbOutput( "No valid device detected." );
        return 0;
    }

    MyCaptureDisplay resultingImageDisplay;
    const vector<Device*>::size_type devCnt = validDevices.size();
    for( vector<Device*>::size_type i = 0; i < devCnt; i++ )
    {
        DbOutput( "Initialising device %s. This might take some time...\n", validDevices[i]->serial.read().c_str() );
        try
        {
            // if this device offers the 'GenICam' interface switch it on, as this will
            // allow are better control over GenICam compliant devices
            conditionalSetProperty( validDevices[i]->interfaceLayout, dilGenICam );
            // if this device offers a user defined acquisition start/stop behaviour
            // enable it as this allows finer control about the streaming behaviour
            conditionalSetProperty( validDevices[i]->acquisitionStartStopBehaviour, assbUser );
            validDevices[i]->open();
        }
        catch( const ImpactAcquireException& e )
        {
            // this e.g. might happen if the same device is already opened in another process...
            DbOutput( "An error occurred while opening the device %s(error code: %s)\n", validDevices[i]->serial.read().c_str(), ImpactAcquireException::getErrorCodeAsString( e.getErrorCode() ).c_str() );
            continue;
        }

        g_paramSets.push_back( new CaptureParameters( validDevices[i] ) );
        g_paramSets.back()->attachDisplay( &resultingImageDisplay );
    }

    if( g_paramSets.empty() )
    {
        DbOutput( "NO compatible device could be initialised. Unable to continue!\n" );
        return 0;
    }

    if( InitApp( hInstance, nCmdShow ) != DD_OK )
    {
        return FALSE;
    }

    memset( g_NullImage.getBuffer()->vpData, 0, g_NullImage.getBuffer()->iSize );
    SelectDevice( 0 );

    MSG msg;
    while( GetMessage( &msg, NULL, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    Cleanup( g_paramSets );
    return static_cast<int>( msg.wParam );
}
