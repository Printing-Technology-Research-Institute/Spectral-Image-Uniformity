#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#ifdef _WIN32
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

using StringPropMap = map<string, Property>;

//-----------------------------------------------------------------------------
struct CaptureParameter
//-----------------------------------------------------------------------------
{
    Device* pDev;
#ifdef USE_DISPLAY
    shared_ptr<ImageDisplayWindow> pDisplayWindow;
#endif // #ifdef USE_DISPLAY
    FunctionInterface fi;
    explicit CaptureParameter( Device* p ) : pDev( p ), fi( p )
    {
#ifdef USE_DISPLAY
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        pDisplayWindow = make_shared<ImageDisplayWindow>( "mvIMPACT_acquire sample, Device " + pDev->serial.read() );
#endif // #ifdef USE_DISPLAY
    }
    CaptureParameter( const CaptureParameter& src ) = delete;
    CaptureParameter& operator=( const CaptureParameter& rhs ) = delete;
};

//-----------------------------------------------------------------------------
void populatePropertyMap( StringPropMap& m, Component it, const string& currentPath = "" )
//-----------------------------------------------------------------------------
{
    while( it.isValid() )
    {
        string fullName( currentPath );
        if( fullName != "" )
        {
            fullName += "/";
        }
        fullName += it.name();
        if( it.isList() )
        {
            populatePropertyMap( m, it.firstChild(), fullName );
        }
        else if( it.isProp() )
        {
            m.insert( make_pair( fullName, Property( it ) ) );
        }
        ++it;
        // method object will be ignored...
    }
}

//-----------------------------------------------------------------------------
void singleCapture( CaptureParameter& params, int maxWaitTime_ms )
//-----------------------------------------------------------------------------
{
    // send a request to the default request queue of the device and
    // wait for the result.
    params.fi.imageRequestSingle();
    manuallyStartAcquisitionIfNeeded( params.pDev, params.fi );
    // wait for the image send to the default capture queue
    int requestNr = params.fi.imageRequestWaitFor( maxWaitTime_ms );

    // check if the image has been captured without any problems
    if( params.fi.isRequestNrValid( requestNr ) )
    {
        const Request* pRequest = params.fi.getRequest( requestNr );
        if( pRequest->isOK() )
        {
            // everything went well. Display the result...
#ifdef USE_DISPLAY
            params.pDisplayWindow->GetImageDisplay().SetImage( pRequest );
            params.pDisplayWindow->GetImageDisplay().Update();
#else
            cout << "Image captured(" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << ")" << endl;
#endif  // #ifdef USE_DISPLAY
        }
        else
        {
            cout << "A request has been returned, but the acquisition was not successful. Reason: " << pRequest->requestResult.readS() << endl;
        }
        // ... unlock the request again, so that the driver can use it again
        params.fi.imageRequestUnlock( requestNr );
    }
    else
    {
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
        cout << "The acquisition failed: " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << endl;
    }
    manuallyStopAcquisitionIfNeeded( params.pDev, params.fi );
    // in any case clear the queue to have consistent behaviour the next time this function gets called
    // as otherwise an image not ready yet might be returned directly when this function gets called the next time
    params.fi.imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
bool isDeviceSupportedBySample( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    if( !pDev->interfaceLayout.isValid() )
    {
        return false;
    }

    vector<TDeviceInterfaceLayout> availableInterfaceLayouts;
    pDev->interfaceLayout.getTranslationDictValues( availableInterfaceLayouts );
    return find( availableInterfaceLayouts.begin(), availableInterfaceLayouts.end(), dilGenICam ) != availableInterfaceLayouts.end();
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    cout << "This sample is meant for devices that support the GenICam interface layout only. Other devices might be installed" << endl
         << "but won't be recognized by the application." << endl
         << endl;

    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
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
        cout << "An error occurred while opening the device(error code: " << e.getErrorCode() << ")." << endl
             << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    CaptureParameter captureParameter( pDev );

    // obtain all the settings related properties available for this device
    // Only work with the 'Base' setting. For more information please refer to the manual (working with settings)
    StringPropMap propertyMap;
    DeviceComponentLocator locator( pDev, dltSetting, "Base" );
    populatePropertyMap( propertyMap, Component( locator.searchbase_id() ).firstChild() );
    locator = DeviceComponentLocator( pDev, dltRequest );
    populatePropertyMap( propertyMap, Component( locator.searchbase_id() ).firstChild() );
    locator = DeviceComponentLocator( pDev, dltSystemSettings );
    populatePropertyMap( propertyMap, Component( locator.searchbase_id() ).firstChild(), string( "SystemSettings" ) );
    locator = DeviceComponentLocator( pDev, dltInfo );
    populatePropertyMap( propertyMap, Component( locator.searchbase_id() ).firstChild(), string( "Info" ) );
    populatePropertyMap( propertyMap, Component( pDev->hDev() ).firstChild(), string( "Device" ) );
    string cmd;
    int timeout_ms = 500;
    bool boRun = true;
    while( boRun )
    {
        cout << "enter quit, snap, list, help or the name of the property you want to modify followed by [ENTER]: ";
        cin >> cmd;

        if( cmd == "snap" )
        {
            singleCapture( captureParameter, timeout_ms );
        }
        else if( cmd == "list" )
        {
            for_each( propertyMap.begin(), propertyMap.end(), DisplayProperty() );
        }
        else if( cmd == "help" )
        {
            cout << "quit: terminates the sample" << endl
                 << "snap: takes and displays one image with the current settings" << endl
                 << "list: displays all properties available for this device" << endl
                 << "help: displays this help text" << endl
                 << "timeout: set a new timeout(in ms) used as a max. timeout to wait for an image" << endl
                 << "the full name of a property must be specified" << endl;
        }
        else if( cmd == "quit" )
        {
            boRun = false;
            continue;
        }
        else if( cmd == "timeout" )
        {
            cout << "Enter the new timeout to be passed to the imageRequestWaitFor function: ";
            cin >> timeout_ms;
        }
        else
        {
            const StringPropMap::const_iterator it = propertyMap.find( cmd );
            if( it == propertyMap.end() )
            {
                cout << "unknown command or property" << endl;
            }
            else
            {
                displayPropertyData( it->second );
                if( it->second.hasDict() )
                {
                    cout << "This function expects the string representation as input!" << endl;
                }
                modifyPropertyValue( it->second );
            }
        }
    }

    return 0;
}
