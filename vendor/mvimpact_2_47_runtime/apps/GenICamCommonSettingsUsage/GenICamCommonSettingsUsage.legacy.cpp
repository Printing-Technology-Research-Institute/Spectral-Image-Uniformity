#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <iostream>
#include <ios>
#include <sstream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#if defined(linux) || defined(__linux) || defined(__linux__)
#   include <stdio.h>
#   include <unistd.h>
#else
#   include <windows.h>
#   include <process.h>
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)

using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::GenICam;
using namespace std;

static bool s_boTerminated = false;

//=============================================================================
//================= Data type definitions =====================================
//=============================================================================
//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev;
#if defined(linux) || defined(__linux) || defined(__linux__)
    explicit ThreadParameter( Device* p ) : pDev( p ) {}
#else
    ImageDisplayWindow  displayWindow;
    explicit ThreadParameter( Device* p, const std::string& windowTitle ) : pDev( p ), displayWindow( windowTitle ) {}
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
};

//-----------------------------------------------------------------------------
static bool canRestoreFactoryDefault( Device* pDev )
//-----------------------------------------------------------------------------
{
    GenICam::UserSetControl usc( pDev );
    if( !usc.userSetSelector.isValid() || !usc.userSetLoad.isValid() )
    {
        return false;
    }
    vector<string> validUserSetSelectorStrings;
    usc.userSetSelector.getTranslationDictStrings( validUserSetSelectorStrings );
    return find( validUserSetSelectorStrings.begin(), validUserSetSelectorStrings.end(), "Default" ) != validUserSetSelectorStrings.end();
}

