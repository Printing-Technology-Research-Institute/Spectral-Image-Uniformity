#include <algorithm>
#include <ctime>
#include <functional>
#include <iostream>
#include <thread>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam_CustomCommands.h>
#ifdef _WIN32
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#else
#   include <stdio.h>
#   include <unistd.h>
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

static bool s_boTerminated = false;

//=============================================================================
//================= Data type definitions =====================================
//=============================================================================
//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev;
    GenICam::CustomCommandGenerator ccg;
    int width; // the width of the ROI to request for transmission
    int height; // the height of the ROI to request for transmission
    int requestRate; // the rate at which full resolution frames shall be requested. A 'requestRate' of 2 will request every 2nd image from the stream again in full resolution
#ifdef USE_DISPLAY
    ImageDisplayWindow displayWindowStream;
    // As the display might want to repaint the image we must keep it until the next one has been assigned to the display
    Request* pLastRequestForStream;
    ImageDisplayWindow displayWindowExplicitRequestedByHost;
    // As the display might want to repaint the image we must keep it until the next one has been assigned to the display
    Request* pLastRequestExplicitlyRequested;
#endif // #ifdef USE_DISPLAY
    explicit ThreadParameter( Device* p, int w, int h, int rr ) : pDev( p ), ccg( pDev ), width( w ), height( h ), requestRate( rr )
#ifdef USE_DISPLAY
        // initialise display window
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        , displayWindowStream( "mvIMPACT_acquire sample, Device " + pDev->serial.read() + "(live stream)" )
        , pLastRequestForStream( 0 )
        , displayWindowExplicitRequestedByHost( "mvIMPACT_acquire sample, Device " + pDev->serial.read() + "(explicit requests by the host)" )
        , pLastRequestExplicitlyRequested( 0 )
#endif // #ifdef USE_DISPLAY
    {}
    ThreadParameter( const ThreadParameter& src ) = delete;
    ThreadParameter& operator=( const ThreadParameter& rhs ) = delete;
};

//=============================================================================
//================= function declarations =====================================
//=============================================================================
static void configureDevice( Device* pDev, int64_type skipRatio, int64_type thumbnailScaling, double framesPerSecond );
static bool isDeviceSupportedBySample( const Device* const pDev );
static void liveThread( ThreadParameter* pThreadParameter );
static void reportProblemAndExit( Device* pDev, const string& prologue, const string& epilogue = "" );
static void unlockAndRequestNext( FunctionInterface& fi, Request* pRequest );

