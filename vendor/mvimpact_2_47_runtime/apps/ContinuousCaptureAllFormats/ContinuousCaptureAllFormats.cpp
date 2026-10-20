#include <functional>
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_helper.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    unsigned int requestsCaptured_;
    Statistics statistics_;
    ImageDisplayWindow dispRaw_;
    ImageDisplayWindow dispConverted_;
    explicit ThreadParameter( Device* pDev, const string& windowTitle ) : pDev_( pDev ), requestsCaptured_( 0 ), statistics_( pDev ), dispRaw_( windowTitle ), dispConverted_( windowTitle + "(converted)" ) {}
    ThreadParameter( const ThreadParameter& src ) = delete;
    ThreadParameter& operator=( const ThreadParameter& rhs ) = delete;
};

//-----------------------------------------------------------------------------
void myThreadCallback( shared_ptr<Request> pRequest, ThreadParameter& threadParameter )
//-----------------------------------------------------------------------------
{
    ++threadParameter.requestsCaptured_;
    // display some statistical information every 100th image
    if( threadParameter.requestsCaptured_ % 100 == 0 )
    {
        const Statistics& s = threadParameter.statistics_;
        cout << "Info from " << threadParameter.pDev_->serial.read()
             << ": " << s.framesPerSecond.name() << ": " << s.framesPerSecond.readS()
             << ", " << s.errorCount.name() << ": " << s.errorCount.readS()
             << ", " << s.captureTime_s.name() << ": " << s.captureTime_s.readS() << endl;
    }

    if( pRequest->isOK() )
    {
        // raw (this might not always display a correct image but is fast and for certain applications the raw data will be the data to process
        // IMPORTANT: In this example this display does indirectly store a raw pointer to request data right in the next line! When using the 'RequestProvider' class this
        // is NOT allowed. Only because the other display does store the shared_ptr<Request> this can be done here!
        threadParameter.dispRaw_.GetImageDisplay().SetImage( pRequest->imageData.read(), pRequest->imageWidth.read(), pRequest->imageHeight.read(), pRequest->imagePixelPitch.read() * 8, pRequest->imageLinePitch.read() );
        threadParameter.dispRaw_.GetImageDisplay().Update();

        // converted. This might be slower as conversions might be needed but will display a correct image (that is not necessarily the data
        // transferred from the device) e.g. 12 bit mono data can't be displayed directly
        threadParameter.dispConverted_.GetImageDisplay().SetImage( pRequest );
        threadParameter.dispConverted_.GetImageDisplay().Update();
    }
    else
    {
        cout << "Error: " << pRequest->requestResult.readS() << endl;
    }
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr );
    if( pDev == nullptr )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    cout << "Initialising the device. This might take some time..." << endl;
    try
    {
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

    cout << "Please note that only the window displaying the converted image will display correct" << endl
         << "images all the time. The other window will simply blit the raw image data in the client" << endl
         << "area. This is faster but e.g. for 12 bit mono data or YUV data will result in unexpected" << endl
         << "results. For planar formats e.g. just the first plane might be visible." << endl
         << "For certain other formats the unconverted window might even blit no data at all." << endl
         << "This e.g. will be the case for 10-16 bit RGB packed data." << endl;

    ImageDestination id( pDev );
    cout << "The following formats are available:" << endl;
    DisplayPropertyDictionary<PropertyIImageDestinationPixelFormat>( id.pixelFormat );

    helper::RequestProvider requestProvider( pDev );
    vector<pair<string, TImageDestinationPixelFormat>> validDestinationPixelFormats;
    id.pixelFormat.getTranslationDict( validDestinationPixelFormats );
    for( const auto validDestinationPixelFormat : validDestinationPixelFormats )
    {
        id.pixelFormat.write( validDestinationPixelFormat.second );
        // initialise display windows
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        const string windowTitle( "Device " + pDev->serial.read() + "(pixel format " + validDestinationPixelFormat.first + ")" );
        cout << "Currently using " << validDestinationPixelFormat.first << ". Press [ENTER] to switch to the next format" << endl;
        ThreadParameter threadParam( pDev, windowTitle );
        requestProvider.acquisitionStart( myThreadCallback, std::ref( threadParam ) );
        cin.get();
        requestProvider.acquisitionStop();
    }
    cout << "Press [ENTER] to end the application" << endl;
    cin.get();
    return 0;
}
