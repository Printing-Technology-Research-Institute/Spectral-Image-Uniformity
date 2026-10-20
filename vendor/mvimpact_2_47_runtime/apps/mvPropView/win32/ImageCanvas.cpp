#include <algorithm>
#include "../ImageCanvas.h"
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>
#undef Yield // unfortunately 'winbase.h' defines a macro 'Yield' and wxWidgets defines a function 'Yield' in 'ax/app.h'
#include "../PropViewFrame.h"

#undef min // otherwise we can't work with the 'numeric_limits' template here as someone might define a macro 'min'
#undef max // otherwise we can't work with the 'numeric_limits' template here as someone might define a macro 'max'

using namespace std;
using namespace mvIMPACT::acquire;

//-----------------------------------------------------------------------------
void CopyImage( unsigned char* pDst, const unsigned char* pSrc, const int srcWidth, const int srcHeight, const int srcPitch, const int srcInc, const int srcOrder[3] )
//-----------------------------------------------------------------------------
{
    for( int y = 0; y < srcHeight; y++ )
    {
        const unsigned char* pLine = pSrc + y * srcPitch;
        for( int x = 0; x < srcWidth; x++ )
        {
            *pDst++ = pLine[srcOrder[0]];
            *pDst++ = pLine[srcOrder[1]];
            *pDst++ = pLine[srcOrder[2]];
            pLine += srcInc;
        }
    }
}

//-----------------------------------------------------------------------------
struct ImageCanvasImpl
//-----------------------------------------------------------------------------
{
    mvIMPACT::acquire::display::ImageDisplay* pImageDisplay;
    HBRUSH hBackgroundBrush;
    wxWindow* pParent;
    ImageCanvasImpl( wxWindow* p ) : hBackgroundBrush( 0 ), pParent( p )
    {
        pImageDisplay = new mvIMPACT::acquire::display::ImageDisplay( reinterpret_cast<WindowHandle>( pParent->GetHandle() ) );
        pImageDisplay->SetImage( ( ImageBuffer* )0 );
        pImageDisplay->SetDisplayMode( mvIMPACT::acquire::display::DM_Default );
        hBackgroundBrush = CreateHatchBrush( HS_DIAGCROSS, GetSysColor( COLOR_APPWORKSPACE ) );
        pImageDisplay->SetBackgroundBrush( hBackgroundBrush );
    }
    ~ImageCanvasImpl()
    {
        delete pImageDisplay;
        DeleteObject( hBackgroundBrush );
    }
};

//-----------------------------------------------------------------------------
ImageCanvas::ImageCanvas( wxWindow* pApp, wxWindow* parent, wxWindowID id /* = -1 */, const wxPoint& pos /* = wxDefaultPosition */,
                          const wxSize& size /* = wxDefaultSize */, long style /* = wxBORDER_NONE */, const wxString& name /* = "ImageCanvas" */, bool boActive /* = true */ )
    : DrawingCanvas( parent, id, pos, size, style, name, boActive ), m_boSupportsFullScreenMode( true ), m_boSupportsDifferentScalingModes( true )
//-----------------------------------------------------------------------------
{
    Init( pApp );
    m_pImpl = new ImageCanvasImpl( this );
}

//-----------------------------------------------------------------------------
ImageCanvas::~ImageCanvas()
//-----------------------------------------------------------------------------
{
    Shutdown();
    delete m_pImpl;
}

//-----------------------------------------------------------------------------
void ImageCanvas::DecreaseShiftValue( void )
//-----------------------------------------------------------------------------
{
    m_pImpl->pImageDisplay->SetShift( m_pImpl->pImageDisplay->GetShift() - 1 );
    Refresh( true );
}

//-----------------------------------------------------------------------------
int ImageCanvas::GetAppliedShiftValue( void ) const
//-----------------------------------------------------------------------------
{
    return m_pImpl->pImageDisplay->GetAppliedShift();
}

