#include <algorithm>
#include "ImageCanvas.h"
#include "PropViewFrame.h"

#include <apps/Common/wxIncludePrologue.h>
#include <wx/dcbuffer.h>
#include <wx/clipbrd.h>
#include <wx/settings.h>
#ifdef USE_RAW_BITMAP_SUPPORT
#   include <wx/rawbmp.h>
#else
#   include <wx/image.h>
#endif // USE_RAW_BITMAP_SUPPORT
#include <apps/Common/wxIncludeEpilogue.h>

#ifdef _OPENMP
#   include <omp.h>
#endif // #ifdef _OPENMP

#ifdef USE_RAW_BITMAP_SUPPORT
typedef wxPixelData<wxBitmap, wxNativePixelFormat> PixelData;
#endif // USE_RAW_BITMAP_SUPPORT

using namespace std;

//-----------------------------------------------------------------------------
struct ImageCanvasImpl
//-----------------------------------------------------------------------------
{
    const wxBrush backgroundBrush;
    wxBitmap currentImage;
    TImageBufferPixelFormat format;
    int currentShiftValue;
    int appliedShiftValue;
    ImageCanvasImpl() : backgroundBrush(
#ifdef __GNUC__
            wxColour( wxT( "GREY" ) ), // needed on Linux to get the same ugly grey pattern as on Windows
#else
            wxSystemSettings::GetColour( wxSYS_COLOUR_APPWORKSPACE ),
#endif
            wxBRUSHSTYLE_CROSSDIAG_HATCH ), format( ibpfRaw ), currentShiftValue( 0 ), appliedShiftValue( 0 ) {}
    int GetShift( int maxShift )
    {
        appliedShiftValue = ( maxShift > currentShiftValue ) ? maxShift - currentShiftValue : 0;
        return appliedShiftValue;
    }
};

//-----------------------------------------------------------------------------
template<typename _Ty>
inline void YUV2RGB8( int& r, int& g, int& b, _Ty y, _Ty u, _Ty v )
//-----------------------------------------------------------------------------
{
    const int OFFSET = 128;
    r = static_cast<int>( static_cast<double>( y )                                             + 1.140 * static_cast<double>( v - OFFSET ) );
    g = static_cast<int>( static_cast<double>( y ) - 0.394 * static_cast<double>( u - OFFSET ) - 0.581 * static_cast<double>( v - OFFSET ) );
    b = static_cast<int>( static_cast<double>( y ) + 2.032 * static_cast<double>( u - OFFSET ) );
    r = max( 0, min( r, 255 ) );
    g = max( 0, min( g, 255 ) );
    b = max( 0, min( b, 255 ) );
}

#ifdef USE_RAW_BITMAP_SUPPORT
//-----------------------------------------------------------------------------
void Copy2ByteMonoImage( const ImageBuffer* pIB, PixelData& data, unsigned int shift )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned short* pSrc = reinterpret_cast<unsigned short*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
        PixelData::Iterator p( data );
        p.OffsetY( data, y );
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            char val = static_cast<char>( max( 0, min( ( *pSrc ) >> shift, 255 ) ) );
            ++pSrc;
            p.Red() = val;
            p.Green() = val;
            p.Blue() = val;
        }
    }
}

//-----------------------------------------------------------------------------
void CopyRGBImage( const ImageBuffer* pIB, PixelData& data, const int order[3], unsigned int inc )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned char* pSrc = reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
        PixelData::Iterator p( data );
        p.OffsetY( data, y );
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            p.Red() = pSrc[order[0]];
            p.Green() = pSrc[order[1]];
            p.Blue() = pSrc[order[2]];
            pSrc += inc;
        }
    }
}

