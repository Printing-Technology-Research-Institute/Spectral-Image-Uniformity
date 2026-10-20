#ifdef _MSC_VER         // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#ifdef _WIN32
#   include <windows.h>
#   include <process.h>
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#endif // #ifdef WIN32

using namespace mvIMPACT::acquire;
using namespace std;

//=============================================================================
//================= static variables ==========================================
//=============================================================================
static bool s_boTerminated = false;
static bool s_boSwitchedToSet1 = false;

//=============================================================================
//================= function declarations =====================================
//=============================================================================
static void checkedMethodCall( Device* pDev, Method& method );

//=============================================================================
//================= sequencer specific stuff ==================================
//=============================================================================
//-----------------------------------------------------------------------------
struct SequencerSetParameter
//-----------------------------------------------------------------------------
{
    const int64_type setNr_;
    const double exposureTime_us_;
    const int64_type horizontalBinningOrDecimation_;
    const int64_type verticalBinningOrDecimation_;
    double expectedFrameRate_;
    explicit SequencerSetParameter( const int64_type setNr, const double exposureTime_us, const int64_type horizontalBinningOrDecimation, const int64_type verticalBinningOrDecimation ) :
        setNr_( setNr ), exposureTime_us_( exposureTime_us ), horizontalBinningOrDecimation_( horizontalBinningOrDecimation ), verticalBinningOrDecimation_( verticalBinningOrDecimation ), expectedFrameRate_( 0.0 )
    {
    }
};

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev;
    FunctionInterface fi;
    Statistics statistics;
    GenICam::AcquisitionControl ac;
    GenICam::ImageFormatControl ifc;
    GenICam::SequencerControl sc;
    GenICam::DigitalIOControl dic;
#if defined(linux) || defined(__linux) || defined(__linux__)
    explicit ThreadParameter( Device* p ) : pDev( p ), fi( pDev ), statistics( pDev ), ac( pDev ), ifc( pDev ), sc( pDev ), dic( pDev )
    {
    }
#else
    ImageDisplayWindow  displayWindow;
    explicit ThreadParameter( Device* p, const std::string& windowTitle ) : pDev( p ), fi( pDev ), statistics( pDev ), ac( pDev ), ifc( pDev ), sc( pDev ), dic( pDev ), displayWindow( windowTitle )
    {
    }
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
};

//-----------------------------------------------------------------------------
static SequencerSetParameter s_SequencerData[] =
//-----------------------------------------------------------------------------
{
    SequencerSetParameter( 0, 1000., 2, 2 ),   // Set 0: Capture Exposure =   1000 us HBinning = 2 VBinning = 2, then jump to set 0, except if UserOutput0 then jump to set 1
    SequencerSetParameter( 1, 200000., 1, 1 ), // Set 1: Capture Exposure = 200000 us HBinning = 1 VBinning = 1, then jump back to set 0 */
};

//-----------------------------------------------------------------------------
// Check if a key has been pressed and act accordingly, by either switching between
// Sequencer sets or terminating the application.
void checkInput( ThreadParameter* pThreadParameter, const int& in )
//-----------------------------------------------------------------------------
{
    switch( in )
    {
    case '1':
        if( !s_boSwitchedToSet1 )
        {
            cout << "Setting UserOutput0 leads to switching to SequencerSet 1..." << endl;
            pThreadParameter->dic.userOutputSelector.write( 0 );
            pThreadParameter->dic.userOutputValue.write( bTrue );
            s_boSwitchedToSet1 = true;
        }
        cin.get(); // remove the '\n' from the stream
        break;
    case '0':
        if( s_boSwitchedToSet1 )
        {
            cout << "Unsetting UserOutput0 leads to switching back to SequencerSet 0..." << endl;
            pThreadParameter->dic.userOutputSelector.write( 0 );
            pThreadParameter->dic.userOutputValue.write( bFalse );
            s_boSwitchedToSet1 = false;
        }
        cin.get(); // remove the '\n' from the stream
        break;
    default:
        s_boTerminated = true;
        break;
    }
}