//-----------------------------------------------------------------------------
ImageCanvas::TSaveResult ImageCanvas::GetCurrentImage( wxImage& img, int* pBitsPerPixel /* = 0 */ ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    const void* pData = 0;
    int pitch = 0;
    int bipp = 0;
    m_pImpl->pImageDisplay->GetImage( &pData, 0, 0, &bipp, &pitch );
    if( !pData )
    {
        return srNoImage;
    }

    img = wxImage( m_pIB->iWidth, m_pIB->iHeight, false );
    if( !img.IsOk() )
    {
        return srFailedToSave;
    }

    switch( bipp )
    {
    case 8:
        {
            const int srcOrder[3] = { 0, 0, 0 };
            CopyImage( img.GetData(), static_cast<const unsigned char*>( pData ), m_pIB->iWidth, m_pIB->iHeight, pitch, 1, srcOrder );
        }
        break;
    case 24:
        {
            const int srcOrder[3] = { 2, 1, 0 };
            CopyImage( img.GetData(), static_cast<const unsigned char*>( pData ), m_pIB->iWidth, m_pIB->iHeight, pitch, 3, srcOrder );
        }
        break;
    case 32:
        {
            const int srcOrder[3] = { 2, 1, 0 };
            CopyImage( img.GetData(), static_cast<const unsigned char*>( pData ), m_pIB->iWidth, m_pIB->iHeight, pitch, 4, srcOrder );
        }
        break;
    default:
        return srFailedToSave;
    }

    if( pBitsPerPixel )
    {
        *pBitsPerPixel = bipp;
    }

    return srOK;
}

//-----------------------------------------------------------------------------
wxPoint ImageCanvas::GetScaledMousePos( int mouseXPos, int mouseYPos ) const
//-----------------------------------------------------------------------------
{
    if( m_boScaleToClientSize )
    {
        return wxPoint( static_cast<int>( ( mouseXPos - m_lastStartPoint.x ) / m_lastScaleFactor ),
                        static_cast<int>( ( mouseYPos - m_lastStartPoint.y ) / m_lastScaleFactor ) );
    }

    return wxPoint( ( m_lastStartPoint.x >= 0 ) ? static_cast<int>( ( mouseXPos - m_lastStartPoint.x ) / m_lastScaleFactor ) : static_cast<int>( ( mouseXPos / m_lastScaleFactor ) - m_lastStartPoint.x ),
                    ( m_lastStartPoint.y >= 0 ) ? static_cast<int>( ( mouseYPos - m_lastStartPoint.y ) / m_lastScaleFactor ) : static_cast<int>( ( mouseYPos / m_lastScaleFactor ) - m_lastStartPoint.y ) );
}

//-----------------------------------------------------------------------------
int ImageCanvas::GetShiftValue( void ) const
//-----------------------------------------------------------------------------
{
    return m_pImpl->pImageDisplay->GetShift();
}

//-----------------------------------------------------------------------------
void ImageCanvas::IncreaseShiftValue( void )
//-----------------------------------------------------------------------------
{
    m_pImpl->pImageDisplay->SetShift( m_pImpl->pImageDisplay->GetShift() + 1 );
    Refresh( true );
}

