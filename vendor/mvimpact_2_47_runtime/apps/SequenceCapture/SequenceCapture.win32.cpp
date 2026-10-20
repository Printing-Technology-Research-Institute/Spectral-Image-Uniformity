/// \todo ?
#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <windows.h>
#include <process.h>
#include <conio.h>
#include <iostream>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>
#include <deque>
#include <apps/Common/exampleHelper.h>
#include <apps/Common/aviwrapper.h>

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

static bool s_boTerminated = false;

typedef deque<ImageBufferDesc> ImageQueue;

//-----------------------------------------------------------------------------
class ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*                 pDev_;
    ImageDisplayWindow      displayWindow_;
    ImageQueue&             imageQueue_;
    ImageQueue::size_type   maxQueueSize_;
    int                     frameRate_;

    ThreadParameter& operator=( const ThreadParameter& rhs ); // do NOT allow assignment
public:
    ThreadParameter( Device* p, const std::string& windowTitle, ImageQueue& q, ImageQueue::size_type maxSize, int fr )
        : pDev_( p ), displayWindow_( windowTitle ), imageQueue_( q ), maxQueueSize_( maxSize ), frameRate_( fr ) {}
    int getFrameRate( void ) const
    {
        return frameRate_;
    }
    void setFrameRate( int fr )
    {
        frameRate_ = fr;
    }
    Device* getDevice( void ) const
    {
        return pDev_;
    }
    ImageDisplayWindow& getDisplayWindow( void )
    {
        return displayWindow_;
    }
    ImageQueue& getImageQueue( void ) const
    {
        return imageQueue_;
    }
    ImageQueue::size_type getMaxQueueSize( void ) const
    {
        return maxQueueSize_;
    }
};

//-----------------------------------------------------------------------------
void inplaceHorizontalMirror( const ImageBuffer* p )
//-----------------------------------------------------------------------------
{
    int upperHalfOfLines = p->iHeight / 2; // the line in the middle (if existent) doesn't need to be processed!
    size_t pitch = p->pChannels[0].iLinePitch; // this only works for image formats where each channel has the same line pitch!
    char* pLowerLine = static_cast<char*>( p->vpData ) + ( ( p->iHeight - 1 ) * pitch );
    char* pUpperLine = static_cast<char*>( p->vpData );
    char* pTmpLine = new char[pitch];

    for( int y = 0; y < upperHalfOfLines; y++ )
    {
        memcpy( pTmpLine, pUpperLine, pitch );
        memcpy( pUpperLine, pLowerLine, pitch );
        memcpy( pLowerLine, pTmpLine, pitch );
        pUpperLine += pitch;
        pLowerLine -= pitch;
    }
    delete [] pTmpLine;
}

