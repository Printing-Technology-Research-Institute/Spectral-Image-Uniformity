#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <windows.h>
#include <process.h>
#include <algorithm>
#include <iostream>
#include <set>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

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
    ThreadParameter( Device* p, const string& windowTitle ) : pDev( p ), displayWindow( windowTitle ) {}
};

//-----------------------------------------------------------------------------
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );
    ImageDisplay& display = pThreadParameter->displayWindow.GetImageDisplay();
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
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 500;
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // cannot free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
    unsigned int cnt = 0;
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        requestNr = fi.imageRequestWaitFor( timeout_ms );
        if( fi.isRequestNrValid( requestNr ) )
        {
            pRequest = fi.getRequest( requestNr );
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    cout << "Info from " << pThreadParameter->pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
                display.SetImage( pRequest );
                display.Update();
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
void modifyConnectorFromUserInput( Connector& connector )
//-----------------------------------------------------------------------------
{
    bool boRun = true;
    while( boRun )
    {
        cout << endl << "Current connector settings:" << endl
             << "  Camera output: " << connector.cameraOutputUsed.readS() << endl
             << "  Grabber input: " << connector.videoChannel.read() << "(" << connector.pinDescription.read() << ")" << endl;
        cout << "Press" << endl
             << "  'o' to select a different camera output" << endl
             << "  'i' to select a different grabber input" << endl
             << "  any other key when done" << endl;
        string cmd;
        cin >> cmd;
        // remove the '\n' from the stream
        cin.get();
        if( cmd == "o" )
        {
            cout << "Available camera outputs as defined by the camera description:" << endl;
            vector<pair<string, TCameraOutput> > v;
            // query the properties translation dictionary, which will contain every camera description
            // recognized by this device
            connector.cameraOutputUsed.getTranslationDict( v );
            for_each( v.begin(), v.end(), DisplayDictEntry<int>() );
            cout << "Enter the new (numerical) value: ";
            int newOutput;
            cin >> newOutput;
            try
            {
                connector.cameraOutputUsed.write( static_cast<TCameraOutput>( newOutput ) );
            }
            catch( const ImpactAcquireException& e )
            {
                cout << e.getErrorString() << endl;
            }
        }
        else if( cmd == "i" )
        {
            // The allowed range depends on the currently selected camera output as e.g. for
            // a RGB camera signal 3 video input are required on the device side, while a
            // composite signal just requires 1.
            cout << "Allowed grabber video inputs in the current camera output mode: " <<
                 connector.videoChannel.read( plMinValue ) << " - " <<
                 connector.videoChannel.read( plMaxValue ) << endl;
            cout << "Enter the new (numerical) value: ";
            int newVideoInput;
            cin >> newVideoInput;
            try
            {
                connector.videoChannel.write( newVideoInput );
            }
            catch( const ImpactAcquireException& e )
            {
                cout << e.getErrorString() << endl;
            }
        }
        else
        {
            boRun = false;
            continue;
        }
    }
}

//-----------------------------------------------------------------------------
void selectCameraDescriptionFromUserInput( CameraSettingsFrameGrabber& cs )
//-----------------------------------------------------------------------------
{
    // display the name of every camera description available for this device.
    // this might be less than the number of camera descriptions available on the system as e.g.
    // an analog frame grabber can't use descriptions for digital cameras
    vector<pair<string, int> > vAvailableDescriptions;
    cs.type.getTranslationDict( vAvailableDescriptions );
    cout << endl << "Available camera descriptions: " << vAvailableDescriptions.size() << endl
         << "----------------------------------" << endl;
    for_each( vAvailableDescriptions.begin(), vAvailableDescriptions.end(), DisplayDictEntry<int>() );
    cout << endl << "Please select a camera description to use for this sample (numerical value): ";

    int cameraDescriptionToUse;
    cin >> cameraDescriptionToUse;
    // remove the '\n' from the stream
    cin.get();
    // wrong input will raise an exception
    // The camera description contains all the parameters needed to capture the associated video
    // signal. Camera descriptions for a certain camera can be obtained from MATRIX VISION. Experienced
    // users can also create their own camera descriptions using wxPropView. The documentation will
    // explain how to create and configure camera description files
    cs.type.write( cameraDescriptionToUse );
}

//-----------------------------------------------------------------------------
bool isDeviceSupportedBySample( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    return pDev->hasCapability( dcCameraDescriptionSupport );
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;

    cout << "This sample is meant for devices that support camera descriptions only. Other devices might be installed" << endl
         << "but won't be recognized by the application." << endl
         << endl;

    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( !pDev )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    cout << "Initialising device " << pDev->serial.read() << ". This might take some time..." << endl;
    try
    {
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device(error code: " << e.getErrorCode() << ")." << endl
             << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    // Assume that this is a frame grabber as currently only these devices support camera descriptions
    CameraSettingsFrameGrabber cs( pDev );
    if( !cs.type.isValid() )
    {
        cout << "Device " << pDev->serial.read() << " doesn't seem to support camera descriptions." << endl
             << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    Connector connector( pDev );
    CameraDescriptionManager cdm( pDev );
    bool boRun = true;
    while( boRun )
    {
        try
        {
            s_boTerminated = false;
            selectCameraDescriptionFromUserInput( cs );
            cdm.getStandardCameraDescriptionCount();
            modifyConnectorFromUserInput( connector );

            // start the execution of the 'live' thread.
            cout << endl << "Press [ENTER] to end the acquisition" << endl << endl;
            unsigned int dwThreadID;
            // initialise display window
            string windowTitle( "mvIMPACT_acquire sample, Device " + pDev->serial.read() );
            // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
            ThreadParameter threadParam( pDev, windowTitle );
            HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
            cin.get();
            s_boTerminated = true;
            WaitForSingleObject( hThread, INFINITE );
            CloseHandle( hThread );
            cout << endl << "Press 'q' followed by [ENTER] to end to application or any other key followed by [ENTER] to select a different camera description: ";
            string cmd;
            cin >> cmd;
            if( cmd == "q" )
            {
                boRun = false;
                continue;
            }
        }
        catch( const ImpactAcquireException& e )
        {
            cout << "Invalid selection (Error message: " << e.getErrorString() << ")" << endl
                 << "Please make a valid selection" << endl;
        }
    }
    return 0;
}
