#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <apps/Common/exampleHelper.h>
#include <common/crt/mvstdio.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_helper.h>
#ifdef _WIN32
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

//=============================================================================
//================= Data type definitions =====================================
//=============================================================================
//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    unsigned int requestsCaptured_;
    Statistics statistics_;
#ifdef USE_DISPLAY
    ImageDisplayWindow displayWindow_;
#endif // #ifdef USE_DISPLAY
    explicit ThreadParameter( Device* pDev ) : pDev_( pDev ), requestsCaptured_( 0 ), statistics_( pDev )
#ifdef USE_DISPLAY
        // initialise display window
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        , displayWindow_( "mvIMPACT_acquire sample, Device " + pDev_->serial.read() )
#endif // #ifdef USE_DISPLAY
    {}
    ThreadParameter( const ThreadParameter& src ) = delete;
    ThreadParameter& operator=( const ThreadParameter& rhs ) = delete;
};

//=============================================================================
//================= function declarations =====================================
//=============================================================================
static void configureDevice( Device* pDev );
static void configureBlueCOUGAR_X( Device* pDev );
static void configureVirtualDevice( Device* pDev );
static bool isDeviceSupportedBySample( const Device* const pDev );
static void reportProblemAndExit( Device* pDev, const string& prologue, const string& epilogue = "" );

//=============================================================================
//================= implementation ============================================
//=============================================================================
//-----------------------------------------------------------------------------
inline bool isBlueCOUGAR_X( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    return ( pDev->product.read().find( "mvBlueCOUGAR-X" ) == 0 ) ||
           ( pDev->product.read().find( "mvBlueCOUGAR-2X" ) == 0 ) ||
           ( pDev->product.read().find( "mvBlueCOUGAR-3X" ) == 0 );
}

//-----------------------------------------------------------------------------
// Check if all features needed by this application are actually available
// and set up these features in a way that the multi-part streaming is active.
// If a crucial feature is missing/not available this function will terminate
// with an error message.
void configureDevice( Device* pDev )
//-----------------------------------------------------------------------------
{
    if( pDev->product.read() == "VirtualDevice" )
    {
        configureVirtualDevice( pDev );
    }
    else if( isBlueCOUGAR_X( pDev ) )
    {
        configureBlueCOUGAR_X( pDev );
    }
    else
    {
        reportProblemAndExit( pDev, "The selected device is not supported by this application!" );
    }
}

//-----------------------------------------------------------------------------
void configureVirtualDevice( Device* pDev )
//-----------------------------------------------------------------------------
{
    CameraSettingsVirtualDevice cs( pDev );
    if( !cs.bufferPartCount.isValid() )
    {
        reportProblemAndExit( pDev, "This version of the mvVirtualDevice driver does not support the multi-part format. An update will fix this problem!" );
    }

    cs.bufferPartCount.write( cs.bufferPartCount.getMaxValue() );

    // because every jpg image is written to disk we limit the frames per sec to 5 fps
    cs.frameDelay_us.write( 200000 );
}

//-----------------------------------------------------------------------------
void configureBlueCOUGAR_X( Device* pDev )
//-----------------------------------------------------------------------------
{
    // make sure the device is running with default parameters
    GenICam::UserSetControl usc( pDev );
    usc.userSetSelector.writeS( "Default" );
    usc.userSetLoad.call();

    // now set up the device
    GenICam::TransportLayerControl tlc( pDev );
    if( !tlc.gevGVSPExtendedIDMode.isValid() )
    {
        reportProblemAndExit( pDev, "This device does not support the multi-part format. A firmware update will fix this problem!" );
    }

    //enable extended ID mode - this is mandatory in order to stream JPEG and/or multi part data
    tlc.gevGVSPExtendedIDMode.writeS( "On" );

    //enable JPEGWithRaw mode
    GenICam::AcquisitionControl ac( pDev );
    ac.mvFeatureMode.writeS( "mvJPEGWithRaw" );
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
    return ( pDev->product.read() == "VirtualDevice" ) || isBlueCOUGAR_X( pDev );
}

