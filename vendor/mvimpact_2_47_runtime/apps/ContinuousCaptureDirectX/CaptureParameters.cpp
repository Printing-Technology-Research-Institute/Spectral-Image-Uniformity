//-----------------------------------------------------------------------------
#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <algorithm>
#include <apps/Common/exampleHelper.h>
#include "CaptureParameters.h"
#include <iostream>
#include <sstream>

using namespace std;

//=============================================================================
//================= Implementation CaptureParameters ==========================
//=============================================================================
//-----------------------------------------------------------------------------
CaptureParameters::CaptureParameters( Device* p ) : pDev_( p ), c_( p ), fi_( p ), statistics_( p ),
    hThread_( 0 ), boThreadRunning_( false ),
    boStatisticalOutputActive_( false ),
    DESTINATION_WIDTH_( GetSystemMetrics( SM_CXSCREEN ) ),
    DESTINATION_HEIGHT_( GetSystemMetrics( SM_CYSCREEN ) ),
    pDisplay_( 0 )
//-----------------------------------------------------------------------------
{
    ImageDestination id( pDev_ );
    id.pixelFormat.write( idpfRGB888Packed );
}

//-----------------------------------------------------------------------------
CaptureParameters::~CaptureParameters()
//-----------------------------------------------------------------------------
{
    stopThread();
    if( pDev_->isOpen() )
    {
        pDev_->close();
    }
}

//-----------------------------------------------------------------------------
void CaptureParameters::captureThread( void )
//-----------------------------------------------------------------------------
{
    boThreadRunning_ = true;
    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( fi_.imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pDev_, fi_ );
    // run thread loop
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 500;
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // cannot free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
    while( boThreadRunning_ )
    {
        // wait for results from the default capture queue
        requestNr = fi_.imageRequestWaitFor( timeout_ms );
        if( fi_.isRequestNrValid( requestNr ) )
        {
            pRequest = fi_.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                if( pDisplay_ )
                {
                    pDisplay_->SetImage( pRequest->getImageBufferDesc().getBuffer() );
                }
            }
            else
            {
                DbOutput( "Request did return, but with an error: %s.\n", pRequest->requestResult.readS().c_str() );
            }
            if( fi_.isRequestNrValid( lastRequestNr ) )
            {
                // this image has been displayed thus the buffer is no longer needed...
                fi_.imageRequestUnlock( lastRequestNr );
            }
            lastRequestNr = requestNr;
            // send a new image request into the capture queue
            fi_.imageRequestSingle();
        }
        // Please note that slow systems or interface technologies in combination with high resolution sensors
        // might need more time to transmit an image than the timeout value which has been passed to imageRequestWaitFor().
        // If this is the case simply wait multiple times OR increase the timeout(not recommended as usually not necessary
        // and potentially makes the capture thread less responsive) and rebuild this application.
        // Once the device is configured for triggered image acquisition and the timeout elapsed before
        // the device has been triggered this might happen as well.
        // The return code would be -2119(DEV_WAIT_FOR_REQUEST_FAILED) in that case, the documentation will provide
        // additional information under TDMR_ERROR in the interface reference.
        // If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
        // to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
        // cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
        //   << ", timeout value too small?" << endl;
        //else
        //{
        //    DbOutput( "No request has been returned after %d ms. imageRequestWaitFor returned %s.\n", timeout_ms, ImpactAcquireException::getErrorCodeAsString( requestNr ).c_str() );
        //}
    }
    manuallyStopAcquisitionIfNeeded( pDev_, fi_ );

    // stop the display from showing freed memory
    if( pDisplay_ )
    {
        pDisplay_->RemoveImage();
    }
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( fi_.isRequestNrValid( requestNr ) )
    {
        fi_.imageRequestUnlock( requestNr );
    }
    // clear all queues
    fi_.imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
int CaptureParameters::availableVideoChannels( void ) const
//-----------------------------------------------------------------------------
{
    return ( ( c_.videoChannel.isValid() ) ? c_.videoChannel.read( plMaxValue ) + 1 : 1 );
}

//-----------------------------------------------------------------------------
int CaptureParameters::currentVideoChannel( void ) const
//-----------------------------------------------------------------------------
{
    return ( ( c_.videoChannel.isValid() ) ? c_.videoChannel.read() : 0 );
}

//-----------------------------------------------------------------------------
void CaptureParameters::selectNextVideoChannel( void )
//-----------------------------------------------------------------------------
{
    if( c_.videoChannel.isValid() )
    {
        c_.videoChannel.write( ( c_.videoChannel.read() + 1 ) % ( c_.videoChannel.read( plMaxValue ) + 1 ) );
    }
}

//-----------------------------------------------------------------------------
unsigned int __stdcall CaptureParameters::startCaptureThread( void* pData )
//-----------------------------------------------------------------------------
{
    CaptureParameters* pParams = reinterpret_cast<CaptureParameters*>( pData );

    pParams->captureThread();
    return 0;
}

//-----------------------------------------------------------------------------
void CaptureParameters::startThread( void )
//-----------------------------------------------------------------------------
{
    if( !boThreadRunning_ )
    {
        unsigned int dwThreadID = 0;
        hThread_ = ( HANDLE )_beginthreadex( 0, 0, startCaptureThread, ( LPVOID )( this ), 0, &dwThreadID );
    }
}

//-----------------------------------------------------------------------------
void CaptureParameters::stopThread( void )
//-----------------------------------------------------------------------------
{
    if( boThreadRunning_ )
    {
        boThreadRunning_ = false;
        WaitForSingleObject( hThread_, INFINITE );
        CloseHandle( hThread_ );
        hThread_ = 0;
    }
}

//=============================================================================
//================= Implementation helper functions ===========================
//=============================================================================
//---------------------------------------------------------------------------
void DbOutput( const char* pszFormat, ... )
//---------------------------------------------------------------------------
{
    size_t bufSize = 256;
    char* pBuf = new char[bufSize];
    va_list argptr;
    va_start( argptr, pszFormat );
#if (_MSC_VER >= 1400) // is at least VC 2005 compiler
    while( vsnprintf_s( pBuf, bufSize, _TRUNCATE, pszFormat, argptr ) < 0 )
#else
    while( _vsnprintf( pBuf, bufSize - 1, pszFormat, argptr ) < 0 )
#endif // if (_MSC_VER >= 1400)
    {
        va_end( argptr );
        delete [] pBuf;
        bufSize *= 2;
        pBuf = new char[bufSize];
        va_start( argptr, pszFormat );
    }
    pBuf[bufSize - 1] = '\0';
    va_end( argptr );
    OutputDebugStringA( pBuf );
    delete [] pBuf;
}

//---------------------------------------------------------------------------
string BuildFormattedString( const char* pszFormat, ... )
//---------------------------------------------------------------------------
{
    size_t bufSize = 256;
    char* pBuf = new char[bufSize];
    va_list argptr;
    va_start( argptr, pszFormat );
#if (_MSC_VER >= 1400) // is at least VC 2005 compiler
    while( vsnprintf_s( pBuf, bufSize, _TRUNCATE, pszFormat, argptr ) < 0 )
#else
    while( _vsnprintf( pBuf, bufSize - 1, pszFormat, argptr ) < 0 )
#endif // if (_MSC_VER >= 1400)
    {
        va_end( argptr );
        delete [] pBuf;
        bufSize *= 2;
        pBuf = new char[bufSize];
        va_start( argptr, pszFormat );
    }
    pBuf[bufSize - 1] = '\0';
    va_end( argptr );
    string result( pBuf );
    delete [] pBuf;
    return result;
}