//=============================================================================
//================= implementation ============================================
//=============================================================================
//-----------------------------------------------------------------------------
// Check if all features needed by this application are actually available
// and set up these features in a way that the 'smart frame recall' can be used.
// If a crucial feature is missing/not available this function will terminate
// with an error message.
void configureDevice( Device* pDev, int64_type skipRatio, int64_type thumbnailScaling, double framesPerSecond )
//-----------------------------------------------------------------------------
{
    //switch to Bayer pixel format if this is a color camera
    GenICam::DeviceControl dc( pDev );
    GenICam::ImageFormatControl ifc( pDev );
    if( ( dc.mvDeviceSensorColorMode.isValid() ) && ( dc.mvDeviceSensorColorMode.readS() == "BayerMosaic" ) )
    {
        if( ifc.pixelColorFilter.isValid() )
        {
            const string parity = ifc.pixelColorFilter.readS();
            if( parity == "BayerBG" )
            {
                ifc.pixelFormat.writeS( "BayerBG8" );
            }
            else if( parity == "BayerGB" )
            {
                ifc.pixelFormat.writeS( "BayerGB8" );
            }
            else if( parity == "BayerRG" )
            {
                ifc.pixelFormat.writeS( "BayerRG8" );
            }
            else if( parity == "BayerGR" )
            {
                ifc.pixelFormat.writeS( "BayerGR8" );
            }
            else
            {
                cout << "Undefined BayerMosaicParity! Terminating..." << endl;
                exit( 42 );
            }
        }
        else
        {
            cout << "Cannot determine BayerMosaicParity! Terminating..." << endl;
            exit( 42 );
        }
    }

    // Configure the device to use max. decimation in both directions in order to achieve minimum bandwidth
    // for the preview stream. A real world application could use this frame e.g. to do some blob analysis
    // on the reduced data saving both transmission bandwidth and processing time. Whenever anything of
    // interest is detected an application could then request the same image with full resolution or even
    // just a ROI of the image in full resolution.
    if( ifc.decimationHorizontal.isValid() )
    {
        ifc.decimationHorizontal.write( thumbnailScaling != 0 ? thumbnailScaling : ifc.decimationHorizontal.getMaxValue() );
        displayPropertyData( ifc.decimationHorizontal );
    }
    if( ifc.decimationVertical.isValid() )
    {
        ifc.decimationVertical.write( thumbnailScaling != 0 ? thumbnailScaling : ifc.decimationVertical.getMaxValue() );
        displayPropertyData( ifc.decimationVertical );
    }

    // Enable the chunk mode and switch on the 'mvCustomIdentifier' chunk that can be used to easily distinguish frames
    // belonging to the normal stream from the ones explicitly requested by the host application.
    GenICam::ChunkDataControl cdc( pDev );
    if( !cdc.chunkModeActive.isValid() )
    {
        reportProblemAndExit( pDev, "Chunk data is NOT supported" );
    }
    cdc.chunkModeActive.write( bTrue );

    if( !supportsEnumStringValue( cdc.chunkSelector, "mvCustomIdentifier" ) )
    {
        reportProblemAndExit( pDev, "'mvCustomIdentifier' chunk is NOT supported", " Can't distinguish requested frames from streamed ones..." );
    }

    // The image data itself of course is needed in any case!
    cdc.chunkSelector.writeS( "Image" );
    cdc.chunkEnable.write( bTrue );
    cdc.chunkSelector.writeS( "mvCustomIdentifier" );
    cdc.chunkEnable.write( bTrue );

    //Make sure the image width is a multiple of 8. This is needed in order to successfully enable
    //SmartFrameRecall
    const int modulo = ifc.width.read() % 8;
    if( modulo != 0 )
    {
        ifc.width.write( ifc.width.read() - modulo );
    }

    // Enable the smart frame recall feature itself. This will configure the devices internal memory
    // to store each frame that gets transmitted to the host in full resolution. These images (or ROIs of these images)
    // can be requested by an application when needed. However once the internal memory is full
    // the oldest frame will be removed from the memory whenever a new one becomes ready.
    GenICam::AcquisitionControl ac( pDev );
    if( !ac.mvSmartFrameRecallEnable.isValid() )
    {
        reportProblemAndExit( pDev, "The 'Smart Frame Recall' feature is NOT supported" );
    }

    if( framesPerSecond != -1 )
    {
        if( ac.mvAcquisitionFrameRateLimitMode.isValid() )
        {
            ac.mvAcquisitionFrameRateLimitMode.writeS( "mvDeviceMaxSensorThroughput" );
        }

        if( ac.exposureTime.isValid() )
        {
            if( 1000000 / ac.exposureTime.read() < framesPerSecond )
            {
                ac.exposureTime.write( 1000000 / ( framesPerSecond * 1.1 ) );
                cout << "ExposureTime changed to " << ac.exposureTime.read() << endl;
            }
        }
        if( ac.acquisitionFrameRate.isValid() )
        {
            ac.acquisitionFrameRateEnable.write( bTrue );
            ac.acquisitionFrameRate.write( framesPerSecond );
        }
    }

    // Since firmware version 2.30 you can specify a drop rate so the device
    // will throw away <skipRatio> frames between two images of reduced resolution.
    // That way you will further reduce the needed bandwidth.
    // When issuing requests for AOIs all images can be used - the ones you
    // got no thumbnail for have to be guessed (timestamp-wise).
    if( ac.mvSmartFrameRecallFrameSkipRatio.isValid() )
    {
        ac.mvSmartFrameRecallFrameSkipRatio.write( skipRatio );

        // in order to request the frames you cannot see, the Lookup Accuracy can
        // be adjusted - for our free-running-example this is done in an easy way
        // => if you skip 5 frames between two frames, you know the timestamps of
        //    we accept the timestamps even if they are 3*5=15 us away from the real one...
        // => please ensure that the achievable accuracy of your setup (trigger etc.)
        //    shows up in this parameter ...
        ac.mvSmartFrameRecallTimestampLookupAccuracy.write( ( skipRatio != 0 ) ? skipRatio * 3 : 1 );
    }
    else
    {
        cout << "===================================================" << endl;
        cout << "  mvSmartFrameRecallFrameSkipRatio not available   " << endl;
        cout << "===================================================" << endl;
    }

    ac.mvSmartFrameRecallEnable.write( bTrue );
    // after switching on the smart frame recall certain properties (e.g. everything related
    // to binning and decimation) will become read-only!
    cout << "Required PreviewFPS = " << ac.mvResultingFrameRate.read() << endl;
}