//-----------------------------------------------------------------------------
void Copy2ByteRGBImage( const ImageBuffer* pIB, PixelData& data, unsigned int shift, unsigned int inc )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned short* pSrc = reinterpret_cast<unsigned short*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
        PixelData::Iterator p( data );
        p.OffsetY( data, y );
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            p.Red() = static_cast<PixelData::Iterator::ChannelType>( max( 0, min( pSrc[2] >> shift, 255 ) ) );
            p.Green() = static_cast<PixelData::Iterator::ChannelType>( max( 0, min( pSrc[1] >> shift, 255 ) ) );
            p.Blue() = static_cast<PixelData::Iterator::ChannelType>( max( 0, min( pSrc[0] >> shift, 255 ) ) );
            pSrc += inc;
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertMono12PackedToMono8( _Ty conversionFunction, const ImageBuffer* pIB, PixelData& data, const int shift )
//-----------------------------------------------------------------------------
{
    /// \todo how can we deal with padding in x-direction here???
    const unsigned char* const pSrc = static_cast<unsigned char*>( pIB->vpData );
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        PixelData::Iterator p( data );
        p.OffsetY( data, y );
        const int pixOffset = y * pIB->iWidth;
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            const unsigned char value = static_cast<unsigned char>( max( 0, min( conversionFunction( pSrc, pixOffset + x ) >> shift, 255 ) ) );
            p.Red() = value;
            p.Green() = value;
            p.Blue() = value;
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertPackedYUV411_UYYVYYToRGB( const ImageBuffer* pIB, PixelData& data, const int shift = 0 )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        PixelData::Iterator p( data );
        p.OffsetY( data, line );
        _Ty* pSrc = reinterpret_cast<_Ty*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + line * pIB->pChannels[0].iLinePitch );
        _Ty* y = pSrc + 1;
        _Ty* u = pSrc;
        _Ty* v = pSrc + 3;
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            YUV2RGB8( r, g, b,
                      max( 0, min( *y >> shift, 255 ) ),
                      max( 0, min( *u >> shift, 255 ) ),
                      max( 0, min( *v >> shift, 255 ) ) );
            p.Red() = static_cast<PixelData::Iterator::ChannelType>( r );
            p.Green() = static_cast<PixelData::Iterator::ChannelType>( g );
            p.Blue() = static_cast<PixelData::Iterator::ChannelType>( b );
            y += ( x % 2 ) ? 2 : 1;
            if( ( x > 0 ) && ( ( x % 4 ) == 0 ) )
            {
                u += 6;
                v += 6;
            }
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertPackedYUV422ToRGB( const ImageBuffer* pIB, PixelData& data, const int shift = 0 )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        PixelData::Iterator p( data );
        p.OffsetY( data, line );
        _Ty* pSrc = reinterpret_cast<_Ty*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + line * pIB->pChannels[0].iLinePitch );
        _Ty* y = ( ( pIB->pixelFormat == ibpfYUV422Packed ) || ( pIB->pixelFormat == ibpfYUV422_10Packed ) ) ? pSrc     : pSrc + 1;
        _Ty* u = ( ( pIB->pixelFormat == ibpfYUV422Packed ) || ( pIB->pixelFormat == ibpfYUV422_10Packed ) ) ? pSrc + 1 : pSrc;
        _Ty* v = ( ( pIB->pixelFormat == ibpfYUV422Packed ) || ( pIB->pixelFormat == ibpfYUV422_10Packed ) ) ? pSrc + 3 : pSrc + 2;

        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            YUV2RGB8( r, g, b,
                      max( 0, min( *y >> shift, 255 ) ),
                      max( 0, min( *u >> shift, 255 ) ),
                      max( 0, min( *v >> shift, 255 ) ) );
            p.Red() = static_cast<PixelData::Iterator::ChannelType>( r );
            p.Green() = static_cast<PixelData::Iterator::ChannelType>( g );
            p.Blue() = static_cast<PixelData::Iterator::ChannelType>( b );
            y += 2;
            if( x & 1 )
            {
                u += 4;
                v += 4;
            }
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertPackedYUV444ToRGB( const ImageBuffer* pIB, PixelData& data, const int order[3], const int shift = 0 )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        PixelData::Iterator p( data );
        p.OffsetY( data, line );
        _Ty* pSrc = reinterpret_cast<_Ty*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + line * pIB->pChannels[0].iLinePitch );
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            YUV2RGB8( r, g, b,
                      max( 0, min( pSrc[order[0]] >> shift, 255 ) ),
                      max( 0, min( pSrc[order[1]] >> shift, 255 ) ),
                      max( 0, min( pSrc[order[2]] >> shift, 255 ) ) );
            pSrc += 3;
            p.Red() = static_cast<PixelData::Iterator::ChannelType>( r );
            p.Green() = static_cast<PixelData::Iterator::ChannelType>( g );
            p.Blue() = static_cast<PixelData::Iterator::ChannelType>( b );
        }
    }
}

//-----------------------------------------------------------------------------
void ConvertPlanarYUVToRGB( const ImageBuffer* pIB, PixelData& data )
//-----------------------------------------------------------------------------
{
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        PixelData::Iterator p( data );
        p.OffsetY( data, line );
        unsigned char* y = reinterpret_cast<unsigned char*>( pIB->vpData ) + pIB->pChannels[0].iLinePitch * line;
        unsigned char* u = reinterpret_cast<unsigned char*>( pIB->vpData ) + pIB->pChannels[1].iChannelOffset + pIB->pChannels[1].iLinePitch * line;
        unsigned char* v = reinterpret_cast<unsigned char*>( pIB->vpData ) + pIB->pChannels[2].iChannelOffset + pIB->pChannels[2].iLinePitch * line;
        for( int x = 0; x < pIB->iWidth; x++, p++ )
        {
            YUV2RGB8( r, g, b, *y, *u, *v );
            p.Red() = static_cast<PixelData::Iterator::ChannelType>( r );
            p.Green() = static_cast<PixelData::Iterator::ChannelType>( g );
            p.Blue() = static_cast<PixelData::Iterator::ChannelType>( b );
            ++y;
            if( x & 1 )
            {
                ++u;
                ++v;
            }
        }
    }
}
#else
//-----------------------------------------------------------------------------
void Copy2ByteMonoImage( const ImageBuffer* pIB, wxImage& img, unsigned int shift )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned short* pSrc = reinterpret_cast<unsigned short*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
        unsigned char* pDst = img.GetData() + dstPitch * y;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            const unsigned char val = static_cast<unsigned char>( max( 0, min( ( *pSrc ) >> shift, 255 ) ) );
            ++pSrc;
            *pDst++ = val;
            *pDst++ = val;
            *pDst++ = val;
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void CopyMono12PackedToMono8( _Ty conversionFunction, const ImageBuffer* pIB, wxImage& img, const int shift )
//-----------------------------------------------------------------------------
{
    /// \todo how can we deal with padding in x-direction here???
    const unsigned char* const pSrc = static_cast<unsigned char*>( pIB->vpData );
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        const int pixOffset = y * pIB->iWidth;
        unsigned char* pDst = img.GetData() + dstPitch * y;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            const unsigned char value = static_cast<unsigned char>( max( 0, min( conversionFunction( pSrc, pixOffset + x ) >> shift, 255 ) ) );
            *pDst++ = value;
            *pDst++ = value;
            *pDst++ = value;
        }
    }
}