//-----------------------------------------------------------------------------
bool ImageCanvas::IsFullScreen( void ) const
//-----------------------------------------------------------------------------
{
    return m_pImpl->pImageDisplay->GetDisplayMode() == mvIMPACT::acquire::display::DM_FullScreen;
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPaint( wxPaintEvent& )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    wxPaintDC dc( this );

    if( !IsActive() || !m_pIB )
    {
        m_boRefreshInProgress = false;
        return;
    }

    bool boIsFullScreen = IsFullScreen();
    wxCoord clientWidth( 0 ), clientHeight( 0 );
    if( boIsFullScreen )
    {
        clientWidth = GetSystemMetrics( SM_CXSCREEN );
        clientHeight = GetSystemMetrics( SM_CYSCREEN );
    }
    else
    {
        dc.GetSize( &clientWidth, &clientHeight );
    }

    int imageHeight = m_pIB->iHeight;
    int imageWidth = m_pIB->iWidth;
    if( ( imageHeight > 0 ) && ( imageWidth > 0 ) )
    {
        int blitXOffsetInClient = 0, blitYOffsetInClient = 0;
        double scaleFactor = 1.0;
        const bool boMustScale = m_boScaleToClientSize || ( m_currentZoomFactor != 1.0 );
        if( m_boScaleToClientSize )
        {
            double scaleX = static_cast<double>( clientWidth ) / static_cast<double>( imageWidth );
            double scaleY = static_cast<double>( clientHeight ) / static_cast<double>( imageHeight );
            // always keep the aspect ratio
            scaleFactor = ( scaleX <= scaleY ) ? scaleX : scaleY;
        }
        else
        {
            if( m_currentZoomFactor > 1.0 )
            {
                const double pixelsFittingOnCanvasX = round( static_cast< double >( clientWidth ) / m_currentZoomFactor );
                const double pixelsFittingOnCanvasY = round( static_cast< double >( clientHeight ) / m_currentZoomFactor );
                double scaleX = static_cast<double>( clientWidth ) / pixelsFittingOnCanvasX;
                double scaleY = static_cast<double>( clientHeight ) / pixelsFittingOnCanvasY;
                // Use the smaller scale so that the selected image part fits into the client area
                if( scaleX > scaleY )
                {
                    blitXOffsetInClient = static_cast< int >( clientWidth - ( pixelsFittingOnCanvasX * scaleY ) ) / 2;
                }
                else
                {
                    blitYOffsetInClient = static_cast< int >( clientHeight - ( pixelsFittingOnCanvasY * scaleX ) ) / 2;
                }
                // always keep the aspect ratio
                scaleFactor = ( scaleX <= scaleY ) ? scaleX : scaleY;
            }
            else
            {
                scaleFactor = m_currentZoomFactor;
            }
        }

        if( !boIsFullScreen )
        {
            m_pImpl->pImageDisplay->SetDisplayMode( ( boMustScale ) ? mvIMPACT::acquire::display::DM_Default : mvIMPACT::acquire::display::DM_Fastest );
        }
        double scaledImageWidth = imageWidth * scaleFactor;
        double scaledImageHeight = imageHeight * scaleFactor;
        if( clientWidth > scaledImageWidth )
        {
            blitXOffsetInClient = static_cast<int>( clientWidth - scaledImageWidth ) / 2;
        }

        if( clientHeight > scaledImageHeight )
        {
            blitYOffsetInClient = static_cast<int>( clientHeight - scaledImageHeight ) / 2;
        }

        int viewXStart, viewYStart;
        GetViewStart( &viewXStart, &viewYStart );
        int areaWidth = 0;
        int areaHeight = 0;
        const bool boImageWidthFitsIntoClient = ( static_cast<int>( scaledImageWidth ) <= clientWidth );
        const bool boImageHeightFitsIntoClient = ( static_cast<int>( scaledImageHeight ) <= clientHeight );

        if( ( m_currentZoomFactor > 1.0 ) || boMustScale || boIsFullScreen )
        {
            areaWidth = boImageWidthFitsIntoClient ? imageWidth : static_cast<int>( static_cast<double>( clientWidth ) / scaleFactor );
            areaHeight = boImageHeightFitsIntoClient ? imageHeight : static_cast<int>( static_cast<double>( clientHeight ) / scaleFactor );
            m_pImpl->pImageDisplay->SetBitmapRectangle( viewXStart, viewYStart, areaWidth, areaHeight );
        }
        else
        {
            m_pImpl->pImageDisplay->SetBitmapRectangle( viewXStart,
                    ( imageHeight > clientHeight ) ? imageHeight - viewYStart - clientHeight : 0,
                    imageWidth,
                    imageHeight );
        }

        m_lastStartPoint.x = boImageWidthFitsIntoClient ? blitXOffsetInClient : -viewXStart;
        m_lastStartPoint.y = boImageHeightFitsIntoClient ? blitYOffsetInClient : -viewYStart;
        m_lastScaleFactor = scaleFactor;

        if( scaleFactor > 1.0 )
        {
            m_pImpl->pImageDisplay->SetDisplayRectangle( blitXOffsetInClient, blitYOffsetInClient, static_cast<int>( clientWidth - ( 2 * blitXOffsetInClient ) ), static_cast<int>( clientHeight - ( 2 * blitYOffsetInClient ) ) );
        }
        else
        {
            m_pImpl->pImageDisplay->SetDisplayRectangle( blitXOffsetInClient, blitYOffsetInClient, static_cast<int>( scaledImageWidth ), static_cast<int>( scaledImageHeight ) );
        }

        m_pImpl->pImageDisplay->Update();

        if( !boIsFullScreen && m_pMonitorDisplay && m_pVisiblePartOfImage )
        {
            m_pVisiblePartOfImage->m_rect = wxRect( ( static_cast<wxCoord>( scaledImageWidth ) > clientWidth ) ? viewXStart : 0,
                                                    ( static_cast<wxCoord>( scaledImageHeight ) > clientHeight ) ? viewYStart : 0,
                                                    ( static_cast<wxCoord>( scaledImageWidth ) > clientWidth ) ? static_cast<int>( clientWidth / scaleFactor ) : imageWidth,
                                                    ( static_cast<wxCoord>( scaledImageHeight ) > clientHeight ) ? static_cast<int>( clientHeight / scaleFactor ) : imageHeight );
            m_pMonitorDisplay->Refresh( false );
        }

        // this would draw the image data on top of the image, however this is kind of slow and clumsy so far
        //if( m_currentZoomFactor >= 32.0 )
        //{
        //  for( int i=0; i<areaWidth; i++ )
        //  {
        //      for( int j=0; j<areaHeight; j++ )
        //      {
        //          switch( m_pIB->pixelFormat )
        //          {
        //          case ibpfMono8:
        //              {
        //                  unsigned char* pData = static_cast<unsigned char*>(m_pIB->vpData) + ( ( viewYStart + j ) * m_pIB->pChannels[0].iLinePitch ) + ( ( viewXStart + i ) * m_pIB->iBytesPerPixel );
        //                  dc.DrawText( wxString::Format( wxT("%d"), *pData ), ((static_cast<double>(i) + 0.5)*m_currentZoomFactor)-2, ((static_cast<double>(j) + 0.5)*m_currentZoomFactor)-2 );
        //              }
        //              break;
        //          }
        //      }
        //  }
        //}

        if( !boIsFullScreen )
        {
            wxWindowDC wdc( this );
            BlitAOIs( wdc, scaleFactor, static_cast<int>( blitXOffsetInClient - ( viewXStart * scaleFactor ) ), static_cast<int>( blitYOffsetInClient - ( viewYStart * scaleFactor ) ), imageWidth, imageHeight );
            BlitPerformanceMessages( wdc, blitXOffsetInClient, blitYOffsetInClient, m_pIB->pixelFormat );
            BlitInfoStrings( wdc, scaleFactor, static_cast<int>( blitXOffsetInClient - ( viewXStart * scaleFactor ) ), static_cast<int>( blitYOffsetInClient - ( viewYStart * scaleFactor ) ), blitXOffsetInClient, blitYOffsetInClient, imageWidth, imageHeight );
            // restore old brush
            wdc.SetBrush( wxNullBrush );
        }
    }
    else
    {
        m_pImpl->pImageDisplay->SetDisplayRectangle( 0, 0, 0, 0 );
        m_pImpl->pImageDisplay->Update();
        if( !boIsFullScreen )
        {
            wxWindowDC wdc( this );
            BlitInfoStrings( wdc, 1.0, 10, 10, 10, 10, imageWidth, imageHeight );
        }
        m_lastStartPoint.x = -1;
        m_lastStartPoint.y = -1;
        m_lastScaleFactor = -1.0;
    }

    if( boIsFullScreen && m_pImpl->pImageDisplay->GetLastError() != IDE_NoError )
    {
        wxCommandEvent eEndFullScreenMode( endFullScreenMode, m_pApp->GetId() );
        ::wxPostEvent( m_pApp->GetEventHandler(), eEndFullScreenMode );
    }

    m_boRefreshInProgress = false;
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPopUpFullScreen( wxCommandEvent& e )
//-----------------------------------------------------------------------------
{
    SetFullScreenMode( e.IsChecked() );
}

//-----------------------------------------------------------------------------
void ImageCanvas::RefreshScrollbars( bool boMoveToMousePos /* = false */ )
//-----------------------------------------------------------------------------
{
    if( m_pIB && !m_boScaleToClientSize )
    {
        int clientWidth, clientHeight;
        GetClientSize( &clientWidth, &clientHeight );
        const int xScrollUnits = ( clientWidth < m_pIB->iWidth * m_currentZoomFactor ) ? clientWidth + m_pIB->iWidth - static_cast<int>( ( static_cast<double>( clientWidth ) / m_currentZoomFactor ) ) : clientWidth;
        const int yScrollUnits = ( clientHeight < m_pIB->iHeight * m_currentZoomFactor ) ? clientHeight + m_pIB->iHeight - static_cast<int>( ( static_cast<double>( clientHeight ) / m_currentZoomFactor ) ) : clientHeight;
        SetScrollbars( 1, 1, xScrollUnits, yScrollUnits );
        if( boMoveToMousePos )
        {
            const int x = ( xScrollUnits == clientWidth ) ? 0 : m_lastMousePos.x;
            const int y = ( yScrollUnits == clientHeight ) ? 0 : m_lastMousePos.y;
            const int xScrollUnitsFull = ( clientWidth < m_pIB->iWidth * m_currentZoomFactor ) ? m_pIB->iWidth - static_cast<int>( ( static_cast<double>( clientWidth ) / m_currentZoomFactor ) ) : clientWidth;
            const int yScrollUnitsFull = ( clientHeight < m_pIB->iHeight * m_currentZoomFactor ) ? m_pIB->iHeight - static_cast<int>( ( static_cast<double>( clientHeight ) / m_currentZoomFactor ) ) : clientHeight;
            const int xScrollPos = static_cast<int>( static_cast<double>( x ) * static_cast<double>( xScrollUnitsFull ) / static_cast<double>( m_pIB->iWidth ) );
            const int yScrollPos = static_cast<int>( static_cast<double>( y ) * static_cast<double>( yScrollUnitsFull ) / static_cast<double>( m_pIB->iHeight ) );
            Scroll( xScrollPos, yScrollPos );
        }
    }
    else
    {
        SetScrollbars( 1, 1, 0, 0 );
    }
}

//-----------------------------------------------------------------------------
ImageCanvas::TSaveResult ImageCanvas::SaveCurrentImage( const wxString& filenameAndPath, const wxString& extension ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );

    wxImage img;
    int bitsPerPixel = 0;
    const ImageCanvas::TSaveResult result = GetCurrentImage( img, &bitsPerPixel );
    if( result != srOK )
    {
        return result;
    }

    if( ( bitsPerPixel == 8 ) && ( extension == wxT( "bmp" ) ) )
    {
        img.SetOption( wxIMAGE_OPTION_BMP_FORMAT, wxBMP_8BPP_RED );
    }
    return StoreImage( img, filenameAndPath, extension );
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetFullScreenMode( bool boActive )
//-----------------------------------------------------------------------------
{
    delete m_pImpl;
    if( boActive )
    {
        m_pImpl = new ImageCanvasImpl( m_pApp );
        m_pImpl->pImageDisplay->SetDisplayMode( mvIMPACT::acquire::display::DM_FullScreen );
    }
    else
    {
        m_pImpl = new ImageCanvasImpl( this );
    }
    wxCommandEvent e( imageCanvasFullScreen, m_pApp->GetId() );
    e.SetInt( boActive ? 1 : 0 );
    ::wxPostEvent( m_pApp->GetEventHandler(), e );
}

//-----------------------------------------------------------------------------
bool ImageCanvas::SetImage( const ImageBuffer* pIB, unsigned int bufferPartIndex, bool boMustRefresh /* = true */ )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );

    // changing the pixel format should reset the shift factor
    if( ( m_pIB && pIB && ( m_pIB->pixelFormat != pIB->pixelFormat ) ) ||
        !m_pIB ||
        !pIB )
    {
        m_pImpl->pImageDisplay->SetShift( 0 );
    }

    if( !IsActive() )
    {
        m_pIB = pIB;
        return true;
    }

    int oldWidth = 0;
    int oldHeight = 0;
    int oldMaxDim = 0;
    if( m_pIB )
    {
        oldWidth = m_pIB->iWidth;
        oldHeight = m_pIB->iHeight;
        oldMaxDim = max( oldHeight, oldWidth );
    }
    m_pIB = pIB;
    m_bufferPartIndexDisplayed = bufferPartIndex;
    SetMaxZoomFactor( pIB, oldMaxDim );

    m_pImpl->pImageDisplay->SetImage( m_pIB );
    if( m_pIB && ( ( oldWidth != m_pIB->iWidth ) || ( oldHeight != m_pIB->iHeight ) ) )
    {
        RefreshScrollbars();
    }

    if( m_boRefreshInProgress )
    {
        ++m_skippedPaintEvents;
        return true;
    }

    if( boMustRefresh )
    {
        m_boRefreshInProgress = true;
        Refresh( false );
    }
    return true;
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetScalingMode( TScalingMode mode )
//-----------------------------------------------------------------------------
{
    m_scalingMode = mode;
    switch( m_scalingMode )
    {
    case smNearestNeighbour:
        m_pImpl->pImageDisplay->SetInterpolationMode( mvIMPACT::acquire::display::IM_NEAREST_NEIGHBOUR );
        break;
    case smLinear:
        m_pImpl->pImageDisplay->SetInterpolationMode( mvIMPACT::acquire::display::IM_LINEAR );
        break;
    case smCubic:
        m_pImpl->pImageDisplay->SetInterpolationMode( mvIMPACT::acquire::display::IM_CUBIC );
        break;
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetShiftValue( int value )
//-----------------------------------------------------------------------------
{
    if( m_pImpl->pImageDisplay->GetShift() != value )
    {
        m_pImpl->pImageDisplay->SetShift( value );
        Refresh( true );
    }
}
