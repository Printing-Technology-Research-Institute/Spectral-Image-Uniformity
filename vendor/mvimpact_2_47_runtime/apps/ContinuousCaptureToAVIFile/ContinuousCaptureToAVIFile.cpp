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
#include <apps/Common/exampleHelper.h>
#include <apps/Common/aviwrapper.h>

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device*             pDev;
    ImageDisplayWindow  displayWindow;
    AVIWrapper*         pAVIWrapper;
    ThreadParameter( Device* p, std::string& windowTitle, AVIWrapper* pAVI )
        : pDev( p ), displayWindow( windowTitle ), pAVIWrapper( pAVI ) {}
};

//-----------------------------------------------------------------------------
void displayCommandLineOptions( void )
//-----------------------------------------------------------------------------
{
    cout << "Available parameters:" << endl
         << "  'outputFile' or 'of' to specify the name of the resulting AVI file" << endl
         << "  'frameRate' or 'fr' to specify the frame rate(frames per second for playback) of the resulting AVI file" << endl
         << "  'recordingTime' or 'rt' to specify the time in ms the sample shall capture image data. If this parameter" << endl
         << "    is omitted, the capture process will be aborted after the user pressed a key" << endl
         << endl
         << "USAGE EXAMPLE:" << endl
         << "  ContinuousCaptureToAVIFile rt=5000 of=myfile.avi frameRate=25" << endl << endl;
}

//-----------------------------------------------------------------------------
void inplaceHorizontalMirror( void* pData, int height, size_t pitch )
//-----------------------------------------------------------------------------
{
    int upperHalfOfLines = height / 2; // the line in the middle (if existent) doesn't need to be processed!
    char* pLowerLine = static_cast<char*>( pData ) + ( ( height - 1 ) * pitch );
    char* pUpperLine = static_cast<char*>( pData );
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
void setupBlueFOXFrameRate( Device* pDev, int frameRate_Hz )
//-----------------------------------------------------------------------------
{
    cout << "To use the HRTC to configure the mvBlueFOX to capture with a defined frequency press 'y'." << endl;
    if( _getch() != 'y' )
    {
        return;
    }
    // mvBlueFOX devices can define a fixed frame frequency
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
    RTCtrProgramStep* pRTCtrlStep = 0;
    pRTCtrlStep = pRTCtrlprogram->programStep( progStep++ );
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
void storeImageToStream( FunctionInterface& fi, int requestNr, AVIWrapper* pAVIWrapper )
//-----------------------------------------------------------------------------
{
    if( fi.isRequestNrValid( requestNr ) )
    {
        const Request* pRequest = fi.getRequest( requestNr );
        if( pRequest->isOK() )
        {
            // store to AVI file
            try
            {
                // in a real application this would be done in a separate thread in order to
                // buffer hard-disc delays.
                // Unfortunately we have to flip the images as they are stored upside down in the stream...

                // this function only works for image formats where each channel has the same line pitch!
                inplaceHorizontalMirror( pRequest->imageData.read(), pRequest->imageHeight.read(), pRequest->imageLinePitch.read() );
                pAVIWrapper->SaveDataToAVIStream( reinterpret_cast<unsigned char*>( pRequest->imageData.read() ), pRequest->imageSize.read() );
            }
            catch( const AVIException& e )
            {
                cout << "Could not store image to stream(" << string( e.what() ) << ")" << endl;
            }
        }
    }
}

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );

    ImageDisplay& display = pThreadParameter->displayWindow.GetImageDisplay();

    // create an interface to the device found
    FunctionInterface fi( pThreadParameter->pDev );

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

    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, fi );
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 500;
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // cannot free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;

    // run thread loop
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            pRequest = fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                display.SetImage( pRequest );
                display.Update();
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            // As images might be redrawn by the display window, we can't process the image currently
            // displayed. In order not to copy the current image, which would cause additional CPU load
            // we will flip and store the previous image if available
            storeImageToStream( fi, lastRequestNr, pThreadParameter->pAVIWrapper );
            if( fi.isRequestNrValid( lastRequestNr ) )
            {
                // this image has been displayed thus the buffer is no longer needed...
                fi.imageRequestUnlock( lastRequestNr );
            }
            lastRequestNr = requestNr;
            // send a new image request into the capture queue
            fi.imageRequestSingle();
        }
        //else
        //{
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
        //}
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->pDev, fi );

    // stop the display from showing freed memory
    display.RemoveImage();
    // try to store the last image into the stream
    storeImageToStream( fi, requestNr, pThreadParameter->pAVIWrapper );
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( fi.isRequestNrValid( requestNr ) )
    {
        fi.imageRequestUnlock( requestNr );
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
    return 0;
}