//-----------------------------------------------------------------------------
void CopyBGR888Image( const ImageBuffer* pIB, wxImage& img )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned short* pSrc = reinterpret_cast<unsigned short*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
        unsigned char* pDst = img.GetData() + dstPitch * y;
        memcpy( pDst, pSrc, dstPitch );
    }
}

//-----------------------------------------------------------------------------
void CopyRGBImage( const ImageBuffer* pIB, wxImage& img, unsigned int inc )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned char* pSrc = reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
        unsigned char* pDst = img.GetData() + dstPitch * y;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            *pDst++ = pSrc[2];
            *pDst++ = pSrc[1];
            *pDst++ = pSrc[0];
            pSrc += inc;
        }
    }
}

//-----------------------------------------------------------------------------
void CopyRGBImage( const ImageBuffer* pIB, wxImage& img, const int order[3], unsigned int inc )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned char* pSrc = reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
        unsigned char* pDst = img.GetData() + dstPitch * y;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            *pDst++ = pSrc[order[0]];
            *pDst++ = pSrc[order[1]];
            *pDst++ = pSrc[order[2]];
            pSrc += inc;
        }
    }
}

//-----------------------------------------------------------------------------
void Copy2ByteRGBImage( const ImageBuffer* pIB, wxImage& img, unsigned int shift, unsigned int inc )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int y = 0; y < pIB->iHeight; y++ )
    {
        unsigned short* pSrc = reinterpret_cast<unsigned short*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
        unsigned char* pDst = img.GetData() + dstPitch * y;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            *pDst++ = static_cast<unsigned char>( max( 0, min( pSrc[2] >> shift, 255 ) ) );
            *pDst++ = static_cast<unsigned char>( max( 0, min( pSrc[1] >> shift, 255 ) ) );
            *pDst++ = static_cast<unsigned char>( max( 0, min( pSrc[0] >> shift, 255 ) ) );
            pSrc += inc;
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertPackedYUV411_UYYVYYToRGB( const ImageBuffer* pIB, wxImage& img, const int shift = 0 )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        _Ty* pSrc = reinterpret_cast<_Ty*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + line * pIB->pChannels[0].iLinePitch );
        _Ty* y = pSrc + 1;
        _Ty* u = pSrc;
        _Ty* v = pSrc + 3;
        unsigned char* pDst = img.GetData() + dstPitch * line;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            YUV2RGB8( r, g, b,
                      max( 0, min( *y >> shift, 255 ) ),
                      max( 0, min( *u >> shift, 255 ) ),
                      max( 0, min( *v >> shift, 255 ) ) );
            *pDst++ = static_cast<unsigned char>( r );
            *pDst++ = static_cast<unsigned char>( g );
            *pDst++ = static_cast<unsigned char>( b );
            y += ( x % 2 ) ? 2 : 1;
            if( ( x > 0 ) && ( ( x % 4 ) == 0 ) )
            {
                u += 6;
                v += 6;
            }
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertPackedYUV422ToRGB( const ImageBuffer* pIB, wxImage& img, const int shift = 0 )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        _Ty* pSrc = reinterpret_cast<_Ty*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + line * pIB->pChannels[0].iLinePitch );
        _Ty* y = ( ( pIB->pixelFormat == ibpfYUV422Packed ) || ( pIB->pixelFormat == ibpfYUV422_10Packed ) ) ? pSrc     : pSrc + 1;
        _Ty* u = ( ( pIB->pixelFormat == ibpfYUV422Packed ) || ( pIB->pixelFormat == ibpfYUV422_10Packed ) ) ? pSrc + 1 : pSrc;
        _Ty* v = ( ( pIB->pixelFormat == ibpfYUV422Packed ) || ( pIB->pixelFormat == ibpfYUV422_10Packed ) ) ? pSrc + 3 : pSrc + 2;
        unsigned char* pDst = img.GetData() + dstPitch * line;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            YUV2RGB8( r, g, b,
                      max( 0, min( *y >> shift, 255 ) ),
                      max( 0, min( *u >> shift, 255 ) ),
                      max( 0, min( *v >> shift, 255 ) ) );
            *pDst++ = static_cast<unsigned char>( r );
            *pDst++ = static_cast<unsigned char>( g );
            *pDst++ = static_cast<unsigned char>( b );
            y += 2;
            if( x & 1 )
            {
                u += 4;
                v += 4;
            }
        }
    }
}

//-----------------------------------------------------------------------------
template<typename _Ty>
void ConvertPackedYUV444ToRGB( const ImageBuffer* pIB, wxImage& img, const int order[3], const int shift = 0 )
//-----------------------------------------------------------------------------
{
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        _Ty* pSrc = reinterpret_cast<_Ty*>( reinterpret_cast<unsigned char*>( pIB->vpData ) + line * pIB->pChannels[0].iLinePitch );
        unsigned char* pDst = img.GetData() + dstPitch * line;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            YUV2RGB8( r, g, b,
                      max( 0, min( pSrc[order[0]] >> shift, 255 ) ),
                      max( 0, min( pSrc[order[1]] >> shift, 255 ) ),
                      max( 0, min( pSrc[order[2]] >> shift, 255 ) ) );
            pSrc += 3;
            *pDst++ = static_cast<unsigned char>( r );
            *pDst++ = static_cast<unsigned char>( g );
            *pDst++ = static_cast<unsigned char>( b );
        }
    }
}