//-----------------------------------------------------------------------------
// Configures all the stuff that needs to be done only once. All the stuff related
// to setting up the actual sequencer could be called multiple times whenever an
// application gets re-configured. This is not the case here, but the code has been
// split in order to logically group what belongs together.
//
// Whenever 'conditionalSetEnumPropertyByString' or 'conditionalSetProperty' is
// not used here the stuff MUST succeed as otherwise when the device doesn't allow
// this feature the whole example does not work!
void configureDevice( Device* pDev )
//-----------------------------------------------------------------------------
{
    try
    {
        // Restore the factory default first in order to make sure nothing is incorrectly configured
        GenICam::UserSetControl usc( pDev );
        conditionalSetEnumPropertyByString( usc.userSetSelector, "Default" );
        const TDMR_ERROR result = static_cast<TDMR_ERROR>( usc.userSetLoad.call() );
        if( result != DMR_NO_ERROR )
        {
            cout << "An error occurred while restoring the factory default for device " << pDev->serial.read()
                 << "(error code: " << ImpactAcquireException::getErrorCodeAsString( result ) << ")." << endl;
        }

        // Auto exposure or an open shutter will not be helpful for this example thus switch it off if possible.
        GenICam::AcquisitionControl acqc( pDev );
        conditionalSetEnumPropertyByString( acqc.exposureMode, "Timed" );
        conditionalSetEnumPropertyByString( acqc.exposureAuto, "Off" );
        conditionalSetEnumPropertyByString( acqc.acquisitionMode, "Continuous" );

        // Auto gain will not be helpful for this example either thus switch it off if possible.
        GenICam::AnalogControl ac( pDev );
        if( ac.gainSelector.isValid() )
        {
            // There might be more than a single 'Gain' as a 'GainSelector' is present. Iterate over all
            // 'Gain's that can be configured and switch off every 'Auto' feature detected.
            vector<string> validGainSelectorValues;
            ac.gainSelector.getTranslationDictStrings( validGainSelectorValues );
            const vector<string>::size_type cnt = validGainSelectorValues.size();
            for( vector<string>::size_type i = 0; i < cnt; i++ )
            {
                conditionalSetEnumPropertyByString( ac.gainSelector, validGainSelectorValues[i] );
                conditionalSetEnumPropertyByString( ac.gainAuto, "Off" );
            }
        }
        else
        {
            // There is just a single 'Gain' turn off the 'Auto' feature if supported.
            conditionalSetEnumPropertyByString( ac.gainAuto, "Off" );
        }

        // This is needed to correctly calculate the expected capture time
        conditionalSetEnumPropertyByString( acqc.mvAcquisitionFrameRateLimitMode, "mvDeviceMaxSensorThroughput" );
        conditionalSetEnumPropertyByString( acqc.mvAcquisitionFrameRateEnable, "Off" );

        // As we want to keep ALL images belonging to the full sequence in RAM we need as many requests as
        // there are frames defined by the sequence.
        SystemSettings ss( pDev );
        ss.requestCount.write( 50 );

        // We want to act fast, thus if e.g. Bayer-images arrive in the system do NOT convert them on the fly as depending
        // on the device speed the host system might be too slow deal with the amount of data
        ImageProcessing ip( pDev );
        ip.colorProcessing.write( cpmRaw );
        if( ip.tapSortEnable.isValid() )
        {
            ip.tapSortEnable.write( bFalse );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        // This e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while configuring the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        exit( 1 );
    }
}

//-----------------------------------------------------------------------------
// Configures a single 'SequencerSet' so that 'X' frames are captured using a
// certain exposure time and afterwards another sets will be used.
void configureSequencerSet( ThreadParameter* pThreadParameter, const SequencerSetParameter& ssp )
//-----------------------------------------------------------------------------
{
    pThreadParameter->sc.sequencerSetSelector.write( ssp.setNr_ );
    pThreadParameter->ac.exposureTime.write( ssp.exposureTime_us_ );
    if( pThreadParameter->ifc.binningHorizontal.isValid() )
    {
        pThreadParameter->ifc.binningHorizontal.write( ssp.horizontalBinningOrDecimation_ );
    }
    else if( pThreadParameter->ifc.decimationHorizontal.isValid() )
    {
        pThreadParameter->ifc.decimationHorizontal.write( ssp.horizontalBinningOrDecimation_ );
    }
    if( pThreadParameter->ifc.binningVertical.isValid() )
    {
        pThreadParameter->ifc.binningVertical.write( ssp.verticalBinningOrDecimation_ );
    }
    else if( pThreadParameter->ifc.decimationVertical.isValid() )
    {
        pThreadParameter->ifc.decimationVertical.write( ssp.verticalBinningOrDecimation_ );
    }
    pThreadParameter->ifc.height.write( pThreadParameter->ifc.heightMax.read() );
    pThreadParameter->sc.sequencerPathSelector.write( 0LL );
    pThreadParameter->sc.sequencerTriggerSource.writeS( "ExposureEnd" );
    pThreadParameter->sc.sequencerSetNext.write( 0LL );
    pThreadParameter->sc.sequencerPathSelector.write( 1LL );
    if( 0 == ssp.setNr_ )
    {
        pThreadParameter->sc.sequencerTriggerSource.writeS( "UserOutput0" );
        pThreadParameter->sc.sequencerTriggerActivation.writeS( "LevelHigh" );
        pThreadParameter->sc.sequencerSetNext.write( 1LL );
    }
    else
    {
        pThreadParameter->sc.sequencerTriggerSource.writeS( "Off" );
        pThreadParameter->sc.sequencerSetNext.write( 0LL );
    }
    checkedMethodCall( pThreadParameter->pDev, pThreadParameter->sc.sequencerSetSave );
}

//-----------------------------------------------------------------------------
// This function will configure the sequencer on the device to take continuously
// images where the images are smaller and as fast as possible. Then wait for
// user input to change to set 1. To change the sequence edit the 's_SequencerData'
// data array and recompile the application.
void configureSequencer( ThreadParameter* pThreadParameter )
//-----------------------------------------------------------------------------
{
    try
    {
        pThreadParameter->sc.sequencerMode.writeS( "Off" );
        pThreadParameter->sc.sequencerConfigurationMode.writeS( "On" );
        pThreadParameter->sc.sequencerFeatureSelector.writeS( "ExposureTime" );
        pThreadParameter->sc.sequencerFeatureEnable.write( bTrue );
        pThreadParameter->sc.sequencerFeatureSelector.writeS( "CounterDuration" );
        pThreadParameter->sc.sequencerFeatureEnable.write( bFalse );
        const size_t cnt = sizeof( s_SequencerData ) / sizeof( s_SequencerData[0] );
        for( size_t i = 0; i < cnt; i++ )
        {
            configureSequencerSet( pThreadParameter, s_SequencerData[i] );
            s_SequencerData[i].expectedFrameRate_ = pThreadParameter->ac.mvResultingFrameRate.read();
        }
        pThreadParameter->sc.sequencerSetStart.write( 0 );
        pThreadParameter->sc.sequencerConfigurationMode.writeS( "Off" );
        pThreadParameter->sc.sequencerMode.writeS( "On" );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "An error occurred while setting up the sequencer for device " << pThreadParameter->pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl;
        s_boTerminated = true;
    }
}

//=============================================================================
//================= helper functions ==========================================
//=============================================================================
//-----------------------------------------------------------------------------
// Calls the function bound to an mvIMPACT::acquire::Method object and displays
// an error message if the function call did fail.
void checkedMethodCall( Device* pDev, Method& method )
//-----------------------------------------------------------------------------
{
    const TDMR_ERROR result = static_cast<TDMR_ERROR>( method.call() );
    if( result != DMR_NO_ERROR )
    {
        cout << "An error was returned while calling function '" << method.displayName() << "' on device " << pDev->serial.read()
             << "(" << pDev->product.read() << "): " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
    }
}

//=============================================================================
//================= main implementation =======================================
//=============================================================================
//-----------------------------------------------------------------------------
unsigned int DMR_CALL liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    ThreadParameter* pThreadParameter = reinterpret_cast<ThreadParameter*>( pData );

    // Now configure SFNC(Standard Feature Naming Convention) compliant features(see http://www.emva.org to find out more
    // about the standard and to download the latest SFNC document version)
    //
    // IMPORTANT:
    //
    // The SFNC unfortunately does NOT define numerical values for enumerations, thus a device independent piece of software
    // should use the enum-strings defined in the SFNC to ensure interoperability between devices. This is slightly slower
    // but should not cause problems in real world applications. When the device type AND GenICam XML file version is
    // guaranteed to be constant for a certain version of software, the driver internal code generator can be used to create
    // a interface header, that has numerical constants for enumerations as well. See device driver documentation under
    // 'Use Cases -> GenICam to mvIMPACT Acquire code generator' for details.
    configureDevice( pThreadParameter->pDev );
    configureSequencer( pThreadParameter );

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( pThreadParameter->fi.imageRequestSingle() ) ) == DMR_NO_ERROR )
    {
    };
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << mvIMPACT::acquire::ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->pDev, pThreadParameter->fi );

    mvIMPACT::acquire::Request* pRequest = 0;
    // we always have to keep at least 2 images as the displayWindow module might want to repaint the image, thus we
    // can free it unless we have a assigned the displayWindow to a new buffer.
    mvIMPACT::acquire::Request* pPreviousRequest = 0;
    unsigned int cnt = 0;
    const unsigned int timeout_ms = 2500;
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        int requestNr = pThreadParameter->fi.imageRequestWaitFor( timeout_ms );
        pRequest = pThreadParameter->fi.isRequestNrValid( requestNr ) ? pThreadParameter->fi.getRequest( requestNr ) : 0;
        if( pRequest )
        {
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    cout << "Info from " << pThreadParameter->pDev->serial.read()
                         << ": " << pThreadParameter->statistics.framesPerSecond.name() << ": " << pThreadParameter->statistics.framesPerSecond.readS()
                         << ", " << pThreadParameter->statistics.errorCount.name() << ": " << pThreadParameter->statistics.errorCount.readS()
                         << ", " << pThreadParameter->statistics.captureTime_s.name() << ": " << pThreadParameter->statistics.captureTime_s.readS()
                         << ", " << "AOI: " << pRequest->imageWidth.readS() << "x" << pRequest->imageHeight.readS() << endl;
                }
