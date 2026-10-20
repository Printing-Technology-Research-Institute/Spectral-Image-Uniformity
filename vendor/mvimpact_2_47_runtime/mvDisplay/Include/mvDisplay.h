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
#ifndef mvDisplayH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define mvDisplayH mvDisplayH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//---------------------------------------------------------------------------
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   ifdef _WIN32
#       ifndef WRAP_ANY
#           if defined(__BORLANDC__) // is Borland compiler
#               pragma option -b // force enums to size of integer
#           endif // #if defined(__BORLANDC__)
#           ifndef NO_MV_DISPLAY_AUTOLINK
#               if defined(__BORLANDC__) // is Borland compiler
#                   if __CODEGEARC__ >= 0X710
#                       pragma comment(lib,"mvDisplayb")
#                   else
#                       pragma comment(lib,"mvDisplayb.lib")
#                   endif // #if __CODEGEARC__ >= 0X710
#                   pragma message( "Automatically linking with mvDisplayb.lib" )
#               elif defined(_MSC_VER) // Microsoft compiler
#                   pragma comment(lib,"mvDisplay.lib")
#                   pragma message( "Automatically linking with mvDisplay.lib" )
#               endif // #if defined(__BORLANDC__)
#           endif // #ifndef NO_MV_DISPLAY_AUTOLINK
#           include <windows.h>
#           include <windowsx.h>
typedef COLORREF ColorValue;
#       else
typedef unsigned int ColorValue;
#       endif // #ifndef WRAP_ANY
#       define MV_DISPLAY_API_CALL __stdcall
typedef HWND WindowHandle;
typedef HDC DeviceContextHandle;
typedef HBRUSH BrushHandle;
#   elif !defined(DOXYGEN_JAVA_DOCUMENTATION) && !defined(DOXYGEN_PYTHON_DOCUMENTATION)
#       error Unsupported operating system
#   endif // #ifdef _WIN32
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <mvDisplay/Include/mvDisplayDatatypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
namespace mvIMPACT
{
namespace acquire
{
namespace display
{
#endif // #if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifndef WRAP_ANY

/// \brief Prototype for the overlay callback function that can be passed to \b mvDispSetOverlayCallbackFunction()
typedef void ( MV_DISPLAY_API_CALL* TImageDisplayOverlayFunction )( DeviceContextHandle hdc, WindowHandle hwnd, int bmpLeft, int bmpTop, int bmpWidth, int bmpHeight, int dispLeft, int dispTop, int dispWidth, int dispHeight, void* pUserParam );

TDisp*             MV_DISPLAY_API_CALL mvDispInit( WindowHandle hwnd );
void               MV_DISPLAY_API_CALL mvDispDeinit( TDisp** ppDisp );
void               MV_DISPLAY_API_CALL mvDispClear( TDisp* pDisp );
int                MV_DISPLAY_API_CALL mvDispGetError( TDisp* pDisp );
void               MV_DISPLAY_API_CALL mvDispSetBitmapRectangle( TDisp* pDisp, int left, int top, int width, int height );
void               MV_DISPLAY_API_CALL mvDispSetDisplayMode( TDisp* pDisp, TDisplayMode mode );
TDisplayMode       MV_DISPLAY_API_CALL mvDispGetDisplayMode( TDisp* pDisp );
void               MV_DISPLAY_API_CALL mvDispSetDisplayRectangle( TDisp* pDisp, int left, int top, int width, int height );
void               MV_DISPLAY_API_CALL mvDispSetBackgroundBrush( TDisp* pDisp, HBRUSH hBrush );
void               MV_DISPLAY_API_CALL mvDispSetDDrawOverlayKeyColor( TDisp* pDisp, COLORREF keyColor );
void               MV_DISPLAY_API_CALL mvDispSetOverlayCallbackFunction( TDisp* pDisp, TImageDisplayOverlayFunction fctOverlay, void* pUserParam );
void               MV_DISPLAY_API_CALL mvDispGetImage( TDisp* pDisp, const void** ppData, int* pWidth, int* pHeight, int* pBitsPerPixel, int* pPitch );
void               MV_DISPLAY_API_CALL mvDispSetImage( TDisp* pDisp, const void* pData, int width, int height, int bitsPerPixel, int pitch );
void               MV_DISPLAY_API_CALL mvDispSetImageEx( TDisp* pDisp, const void** ppData, size_t ppDataArraySize, TFormatFlags format, int width, int height, int bitsPerPixel, int pitch );
void               MV_DISPLAY_API_CALL mvDispUpdate( TDisp* pDisp );
void               MV_DISPLAY_API_CALL mvDispSetWindowHandle( TDisp* pDisp, WindowHandle hwnd );
WindowHandle       MV_DISPLAY_API_CALL mvDispGetWindowHandle( TDisp* pDisp );
void               MV_DISPLAY_API_CALL mvDispSetInterpolationMode( TDisp* pDisp, TInterpolationMode interpolationMode );
TInterpolationMode MV_DISPLAY_API_CALL mvDispGetInterpolationMode( TDisp* pDisp );
int                MV_DISPLAY_API_CALL mvDispGetShift( TDisp* pDisp );
void               MV_DISPLAY_API_CALL mvDispSetShift( TDisp* pDisp, int shift );
int                MV_DISPLAY_API_CALL mvDispGetAppliedShift( TDisp* pDisp );

#endif // #ifndef WRAP_ANY

#if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
} // namespace display
} // namespace acquire
} // namespace mvIMPACT
#endif // #if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // mvDisplayH
