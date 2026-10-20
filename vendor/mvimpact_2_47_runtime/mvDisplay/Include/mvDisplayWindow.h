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
#ifndef mvDisplayWindowH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define mvDisplayWindowH mvDisplayWindowH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//---------------------------------------------------------------------------
#include <mvDisplay/Include/mvDisplay.h>

#if defined(__cplusplus)
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

/// \brief Display window handle for C compliant interface.
typedef void* HDISP;

#ifndef WRAP_ANY

/// \brief Prototype for the callback function that can be passed to \b mvDispWindowSetMessageHandler()
typedef void ( *TPreProcessMessage )( HDISP hDisp, UINT message, WPARAM wParam, LPARAM lParam );

void               MV_DISPLAY_API_CALL mvDispWindowDestroy( HDISP hDisp );
WindowHandle       MV_DISPLAY_API_CALL mvDispWindowGetWindowHandle( HDISP hDisp );
TDisp*             MV_DISPLAY_API_CALL mvDispWindowGetDisplayHandle( HDISP hDisp );
HDISP              MV_DISPLAY_API_CALL mvDispWindowCreate( const char* title );
int                MV_DISPLAY_API_CALL mvDispWindowSetRefreshTime( HDISP hDisp, int time_ms );
void               MV_DISPLAY_API_CALL mvDispWindowShow( HDISP hDisp );
void               MV_DISPLAY_API_CALL mvDispWindowSetMessageHandler( HDISP hDisp, TPreProcessMessage handler );
TPreProcessMessage MV_DISPLAY_API_CALL mvDispWindowGetMessageHandler( HDISP hDisp );

#endif // #ifndef WRAP_ANY

#if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
} // namespace display
} // namespace acquire
} // namespace mvIMPACT
#endif // #if defined(MVIMPACT_ACQUIRE_DISPLAY_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#if defined(__cplusplus)
}
#endif

#endif // mvDisplayWindowH
