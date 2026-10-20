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
#ifndef TImageBufferPixelFormatH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define TImageBufferPixelFormatH TImageBufferPixelFormatH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// \brief Valid image buffer pixel formats.
/// \ingroup CommonInterface
enum TImageBufferPixelFormat
//-----------------------------------------------------------------------------
{
    /// \brief An unprocessed block of data.
    ibpfRaw = 0,
    /// \brief A single channel 8 bit per pixel format.
    ibpfMono8 = 1,
    /// \brief A single channel 16 bit per pixel format.
    ibpfMono16 = 2,
    /// \brief A three channel RGB image with 32 bit per pixel containing one fill byte per pixel.
    /**
     *  This is an interleaved pixel format suitable for most display functions. The data
     *  is stored pixel-wise. The memory layout of the pixel data is like this:
     *
     * \code
     *  4 bytes             4 bytes             etc.
     *  B(1) G(1) R(1) A(1) B(2) G(2) R(2) A(2) etc.
     *  .......................................
     *                      B(n) G(n) R(n) A(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to B(1) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfRGBx888Packed = 3,
    /// \brief This is a YUV422 packed image with 32 bit for a pair of pixels.
    /**
     *  This format uses 2:1 horizontal downsampling, which means that the Y component is
     *  sampled at each pixel, while U(Cb) and V(Cr) components are sampled every 2 pixels in
     *  horizontal direction. If each component takes 8 bits, the pair of pixels requires 32 bits.
     *
     *  Two consecutive pixels (32 bit, 0xaabbccdd ) contain 8 bit luminance of pixel 1(aa),
     *  8 bit chrominance blue of pixel 1 and 2(bb), 8 bit luminance of pixel 2(cc) and finally 8 bit chrominance red of pixels 1 and 2(dd).
     *
     *  Thus in memory the data will be stored like this:
     *
     * \code
     *  4 bytes                   4 bytes                         etc.
     *  Y(1) Cb(1,2) Y(2) Cr(1,2) Y(3)   Cb(3,4)   Y(4) Cr(3,4)   etc.
     *  ..........................Y(n-1) Cb(n-1,n) Y(n) Cr(n-1,n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Y(1) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV422Packed = 4,
    /// \brief The image will be transferred as an RGB image in planar format.
    /**
     *  This is a format best suitable for most image processing functions.
     *  The image will be converted into four planes(a plane for each color component and one
     *  alpha plane).
     *
     * \code
     *  R(1) R(2) R(3) R(4) etc.
     *  ...................
     *  .............. R(n)
     *  G(1) G(2) G(3) G(4) etc.
     *  ...................
     *  .............. G(n)
     *  B(1) B(2) B(3) B(4) etc.
     *  ...................
     *  .............. B(n)
     *  A(1) A(2) A(3) A(4) etc.
     *  ...................
     *  .............. A(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels red component. <b>ImageBuffer::vpData</b> will therefore
     *  point to R(1) when using a byte pointer.
     */
    ibpfRGBx888Planar = 5,
    /// \brief A single channel 10 bit per pixel format.
    /**
     *  Each pixel in this format consumes 2 bytes of memory. The lower 10 bit of this 2 bytes will contain valid data.
     */
    ibpfMono10 = 6,
    /// \brief A single channel 12 bit per pixel format.
    /**
     *  Each pixel in this format consumes 2 bytes of memory. The lower 12 bit of this 2 bytes will contain valid data.
     */
    ibpfMono12 = 7,
    /// \brief A single channel 14 bit per pixel format.
    /**
     *  Each pixel in this format consumes 2 bytes of memory. The lower 14 bit of this 2 bytes will contain valid data.
     */
    ibpfMono14 = 8,
    /// \brief The image will be transferred as an RGB image with 24 bit per pixel.
    /**
     *  This is an interleaved pixel format suitable for most display and processing functions.
     *  The data is stored pixel-wise:
     *
     * \code
     *  3 bytes        3 bytes        3 bytes      etc.
     *  B(1)G(1)R(1)   B(2)G(2)R(2)   B(3)G(3)R(3) etc.
     *  ..........................................
     *  ...........................   B(n)G(n)R(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to B(1) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfRGB888Packed = 9,
    /// \brief This is a YUV444 planar image with 24 bit per pixels.
    /**
     *  A planar YUV format. In memory the data will be stored plane-wise like this:
     *
     * \code
     *  Y(1)    Y(2)    Y(3)    Y(4) etc.
     *  ............................
     *  ..............  Y(n-1)  Y(n)
     *  Cr(1)   Cr(2)   Cr(3)   Cr(4) etc.
     *  ............................
     *  ..............  Cr(n-1) Cr(n)
     *  Cb(1)   Cb(2)   Cb(3)   Cb(4) etc.
     *  ............................
     *  .............   Cb(n-1) Cb(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Y(1) when using a byte pointer.
     */
    ibpfYUV444Planar = 10,
    /// \brief A single channel 32 bit per pixel format.
    ibpfMono32 = 11,
    /// \brief This is a YUV422 planar image with 32 bit for a pair of pixels.
    /**
     *  This format uses 2:1 horizontal downsampling, which means that the Y component is
     *  sampled at each pixel, while U(Cb) and V(Cr) components are sampled every 2 pixels in
     *  horizontal direction. If each component takes 8 bits, the pair of pixels requires 32 bits.
     *
     *  In memory the data will be stored like this:
     *
     * \code
     *  Y(1)    Y(2)    Y(3)    Y(4) etc.
     *  ............................
     *  ..............  Y(n-1)  Y(n)
     *  Cr(1,2) Cr(3,4) etc.
     *  ...............
     *  ....... Cr(n/2)
     *  Cb(1,2) Cb(3,4) etc.
     *  ...............
     *  ....... Cb(n/2)
     * \endcode
     *
     *  Thus the Y planes size in bytes equals the sum of the 2 other planes.
     *
     *  So the first byte in memory is the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Y(1) when using a byte pointer.
     */
    ibpfYUV422Planar = 12,
    /// \brief The image will be transferred as an RGB image with 30 bit of usable data per pixel.
    /**
     *  This is an interleaved pixel format with 2 bytes per color component.
     *  The data is stored pixel-wise:
     *
     * \code
     *  6 bytes        6 bytes        6 bytes      etc.
     *  B(1)G(1)R(1)   B(2)G(2)R(2)   B(3)G(3)R(3) etc.
     *  ..........................................
     *  ...........................   B(n)G(n)R(n)
     * \endcode
     *
     *  The data of each color component will be LSB aligned, thus the 6 MSB of each 16 bit will
     *  not contain valid data.
     *
     *  So the first 2 bytes in memory are the first pixels blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to B(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfRGB101010Packed = 13,
    /// \brief The image will be transferred as an RGB image with 36 bit of usable data per pixel.
    /**
     *  This is an interleaved pixel format with 2 bytes per color component.
     *  The data is stored pixel-wise:
     *
     * \code
     *  6 bytes        6 bytes        6 bytes      etc.
     *  B(1)G(1)R(1)   B(2)G(2)R(2)   B(3)G(3)R(3) etc.
     *  ..........................................
     *  ...........................   B(n)G(n)R(n)
     * \endcode
     *
     *  The data of each color component will be LSB aligned, thus the 4 MSB of each 16 bit will
     *  not contain valid data.
     *
     *  So the first 2 bytes in memory are the first pixels blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to B(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfRGB121212Packed = 14,
    /// \brief The image will be transferred as an RGB image with 42 bit of usable data per pixel.
    /**
     *  This is an interleaved pixel format with 2 bytes per color component.
     *  The data is stored pixel-wise:
     *
     * \code
     *  6 bytes        6 bytes        6 bytes      etc.
     *  B(1)G(1)R(1)   B(2)G(2)R(2)   B(3)G(3)R(3) etc.
     *  ..........................................
     *  ...........................   B(n)G(n)R(n)
     * \endcode
     *
     *  The data of each color component will be LSB aligned, thus the 2 MSB of each 16 bit will
     *  not contain valid data.
     *
     *  So the first 2 bytes in memory are the first pixels blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to B(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfRGB141414Packed = 15,
    /// \brief The image will be transferred as an RGB image with 48 bit of usable data per pixel.
    /**
     *  This is an interleaved pixel format with 2 bytes per color component.
     *  The data is stored pixel-wise:
     *
     * \code
     *  6 bytes        6 bytes        6 bytes      etc.
     *  B(1)G(1)R(1)   B(2)G(2)R(2)   B(3)G(3)R(3) etc.
     *  ..........................................
     *  ...........................   B(n)G(n)R(n)
     * \endcode
     *
     *  The data of each color component will be LSB aligned.
     *
     *  So the first 2 bytes in memory are the first pixels blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to B(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfRGB161616Packed = 16,
    /// \brief This is a YUV422 packed image with 32 bit for a pair of pixels.
    /**
     *  This format uses 2:1 horizontal downsampling, which means that the Y component is
     *  sampled at each pixel, while U(Cb) and V(Cr) components are sampled every 2 pixels in
     *  horizontal direction. If each component takes 8 bits, the pair of pixels requires 32 bits.
     *
     *  Two consecutive pixels (32 bit, 0xaabbccdd ) will contain 8 bit chrominance blue of pixel 1 and 2(aa),
     *  8 bit luminance of pixel 1(bb), 8 bit chrominance red of pixel 1 and 2 (cc) and finally 8 bit luminance of pixel 2(dd).
     *
     *  Thus in memory the data will be stored like this:
     *
     * \code
     *  4 bytes                   4 bytes                         etc.
     *  Cb(1,2) Y(1) Cr(1,2) Y(2) Cb(3,4)   Y(3)   Cr(3,4)   Y(4)    etc.
     *  ..........................Cb(n-1,n) Y(n-1) Cr(n-1,n) Y(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels Cb component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Cb(1,2) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV422_UYVYPacked = 17,
    /// \brief A single channel 12 bit per pixel packed format.
    /**
     *  This format will use 3 bytes to store 2 12 bit pixel. Every 3 bytes will use the following layout in
     *  memory:
     *
     * \code
     *  3 bytes                                               3 bytes                                               etc.
     *  bits 11..4(1) bits 3..0(1) bits 3..0(2) bits 11..4(2) bits 11..4(3) bits 3..0(3) bits 3..0(4) bits 11..4(4) etc.
     * \endcode
     *
     *  \note
     *  When the width is not divisible by 2 the line pitch of a buffer can't be used to calculate line start offsets in a buffer!
     *  In that case something like this can be used to access a certain pixel (pseudo code assuming 'pointerToStartOfTheBuffer' is a 'byte pointer'):
     *
     * \code
     *  GetMono12Packed_V1Pixel( pointerToStartOfTheBuffer, pixelIndex )
     *    const int offsetFromStartOfTheBuffer = (3*pixel)/2
     *    if pixel divisible by 2
     *      return (pointerToStartOfTheBuffer[offset+1] << shift) | (pointerToStartOfTheBuffer[offset] >> 4)
     *    return pointerToStartOfTheBuffer[offset] << shift) | (pointerToStartOfTheBuffer[offset+1] & 0xF)
     * \endcode
     */
    ibpfMono12Packed_V2 = 18,
    /// \brief This is a YUV422 packed image with 64 bit for a pair of pixels.
    /**
     *  This format uses 2:1 horizontal downsampling, which means that the Y component is
     *  sampled at each pixel, while U(Cb) and V(Cr) components are sampled every 2 pixels in
     *  horizontal direction. If each component takes 16 bits, the pair of pixels requires 64 bits.
     *
     *  Two consecutive pixels (64 bit, 0xaaaabbbbccccdddd ) contain 10 bit luminance of pixel 1(aaaa),
     *  10 bit chrominance blue of pixel 1 and 2(bbbb), 10 bit luminance of pixel 2(cccc) and finally 10 bit chrominance red of pixels 1 and 2(dddd).
     *  The upper 6 bits of each component will be 0.
     *
     *  Thus in memory the data will be stored like this:
     *
     * \code
     *  8 bytes                   8 bytes                         etc.
     *  Y(1) Cb(1,2) Y(2) Cr(1,2) Y(3)   Cb(3,4)   Y(4) Cr(3,4)   etc.
     *  ..........................Y(n-1) Cb(n-1,n) Y(n) Cr(n-1,n)
     * \endcode
     *
     *  So the first 2 bytes in memory are the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Y(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV422_10Packed = 20,
    /// \brief This is a YUV422 packed image with 64 bit for a pair of pixels.
    /**
     *  This format uses 2:1 horizontal downsampling, which means that the Y component is
     *  sampled at each pixel, while U(Cb) and V(Cr) components are sampled every 2 pixels in
     *  horizontal direction. If each component takes 16 bits, the pair of pixels requires 64 bits.
     *
     *  Two consecutive pixels (64 bit, 0xaaaabbbbccccdddd ) will contain 10 bit chrominance blue of pixel 1 and 2(aaaa),
     *  10 bit luminance of pixel 1(bbbb), 10 bit chrominance red of pixel 1 and 2 (cccc) and finally 10 bit luminance of pixel 2(dddd).
     *  The upper 6 bits of each component will be 0.
     *
     *  Thus in memory the data will be stored like this:
     *
     * \code
     *  8 bytes                   8 bytes                         etc.
     *  Cb(1,2) Y(1) Cr(1,2) Y(2) Cb(3,4)   Y(3)   Cr(3,4)   Y(4)    etc.
     *  ..........................Cb(n-1,n) Y(n-1) Cr(n-1,n) Y(n)
     * \endcode
     *
     *  So the first 2 bytes in memory are the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Cb(1,2) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV422_UYVY_10Packed = 21,
    /// \brief The image will be transferred as an RGB image with 24 bit per pixel.
    /**
     *  This is an interleaved pixel format suitable for most processing functions. Most
     *  blit/display function however will expect ibpfRGB888Packed.
     *  The data is stored pixel-wise:
     *
     * \code
     *  3 bytes        3 bytes        3 bytes      etc.
     *  R(1)G(1)B(1)   R(2)G(2)B(2)   R(3)G(3)B(3) etc.
     *  ..........................................
     *  ...........................   R(n)G(n)B(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels red component. <b>ImageBuffer::vpData</b> will therefore
     *  point to R(1) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfBGR888Packed = 22,
    /// \brief A 10 bit per color component RGB packed format.
    /**
     *  This format will use 4 bytes to store one 10 bit per color component RGB pixel. The following memory layout is
     *  used for each pixel:
     *
     * \code
     *  byte 0   | byte 1   | byte 2   | byte 3   |
     *  0      7 | 890....5 | 6..90..3 | 4    9xx |
     *  RRRRRRRR | RRGGGGGG | GGGGBBBB | BBBBBB   |
     * \endcode
     *
     * \if (DOXYGEN_C_DOCUMENTATION || DOXYGEN_CPP_DOCUMENTATION)
     * \note
     * Access to a certain pixel can e.g. be implemented like this:
     *
     * \code
     *  //-----------------------------------------------------------------------------
     *  // slow version
     *  inline void GetBGR101010Packed_V2Pixel( void* p, const int pitch, int x, int y, unsigned short& red, unsigned short& green, unsigned short& blue )
     *  //-----------------------------------------------------------------------------
     *  {
     *    unsigned int* pSrc = reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(p) + y * pitch) + x;
     *    red   = static_cast<unsigned short>( (*pSrc)         & 0x3FF);
     *    green = static_cast<unsigned short>(((*pSrc) >> 10 ) & 0x3FF);
     *    blue  = static_cast<unsigned short>(((*pSrc) >> 20 ) & 0x3FF);
     *  }
     *
     *  //-----------------------------------------------------------------------------
     *  // faster version
     *  inline void GetBGR101010Packed_V2Pixel( unsigned int pixel, unsigned short& red, unsigned short& green, unsigned short& blue )
     *  //-----------------------------------------------------------------------------
     *  {
     *    red   = static_cast<unsigned short>(  pixel         & 0x3FF);
     *    green = static_cast<unsigned short>(( pixel >> 10 ) & 0x3FF);
     *    blue  = static_cast<unsigned short>(( pixel >> 20 ) & 0x3FF);
     *  }
     * \endcode
     * \endif
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfBGR101010Packed_V2 = 23,
    /// \brief The image will be transferred as an YUV image with 24 bit per pixel.
    /**
     *  This is an interleaved pixel format.
     *
     *  The data is stored pixel-wise:
     *
     * \code
     *  3 bytes        3 bytes        3 bytes      etc.
     *  Cb(1)Y(1)Cr(1) Cb(2)Y(2)Cr(2) Cb(3)Y(3)Cr(3) etc.
     *  ..........................................
     *  ...........................   Cb(n)Y(n)Cr(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels Cb component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Cb(1) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV444_UYVPacked = 24,
    /// \brief The image will be transferred as an YUV image with 30 bit of usable data per pixel.
    /**
     *  This is an interleaved pixel format with 2 bytes per color component.
     *  The data is stored pixel-wise:
     *
     * \code
     *  6 bytes        6 bytes        6 bytes      etc.
     *  Cb(1)Y(1)Cr(1) Cb(2)Y(2)Cr(2) Cb(3)Y(3)Cr(3) etc.
     *  ..........................................
     *  ...........................   Cb(n)Y(n)Cr(n)
     * \endcode
     *
     *  The data of each color component will be LSB aligned, thus the 6 MSB of each 16 bit will
     *  not contain valid data.
     *
     *  So the first byte in memory is the first pixels Cb component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Cb(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV444_UYV_10Packed = 25,
    /// \brief The image will be transferred as an YUV image with 24 bit per pixel.
    /**
     *  This is an interleaved pixel format.
     *
     *  The data is stored pixel-wise:
     *
     * \code
     *  3 bytes        3 bytes        3 bytes      etc.
     *  Y(1)Cb(1)Cr(1) Y(2)Cb(2)Cr(2) Y(3)Cb(3)Cr(3) etc.
     *  ..........................................
     *  ...........................   Y(n)Cb(n)Cr(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Y(1) when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV444Packed = 26,
    /// \brief The image will be transferred as an YUV image with 30 bit of usable data per pixel.
    /**
     *  This is an interleaved pixel format with 2 bytes per color component.
     *  The data is stored pixel-wise:
     *
     * \code
     *  6 bytes        6 bytes        6 bytes      etc.
     *  Y(1)Cb(1)Cr(1) Y(2)Cb(2)Cr(2) Y(3)Cb(3)Cr(3) etc.
     *  ..........................................
     *  ...........................   Y(n)Cb(n)Cr(n)
     * \endcode
     *
     *  The data of each color component will be LSB aligned, thus the 6 MSB of each 16 bit will
     *  not contain valid data.
     *
     *  So the first byte in memory is the first pixels luminance component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Y(1) when using a 16 bit pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     */
    ibpfYUV444_10Packed = 27,
    /// \brief A single channel 12 bit per pixel packed format.
    /**
     *  This format will use 3 bytes to store 2 12 bit pixel. Every 3 bytes will use the following layout in
     *  memory:
     *
     * \code
     *  3 bytes                                               3 bytes                                               etc.
     *  bits 0..7(1) bits 8..11(1) bits 0..3(2) bits 4..11(2) bits 0..7(3) bits 8..11(3) bits 0..3(4) bits 4..11(4) etc.
     * \endcode
     *
     *  \note
     *  When the width is not divisible by 2 the line pitch of a buffer can't be used to calculate line start offsets in a buffer!
     *  In that case something like this can be used to access a certain pixel (pseudo code assuming 'pointerToStartOfTheBuffer' is a 'byte pointer'):
     *
     * \code
     *  GetMono12Packed_V1Pixel( pointerToStartOfTheBuffer, pixelIndex )
     *    const int offsetFromStartOfTheBuffer = pixel + pixel/2
     *    if pixel divisible by 2
     *      return (pointerToStartOfTheBuffer[offset] >> 4) | (pointerToStartOfTheBuffer[offset+1] << 4)
     *    return pointerToStartOfTheBuffer[offset]) | (pointerToStartOfTheBuffer[offset+1] & 0xF) << 8)
     * \endcode
     *
     * \since 2.5.0
     */
    ibpfMono12Packed_V1 = 28,
    /// \brief This is a YUV411 packed image with 48 bits for four pixels.
    /**
     *  This format uses 4:1 horizontal downsampling, which means that the Y component is
     *  sampled at each pixel, while U(Cb) and V(Cr) components are sampled every 4 pixels in
     *  horizontal direction. If each component takes 8 bits, four pixels require 48 bits.
     *
     *  Four consecutive pixels (48 bit, 0xaabbccddeeff ) contain 8 bit chrominance blue of pixels 1, 2, 3 and 4(aa),
     *  8 bit luminance of pixel 1(bb),8 bit luminance of pixel 2(cc), 8 bit chrominance red of pixels 1, 2, 3 and 4(dd),
     *  8 bit luminance of pixel 3(ee) and finally 8 bit luminance of pixel 4(ff).
     *
     *  Thus in memory the data will be stored like this:
     *
     * \code
     *  6 bytes                                     6 bytes                                     etc.
     *  Cb(1,2,3,4) Y(1) Y(2) Cr(1,2,3,4) Y(3) Y(4) Cb(5,6,7,8) Y(5) Y(6) Cr(5,6,7,8) Y(7) Y(8) etc.
     *  ..................                          Cb(n,n+1,n+2,n+3) Y(n) Y(n+1) Cr(n,n+1,n+2,n+3) Y(n+2) Y(n+3)
     * \endcode
     *
     *  So the first byte in memory is the chrominance blue component. <b>ImageBuffer::vpData</b> will therefore
     *  point to Cb when using a byte pointer.
     *
     * \sa \ref Channel_Split_PackedToPlanar
     *
     * \since 2.13.0
     */
    ibpfYUV411_UYYVYY_Packed = 29,
    /// \brief The image will be transferred as an RGB image in planar format.
    /**
     *  This is a format best suitable for most image processing functions.
     *  The image will be converted into 3 planes(a plane for each color component).
     *
     * \code
     *  R(1) R(2) R(3) R(4) etc.
     *  ...................
     *  .............. R(n)
     *  G(1) G(2) G(3) G(4) etc.
     *  ...................
     *  .............. G(n)
     *  B(1) B(2) B(3) B(4) etc.
     *  ...................
     *  .............. B(n)
     * \endcode
     *
     *  So the first byte in memory is the first pixels red component. <b>ImageBuffer::vpData</b> will therefore
     *  point to R(1) when using a byte pointer.
     *
     * \since 2.17.0
     */
    ibpfRGB888Planar = 30,
    /// \brief The driver will decide which format will be used.
    ibpfAuto = -1
};

#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
typedef enum TImageBufferPixelFormat TImageBufferPixelFormat;
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

#endif // TImageBufferPixelFormatH