//-----------------------------------------------------------------------------
void ConvertPlanarYUVToRGB( const ImageBuffer* pIB, wxImage& img )
//-----------------------------------------------------------------------------
{
    unsigned char* const pSrc = reinterpret_cast<unsigned char*>( pIB->vpData );
    const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
    #pragma omp parallel for
#endif // #ifdef _OPENMP
    for( int line = 0; line < pIB->iHeight; line++ )
    {
        int r, g, b;
        unsigned char* y = pSrc + pIB->pChannels[0].iLinePitch * line;
        unsigned char* u = pSrc + pIB->pChannels[1].iChannelOffset + pIB->pChannels[1].iLinePitch * line;
        unsigned char* v = pSrc + pIB->pChannels[2].iChannelOffset + pIB->pChannels[2].iLinePitch * line;
        unsigned char* pDst = img.GetData() + dstPitch * line;
        for( int x = 0; x < pIB->iWidth; x++ )
        {
            YUV2RGB8( r, g, b, *y, *u, *v );
            *pDst++ = static_cast<unsigned char>( r );
            *pDst++ = static_cast<unsigned char>( g );
            *pDst++ = static_cast<unsigned char>( b );
            ++y;

            if( x & 1 )
            {
                ++u;
                ++v;
            }
        }
    }
}
#endif // #ifdef USE_RAW_BITMAP_SUPPORT

//-----------------------------------------------------------------------------
ImageCanvas::ImageCanvas( wxWindow* pApp, wxWindow* parent, wxWindowID id /* = -1 */, const wxPoint& pos /* = wxDefaultPosition */,
                          const wxSize& size /* = wxDefaultSize */, long style /* = wxSUNKEN_BORDER */, const wxString& name /* = "ImageCanvas" */, bool boActive /* = true */ )
    : DrawingCanvas( parent, id, pos, size, style, name, boActive ), m_boSupportsFullScreenMode( false ), m_boSupportsDifferentScalingModes( false )
//-----------------------------------------------------------------------------
{
    Init( pApp );
    m_pImpl = new ImageCanvasImpl();
}

//-----------------------------------------------------------------------------
ImageCanvas::~ImageCanvas()
//-----------------------------------------------------------------------------
{
    Shutdown();
    delete m_pImpl;
}

//-----------------------------------------------------------------------------
wxPoint ImageCanvas::GetScaledMousePos( int mouseXPos, int mouseYPos ) const
//-----------------------------------------------------------------------------
{
    return wxPoint( static_cast<int>( mouseXPos / m_lastScaleFactor ) - m_lastStartPoint.x,
                    static_cast<int>( mouseYPos / m_lastScaleFactor ) - m_lastStartPoint.y );
}

//-----------------------------------------------------------------------------
void ImageCanvas::IncreaseShiftValue( void )
//-----------------------------------------------------------------------------
{
    if( m_pImpl->currentShiftValue < 8 )
    {
        ++m_pImpl->currentShiftValue;
        SetImage( m_pIB, m_bufferPartIndexDisplayed );
    }
}

//-----------------------------------------------------------------------------
void ImageCanvas::DecreaseShiftValue( void )
//-----------------------------------------------------------------------------
{
    if( m_pImpl->currentShiftValue > 0 )
    {
        --m_pImpl->currentShiftValue;
        SetImage( m_pIB, m_bufferPartIndexDisplayed );
    }
}

//-----------------------------------------------------------------------------
int ImageCanvas::GetShiftValue( void ) const
//-----------------------------------------------------------------------------
{
    return m_pImpl->currentShiftValue;
}

