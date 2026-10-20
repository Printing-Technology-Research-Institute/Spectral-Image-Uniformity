#include "ImageCanvas.h"
#include <limits>

#include <apps/Common/qtIncludePrologue.h>
#include <QTextStream>
#include <apps/Common/qtIncludeEpilogue.h>

//-----------------------------------------------------------------------------
void Copy2ByteMonoImage( const Request* pRequest, QImage& image, unsigned int shift )
//-----------------------------------------------------------------------------
{
    const int width = pRequest->imageWidth.read();
    const int height = pRequest->imageHeight.read();
    const int srcPitch = pRequest->imageLinePitch.read();
    const unsigned char* pSrcData = reinterpret_cast<unsigned char*>( pRequest->imageData.read() );

    for( int y = 0; y < height; ++y )
    {
        const unsigned short* pSrc = reinterpret_cast<const unsigned short*>( pSrcData + y * srcPitch );
        unsigned char* pDst = image.scanLine( y );
        for( int x = 0; x < width; x++ )
        {
            const unsigned char val = static_cast<unsigned char>( std::max( 0, std::min( ( *pSrc ) >> shift, 255 ) ) );
            ++pSrc;
            *pDst++ = val;
        }
    }
}

//-----------------------------------------------------------------------------
void Copy2ByteRGBImage( const Request* pRequest, QImage& image, const unsigned int shift, const unsigned int inc )
//-----------------------------------------------------------------------------
{
    const int width = pRequest->imageWidth.read();
    const int height = pRequest->imageHeight.read();
    const int srcPitch = pRequest->imageLinePitch.read();
    const unsigned char* pSrcData = reinterpret_cast<unsigned char*>( pRequest->imageData.read() );

    for( int y = 0; y < height; y++ )
    {
        const unsigned short* pSrc = reinterpret_cast<const unsigned short*>( pSrcData + y * srcPitch );
        QRgb* pDst = reinterpret_cast<QRgb*>( image.scanLine( y ) );
        for( int x = 0; x < width; x++ )
        {
            *pDst++ = qRgb( static_cast<unsigned char>( std::max( 0, std::min( pSrc[2] >> shift, 255 ) ) ),
                            static_cast<unsigned char>( std::max( 0, std::min( pSrc[1] >> shift, 255 ) ) ),
                            static_cast<unsigned char>( std::max( 0, std::min( pSrc[0] >> shift, 255 ) ) ) );
            pSrc += inc;
        }
    }
}

//=============================================================================
//================= Implementation ImageCanvasImpl ============================
//=============================================================================
//-----------------------------------------------------------------------------
struct ImageCanvasImpl
//-----------------------------------------------------------------------------
{
    QLabel* pLabel_;
    QImage image_;

    ImageCanvasImpl( QLabel* pLabel ): pLabel_( pLabel ) {}
};

//=============================================================================
//================= Implementation ImageCanvas ================================
//=============================================================================
//-----------------------------------------------------------------------------
ImageCanvas::ImageCanvas( QLabel* pLabel )
//-----------------------------------------------------------------------------
{
    pImpl_ = new ImageCanvasImpl( pLabel );
    pImpl_->pLabel_->setAlignment( Qt::AlignHCenter );
}