//-----------------------------------------------------------------------------
// Currently only the mvBlueFOX supports HRTC and thus the definition of an
// absolute frame rate during the capture process.
void setupBlueFOXFrameRate( Device* pDev, int& frameRate_Hz )
//-----------------------------------------------------------------------------
{
    cout << "To use the HRTC to configure the mvBlueFOX to capture with a defined frequency press 'y'." << endl;
    if( _getch() != 'y' )
    {
        return;
    }
    // mvBlueFOX devices can define a fixed frame frequency
    cout << "Enter the desired capture frame rate in Hz: ";
    cin >> frameRate_Hz;
    cout << "Trying to capture at " << frameRate_Hz << " frames per second. Please make sure the device can deliver this frame rate" << endl
         << "as otherwise the resulting AVI stream will be replayed with an incorrect speed" << endl;

    int frametime_us = static_cast<int>( 1000000.0 * ( 1.0 / static_cast<double>( frameRate_Hz ) ) );
    const int TRIGGER_PULSE_WIDTH_us = 100;
    if( frametime_us < 2 * TRIGGER_PULSE_WIDTH_us )
    {
        cout << "frame rate too high (" << frameRate_Hz << "). Using 10 Hz." << endl;
        frametime_us = 100000;
    }

    CameraSettingsBlueFOX bfs( pDev );
    if( bfs.expose_us.read() > frametime_us / 2 )
    {
        ostringstream oss;
        oss << "Reducing frame-time from " << bfs.expose_us.read() << " us to " << frametime_us / 2 << " us." << endl
            << "Higher values are possible but require a more sophisticated HRTC program" << endl;
        bfs.expose_us.write( frametime_us / 2 );
    }

    IOSubSystemBlueFOX bfIOs( pDev );
    // define a HRTC program that results in a define image frequency
    // the hardware real time controller shall be used to trigger an image
    bfs.triggerSource.write( ctsRTCtrl );
    // when the hardware real time controller switches the trigger signal to
    // high the exposure of the image shall start
    bfs.triggerMode.write( ctmOnRisingEdge );

    // error checks
    if( bfIOs.RTCtrProgramCount() == 0 )
    {
        // no HRTC controllers available (this never happens for the mvBlueFOX)
        cout << "This device (" << pDev->product.read() << ") doesn't support HRTC" << endl;
        return;
    }

    RTCtrProgram* pRTCtrlprogram = bfIOs.getRTCtrProgram( 0 );
    if( !pRTCtrlprogram )
    {
        // this only should happen if the system is short of memory
        cout << "Error! No valid program. Short of memory?" << endl;
        return;
    }

    // start of the program
    // we need 5 steps for the program
    pRTCtrlprogram->setProgramSize( 5 );

    // wait a certain amount of time to achieve the desired frequency
    int progStep = 0;
    RTCtrProgramStep* pRTCtrlStep = pRTCtrlprogram->programStep( progStep++ );
    pRTCtrlStep->opCode.write( rtctrlProgWaitClocks );
    pRTCtrlStep->clocks_us.write( frametime_us - TRIGGER_PULSE_WIDTH_us );

    // trigger an image
    pRTCtrlStep = pRTCtrlprogram->programStep( progStep++ );
    pRTCtrlStep->opCode.write( rtctrlProgTriggerSet );

    // high time for the trigger signal (should not be smaller than 100 us)
    pRTCtrlStep = pRTCtrlprogram->programStep( progStep++ );
    pRTCtrlStep->opCode.write( rtctrlProgWaitClocks );
    pRTCtrlStep->clocks_us.write( TRIGGER_PULSE_WIDTH_us );

    // end trigger signal
    pRTCtrlStep = pRTCtrlprogram->programStep( progStep++ );
    pRTCtrlStep->opCode.write( rtctrlProgTriggerReset );

    // restart the program
    pRTCtrlStep = pRTCtrlprogram->programStep( progStep++ );
    pRTCtrlStep->opCode.write( rtctrlProgJumpLoc );
    pRTCtrlStep->address.write( 0 );

    // start the program
    pRTCtrlprogram->mode.write( rtctrlModeRun );

    // Now this camera will deliver images at exactly the desired frequency
    // when it is constantly feed with image requests and the camera can deliver
    // images at this frequency.
}

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );

    cout << "Initialising the device. This might take some time..." << endl;
    try
    {
        if( !pThreadParameter->getDevice()->isOpen() )
        {
            pThreadParameter->getDevice()->open();
        }
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening device " << pThreadParameter->getDevice()->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << "). Press any key to end the application..." << endl;
        return _getch();
    }

    ImageDisplay& display = pThreadParameter->getDisplayWindow().GetImageDisplay();
    // establish access to the statistic properties
    Statistics statistics( pThreadParameter->getDevice() );
    // create an interface to the device found
    FunctionInterface fi( pThreadParameter->getDevice() );

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( fi.imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->getDevice(), fi );
    // run thread loop
    const unsigned int timeout_ms = 500;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // cannot free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
    unsigned int cnt = 0;
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        const int requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            const Request* pRequest = fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    cout << "Info from " << pThreadParameter->getDevice()->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
                display.SetImage( pRequest );
                display.Update();
                // append a new image at the end of the queue (the image will be deep-copied)
                pThreadParameter->getImageQueue().push_back( pRequest->getImageBufferDesc().clone() );
                // if the queue has the user defined max. number of entries remove the oldest one
                // with this method we always keep the most recent images
                if( pThreadParameter->getImageQueue().size() > pThreadParameter->getMaxQueueSize() )
                {
                    pThreadParameter->getImageQueue().pop_front();
                }
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            if( fi.isRequestNrValid( lastRequestNr ) )
            {
                // this image has been displayed thus the buffer is no longer needed...
                fi.imageRequestUnlock( lastRequestNr );
            }
            lastRequestNr = requestNr;
            // send a new image request into the capture queue
            fi.imageRequestSingle();
        }
        else
        {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference
            cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
                 << ", timeout value too small?" << endl;
        }
    }

    // obtain the frame rate for the replay and the AVI stream
    const double fr = statistics.framesPerSecond.read();
    pThreadParameter->setFrameRate( static_cast<int>( fr ) );
    if( ( fr - static_cast<double>( static_cast<int>( fr ) ) ) >= 0.5 )
    {
        pThreadParameter->setFrameRate( pThreadParameter->getFrameRate() + 1 );
    }

    manuallyStopAcquisitionIfNeeded( pThreadParameter->getDevice(), fi );

    // stop the display from showing freed memory
    display.RemoveImage();
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( fi.isRequestNrValid( lastRequestNr ) )
    {
        fi.imageRequestUnlock( lastRequestNr );
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
    return 0;
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr );
    if( !pDev )
    {
        cout << "Could not obtain a valid pointer to a device. Unable to continue! Press any key to end the program." << endl;
        return _getch();
    }

    int captureFrameRate = 0;
    if( pDev->family.read() == "mvBlueFOX" )
    {
        setupBlueFOXFrameRate( pDev, captureFrameRate );
    }

    ImageQueue::size_type maxQueueSize = 0;
    cout << "Enter the length of the sequence to buffer (please note that this might be limited by your systems memory): ";
    cin >> maxQueueSize;

    // display available destination formats
    ImageDestination id( pDev );
    vector<pair<string, TImageDestinationPixelFormat> > vAvailableDestinationFormats;
    id.pixelFormat.getTranslationDict( vAvailableDestinationFormats );
    int vSize = static_cast<int>( vAvailableDestinationFormats.size() );
    cout << "Available destination formats: " << endl;
    for( int i = 0; i < vSize; i++ )
    {
        cout << "[" << vAvailableDestinationFormats[i].first << "]: " << vAvailableDestinationFormats[i].second << endl;
    }
    cout << endl << endl;
    cout << "If AVI files shall be written please note, that most AVI compression handlers" << endl
         << "accept RGB888Packed formats only. Apart from that planar formats are not supported" << endl
         << "by this sample in order to keep things simple." << endl << endl
         << "Destination format (as integer): ";
    int destinationPixelFormat = 0;
    cin >> destinationPixelFormat;

    // set destination format
    try
    {
        id.pixelFormat.write( static_cast<TImageDestinationPixelFormat>( destinationPixelFormat ) );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "Failed to set destination pixel format(" << e.getErrorCodeAsString() << "), using default" << endl;
        id.pixelFormat.write( idpfRGB888Packed );
    }
    cout << "Using " << id.pixelFormat.readS() << "." << endl;

    // start the execution of the 'live' thread.
    cout << "Press [ENTER] to stop the acquisition thread" << endl;
    unsigned int dwThreadID;
    ImageQueue imageQueue;

    string windowTitle( "mvIMPACT_acquire sample, Device " + pDev->serial.read() );
    // initialise display window
    // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
    // IMPORTANT: If you need to access the queue from multiple threads appropriate security
    // mechanisms (e.g. critical sections) must be used. Here we don't care about that as we
    // will NOT access the queue from multiple threads at the same time!
    ThreadParameter threadParam( pDev, windowTitle, imageQueue, maxQueueSize, captureFrameRate );
    HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
    if( _getch() == EOF )
    {
        cout << "Calling '_getch()' did return EOF...\n";
    }
    s_boTerminated = true;
    WaitForSingleObject( hThread, INFINITE );
    CloseHandle( hThread );

    if( imageQueue.empty() )
    {
        cout << "No images have been captured thus no playback or storage can be performed"
             << "Press any key to end the application" << endl;
        if( _getch() == EOF )
        {
            cout << "Calling '_getch()' did return EOF...\n";
        }
        return 1;
    }

    const ImageQueue::size_type qSize = imageQueue.size();
    bool boRun = true;
    while( boRun )
    {
        cout << "Press 'y' to replay the captured sequence of " << qSize << " images from memory or any other key to end the replay loop." << endl;
        char c = static_cast<char>( _getch() );
        if( c != 'y' )
        {
            boRun = false;
            continue;
        }
        // delay between two image during display
        const DWORD frameDelay = ( threadParam.getFrameRate() > 0 ) ? 1000 / threadParam.getFrameRate() : 40;
        // obtain pointer to display structure
        ImageDisplay& display = threadParam.getDisplayWindow().GetImageDisplay();
        cout << "Replaying the last " << qSize << " captured images with " <<  1000 / frameDelay << " Hz..." << endl;
        for( ImageQueue::size_type i = 0; i < qSize; i++ )
        {
            display.SetImage( imageQueue[i].getBuffer() );
            display.Update();
            Sleep( frameDelay );
        }
    }

    // ask the user if the sequence shall be written into a AVI file
    cout << endl;
    cout << "If you want to save the captured sequence press 'y' or any other key to end the application: ";
    if( _getch() != 'y' )
    {
        return 0;
    }

    cout << endl << "Please enter the file name for the resulting AVI stream(use proper file extensions like *.avi as otherwise creating the stream may fail): ";
    string fileName;
    cin >> fileName;
    // Now we have to create and configure the AVI stream
    boRun = true;
    while( boRun )
    {
        try
        {
            // create the AVI file builder
            AVIWrapper myAVIWrapper;
            myAVIWrapper.OpenAVIFile( fileName.c_str(), OF_WRITE | OF_CREATE | OF_SHARE_DENY_WRITE );
            // To select from installed compression handlers, pass codecMax as codec to the next function, which is also
            // the default parameter if not specified. Windows will display a dialog to select the codec then.
            // Most codecs only accept RGB888 data with no alpha byte. Make sure that either the driver is
            // operated in RGB888Packed mode or you supply the correct image data converted by hand here.
            cout << "Please select a compression handler from the dialog box (which might be hidden behind this window)" << endl << endl;
            const ImageBuffer* pIB = imageQueue.front().getBuffer();
            myAVIWrapper.CreateAVIStreamFromDIBs( pIB->iWidth, pIB->iHeight, pIB->iBytesPerPixel * 8, threadParam.getFrameRate(), 8000, "myStream" );
            boRun = false;
            // we should have a valid AVI stream by now thus we can start to write the images to it
            for( ImageQueue::size_type x = 0; x < qSize; x++ )
            {
                cout << "Storing image " << x << " in stream " << fileName << ".\r";
                const ImageBuffer* pIB = imageQueue[x].getBuffer();
                // Unfortunately we have to flip the images as they are stored upside down in the stream...
                inplaceHorizontalMirror( pIB );
                myAVIWrapper.SaveDataToAVIStream( reinterpret_cast<unsigned char*>( pIB->vpData ), pIB->iSize );
            }
        }
        catch( const AVIException& e )
        {
            cout << "Error while creating AVI stream(" << string( e.what() ) << ")." << endl
                 << "Please note, that not every codec will accept every pixel format, thus this error might" << endl
                 << "appear without changing the destination pixel format within the driver. However the" << endl
                 << "format selected in this sample (RGB888Packed) works for the greatest number of codecs" << endl
                 << "Unable to continue. Press 'q' to end the application or any other key to select a different" << endl
                 << "compression handler." << endl;
            if( _getch() == 'q' )
            {
                return 1;
            }
        }
    }

    return 0;
}