//-----------------------------------------------------------------------------
ImageCanvas::TSaveResult ImageCanvas::GetCurrentImage( wxImage& img, int* pBitsPerPixel /* = 0 */ ) const
//-----------------------------------------------------------------------------
{
    if( !m_pImpl->currentImage.IsOk() )
    {
        return srNoImage;
    }

    img = m_pImpl->currentImage.ConvertToImage();
    if( pBitsPerPixel )
    {
        *pBitsPerPixel = m_pImpl->currentImage.GetDepth();
    }
    return srOK;
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetShiftValue( int value )
//-----------------------------------------------------------------------------
{
    if( m_pImpl->currentShiftValue != value )
    {
        m_pImpl->currentShiftValue = value;
        SetImage( m_pIB, m_bufferPartIndexDisplayed );
    }
}

//-----------------------------------------------------------------------------
int ImageCanvas::GetAppliedShiftValue( void ) const
//-----------------------------------------------------------------------------
{
    return m_pImpl->appliedShiftValue;
}

//-----------------------------------------------------------------------------
bool ImageCanvas::IsFullScreen( void ) const
//-----------------------------------------------------------------------------
{
    return false;
}

//-----------------------------------------------------------------------------
void ImageCanvas::OnPaint( wxPaintEvent& )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );
    wxPaintDC dc( this );
    DoPrepareDC( dc );

    if( !IsActive() )
    {
        m_boRefreshInProgress = false;
        return;
    }

    wxCoord clientWidth, clientHeight;
    dc.GetSize( &clientWidth, &clientHeight );

    dc.SetBrush( m_pImpl->backgroundBrush );
    dc.SetPen( *wxTRANSPARENT_PEN ); // no borders round rectangles

    if( !m_pImpl->currentImage.IsOk() )
    {
        dc.DrawRectangle( 0, 0, clientWidth, clientHeight ); // draw background
        m_lastStartPoint.x = -1;
        m_lastStartPoint.y = -1;
        m_lastScaleFactor = -1.0;
    }
    else
    {
        const int imageHeight = m_pImpl->currentImage.GetHeight();
        const int imageWidth = m_pImpl->currentImage.GetWidth();
        if( ( imageHeight > 0 ) && ( imageWidth > 0 ) )
        {
            double scaleFactor;
            if( m_boScaleToClientSize )
            {
                const double scaleX = static_cast<double>( clientWidth ) / static_cast<double>( imageWidth );
                const double scaleY = static_cast<double>( clientHeight ) / static_cast<double>( imageHeight );
                // always keep the aspect ratio
                scaleFactor = ( scaleX <= scaleY ) ? scaleX : scaleY;
            }
            else
            {
                scaleFactor = m_currentZoomFactor;
            }
            const int scaledImageWidth = static_cast<int>( imageWidth * scaleFactor );
            const int scaledImageHeight = static_cast<int>( imageHeight * scaleFactor );
            int scaleCorrectedBlitXOffsetInClient = 0, scaleCorrectedBlitYOffsetInClient = 0;
            int blitXOffsetInClient = 0, blitYOffsetInClient = 0;
            int viewXStart, viewYStart;
            GetViewStart( &viewXStart, &viewYStart );
            // draw bounding box pattern with the background brush...
            if( clientWidth > scaledImageWidth )
            {
                // ... on the left and right
                const int clientRestWidth = clientWidth - scaledImageWidth;
                scaleCorrectedBlitXOffsetInClient = static_cast<int>( static_cast<double>( clientRestWidth ) / ( 2. * scaleFactor ) );
                blitXOffsetInClient = static_cast<int>( clientRestWidth / 2 );
                dc.DrawRectangle( 0, 0, blitXOffsetInClient, clientHeight );
                dc.DrawRectangle( scaledImageWidth + blitXOffsetInClient, 0, clientRestWidth - blitXOffsetInClient, clientHeight );
            }
            if( clientHeight > scaledImageHeight )
            {
                // ... on top and bottom
                const int clientRestHeight = clientHeight - scaledImageHeight;
                scaleCorrectedBlitYOffsetInClient = static_cast<int>( static_cast<double>( clientRestHeight ) / ( 2. * scaleFactor ) );
                blitYOffsetInClient = static_cast<int>( clientRestHeight / 2 );
                dc.DrawRectangle( blitXOffsetInClient, 0, scaledImageWidth, blitYOffsetInClient );
                dc.DrawRectangle( blitXOffsetInClient, scaledImageHeight + blitYOffsetInClient, scaledImageWidth, clientRestHeight - blitYOffsetInClient );
            }
            m_lastStartPoint.x = static_cast<int>( ( ( blitXOffsetInClient > 0 ) ? blitXOffsetInClient : -viewXStart ) / scaleFactor );
            m_lastStartPoint.y = static_cast<int>( ( ( blitYOffsetInClient > 0 ) ? blitYOffsetInClient : -viewYStart ) / scaleFactor );
            m_lastScaleFactor = scaleFactor;

            if( m_pMonitorDisplay && m_pVisiblePartOfImage )
            {
                m_pVisiblePartOfImage->m_rect = wxRect( ( static_cast<int>( scaledImageWidth ) > clientWidth ) ? static_cast<int>( viewXStart / scaleFactor ) : 0,
                                                        ( static_cast<int>( scaledImageHeight ) > clientHeight ) ? static_cast<int>( viewYStart / scaleFactor ) : 0,
                                                        ( static_cast<int>( scaledImageWidth ) > clientWidth ) ? static_cast<int>( clientWidth / scaleFactor ) : imageWidth,
                                                        ( static_cast<int>( scaledImageHeight ) > clientHeight ) ? static_cast<int>( clientHeight / scaleFactor ) : imageHeight );
                m_pMonitorDisplay->Refresh( false );
            }

            dc.SetUserScale( scaleFactor, scaleFactor );
            dc.DrawBitmap( m_pImpl->currentImage, scaleCorrectedBlitXOffsetInClient, scaleCorrectedBlitYOffsetInClient, false );
            BlitAOIs( dc, 1.0, static_cast<int>( blitXOffsetInClient / scaleFactor ), static_cast<int>( blitYOffsetInClient / scaleFactor ), imageWidth, imageHeight );
            dc.SetUserScale( 1.0, 1.0 );
            BlitPerformanceMessages( dc, blitXOffsetInClient, blitYOffsetInClient, m_pImpl->format );
            BlitInfoStrings( dc, scaleFactor, static_cast<int>( blitXOffsetInClient - ( viewXStart * scaleFactor ) ), static_cast<int>( blitYOffsetInClient - ( viewYStart * scaleFactor ) ), blitXOffsetInClient, blitYOffsetInClient, imageWidth, imageHeight );
            // restore old brush
            dc.SetBrush( wxNullBrush );
        }
        else
        {
            m_lastStartPoint.x = -1;
            m_lastStartPoint.y = -1;
            m_lastScaleFactor = -1.0;
            BlitInfoStrings( dc, 1.0, 10, 10, 10, 10, imageWidth, imageHeight );
        }
    }
    m_boRefreshInProgress = false;
}

//-----------------------------------------------------------------------------
/// \brief Not supported without the mvDisplay library (which uses DirectX internally)
void ImageCanvas::OnPopUpFullScreen( wxCommandEvent& )
//-----------------------------------------------------------------------------
{
    return;
}

