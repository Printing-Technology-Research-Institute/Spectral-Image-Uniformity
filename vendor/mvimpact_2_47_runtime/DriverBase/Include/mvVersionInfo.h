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

//-----------------------------------------------------------------------------
#ifndef mvVersionInfoH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define mvVersionInfoH mvVersionInfoH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------

#if !defined(mvDriverBaseEnumsH) && !defined(SWIG)
#   error "This file must NOT be included directly! Include mvDeviceManager.h(C-API) or mvIMPACT_acquire.h(C++ API) instead"
#endif // #if !defined(mvDriverBaseEnumsH) && !defined(SWIG)

/// \brief Returns the major version number of the current mvIMPACT Acquire release.
/**
 * \returns The major version number of mvIMPACT Acquire
*/
#define MVIMPACT_ACQUIRE_MAJOR_VERSION   2

/// \brief Returns the minor version number of the current mvIMPACT Acquire release.
/**
 * \returns The minor version number of mvIMPACT Acquire
*/
#define MVIMPACT_ACQUIRE_MINOR_VERSION   47

/// \brief Returns the release version number of the current mvIMPACT Acquire release.
/**
 * \returns The release version number of mvIMPACT Acquire
*/
#define MVIMPACT_ACQUIRE_RELEASE_VERSION 0

/// \brief Returns the build version number of the current mvIMPACT Acquire release.
/**
 * \returns The build version number of mvIMPACT Acquire
*/
#define MVIMPACT_ACQUIRE_BUILD_VERSION   3493

/// \brief Returns the full version number of the current mvIMPACT Acquire release as a string ("2.47.0.3493").
/**
 * \returns The full version string of mvIMPACT Acquire
*/
#define MVIMPACT_ACQUIRE_VERSION_STRING  "2.47.0.3493"

/// \brief This is a macro which evaluates to true if the current mvIMPACT Acquire version is at least major.minor.release.
/**
 * For example, to test if the program will be compiled with mvIMPACT Acquire 2.0 or higher, the following can be done:
 *
 * \code
 * HDISP hDisp = getDisplayHandleFromSomewhere();
 * #if MVIMPACT_ACQUIRE_CHECK_VERSION(2, 0, 0)
 *   mvDispWindowDestroy( hDisp );
 *  #else // replacement code for old version
 *    mvDestroyImageWindow( hDisp );
 *  #endif
 * \endcode
 *
 *  \since 2.0.0
 */
#define MVIMPACT_ACQUIRE_CHECK_VERSION(MAJOR, MINOR, RELEASE) \
    (MVIMPACT_ACQUIRE_MAJOR_VERSION > (MAJOR) || \
    (MVIMPACT_ACQUIRE_MAJOR_VERSION == (MAJOR) && MVIMPACT_ACQUIRE_MINOR_VERSION > (MINOR)) || \
    (MVIMPACT_ACQUIRE_MAJOR_VERSION == (MAJOR) && MVIMPACT_ACQUIRE_MINOR_VERSION == (MINOR) && MVIMPACT_ACQUIRE_RELEASE_VERSION >= (RELEASE)))

#endif // mvVersionInfoH