#if !defined(linux) && !defined(__linux) && !defined(__linux__)
                pThreadParameter->displayWindow.GetImageDisplay().SetImage( pRequest );
                pThreadParameter->displayWindow.GetImageDisplay().Update();
#endif  // #if !defined(linux) && !defined(__linux) && !defined(__linux__)
            }
            else
            {
                std::cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            if( pPreviousRequest )
            {
                // this image has been displayed thus the buffer is no longer needed...
                pPreviousRequest->unlock();
            }
            pPreviousRequest = pRequest;
            // send a new image request into the capture queue
            pThreadParameter->fi.imageRequestSingle();
        }
        else
        {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference
            std::cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
                      << ", timeout value too small?" << endl;
        }
#if defined(linux) || defined(__linux) || defined(__linux__)
        if( waitForInput( 0, STDIN_FILENO ) != 0 )
        {
            char in = getchar();
            checkInput( pThreadParameter, in );
        }
#endif // #if defined(linux) || defined(__linux) || defined(__linux__)
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->pDev, pThreadParameter->fi );

#if !defined(linux) && !defined(__linux) && !defined(__linux__)
    // stop the displayWindow from showing data we are about to free
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
    pThreadParameter->fi.imageRequestReset( 0, 0 );
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
    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( !pDev )
    {
        std::cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    try
    {
        std::cout << "Initialising the device. This might take some time..." << endl << endl;
        pDev->interfaceLayout.write( dilGenICam ); // This is also done 'silently' by the 'getDeviceFromUserInput' function but your application needs to do this as well so state this here clearly!
        pDev->open();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        std::cout << "An error occurred while opening the device " << pDev->serial.read()
                  << "(error code: " << e.getErrorCodeAsString() << ")." << endl
                  << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 2;
    }

    // start the execution of the 'live' thread.
    cout << endl
         << "Press [1] [ENTER] to switch to sequencer set 1." << endl
         << "Press [0] [ENTER] to switch to sequencer set 0." << endl
         << "Press [ENTER] to end the application." << endl << endl;
#if defined(linux) || defined(__linux) || defined(__linux__)
    ThreadParameter threadParam( pDev );
    liveThread( &threadParam );
#else
    unsigned int dwThreadID;
    string windowTitle( "mvIMPACT_acquire sequencered paths sample, Device " + pDev->serial.read() );
    // initialise displayWindow
    // IMPORTANT: It's NOT safe to create multiple displayWindow's in multiple threads!!!
    ThreadParameter threadParam( pDev, windowTitle );
    HANDLE hThread = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )( &threadParam ), 0, &dwThreadID );
    while( s_boTerminated == false )
    {
        char in;
        cin >> in;
        checkInput( &threadParam, in );
    }
    WaitForSingleObject( hThread, INFINITE );
    CloseHandle( hThread );
#endif  // #if defined(linux) || defined(__linux) || defined(__linux__)
    return 0;
}
