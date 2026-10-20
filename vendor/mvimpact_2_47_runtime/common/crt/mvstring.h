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
#ifndef mvstringH
#define mvstringH mvstringH
//-----------------------------------------------------------------------------
#include <errno.h>
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
/// \brief Version that mimics the C11 \c strncpy_s function.
/**
 * See \c strncpy_s of your runtime implementation for documentation!
 */
inline int mv_strncpy_s( char* pDst, const char* pSrc, size_t bufSize )
//-----------------------------------------------------------------------------
{
#if (defined(_MSC_VER) && (_MSC_VER >= 1400)) // is at least VC 2005 compiler?
    // '_TRUNCATE' is a Microsoft extension!
    return strncpy_s( pDst, bufSize, pSrc, _TRUNCATE );
#elif defined (__STDC_LIB_EXT1__) // does implementation support CRT extensions?
    return strncpy_s( pDst, bufSize, pSrc, bufSize );
#else
    strncpy( pDst, pSrc, bufSize );
    return errno;
#endif // #if (defined(_MSC_VER) && (_MSC_VER >= 1400)) // is at least VC 2005 compiler?
}

//-----------------------------------------------------------------------------
/// \brief Version that mimics the C11 \c strncat_s function.
/**
 * See \c strncat_s of your runtime implementation for documentation!
 */
inline int mv_strncat_s( char* pDst, const char* pSrc, size_t bufSize )
//-----------------------------------------------------------------------------
{
#if (defined(_MSC_VER) && (_MSC_VER >= 1400)) // is at least VC 2005 compiler?
    // '_TRUNCATE' is a Microsoft extension!
    return strncat_s( pDst, bufSize, pSrc, _TRUNCATE );
#elif defined (__STDC_LIB_EXT1__) // does implementation support CRT extensions?
    return strncat_s( pDst, bufSize, pSrc, bufSize - strnlen_s( pDst, bufSize ) );
#else
    strncat( pDst, pSrc, bufSize );
    return errno;
#endif // #if (defined(_MSC_VER) && (_MSC_VER >= 1400)) // is at least VC 2005 compiler?
}

//-----------------------------------------------------------------------------
/// \brief Version that mimics the C11 \c strerror_s function.
/**
 * See \c strerror_s of your runtime implementation for documentation!
 */
inline int mv_strerror_s( char* pBuf, size_t bufSize, int errnum )
//-----------------------------------------------------------------------------
{
#if (defined(_MSC_VER) && (_MSC_VER >= 1400)) || defined (__STDC_LIB_EXT1__) // is at least VC 2005 compiler OR implementation supports CRT extensions?
    return strerror_s( pBuf, bufSize, errnum );
#else
    // Should check the following constraints here:
    // - pBuf is a null pointer
    // - bufSize is zero or greater than RSIZE_MAX
    strncpy( pBuf, strerror( errnum ), bufSize );
    return errno;
#endif // #if (defined(_MSC_VER) && (_MSC_VER >= 1400)) || defined (__STDC_LIB_EXT1__) // is at least VC 2005 compiler OR implementation supports CRT extensions?
}

//-----------------------------------------------------------------------------
inline size_t mv_strerrorlen_s( int errnum )
//-----------------------------------------------------------------------------
{
#ifdef __STDC_LIB_EXT1__ // does implementation supports CRT extensions?
    return strerrorlen_s( errnum );
#else
#   if defined(__clang__)
#       pragma clang diagnostic push
#       pragma clang diagnostic ignored "-Wdeprecated-declarations"
#   elif defined(_MSC_VER) && (_MSC_VER >= 1800) // is at least VC 2013 compiler?
#       pragma warning( push )
#       pragma warning( disable : 4996 ) // 'warning C4996: 'GetVersionExA': was declared deprecated'
#   endif
    return strlen( strerror( errnum ) );
#   if defined(__clang__)
#       pragma clang diagnostic pop
#   elif defined(_MSC_VER) && (_MSC_VER >= 1800) // is at least VC 2013 compiler?
#       pragma warning( pop ) // restore old warning level
#   endif
#endif // #ifdef __STDC_LIB_EXT1__ // does implementation supports CRT extensions?
}

#endif // mvstringH