//-----------------------------------------------------------------------------
unsigned int DMR_CALL liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast< ThreadParameter* >( pData );
    unsigned int cnt = 0;

    cout << "Initialising the device. This might take some time..." << endl;
    cout << endl;
    try
    {
        pThreadParameter->pDev->interfaceLayout.write( dilGenICam ); // This is also done 'silently' by the 'getDeviceFromUserInput' function but your application needs to do this as well so state this here clearly!
        pThreadParameter->pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the device " << pThreadParameter->pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 0;
    }
    // create an interface to the device found

    mvIMPACT::acquire::FunctionInterface fi( pThreadParameter->pDev );

    // To make sure the device will be configured based on a defined state, the default user set will be loaded
    mvIMPACT::acquire::GenICam::UserSetControl usc( pThreadParameter->pDev );

    cout << "The device will be configured now!\n" << endl;

    // Make sure the default user set and the load method exist
    // In general it is a good idea to verify if the GenICam class which should be used exists and is of the expected type
    if( canRestoreFactoryDefault( pThreadParameter->pDev ) )
    {
        if( usc.userSetSelector.isValid() && usc.userSetSelector.isWriteable() && usc.userSetLoad.isValid() && usc.userSetLoad.isMeth() )
        {
            cout << "Loading the device's default user set to avoid undefined settings!\n" << endl;
            // selecting the default user set which includes the factory settings of the device
            usc.userSetSelector.writeS( "Default" );
            // call the userSetLoad method to load the currently selected user set
            usc.userSetLoad.call();
        }
    }
    else
    {
        cout << "The device seems not to support the default user set!" << endl;
    }

    const double dExposureTime = 10000.;
    // initialise the AcquisitionControl class to get access to the exposure time property
    mvIMPACT::acquire::GenICam::AcquisitionControl acq( pThreadParameter->pDev );
    // Make sure the exposure time property does exist and is currently not "read only"
    if( acq.exposureTime.isValid() && acq.exposureTime.isWriteable() )
    {
        cout << "Currently the exposure time is set to " << acq.exposureTime.read() << " us. Changing to " << dExposureTime << " us" << endl;
        cout << endl;
        acq.exposureTime.write( dExposureTime );
    }

    // initialising the ImageFormatControl class to get access to the sensor's AOI settings
    mvIMPACT::acquire::GenICam::ImageFormatControl ifc( pThreadParameter->pDev );

    // check if the sensors properties are available and modifying the AOI settings if width and height are valid properties
    if( ifc.width.isValid() && ifc.height.isValid() )
    {
        cout << "The sensor has a max resolution of about " << ifc.width.getMaxValue() << "x" << ifc.height.getMaxValue() << " pixels" << endl;
        cout << "The resolution will now be adjusted to the half of width and height. The resulting AOI will be: " << ifc.width.getMaxValue() / 2 << "x" << ifc.height.getMaxValue() / 2 << " pixels" << endl;
        // the AOI settings are usually not writable once the senor is exposing images so make sure width and height are not read-only at the moment
        if( !ifc.width.isWriteable() || !ifc.height.isWriteable() )
        {
            cout << "Width or Height are not writable at the moment." << endl;
        }
        else
        {
            ifc.width.write( ifc.width.getMaxValue() / 2 );
            ifc.height.write( ifc.height.getMaxValue() / 2 );
        }
    }

    mvIMPACT::acquire::GenICam::CounterAndTimerControl ctc( pThreadParameter->pDev );
    // Since the device's settings have a huge impact on the frame rate of the sensor, we need the device almost configured at this step. Otherwise the max frame rate would not be correct.
    cout << endl;
    cout << "To avoid some cabling work, we will use an internal timer for triggering in this sample!" << endl;
    cout << "The trigger frequency will be configured to half of the max frequency the sensor would be capable of in your setup." << endl;

    // Figuring out how many timers are available
    vector<string> availableTimers;
    if( ctc.timerSelector.isValid() )
    {
        ctc.timerSelector.getTranslationDictStrings( availableTimers );
    }
    if( ctc.timerSelector.isValid() && ctc.timerSelector.isWriteable() && ( availableTimers.size() >= 2 ) && acq.triggerSelector.isValid() && acq.mvResultingFrameRate.isValid() )
    {
        // Making sure that Timer2End as TimerTriggerSource does exist
        ctc.timerSelector.writeS( "Timer1" );
        vector<string> availableTriggerSources;
        ctc.timerTriggerSource.getTranslationDictStrings( availableTriggerSources );
        if( find( availableTriggerSources.begin(), availableTriggerSources.end(), "Timer2End" ) != availableTriggerSources.end() && acq.mvResultingFrameRate.isValid() )
        {
            const double dPeriod = 1000000. / ( acq.mvResultingFrameRate.read() / 2. );
            if( dPeriod >= 300. )
            {
                // Defining the duration the trigger signal is "low". The timer selector has not to be changed since it has been set to Timer1 already
                ctc.timerDuration.write( 1000. );
                ctc.timerTriggerSource.writeS( "Timer2End" );

                // Defining the duration the trigger signal is "high"
                ctc.timerTriggerSource.writeS( "Timer1End" );
                ctc.timerSelector.writeS( "Timer2" );
                ctc.timerDuration.write( dPeriod - 1000. );

                // Configuring the FrameStart trigger to use the start signal of Timer1 and enabling the trigger mode
                acq.triggerSelector.writeS( "FrameStart" );
                acq.triggerSource.writeS( "Timer1Start" );
                acq.triggerMode.writeS( "On" );
            }
        }
        else
        {
            cout << "This device does not support expected timer trigger sources! The device will work in free run mode instead!" << endl;
        }
    }
    else
    {
        cout << "This device does not support timers! The device will work in free run mode instead!" << endl;
    }

    mvIMPACT::acquire::GenICam::AnalogControl anc( pThreadParameter->pDev );
    // Applying some gain to the signal provided by the device's sensor
    if( anc.gain.isValid() && anc.gain.isWriteable() )
    {
        anc.gain.write( anc.gain.getMaxValue() );
    }

    mvIMPACT::acquire::GenICam::DigitalIOControl dio( pThreadParameter->pDev );
    // Figuring out how many digital IOs are available
    cout << "\nAvailable Digital IOs:" << endl;
    vector<string> availableIOs;
    dio.lineSelector.getTranslationDictStrings( availableIOs );
    bool boConfiguredFirstOutput = false;

    // iterating over the vector of digital IOs to read out the lineMode, lineStatus and lineSource properties
    const vector<string>::size_type IOCnt = availableIOs.size();
    for( vector<string>::size_type i = 0; i < IOCnt; i++ )
    {
        ostringstream oss;
        oss << "Line" << i;
        dio.lineSelector.writeS( oss.str() );

        // using mvExposureAndAcquisitionActive as the lineSource for the first digital output found
        if( !boConfiguredFirstOutput && dio.lineMode.readS() == "Output" )
        {
            dio.lineSource.writeS( "mvExposureAndAcquisitionActive" );
            boConfiguredFirstOutput = true;
        }
        cout << oss.str() << " - " << ( dio.lineMode.isValid() ? dio.lineMode.readS() : string( "UNSUPPORTED" ) )
             << " - LineStatus: " << ( dio.lineStatus.isValid() ? dio.lineStatus.readS() : string( "UNSUPPORTED" ) )
             << " - LineSource: " << ( dio.lineSource.isValid() ? dio.lineSource.readS() : string( "UNSUPPORTED" ) )
             << endl;
    }

    // establish access to the statistic properties
    Statistics statistics( pThreadParameter->pDev );
    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast< TDMR_ERROR >( fi.imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }
    cout << "\nStarting image acquisition\n" << endl;
    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, fi );
    // run thread loop
    mvIMPACT::acquire::Request* pRequest = 0;
    // we always have to keep at least 2 images as the displayWindow module might want to repaint the image, thus we
    // can free it unless we have a assigned the displayWindow to a new buffer.
    mvIMPACT::acquire::Request* pPreviousRequest = 0;
    const unsigned int timeout_ms = 5000;
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        int requestNr = fi.imageRequestWaitFor( timeout_ms );
        pRequest = fi.isRequestNrValid( requestNr ) ? fi.getRequest( requestNr ) : 0;
        if( pRequest )
        {
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    cout << "Info from " << pThreadParameter->pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS()
                         << ", LineStatusAll: " << dio.lineStatusAll.readS() << endl;

                    // to show the difference in the images the gain is adjusted every 100th image
                    anc.gain.write( ( anc.gain.read() == anc.gain.getMaxValue() ) ? anc.gain.getMinValue() : anc.gain.getMaxValue() );
                }
