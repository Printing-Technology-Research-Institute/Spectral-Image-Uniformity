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
#ifndef TBayerMosaicParityH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define TBayerMosaicParityH TBayerMosaicParityH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// \brief Defines valid Bayer formats.
/// \ingroup CommonInterface
enum TBayerMosaicParity
//-----------------------------------------------------------------------------
{
    /// \brief It is not known whether the buffer or image contains raw Bayer data or the buffer or image does NOT contain raw Bayer data.
    bmpUndefined = -1,
    /// \brief The buffer or image starts with a green-red line starting with a green pixel.
    bmpGR,
    /// \brief The buffer or image starts with a green-red line starting with a red pixel.
    bmpRG,
    /// \brief The buffer or image starts with a green-blue line starting with a blue pixel.
    bmpBG,
    /// \brief The buffer or image starts with a green-blue line starting with a green pixel.
    bmpGB
};

#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
typedef enum TBayerMosaicParity TBayerMosaicParity;
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

#endif // TBayerMosaicParityH