//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr );
    if( !pDev )
    {
        cout << "Unable to continue! Press any key to end the program." << endl;
        return _getch();
    }

    // default parameters
    string fileName( ".\\output.avi" );
    unsigned int frameRate = 25;
    unsigned int recordingTime = 0;
    bool boInvalidCommandLineParameterDetected = false;
    // scan command line
    if( argc > 1 )
    {
        for( int i = 1; i < argc; i++ )
        {
            const string param( argv[i] );
            const string::size_type keyEnd = param.find_first_of( "=" );
            if( ( keyEnd == string::npos ) || ( keyEnd == param.length() - 1 ) )
            {
                cout << "Invalid command line parameter: '" << param << "' (ignored)." << endl;
                boInvalidCommandLineParameterDetected = true;
            }
            else
            {
                const string key = param.substr( 0, keyEnd );
                const string value = param.substr( keyEnd + 1 );
                if( ( key == "outputFile" ) || ( key == "of" ) )
                {
                    fileName = value;
                }
                else if( ( key == "frameRate" ) || ( key == "fr" ) )
                {
                    frameRate = static_cast<unsigned int>( atoi( value.c_str() ) );
                }
                else if( ( key == "recordingTime" ) || ( key == "rt" ) )
                {
                    recordingTime = static_cast<unsigned int>( atoi( value.c_str() ) );
                }
                else
                {
                    cout << "Invalid command line parameter: '" << param << "' (ignored)." << endl;
                    boInvalidCommandLineParameterDetected = true;
                }
            }
        }
        if( boInvalidCommandLineParameterDetected )
        {
            displayCommandLineOptions();
        }
    }
    else
    {
        cout << "No command line parameters specified." << endl;
        displayCommandLineOptions();
    }

    cout <<  endl
         << "PLEASE NOTE THAT THIS EXAMPLE APPLICATION MAKES USE OF A VERY OLD, OUTDATED WINDOWS ONLY API WHICH IS NOT RECOMMENDED FOR NEW PROJECTS!" << endl
         << "There are various other, more portable ways to encode/store a video stream there day. Please consider using the FFmpeg library (see" << endl
         << "'ContinuousCaptureFFmpeg' in the C++ manual) or something similar instead!" << endl
         << endl
         << "Using output file '" << fileName << "' with " << frameRate << " frames per second for playback (this has nothing to do with the capture frame rate but only affects the frame rate stored in the header of the AVI file)" << endl
         << endl
         << "Please note that if the frame rate specified only affects the playback speed for the resulting AVI file." << endl
         << "Devices that support a fixed frame rate should be set to the same rate, but this won't be done" << endl
         << "in this sample, thus the playback speed of the AVI file might differ from the real acquisition speed" << endl;

    cout << "Initialising the device. This might take some time..." << endl << endl;
    try
    {
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening device " << pDev->serial.read()
             << "(error code: " << e.getErrorCode() << "). Press any key to end the application..." << endl;
        return _getch();
    }

    int width = 0, height = 0, bitcount = 0;
    try
    {
        // set up the device for AVI output.
        // most codecs only accept RGB888 data with no alpha byte. Make sure that either the driver is
        // operated in RGB888Packed mode or you supply the correct image data converted by hand here.
        // Here we select the color mode satisfying most codecs, so this sample will work in most cases, but not always.
        // For details about the used codec have a look on the net...
        ImageDestination id( pDev );
        id.pixelFormat.write( idpfRGB888Packed );
        // Now we need to find out the dimension of the resulting image. Thus we have to perform a dummy image capture.
        ImageRequestControl irc( pDev );
        FunctionInterface fi( pDev );
        Request* pCurrentCaptureBufferLayout = 0;
        fi.getCurrentCaptureBufferLayout( irc, &pCurrentCaptureBufferLayout );
        // now we have the information needed to configure the AVI stream
        width = pCurrentCaptureBufferLayout->imageWidth.read();
        height = pCurrentCaptureBufferLayout->imageHeight.read();
        bitcount = pCurrentCaptureBufferLayout->imageBytesPerPixel.read() * 8;
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "An exception occurred while configuring the device: " << e.getErrorString() << "(" << e.getErrorCode() << ")." << endl
             << "Unable to continue. Press any key to end the application." << endl << endl;
        return _getch();
    }

    if( pDev->family.read() == "mvBlueFOX" )
    {
        setupBlueFOXFrameRate( pDev, frameRate );
    }

    // Now we have to create and configure the AVI stream
    // create the AVI file builder
    try
    {
        AVIWrapper myAVIWrapper;
        myAVIWrapper.OpenAVIFile( fileName.c_str(), OF_WRITE | OF_CREATE | OF_SHARE_DENY_WRITE );
        // To select from installed compression handlers, pass codecMax as codec to the next function, which is also
        // the default parameter if not specified. Windows will display a dialog to select the codec then.
        // Most codecs only accept RGB888 data with no alpha byte. Make sure that either the driver is
        // operated in RGB888Packed mode or you supply the correct image data converted by hand here.
        cout << "Please select a compression handler from the dialog box" << endl << endl;
        myAVIWrapper.CreateAVIStreamFromDIBs( width, height, bitcount, frameRate, 8000, "myStream" );

        // The remaining work is almost the same as for every other continuous acquisition. We have to start a capture thread
        // and configure the display to show the captured images that will also be written to the stream...

        // start the execution of the 'live' thread.
        unsigned int dwThreadID;
        string windowTitle( "mvIMPACT_acquire sample, Device " + pDev->serial.read() );
        // initialise display window
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        ThreadParameter threadParam( pDev, windowTitle, &myAVIWrapper );
        HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
        if( recordingTime == 0 )
        {
            cout << "Press any key to end the application" << endl;
            if( _getch() == EOF )
            {
                cout << "'_getch()' did return EOF..." << endl;
            }
            s_boTerminated = true;
            WaitForSingleObject( hThread, INFINITE );
            CloseHandle( hThread );
        }
        else
        {
            cout << "Recording for " << recordingTime << " ms. Please wait..." << endl;
            Sleep( recordingTime );
            s_boTerminated = true;
            WaitForSingleObject( hThread, INFINITE );
            CloseHandle( hThread );
            cout << "Press any key to end the application" << endl;
            return _getch();
        }
    }
    catch( const AVIException& e )
    {
        cout << "Error while creating AVI stream(" << string( e.what() ) << ")." << endl
             << "Please note, that not every codec will accept every pixel format, thus this error might" << endl
             << "appear without changing the destination pixel format within the driver. However the" << endl
             << "format selected in this sample (RGB888Packed) works for the greatest number of codecs" << endl
             << "Unable to continue. Press any key to end the application." << endl;
        return _getch();
    }

    return 0;
}