//-----------------------------------------------------------------------------
void displayCommandLineOptions( void )
//-----------------------------------------------------------------------------
{
    cout << "Available parameters:" << endl
         << "  'serial' or 's' to specify the serial number of the device to use" << endl
         << "  'skip' or 'sk' to specify a skip ratio for the stream of thumbnail images. A skip ratio of 3 will only transfer every third thumbnail image. However the skipped images can be requested using the smart frame recall mechanism as well." << endl
         << "  'width' or 'w' to specify the width of the ROI that will be requested for transmission" << endl
         << "  'height' or 'h' to specify the height of the ROI that will be requested for transmission" << endl
         << "  'requestRate' or 'rr' to specify the request rate(default: 3, will request every third image)" << endl
         << "  'thumbnailScaling' or 'tns' to specify the scaling factor of the thumbnail image ( 2,4,8,16 )" << endl
         << "  'framesPerSecond' or 'fps' to specify the required frame rate." << endl
         << "When either width or height is specified the parameter and the corresponding offset will no longer change randomly!"
         << endl
         << "USAGE EXAMPLE:" << endl
         << "  GenICamSmartFrameRecallUsage width=100 rr=2 skip=4" << endl << endl;
}

//-----------------------------------------------------------------------------
// This function will allow to select devices that support the GenICam interface
// layout(these are devices, that claim to be compliant with the GenICam standard)
// and that are bound to drivers that support the user controlled start and stop
// of the internal acquisition engine. Other devices will not be listed for
// selection as the code of the example relies on these features in the code.
bool isDeviceSupportedBySample( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    if( !pDev->interfaceLayout.isValid() &&
        !pDev->acquisitionStartStopBehaviour.isValid() )
    {
        return false;
    }

    vector<TDeviceInterfaceLayout> availableInterfaceLayouts;
    pDev->interfaceLayout.getTranslationDictValues( availableInterfaceLayouts );
    return find( availableInterfaceLayouts.begin(), availableInterfaceLayouts.end(), dilGenICam ) != availableInterfaceLayouts.end();
}