//-----------------------------------------------------------------------------
void ImageCanvas::SetImage( Request* pRequest )
//-----------------------------------------------------------------------------
{
    if( pRequest && ( pRequest->getNumber() != INVALID_ID ) )
    {
        switch( pRequest->imagePixelFormat.read() )
        {
        case ibpfMono8:
            pImpl_->image_ = QImage( static_cast<uchar*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), QImage::Format_Grayscale8 );
            break;
        case ibpfMono16:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_Grayscale8 );
            Copy2ByteMonoImage( pRequest, pImpl_->image_, 8 );
            break;
        case ibpfRGBx888Packed:
            pImpl_->image_ = QImage( static_cast<uchar*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), QImage::Format_RGB32 );
            break;
        case ibpfBGR888Packed:
            pImpl_->image_ = QImage( static_cast<uchar*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), QImage::Format_RGB888 );
            break;
        case ibpfRGB888Packed:
            pImpl_->image_ = QImage( static_cast<uchar*>( pRequest->imageData.read() ), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read(), QImage::Format_RGB888 );
            pImpl_->image_ = pImpl_->image_.rgbSwapped(); // For historical reasons RGB in mvIMPACT Acquire is BGR...
            break;
        case ibpfMono10:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_Grayscale8 );
            Copy2ByteMonoImage( pRequest, pImpl_->image_, 2 );
            break;
        case ibpfMono12:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_Grayscale8 );
            Copy2ByteMonoImage( pRequest, pImpl_->image_, 4 );
            break;
        case ibpfMono14:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_Grayscale8 );
            Copy2ByteMonoImage( pRequest, pImpl_->image_, 6 );
            break;
        case ibpfRGB101010Packed:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_RGB32 );
            Copy2ByteRGBImage( pRequest, pImpl_->image_, 2, 3 );
            break;
        case ibpfRGB121212Packed:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_RGB32 );
            Copy2ByteRGBImage( pRequest, pImpl_->image_, 4, 3 );
            break;
        case ibpfRGB141414Packed:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_RGB32 );
            Copy2ByteRGBImage( pRequest, pImpl_->image_, 6, 3 );
            break;
        case ibpfRGB161616Packed:
            pImpl_->image_ = QImage( pRequest->imageWidth.read(), pRequest->imageHeight.read(), QImage::Format_RGB32 );
            Copy2ByteRGBImage( pRequest, pImpl_->image_, 8, 3 );
            break;
        case ibpfRaw:
        case ibpfYUV422Packed:
        case ibpfRGBx888Planar:
        case ibpfYUV444Planar:
        case ibpfMono32:
        case ibpfYUV422Planar:
        case ibpfYUV422_UYVYPacked:
        case ibpfMono12Packed_V2:
        case ibpfYUV422_10Packed:
        case ibpfYUV422_UYVY_10Packed:
        case ibpfBGR101010Packed_V2:
        case ibpfYUV444_UYVPacked:
        case ibpfYUV444_UYV_10Packed:
        case ibpfYUV444Packed:
        case ibpfYUV444_10Packed:
        case ibpfMono12Packed_V1:
        case ibpfYUV411_UYYVYY_Packed:
        case ibpfRGB888Planar:
        case ibpfAuto:
            // See ImageCanvas.cpp in the top-level directory of the wxPropView source to see how to convert all the formats not supported here into
            // a Qt-digestible way. After all wxWidgets more or less supports an almost similar image concept even though the QImage class offers
            // more thus it should be easy to adapt your code to the pixel format you need.
            {
                QString msg;
                QTextStream( &msg ) << "PIXEL FORMAT '" << pRequest->imagePixelFormat.read() << "' NOT SUPPORTED BY THE DISPLAY" << "\n" << "PLEASE REFER TO THE SOURCE CODE FOR MORE INFORMATION!";
                pImpl_->pLabel_->setText( msg );
                pRequest->unlock();
            }
            return;
        }
        pImpl_->pLabel_->setPixmap( QPixmap::fromImage( pImpl_->image_ ).scaled( pImpl_->pLabel_->width(), pImpl_->pLabel_->height(), Qt::IgnoreAspectRatio ) );
        // IMPORTANT: This approach allows to unlock the request now as Request -> QImage -> QPixmap conversion will result
        // in the data being copied. This is slower than necessary but always works and allows all resize, repaint etc.
        // events to be handled without further thinking. A more efficient yet more complicated in terms of implementation
        // way to display data on Windows is to use the mvDisplay library. See the 'ImageCanvas.cpp' file in the 'win32'
        // folder of this example for additional information.
        pRequest->unlock();
    }
}
