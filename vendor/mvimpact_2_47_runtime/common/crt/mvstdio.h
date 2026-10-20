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
#ifndef mvstdioH
#define mvstdioH mvstdioH
//-----------------------------------------------------------------------------
#include <errno.h>
#include <stdio.h>
#if defined(_MSC_VER)
#   include <share.h>
#endif

//-----------------------------------------------------------------------------
/// \brief Version that mimics the C11 \c fopen_s function.
/**
 * See \c fopen_s of your runtime implementation for documentation!
 */
inline int mv_fopen_s( FILE** ppFile, const char* pName, const char* pMode )
//-----------------------------------------------------------------------------
{
#if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
    // The M$ version of fopen_s does not support sharing thus parallel access to the same file...
    // As this is needed by mvIMPACT Acquire e.g. when writing log files from multiple processes into
    // a common file we have to use the '_fsopen' function with '_SH_DENYNO' here to get
    // what we want, which is parallel access AND no compiler warnings
    *ppFile = _fsopen( pName, pMode, _SH_DENYNO );
    return errno;
#elif defined (__STDC_LIB_EXT1__)
    return fopen_s( ppFile, pName, pMode );
#else
    *ppFile = fopen( pName, pMode );
    return errno;
#endif // #if (defined(_MSC_VER) && (_MSC_VER >= 1400)) || defined (__STDC_LIB_EXT1__) // is at least VC 2005 compiler OR implementation supports CRT extensions?
}

//-----------------------------------------------------------------------------
/// \brief Version that mimics the traditional \c fopen function.
/**
 * See \c fopen of your runtime implementation for documentation!
 */
inline FILE* mv_fopen_s( const char* pName, const char* pMode )
//-----------------------------------------------------------------------------
{
    FILE* pFile = 0;
    mv_fopen_s( &pFile, pName, pMode );
    return pFile;
}

#endif // mvstdioH