//-----------------------------------------------------------------------------
void liveThread( ThreadParameter* pThreadParameter )
//-----------------------------------------------------------------------------
{
    // establish access to the statistic properties
    Statistics statistics( pThreadParameter->pDev );
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
    // run thread loop
    unsigned int previewCnt = {0};
    unsigned int recallCnt = {0};
    const unsigned int timeout_ms = {500};
    int64_type skipRatio = {0};
    int64_type lastReceivedTimestamp = {-1};
    int64_type lastStatisticImageTimestamp = {-1};

    GenICam::AcquisitionControl ac( pThreadParameter->pDev );
    if( ac.mvSmartFrameRecallFrameSkipRatio.isValid() )
    {
        skipRatio = ac.mvSmartFrameRecallFrameSkipRatio.read();
    }

    srand( static_cast<unsigned int>( time( 0 ) ) );
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        const int requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            Request* pRequest = fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                // within this scope we have a valid buffer of data that can be an image or any other chunk of data.
                if( previewCnt == 0 )
                {
                    // make sure 'previewCnt' is never 0 not even when the variable overflows
                    previewCnt = 1;
                    lastStatisticImageTimestamp = pRequest->infoTimeStamp_us.read();
                }
                const int64_type chunkmvCustomIdentifier = pRequest->chunkmvCustomIdentifier.read();
                // Depending on the 'chunkmvCustomIdentifier' an application can easily distinguish between frames belonging to the
                // default 'reduced' stream or if these images have been explicitly requested by the application. A typical application
                // would
                // - perform a quick analysis of the reduced image to check if this image is needed in full resolution. If so it would request this
                //   image or a ROI of this image from the device
                // - perform the actual analysis the application has been developed for on the images that have been requested based on the previous
                //   reduced analysis
                // This can reduce the bandwidth consumed by an application dramatically!
                if( chunkmvCustomIdentifier == 0 )
                {
                    // we only want to count the preview images
                    ++previewCnt;
                    // here we can display some statistical information every 100th preview image
                    const unsigned int printStatisticEveryNthPreviewFrame = 100;
                    if( previewCnt % printStatisticEveryNthPreviewFrame == 0 )
                    {
                        cout << "Info from " << pThreadParameter->pDev->serial.read()
                             << " PreviewCnt: " << previewCnt
                             << " PreviewFPS: " << printStatisticEveryNthPreviewFrame * 1000000 / ( pRequest->infoTimeStamp_us.read() - lastStatisticImageTimestamp ) << " Hz"
                             << ", " << "Recalls: " << recallCnt
                             << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS() << endl;
                        recallCnt = 0;
                        lastStatisticImageTimestamp = pRequest->infoTimeStamp_us.read();
                    }
                }
#ifdef USE_DISPLAY
                ImageDisplay& display = ( chunkmvCustomIdentifier == 0 ) ? pThreadParameter->displayWindowStream.GetImageDisplay() : pThreadParameter->displayWindowExplicitRequestedByHost.GetImageDisplay();
                display.SetImage( pRequest );
                display.Update();
                Request** ppRequest = ( chunkmvCustomIdentifier == 0 ) ? &pThreadParameter->pLastRequestForStream : &pThreadParameter->pLastRequestExplicitlyRequested;
                swap( *ppRequest, pRequest );
#else
                cout << "Image captured(" << ( ( chunkmvCustomIdentifier == 0 ) ? string( "default stream" ) : string( "as requested by the application" ) ) << "): " << pRequest->imageOffsetX.read() << "x" << pRequest->imageOffsetY.read()
                     << "@" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << endl;
#endif  // #ifdef USE_DISPLAY

                // if 'chunkmvCustomIdentifier' is NOT 0 this was an image requested by the host application! Do not use this to trigger another transmission request!
                // if 'chunkmvCustomIdentifier' is 0 trigger transmission requests with the desired request rate
                if( ( previewCnt % pThreadParameter->requestRate == 0 ) && ( chunkmvCustomIdentifier == 0 ) )
                {
                    // here depending on the user supplied input parameters we either roll the dice or we use user supplied values for the ROI
                    // of the image transmission request.
                    int w = pThreadParameter->width;
                    int x = 0;
                    if( w == 0 )
                    {
                        // there is no user supplied 'width' so generate a random one
                        w = rand() % pRequest->imageWidth.read();
                        if( w == 0 )
                        {
                            w = 1;
                        }
                        x = rand() % ( pRequest->imageWidth.read() - w );
                    }
                    int h = pThreadParameter->height;
                    int y = 0;
                    if( h == 0 )
                    {
                        // there is no user supplied 'height' so generate a random one
                        h = rand() % pRequest->imageHeight.read();
                        if( h == 0 )
                        {
                            h = 1;
                        }
                        y = rand() % ( pRequest->imageHeight.read() - h );
                    }

                    // 'previewCnt' will be used as the custom identifier. To distinguish between normal stream an requested transmissions one could also simply set this value to '1'.
                    // When dealing with high frame rates one should consider to request the retransmission of frames from a separate thread in order not to block the acquisition
                    // loop while communicating with the device. Also when multiple AOIs shall be requested from a single image these requests should be sent to the device in a
                    // single command if possible by using one of the overloads of 'queueTransmissionRequest' instead of 'requestTransmission'. Please note that there are versions
                    // NOT using the pointer to the request object but the timestamp of the request. So once the timestamp has been extracted the request can be unlock even
                    // if a retransmission command is constructed afterwards. When using multiple threads be sure to lock all resources appropriately. The more complex C# example
                    // 'SmartFrameRecall' makes use of the recommendations made here thus can be used by those looking for additional information.

                    const int64_type actualTimestamp = pRequest->infoTimeStamp_us.read();
                    cout << "Requesting the full resolution transmission of the image with timestamp " << pRequest->infoTimeStamp_us.read() << " and a user defined identifier of '" << previewCnt
                         << "'. ROI: " << x << "x" << y << "@" << w << "x" << h << " did ";
                    int requestTransmissionResult = pThreadParameter->ccg.requestTransmission( pRequest, x, y, w, h, rtmFullResolution, previewCnt );
                    if( requestTransmissionResult == DMR_NO_ERROR )
                    {
                        cout << "succeed!" << endl;
                    }
                    else
                    {
                        cout << "fail(error code: " << requestTransmissionResult << "(" << ImpactAcquireException::getErrorCodeAsString( requestTransmissionResult ) << "))!" << endl;
                    }

                    recallCnt++;
                    // For further reduction of preview bandwidth you can use 'mvSmartFrameRecallFrameSkipRatio' to skip frames.
                    // All full resolution images are still stored in the camera's frame buffer. In this case you have to calculate the timestamp of the requested image.
                    if( ( skipRatio > 0 ) && ( lastReceivedTimestamp != -1 ) )
                    {
                        int64_type hiddenTimestamp = actualTimestamp;
                        const int64_type timespan = actualTimestamp - lastReceivedTimestamp;
                        const int64_type frame2frame = timespan / ( skipRatio + 1 );
                        for ( int i = 0; i < skipRatio; i++ )
                        {
                            hiddenTimestamp -= frame2frame;
                            w = rand() % pRequest->imageWidth.read();
                            w = ( w == 0 ) ? 1 : w;
                            x = rand() % ( pRequest->imageWidth.read() - w );
                            h = rand() % pRequest->imageHeight.read();
                            h = ( h == 0 ) ? 1 : h;
                            y = rand() % ( pRequest->imageHeight.read() - h );
                            cout << "Requesting the full resolution transmission of the image with timestamp " << hiddenTimestamp << " (hidden frame). ROI: " << x << "x" << y << "@" << w << "x" << h << " did ";

                            // here another overload of the requestTransmission function is used (giving the timestamp explicitly)
                            requestTransmissionResult = pThreadParameter->ccg.requestTransmission( hiddenTimestamp, x, y, w, h, rtmFullResolution, previewCnt );
                            if( requestTransmissionResult == DMR_NO_ERROR )
                            {
                                cout << "succeed!" << endl;
                            }
                            else
                            {
                                cout << "fail(error code: " << requestTransmissionResult << "(" << ImpactAcquireException::getErrorCodeAsString( requestTransmissionResult ) << "))!" << endl;
                            }
                        }
                    }
                    lastReceivedTimestamp = actualTimestamp;
                }
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            unlockAndRequestNext( fi, pRequest );
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

#ifdef USE_DISPLAY
    // stop the display from showing freed memory
    pThreadParameter->displayWindowExplicitRequestedByHost.GetImageDisplay().RemoveImage();
    pThreadParameter->displayWindowStream.GetImageDisplay().RemoveImage();
    // free the last potentially locked requests
    if( pThreadParameter->pLastRequestExplicitlyRequested != nullptr )
    {
        pThreadParameter->pLastRequestExplicitlyRequested->unlock();
    }
    if( pThreadParameter->pLastRequestForStream != nullptr )
    {
        pThreadParameter->pLastRequestForStream->unlock();
    }
#endif // #ifdef USE_DISPLAY

    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // clear all queues
    fi.imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
// When a crucial feature needed for this example application is not available
// this function will get called. It reports an error and then terminates the
// application.
void reportProblemAndExit( Device* pDev, const string& prologue, const string& epilogue /* = "" */ )
//-----------------------------------------------------------------------------
{
    cout << prologue << " by device " << pDev->serial.read() << "(" << pDev->product.read() << ", Firmware version: " << pDev->firmwareVersion.readS() << ")." << epilogue << endl
         << "Press [ENTER] to end the application..." << endl;
    cin.get();
    exit( 42 );
}

//-----------------------------------------------------------------------------
// Unlock the current request and send a new image request down to the driver
void unlockAndRequestNext( FunctionInterface& fi, Request* pRequest )
//-----------------------------------------------------------------------------
{
    if( pRequest != nullptr )
    {
        pRequest->unlock();
        // send a new image request into the capture queue
        fi.imageRequestSingle();
    }
}

//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
//-----------------------------------------------------------------------------
{
    int width = {0};
    int height = {0};
    int requestRate = {3};
    int64_type skipRatio = {0};
    int thumbnailScaling = {0};
    double framesPerSecond = {-1};
    string serial;
    bool boInvalidCommandLineParameterDetected = false;
    // scan command line
    if( argc > 1 )
    {
        for( int i = 1; i < argc; i++ )
        {
            const string param( argv[i] );
            const auto keyEnd = param.find_first_of( "=" );
            if( ( keyEnd == string::npos ) || ( keyEnd == param.length() - 1 ) )
            {
                cout << "Invalid command line parameter: '" << param << "' (ignored)." << endl;
                boInvalidCommandLineParameterDetected = true;
            }
            else
            {
                const string key = param.substr( 0, keyEnd );
                const string value = param.substr( keyEnd + 1 );
                if( ( key == "serial" ) || ( key == "s" ) )
                {
                    serial = value;
                }
                else if( ( key == "skip" ) || ( key == "sk" ) )
                {
                    skipRatio = static_cast<int>( atoi( value.c_str() ) );
                }
                else if( ( key == "requestRate" ) || ( key == "rr" ) )
                {
                    requestRate = static_cast<int>( atoi( value.c_str() ) );
                }
                else if( ( key == "width" ) || ( key == "w" ) )
                {
                    width = static_cast<int>( atoi( value.c_str() ) );
                }
                else if( ( key == "height" ) || ( key == "h" ) )
                {
                    height = static_cast<int>( atoi( value.c_str() ) );
                }
                else if( ( key == "thumbnailScaling" ) || ( key == "tns" ) )
                {
                    thumbnailScaling = static_cast<int>( atoi( value.c_str() ) );
                }
                else if( ( key == "FramesPerSecond" ) || ( key == "fps" ) )
                {
                    framesPerSecond = atof( value.c_str() );
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
    DeviceManager devMgr;
    Device* pDev = nullptr;
    if( !serial.empty() )
    {
        pDev = devMgr.getDeviceBySerial( serial );
        if( ( pDev != nullptr ) && pDev->interfaceLayout.isValid() )
        {
            // if this device offers the 'GenICam' interface switch it on, as this will
            // allow are better control over GenICam compliant devices
            conditionalSetProperty( pDev->interfaceLayout, dilGenICam, true );
        }
        cout << ", interface layout: " << pDev->interfaceLayout.readS();
    }
    if( pDev == nullptr )
    {
        pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    }
    if( pDev == nullptr )
    {
        cout << "Could not obtain a valid pointer to a device. Unable to continue!";
        cout << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    try
    {
        cout << "Initialising the device. This might take some time..." << endl << endl;
        pDev->interfaceLayout.write( dilGenICam ); // This is also done 'silently' by the 'getDeviceFromUserInput' function but your application needs to do this as well so state this here clearly!
        pDev->open();
        // load the default settings
        GenICam::UserSetControl usc( pDev );
        usc.userSetSelector.writeS( "Default" );
        usc.userSetLoad.call();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 1;
    }

    try
    {
        configureDevice( pDev, skipRatio, thumbnailScaling, framesPerSecond );
        // start the execution of the 'live' thread.
        cout << "Press [ENTER] to end the application" << endl;
        ThreadParameter threadParam( pDev, width, height, requestRate );
        thread myThread( liveThread, &threadParam );
        cin.get();
        s_boTerminated = true;
        myThread.join();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while setting up device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 1;
    }
    return 0;
}
