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
#ifndef mvImageBufferH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define mvImageBufferH mvImageBufferH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------
#include <mvPropHandling/Include/mvPropHandlingDatatypes.h>


#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
#   ifdef _WIN32
#       ifdef __BORLANDC__
#           pragma option push -b // force enums to the size of integer
#       endif // __BORLANDC__
#   endif // _WIN32
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

#if defined(MVIMPACT_ACQUIRE_H_) || defined(MVIMPACT_ACQUIRE_GIGEVISION_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
namespace mvIMPACT
{
namespace acquire
{
#endif // #if defined(MVIMPACT_ACQUIRE_H_) || defined(MVIMPACT_ACQUIRE_GIGEVISION_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#include <common/TImageBufferPixelFormat.h>

#ifdef __GNUC__
#   define ATTR_PACK __attribute__((packed)) __attribute__ ((aligned (8)))
#else
#define ATTR_PACK
#   pragma pack(push, 8) // 8 byte structure alignment
#endif // #ifdef __GNUC__

//-----------------------------------------------------------------------------
/// \brief A structure for image buffer channel specific data
/**
 *  Channel specific data in an image is data, that in e.g. and RGB image
 *  might differ for the color components red, green and blue.
 *
 * \ingroup CommonInterface
 */
struct ChannelData
//-----------------------------------------------------------------------------
{
    /// \brief The offset (in bytes) to the next channel.
    int iChannelOffset;
    /// \brief The offset (in bytes) to the next line of this channel.
    int iLinePitch;
    /// \brief The offset (in bytes) to the next pixel of this channel.
    int iPixelPitch;
    /// \brief The string descriptor for this channel.
    /**
     *  For an RGB image the string values of three <b>mvIMPACT::acquire::ChannelData</b>
     *  structures this might e.g. be "R", "G" and "B".
     */
    char szChannelDesc[DEFAULT_STRING_SIZE_LIMIT];
} ATTR_PACK;

#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
typedef struct ChannelData ChannelData;
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

//-----------------------------------------------------------------------------
/// \brief Fully describes a captured image.
/**
 *  This class serves as a describing structure for captured images.
 *
 * \ingroup CommonInterface
 */
struct ImageBuffer
//-----------------------------------------------------------------------------
{
    /// \brief The number of bytes per pixel.
    int iBytesPerPixel;
    /// \brief The height of the image in pixel or lines.
    int iHeight;
    /// \brief The width of the image in pixel.
    int iWidth;
    /// \brief The pixel format of this image.
    /**
     *  This might be important, when the image data needs to be processed or stored in
     *  a file or maybe even if the image shall be displayed.
     */
    TImageBufferPixelFormat pixelFormat;
    /// \brief The size (in bytes) of the whole image.
    /**
     *  This value in connection with <b>mvIMPACT::acquire::ImageBuffer::vpData</b>
     *  is sufficient to copy the complete image without having any additional information about it.
     */
    int iSize;
    /// \brief The starting address of the image.
    /**
     *  This address in connection with <b>mvIMPACT::acquire::ImageBuffer::iSize</b>
     *  is sufficient to copy the complete image without having any additional information about it.
     *
     * \if (DOXYGEN_CPP_DOCUMENTATION || DOXYGEN_C_DOCUMENTATION)
     *  <b>EXAMPLE:</b><br><br>
     * \code
     *  const ImageBuffer* pib = getImageBufferFromSomewhere();
     *  unsigned char* pTempBuf = new unsigned char[ib.iSize];
     *  memcpy( pTempBuf, pib.vpData, pIB.iSize );
     * \endcode
     * \elseif DOXYGEN_NET_DOCUMENTATION
     * \attention To copy this data from within a .NET environment the appropriate marshaling functions must be used!
     * \elseif DOXYGEN_JAVA_DOCUMENTATION
     * \attention Do \b NOT use this pointer in Java! It references native, unmanaged memory and cannot be copied directly into
     * a piece of managed Java memory. Use <b>mvIMPACT::acquire::ImageBufferDesc::copy()</b> instead (see code below)!
     * \code
     *  byte[] data = new byte[pRequest.getImageSize().read()];
     *  pRequest.getImageBufferDesc().copy( data );
     * \endcode
     * \elseif DOXYGEN_PYTHON_DOCUMENTATION
     * \attention There are various ways to access the data captured into a request object. One way would be by using the \c ctypes module:
     * \code
     *  ib = getImageBufferFromSomewhere()
     *  cbuf = (ctypes.c_char * ib.iSize).from_address(int(ib.vpData))
     * \endcode
     * \endif
     *
     *  \note
     *  It's not always necessary to copy the image data! Each <b>mvIMPACT::acquire::ImageBuffer</b> is an
     *  integral part of the <b>mvIMPACT::acquire::Request</b> object returned to the user by a call to
     *  the corresponding 'waitFor' function offered by the interface.
     *  The data in this <b>mvIMPACT::acquire::ImageBuffer</b> remains valid until the user either
     *  unlocks the request buffer or closes the <b>mvIMPACT::acquire::Device</b> again.
     *
     *  \note
     *  By unlocking the <b>mvIMPACT::acquire::Request</b> the user informs the driver, that this <b>mvIMPACT::acquire::Request</b>
     *  and the <b>mvIMPACT::acquire::ImageBuffer</b> belonging to that <b>mvIMPACT::acquire::Request</b> is not longer needed by the
     *  user. The driver then queues this <b>mvIMPACT::acquire::Request</b> for capturing image data into it once
     *  again. However once a <b>mvIMPACT::acquire::Request</b> has been returned to the user, its <b>mvIMPACT::acquire::ImageBuffer</b>
     *  can't be overwritten by the driver! Therefore the user can work with, modify, store or
     *  copy the data safely until he unlocks the <b>mvIMPACT::acquire::Request</b> again.
     */
    void* vpData;
    /// \brief The number of channels this image consists of.
    /**
     *  For an RGB image this value e.g. would be 3. This value defines how many
     *  <b>mvIMPACT::acquire::ChannelData</b> structures <b>mvIMPACT::acquire::ImageBuffer::pChannels</b>
     *  is pointing to once this structure has been allocated and filled with valid data.
     */
    int iChannelCount;
    /// \brief A pointer to an array of channel specific image data.
    ChannelData* pChannels;
} ATTR_PACK;

#ifndef __GNUC__
#   pragma pack(pop) // restore previous structure alignment
#endif
#undef ATTR_PACK

#if defined(MVIMPACT_ACQUIRE_H_) || defined(MVIMPACT_ACQUIRE_GIGEVISION_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
} // namespace acquire
} // namespace mvIMPACT
#endif // #if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifdef __cplusplus
}
#endif // __cplusplus

// restore Borland compiler switch 'force enums to the size of integer'
#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
#   ifdef _WIN32
#       ifdef __BORLANDC__
#           pragma option pop
#       endif // __BORLANDC__
#   endif // _WIN32
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)


#endif // mvImageBufferH
