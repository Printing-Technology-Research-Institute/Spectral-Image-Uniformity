#include <apps/Common/exampleHelper.h>
#include <functional>
#include <iostream>
#include <mvIMPACT_CPP/mvIMPACT_acquire_helper.h>
#ifdef _WIN32
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#endif // #ifdef _WIN32

using namespace mvIMPACT::acquire;
using namespace std;

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
//================= implementation ============================================
//=============================================================================
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
#ifdef USE_DISPLAY
        threadParameter.displayWindow_.GetImageDisplay().SetImage( pRequest );
        threadParameter.displayWindow_.GetImageDisplay().Update();
#else
        cout << "Image captured: " << pRequest->imageOffsetX.read() << "x" << pRequest->imageOffsetY.read() << "@" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << endl;
#endif // #ifdef USE_DISPLAY
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
        cout << "An error occurred while opening the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ").";
        return 1;
    }

    cout << "Press [ENTER] to stop the acquisition thread" << endl;
    ThreadParameter threadParam( pDev );
    helper::RequestProvider requestProvider( pDev );
    requestProvider.acquisitionStart( myThreadCallback, std::ref( threadParam ) );
    cin.get();
    requestProvider.acquisitionStop();
    return 0;
}