//-----------------------------------------------------------------------------
void myThreadCallback( shared_ptr<Request> pRequest, ThreadParameter& threadParameter )
//-----------------------------------------------------------------------------
{
    if( pRequest->isOK() )
    {
        const unsigned int bufferPartCount = pRequest->getBufferPartCount();
        ++threadParameter.requestsCaptured_;
        // display some statistical information every 100th image
        if( threadParameter.requestsCaptured_ % 100 == 0 )
        {
            const Statistics& s = threadParameter.statistics_;
            cout << "Info from " << threadParameter.pDev_->serial.read();
            if( bufferPartCount == 0 )
            {
                cout << " NOT running in multi-part mode";
            }
            else
            {
                cout << " running in multi-part mode, delivering " << bufferPartCount << " parts";
            }
            cout << ": " << s.framesPerSecond.name() << ": " << s.framesPerSecond.readS()
                 << ", " << s.errorCount.name() << ": " << s.errorCount.readS()
                 << ", " << s.captureTime_s.name() << ": " << s.captureTime_s.readS() << endl;
        }
        if( bufferPartCount == 0 )
        {
            // the device is not running in multi-part mode
#ifdef USE_DISPLAY
            ImageDisplay& display = threadParameter.displayWindow_.GetImageDisplay();
            display.SetImage( pRequest );
            display.Update();
#else
            cout << "Image captured: " << pRequest->imageOffsetX.read() << "x" << pRequest->imageOffsetY.read() << "@" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << endl;
#endif  // #ifdef USE_DISPLAY
        }
        else
        {
#ifdef USE_DISPLAY
            // we have just one display so only show the first buffer part that can be displayed there, output text for all others
            bool boImageDisplayed = false;
            ImageDisplay& display = threadParameter.displayWindow_.GetImageDisplay();
#endif // #ifdef USE_DISPLAY
            for( unsigned int i = 0; i < bufferPartCount; i++ )
            {
                const BufferPart& bufferPart = pRequest->getBufferPart( i );
                switch( bufferPart.dataType.read() )
                {
                case bpdt2DImage:
                case bpdt2DPlaneBiplanar:
                case bpdt2DPlaneTriplanar:
                case bpdt2DPlaneQuadplanar:
                case bpdt3DImage:
                case bpdt3DPlaneBiplanar:
                case bpdt3DPlaneTriplanar:
                case bpdt3DPlaneQuadplanar:
                case bpdtConfidenceMap:
                    {
#ifdef USE_DISPLAY
                        if( !boImageDisplayed )
                        {
                            display.SetImage( bufferPart.getImageBufferDesc().getBuffer() );
                            display.Update();
                            boImageDisplayed = true;
                        }
                        else
#endif // #ifdef USE_DISPLAY
                        {
                            cout << "Buffer Part handled: " << bufferPart.dataType.readS() << "(index " << i << "), dimensions: " << bufferPart.offsetX.read() << "x" << bufferPart.offsetY.read() << "@" << bufferPart.width.read() << "x" << bufferPart.height.read() << endl;
                        }
                    }
                    break;
                case bpdtJPEG:
                case bpdtJPEG2000:
                    {
                        // store JPEG data into a file. Please note that at higher frame/data rates this might result in lost images during the capture
                        // process as storing the images to disc might be very slow. Real world applications should therefore move these operations
                        // into a thread. Probably it makes sense to copy the JPEG image into another RAM location as well in order to free this capture
                        // buffer as fast as possible.
                        ostringstream oss;
                        oss << "Buffer.part" << i
                            << "id" << std::setfill( '0' ) << std::setw( 16 ) << pRequest->infoFrameID.read() << "."
                            << "ts" << std::setfill( '0' ) << std::setw( 16 ) << pRequest->infoTimeStamp_us.read() << "."
                            << "jpeg";
                        FILE* pFile = mv_fopen_s( oss.str().c_str(), "wb" );
                        if( pFile )
                        {
                            if( fwrite( bufferPart.address.read(), static_cast<size_t>( bufferPart.dataSize.read() ), 1, pFile ) != 1 )
                            {
                                reportProblemAndExit( threadParameter.pDev_, "Could not write file '" +  oss.str() + "'" );
                            }
                            cout << "Buffer Part handled: " << bufferPart.dataType.readS() << "(index " << i << "): Stored to disc as '" << oss.str() << "'." << endl;
                            fclose( pFile );
                        }
                    }
                    break;
                case bpdtUnknown:
                    cout << "Buffer Part" << "(index " << i << ") has an unknown data type. Skipped!" << endl;
                    break;
                }
            }
        }
    }
    else
    {
        cout << "Error: " << pRequest->requestResult.readS() << endl;
    }
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
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( pDev == nullptr )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    try
    {
        cout << "Initialising the device. This might take some time..." << endl << endl;
        pDev->open();
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
        configureDevice( pDev );
        // start the execution of the 'live' thread.
        cout << "Press [ENTER] to end the application" << endl;
        ThreadParameter threadParam( pDev );
        helper::RequestProvider requestProvider( pDev );
        requestProvider.acquisitionStart( myThreadCallback, std::ref( threadParam ) );
        cin.get();
        requestProvider.acquisitionStop();
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