//-----------------------------------------------------------------------------
void ImageCanvas::RefreshScrollbars( bool /* boMoveToMousePos = false */ )
//-----------------------------------------------------------------------------
{
    const int HScroll_Y = wxSystemSettings::GetMetric( wxSYS_HSCROLL_Y );
    const int VScroll_X = wxSystemSettings::GetMetric( wxSYS_VSCROLL_X );
    const int HScrollRange = GetScrollRange( wxHORIZONTAL );
    const int VScrollRange = GetScrollRange( wxVERTICAL );
    int clientWidth, clientHeight;
    GetClientSize( &clientWidth, &clientHeight );
    int scaledImageWidth = m_pIB ? static_cast<int>( m_pIB->iWidth * m_currentZoomFactor ) : 0;
    int scaledImageHeight = m_pIB ? static_cast<int>( m_pIB->iHeight * m_currentZoomFactor ) : 0;

    int virtualWidth = 0;
    int virtualHeight = 0;
    if( HScrollRange > 0 )
    {
        if( clientWidth + HScroll_Y < scaledImageWidth )
        {
            virtualWidth = scaledImageWidth;
        }
    }
    else if( clientWidth < scaledImageWidth )
    {
        virtualWidth = scaledImageWidth;
    }

    if( VScrollRange > 0 )
    {
        if( clientHeight + VScroll_X < scaledImageHeight )
        {
            virtualHeight = scaledImageHeight;
        }
    }
    else if( clientHeight < scaledImageHeight )
    {
        virtualHeight = scaledImageHeight;
    }

    if( m_boScaleToClientSize || ( ( virtualWidth == 0 ) && ( virtualHeight == 0 ) ) )
    {
        SetScrollbars( 1, 1, 0, 0 );
    }
    else
    {
        SetVirtualSize( virtualWidth, virtualHeight );
    }
}

//-----------------------------------------------------------------------------
ImageCanvas::TSaveResult ImageCanvas::SaveCurrentImage( const wxString& filenameAndPath, const wxString& extension ) const
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker scopeLock( m_critSect );
    if( !m_pImpl->currentImage.IsOk() )
    {
        return srNoImage;
    }

    wxImage img = m_pImpl->currentImage.ConvertToImage();
    if( ( extension == wxT( "bmp" ) ) && ( m_pImpl->format == ibpfMono8 ) )
    {
        img.SetOption( wxIMAGE_OPTION_BMP_FORMAT, wxBMP_8BPP_RED );
    }

    return StoreImage( img, filenameAndPath, extension );
}

//-----------------------------------------------------------------------------
/// \brief Not supported without the mvDisplay library (which uses DirectX internally)
void ImageCanvas::SetFullScreenMode( bool )
//-----------------------------------------------------------------------------
{
    return;
}

