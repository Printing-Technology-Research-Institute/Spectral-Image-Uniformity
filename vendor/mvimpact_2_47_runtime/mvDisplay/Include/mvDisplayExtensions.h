//-----------------------------------------------------------------------------
// (C) Copyright 2005 - 2022 by MATRIX VISION GmbH
//
// This software is provided by MATRIX VISION GmbH "as is"
// and any express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular purpose
// are disclaimed.
//
// In no event shall MATRIX VISION GmbH be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused and
// on any theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use of
// this software, even if advised of the possibility of such damage.

//---------------------------------------------------------------------------
#ifndef mvDisplayExtensionsH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define mvDisplayExtensionsH mvDisplayExtensionsH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//---------------------------------------------------------------------------
#include <mvDisplay/Include/mvDisplay.h>
#ifdef __cplusplus
#   include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#else
#   include <mvDeviceManager/Include/mvDeviceManager.h>
#endif // __cplusplus

#if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
namespace mvIMPACT
{
namespace acquire
{
namespace display
{
#endif // #if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifndef WRAP_ANY

#ifdef __cplusplus
extern "C" {
#endif

void MV_DISPLAY_API_CALL mvDispConvertFormat( TImageBufferPixelFormat pixelFormat, TFormatFlags* pFormat, int* pBitsPerPixel );
void MV_DISPLAY_API_CALL mvDispSetImageFromImageBuffer( TDisp* pDisp, const ImageBuffer* pBuf );

#ifdef __cplusplus
}
#endif // __cplusplus

#ifdef __cplusplus
//-----------------------------------------------------------------------------
/// \brief Sets the next image to display.
/**
 *  This function can deal with any pixel format supported by mvIMPACT Acquire.
 *  \since 1.12.68
 */
inline void mvDispSetImageFromRequest(
    /// [in] A handle to a display structure obtained
    /// from a successful call to \b mvDispInit().
    TDisp* pDisp,
    /// [in] The request object that shall be displayed.
    const mvIMPACT::acquire::Request* pRequest )
//-----------------------------------------------------------------------------
{
if( pRequest )
{
    const size_t MAX_PLANE_CNT = 3;
    size_t       planeCount = 1;
    const void*  dataPtrArray[MAX_PLANE_CNT];
    char*        p = ( char* )pRequest->imageData.read();
    int          bitsPerPixel = 8;
    TFormatFlags format = ffMono;

    mvDispConvertFormat( static_cast<TImageBufferPixelFormat>( pRequest->imagePixelFormat.read() ), &format, &bitsPerPixel );
    switch( pRequest->imagePixelFormat.read() )
    {
    case ibpfRGB888Planar:
    case ibpfRGBx888Planar:
        dataPtrArray[2] = p + pRequest->imageChannelOffset.read( 0 );
        dataPtrArray[1] = p + pRequest->imageChannelOffset.read( 1 );
        dataPtrArray[0] = p + pRequest->imageChannelOffset.read( 2 );
        planeCount = 3;
        break;
    case ibpfYUV422Planar:
        dataPtrArray[0] = p + pRequest->imageChannelOffset.read( 0 );
        dataPtrArray[1] = p + pRequest->imageChannelOffset.read( 1 );
        dataPtrArray[2] = p + pRequest->imageChannelOffset.read( 2 );
        planeCount = 3;
        break;
    default:
        dataPtrArray[0] = p;
        break;
    }
    mvDispSetImageEx( pDisp, dataPtrArray, planeCount, format, pRequest->imageWidth.read(), pRequest->imageHeight.read(), bitsPerPixel, pRequest->imageLinePitch.read() );
}
else
{
    mvDispSetImage( pDisp, 0, 0, 0, 8, 0 );
}
}
#endif // #ifdef __cplusplus

#endif // #ifndef WRAP_ANY

#if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
} // namespace display
} // namespace acquire
} // namespace mvIMPACT
#endif // #if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#endif // mvDisplayExtensionsH