#if defined(linux) || defined(__linux) || defined(__linux__)
                cout << "Image captured(" << pRequest->imageWidth.read() << "x" << pRequest->imageHeight.read() << ")" << endl;
#else
                pThreadParameter->displayWindow.GetImageDisplay().SetImage( pRequest );
                pThreadParameter->displayWindow.GetImageDisplay().Update();
#endif  // #if defined(linux) || defined(__linux) || defined(__linux__)
            }
            else
            {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            if( pPreviousRequest )
            {
                // this image has been displayed thus the buffer is no longer needed...
                pPreviousRequest->unlock();
            }
            pPreviousRequest = pRequest;
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
#if defined(linux) || defined(__linux) || defined(__linux__)
        s_boTerminated = waitForInput( 0, STDOUT_FILENO ) == 0 ? false : true; // break by STDIN
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->pDev, fi );
#if !defined(linux) && !defined(__linux) && !defined(__linux__)
    // stop the display from showing freed memory
    pThreadParameter->displayWindow.GetImageDisplay().RemoveImage();
#endif // #if !defined(linux) && !defined(__linux) && !defined(__linux__)

    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( pRequest )
    {
        pRequest->unlock();
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
    return 0;
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
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    cout << "--------------------------------------------!!! ATTENTION !!!--------------------------------------------" << endl;
    cout << "Please be aware that the digital outputs of the device might be enabled during the test." << endl
         << "This might lead to unexpected behavior in case of devices which are connected to one of the digital outputs," << endl
         << "so only proceed if you are sure that this will not cause any issue with connected hardware!!" << endl;
    cout << "---------------------------------------------------------------------------------------------------------" << endl;
    cout << "" << endl;

    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( !pDev )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    // start the execution of the 'live' thread.
    cout << "Press [ENTER] to end the application" << endl;

#if defined(linux) || defined(__linux) || defined(__linux__)
    ThreadParameter threadParam( pDev );
    liveThread( &threadParam );
#else
    unsigned int dwThreadID;
    string windowTitle( "mvIMPACT_acquire sample, Device " + pDev->serial.read() );
    // initialise displayWindow window
    // IMPORTANT: It's NOT safe to create multiple displayWindow windows in multiple threads!!!
    ThreadParameter threadParam( pDev, windowTitle );
    HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
    cin.get();
    s_boTerminated = true;
    WaitForSingleObject( hThread, INFINITE );
    CloseHandle( hThread );
#endif  // #if defined(linux) || defined(__linux) || defined(__linux__)
    return 0;
}