//-----------------------------------------------------------------------------
bool ImageCanvas::SetImage( const ImageBuffer* pIB, unsigned int bufferPartIndex, bool boMustRefresh /* = true */ )
//-----------------------------------------------------------------------------
{
    wxCriticalSectionLocker locker( m_critSect );

    if( !IsActive() || !pIB )
    {
        return true;
    }

    m_pIB = pIB;
    m_bufferPartIndexDisplayed = bufferPartIndex;
    if( !boMustRefresh )
    {
        return true;
    }

    if( m_boRefreshInProgress )
    {
        ++m_skippedPaintEvents;
        return true;
    }

    bool boImageSizeChanged = false;
    if( !m_pImpl->currentImage.IsOk() ||
        ( m_pImpl->currentImage.GetWidth() != pIB->iWidth ) ||
        ( m_pImpl->currentImage.GetHeight() != pIB->iHeight ) )
    {
        boImageSizeChanged = true;
        if( m_pImpl->currentImage.IsOk() )
        {
            SetMaxZoomFactor( pIB, std::max( m_pImpl->currentImage.GetWidth(), m_pImpl->currentImage.GetHeight() ) );
        }
        else
        {
            SetMaxZoomFactor( pIB, 0 );
        }
    }

    m_pImpl->appliedShiftValue = 0;
    if( !pIB->vpData || ( pIB->iWidth == 0 ) || ( pIB->iHeight == 0 ) )
    {
        // restore initial status as no valid image is currently available
        m_pImpl->currentImage = wxBitmap();
        Refresh( false );
        return true;
    }
#ifdef USE_RAW_BITMAP_SUPPORT
    if( boImageSizeChanged )
    {
        m_pImpl->currentImage = wxBitmap( pIB->iWidth, pIB->iHeight, 24 );
    }
    PixelData data( m_pImpl->currentImage );
    switch( pIB->pixelFormat )
    {
    case ibpfMono8:
#ifdef _OPENMP
        #pragma omp parallel for
#endif // #ifdef _OPENMP
        for( int y = 0; y < pIB->iHeight; y++ )
        {
            unsigned char* pSrc = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
            PixelData::Iterator p( data );
            p.OffsetY( data, y );
            for( int x = 0; x < pIB->iWidth; x++, p++ )
            {
                p.Red() = *pSrc;
                p.Green() = *pSrc;
                p.Blue() = *pSrc++;
            }
        }
        break;
    case ibpfMono16:
        Copy2ByteMonoImage( pIB, data, m_pImpl->GetShift( 8 ) );
        break;
    case ibpfRGBx888Packed:
        {
            const int order[3] = { 2, 1, 0 };
            CopyRGBImage( pIB, data, order, 4 );
        }
        break;
    case ibpfRGB888Packed:
        {
            const int order[3] = { 2, 1, 0 };
            CopyRGBImage( pIB, data, order, 3 );
        }
        break;
    case ibpfBGR888Packed:
        {
            const int order[3] = { 0, 1, 2 };
            CopyRGBImage( pIB, data, order, 3 );
        }
        break;
    case ibpfBGR101010Packed_V2:
        {
            ///  \todo This one doesn't work!
            const int shift = m_pImpl->GetShift( 2 );
#ifdef _OPENMP
            #pragma omp parallel for
#endif // #ifdef _OPENMP
            for( int y = 0; y < pIB->iHeight; y++ )
            {
                unsigned short red, green, blue;
                unsigned int* pPixel = reinterpret_cast<unsigned int*>( static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
                PixelData::Iterator p( data );
                p.OffsetY( data, y );
                for( int x = 0; x < pIB->iWidth; x++, p++ )
                {
                    DrawingCanvas::GetBGR101010Packed_V2Pixel( *pPixel++, red, green, blue, shift );
                    p.Red() = static_cast<PixelData::Iterator::ChannelType>( ( red <= 255 ) ? red : 255 );
                    p.Green() = static_cast<PixelData::Iterator::ChannelType>( ( green <= 255 ) ? green : 255 );
                    p.Blue() = static_cast<PixelData::Iterator::ChannelType>( ( blue <= 255 ) ? blue : 255 );
                }
            }
        }
        break;
    case ibpfYUV411_UYYVYY_Packed:
        ConvertPackedYUV411_UYYVYYToRGB<unsigned char>( pIB, data );
        break;
    case ibpfYUV422Packed:
    case ibpfYUV422_UYVYPacked:
        ConvertPackedYUV422ToRGB<unsigned char>( pIB, data );
        break;
    case ibpfYUV422_10Packed:
    case ibpfYUV422_UYVY_10Packed:
        ConvertPackedYUV422ToRGB<unsigned short>( pIB, data, m_pImpl->GetShift( 2 ) );
        break;
    case ibpfYUV444_UYVPacked:
        {
            const int order[3] = { 1, 0, 2 };
            ConvertPackedYUV444ToRGB<unsigned char>( pIB, data, order );
        }
        break;
    case ibpfYUV444_UYV_10Packed:
        {
            const int order[3] = { 1, 0, 2 };
            ConvertPackedYUV444ToRGB<unsigned short>( pIB, data, order, m_pImpl->GetShift( 2 ) );
        }
        break;
    case ibpfYUV444Packed:
        {
            const int order[3] = { 0, 1, 2 };
            ConvertPackedYUV444ToRGB<unsigned char>( pIB, data, order );
        }
        break;
    case ibpfYUV444_10Packed:
        {
            const int order[3] = { 0, 1, 2 };
            ConvertPackedYUV444ToRGB<unsigned short>( pIB, data, order, m_pImpl->GetShift( 2 ) );
        }
        break;
    case ibpfYUV422Planar:
        ConvertPlanarYUVToRGB( pIB, data );
        break;
    case ibpfRGB888Planar:
    case ibpfRGBx888Planar:
#ifdef _OPENMP
        #pragma omp parallel for
#endif // #ifdef _OPENMP
        for( int y = 0; y < pIB->iHeight; y++ )
        {
            unsigned char* pr = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
            unsigned char* pg = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[1].iLinePitch + pIB->pChannels[1].iChannelOffset;
            unsigned char* pb = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[2].iLinePitch + pIB->pChannels[2].iChannelOffset;
            PixelData::Iterator p( data );
            p.OffsetY( data, y );
            for( int x = 0; x < pIB->iWidth; x++, p++ )
            {
                p.Red() = *pr++;
                p.Green() = *pg++;
                p.Blue() = *pb++;
            }
        }
        break;
    case ibpfMono10:
        Copy2ByteMonoImage( pIB, data, m_pImpl->GetShift( 2 ) );
        break;
    case ibpfMono12:
        Copy2ByteMonoImage( pIB, data, m_pImpl->GetShift( 4 ) );
        break;
    case ibpfMono12Packed_V1:
        ConvertMono12PackedToMono8( DrawingCanvas::GetMono12Packed_V1Pixel, pIB, data, m_pImpl->GetShift( 4 ) );
        break;
    case ibpfMono12Packed_V2:
        ConvertMono12PackedToMono8( DrawingCanvas::GetMono12Packed_V2Pixel, pIB, data, m_pImpl->GetShift( 4 ) );
        break;
    case ibpfMono14:
        Copy2ByteMonoImage( pIB, data, m_pImpl->GetShift( 6 ) );
        break;
    case ibpfRGB101010Packed:
        Copy2ByteRGBImage( pIB, data, m_pImpl->GetShift( 2 ), 3 );
        break;
    case ibpfRGB121212Packed:
        Copy2ByteRGBImage( pIB, data, m_pImpl->GetShift( 4 ), 3 );
        break;
    case ibpfRGB141414Packed:
        Copy2ByteRGBImage( pIB, data, m_pImpl->GetShift( 6 ), 3 );
        break;
    case ibpfRGB161616Packed:
        Copy2ByteRGBImage( pIB, data, m_pImpl->GetShift( 8 ), 3 );
        break;
    default:
        return false;
    }
#else
    wxImage img( pIB->iWidth, pIB->iHeight, false );
    switch( pIB->pixelFormat )
    {
    case ibpfMono8:
        {
            const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
            #pragma omp parallel for
#endif // #ifdef _OPENMP
            for( int y = 0; y < pIB->iHeight; y++ )
            {
                unsigned char* pSrc = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
                unsigned char* pDst = img.GetData() + dstPitch * y;
                for( int x = 0; x < pIB->iWidth; x++ )
                {
                    *pDst++ = *pSrc;
                    *pDst++ = *pSrc;
                    *pDst++ = *pSrc++;
                }
            }
        }
        break;
    case ibpfMono16:
        Copy2ByteMonoImage( pIB, img, m_pImpl->GetShift( 8 ) );
        break;
    case ibpfRGBx888Packed:
        CopyRGBImage( pIB, img, 4 );
        break;
    case ibpfRGB888Packed:
        CopyRGBImage( pIB, img, 3 );
        break;
    case ibpfBGR888Packed:
        CopyBGR888Image( pIB, img );
        break;
    case ibpfBGR101010Packed_V2:
        {
            const int shift = m_pImpl->GetShift( 2 );
            const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
            #pragma omp parallel for
#endif // #ifdef _OPENMP
            for( int y = 0; y < pIB->iHeight; y++ )
            {
                unsigned char* pDst = img.GetData() + dstPitch * y;
                unsigned int* pPixel = reinterpret_cast<unsigned int*>( static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch );
                unsigned short red, green, blue;
                for( int x = 0; x < pIB->iWidth; x++ )
                {
                    DrawingCanvas::GetBGR101010Packed_V2Pixel( *pPixel++, red, green, blue, shift );
                    *pDst++ = static_cast<unsigned char>( ( red <= 255 ) ? red : 255 );
                    *pDst++ = static_cast<unsigned char>( ( green <= 255 ) ? green : 255 );
                    *pDst++ = static_cast<unsigned char>( ( blue <= 255 ) ? blue : 255 );
                }
            }
        }
        break;
    case ibpfYUV411_UYYVYY_Packed:
        ConvertPackedYUV411_UYYVYYToRGB<unsigned char>( pIB, img );
        break;
    case ibpfYUV422Packed:
    case ibpfYUV422_UYVYPacked:
        ConvertPackedYUV422ToRGB<unsigned char>( pIB, img );
        break;
    case ibpfYUV422_10Packed:
    case ibpfYUV422_UYVY_10Packed:
        ConvertPackedYUV422ToRGB<unsigned short>( pIB, img, m_pImpl->GetShift( 2 ) );
        break;
    case ibpfYUV444_UYVPacked:
        {
            const int order[3] = { 1, 0, 2 };
            ConvertPackedYUV444ToRGB<unsigned char>( pIB, img, order );
        }
        break;
    case ibpfYUV444_UYV_10Packed:
        {
            const int order[3] = { 1, 0, 2 };
            ConvertPackedYUV444ToRGB<unsigned short>( pIB, img, order, m_pImpl->GetShift( 2 ) );
        }
        break;
    case ibpfYUV444Packed:
        {
            const int order[3] = { 0, 1, 2 };
            ConvertPackedYUV444ToRGB<unsigned char>( pIB, img, order );
        }
        break;
    case ibpfYUV444_10Packed:
        {
            const int order[3] = { 0, 1, 2 };
            ConvertPackedYUV444ToRGB<unsigned short>( pIB, img, order, m_pImpl->GetShift( 2 ) );
        }
        break;
    case ibpfYUV422Planar:
        ConvertPlanarYUVToRGB( pIB, img );
        break;
    case ibpfRGB888Planar:
    case ibpfRGBx888Planar:
        {
            const int dstPitch = 3 * img.GetWidth();
#ifdef _OPENMP
            #pragma omp parallel for
#endif // #ifdef _OPENMP
            for( int y = 0; y < pIB->iHeight; y++ )
            {
                unsigned char* pDst = img.GetData() + dstPitch * y;
                unsigned char* pr = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[0].iLinePitch;
                unsigned char* pg = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[1].iLinePitch + pIB->pChannels[1].iChannelOffset;
                unsigned char* pb = static_cast<unsigned char*>( pIB->vpData ) + y * pIB->pChannels[2].iLinePitch + pIB->pChannels[2].iChannelOffset;
                for( int x = 0; x < pIB->iWidth; x++ )
                {
                    *pDst++ = *pr++;
                    *pDst++ = *pg++;
                    *pDst++ = *pb++;
                }
            }
        }
        break;
    case ibpfMono10:
        Copy2ByteMonoImage( pIB, img, m_pImpl->GetShift( 2 ) );
        break;
    case ibpfMono12:
        Copy2ByteMonoImage( pIB, img, m_pImpl->GetShift( 4 ) );
        break;
    case ibpfMono12Packed_V1:
        CopyMono12PackedToMono8( DrawingCanvas::GetMono12Packed_V1Pixel, pIB, img, m_pImpl->GetShift( 4 ) );
        break;
    case ibpfMono12Packed_V2:
        CopyMono12PackedToMono8( DrawingCanvas::GetMono12Packed_V2Pixel, pIB, img, m_pImpl->GetShift( 4 ) );
        break;
    case ibpfMono14:
        Copy2ByteMonoImage( pIB, img, m_pImpl->GetShift( 6 ) );
        break;
    case ibpfRGB101010Packed:
        Copy2ByteRGBImage( pIB, img, m_pImpl->GetShift( 2 ), 3 );
        break;
    case ibpfRGB121212Packed:
        Copy2ByteRGBImage( pIB, img, m_pImpl->GetShift( 4 ), 3 );
        break;
    case ibpfRGB141414Packed:
        Copy2ByteRGBImage( pIB, img, m_pImpl->GetShift( 6 ), 3 );
        break;
    case ibpfRGB161616Packed:
        Copy2ByteRGBImage( pIB, img, m_pImpl->GetShift( 8 ), 3 );
        break;
    default:
        return false;
    }
    m_pImpl->currentImage = wxBitmap( img, -1 );
#endif // USE_RAW_BITMAP_SUPPORT
    m_pImpl->format = pIB->pixelFormat;
    if( boImageSizeChanged )
    {
        RefreshScrollbars();
    }
    m_boRefreshInProgress = true;
    Refresh( boImageSizeChanged );
    return true;
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetScalingMode( TScalingMode /*mode*/ )
//-----------------------------------------------------------------------------
{

}
