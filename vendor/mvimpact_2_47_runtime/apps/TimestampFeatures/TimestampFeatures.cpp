#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <thread>
#include <memory>
#include <mutex>
#include <vector>
#include <apps/Common/exampleHelper.h>
#include <common/minmax.h>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#ifdef _WIN32
#   define USE_DISPLAY
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::GenICam;

static mutex s_mutex;
static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
enum TTimestampTestMode
//-----------------------------------------------------------------------------
{
    ttmNone = -1,
    ttmPPSWithIOLine,
    ttmPPSWithIOLineAndOffset,
    ttmPreLoadEverySecond,
    ttmPTPWithTwoCams
};

//-----------------------------------------------------------------------------
class ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev_;
#ifdef USE_DISPLAY
    unique_ptr<ImageDisplayWindow> pDisplayWindow_;
#endif // #ifdef USE_DISPLAY
public:
    explicit ThreadParameter( Device* pDev ) : pDev_( pDev ) {}
    ThreadParameter( const ThreadParameter& src ) = delete;
    Device* device( void ) const
    {
        return pDev_;
    }
#ifdef USE_DISPLAY
    void createDisplayWindow( const string& windowTitle )
    {
        pDisplayWindow_ = unique_ptr<ImageDisplayWindow>( new ImageDisplayWindow( windowTitle ) );
    }
    ImageDisplayWindow& displayWindow( void )
    {
        return *pDisplayWindow_;
    }
#endif // #ifdef USE_DISPLAY
};

//-----------------------------------------------------------------------------
void writeToStdout( const string& msg )
//-----------------------------------------------------------------------------
{
    lock_guard<mutex> lockedScope( s_mutex );
    cout << msg << endl;
}

//-----------------------------------------------------------------------------
string getStringFromCIN( void )
//-----------------------------------------------------------------------------
{
    cout << endl << ">>> ";
    string cmd;
    cin >> cmd;
    // remove the '\n' from the stream
    cin.get();
    return cmd;
}

//-----------------------------------------------------------------------------
TTimestampTestMode getSynchronizationModeFromUser( void )
//-----------------------------------------------------------------------------
{
    TTimestampTestMode testMode = ttmNone;
    bool boRun = true;

    while( boRun )
    {
        cout << endl
             << " Please select synchronization mode" << endl
             << " ------------------------------------------" << endl
             << " [0] - (1 Camera ) - Pulse per Second controller by I/O Line" << endl
             << " [1] - (1 Camera ) - Pulse per Second controller by I/O Line with initial system time" << endl
             << " [2] - (1 Camera ) - Load the timer every second beforehand" << endl
             << " [3] - (2 Cameras) - Precision Time Protocol with two cameras (PTP)" << endl
             << " ------------------------------------------" << endl
             << endl
             << "Please enter a valid option followed by [ENTER]:" << endl
             << "- or enter 'c' followed by [ENTER] to cancel:" << endl;

        const string cmd( getStringFromCIN() );
        if( cmd == "c" )
        {
            boRun = false;
            continue;
        }
        const int selectedMode = static_cast<int>( atoi( cmd.c_str() ) );
        if( ( selectedMode <= ttmNone ) || ( selectedMode > ttmPTPWithTwoCams ) )
        {
            cout << "Invalid selection" << endl;
            continue;
        }

        testMode = static_cast<TTimestampTestMode>( selectedMode );
        boRun = false;
    }

    return testMode;
}

//-----------------------------------------------------------------------------
void loadDefaultUserSet( Device* pDev )
//-----------------------------------------------------------------------------
{
    UserSetControl usc( pDev );
    usc.userSetSelector.writeS( "Default" );
    usc.userSetLoad.call();
    cout << "Loading default UserSet of Device " << pDev->serial.read() << "(" << pDev->product << ")" << endl;
}

//-----------------------------------------------------------------------------
std::string printHumanReadableUnixTime( unsigned long long time )
//-----------------------------------------------------------------------------
{
    time_t rawtime = ( time_t )time;
    std::time_t t = std::time( &rawtime );
    char mbstr[100];
    std::strftime( mbstr, sizeof( mbstr ), "%A %c", std::localtime( &t ) );
    return std::string( mbstr );
}

//-----------------------------------------------------------------------------
bool testModeRequiresSecondCamera( TTimestampTestMode type )
//-----------------------------------------------------------------------------
{
    return type == TTimestampTestMode::ttmPTPWithTwoCams;
}

//-----------------------------------------------------------------------------
void printPulsePTPOnLine4( void )
//-----------------------------------------------------------------------------
{
    cout << " ---------------------------------------------------" << endl
         << "  Attention 'Please connect IO Lines as follows'    " << endl
         << " ---------------------------------------------------" << endl
         << " |\\ |---------|   " << endl
         << " | \\|         | Line4 Input " << endl
         << " |  |  Slave  |---------------|" << endl
         << " | /|         |               |" << endl
         << " |/ |---------|               |" << endl
         << "                              |" << endl
         << " |\\ |---------|               |" << endl
         << " | \\|         | Line4 Input   |" << endl
         << " |  |  Master |---------------X" << endl
         << " | /|         |---------------|" << endl
         << " |/ |---------| Line0 Output" << endl
         << endl;
}

//-----------------------------------------------------------------------------
void printPulseGeneratorOnLine4( void )
//-----------------------------------------------------------------------------
{
    cout << " ---------------------------------------------------" << endl
         << "  Attention 'Please connect IO Lines as follows'    " << endl
         << " ---------------------------------------------------" << endl
         << " |\\ |---------|                     |----------|" << endl
         << " | \\|         | Line4 Input         |          |" << endl
         << " |  |  MV0    |---------------------|  _|1s|_  |" << endl
         << " | /|         |                     |  Pulse   |" << endl
         << " |/ |---------|                     |----------|" << endl
         << endl;
}

//-----------------------------------------------------------------------------
void printPulseGeneratorWarning( void )
//-----------------------------------------------------------------------------
{
    cout << " ---------------------------------------------------" << endl
         << "  Attention 'External Pulse Generator is required!' " << endl
         << " ---------------------------------------------------" << endl
         << " This example requires an external GPS PPS pulse generator" << endl
         << " or any other equivalent pulse generator which outputs" << endl
         << " equidistant pulses every second. To simply execute the" << endl
         << " example, a second camera can be used as pulse generator." << endl
         << " Therefore a timer can be programmed to set a cameras output" << endl
         << endl;
}

//-----------------------------------------------------------------------------
void printPTPWarning( void )
//-----------------------------------------------------------------------------
{
    cout << " -----------------------------------------------------" << endl
         << "  Attention 'IEEE1588 compliant Hardware is required!'" << endl
         << " -----------------------------------------------------" << endl
         << " This example requires IEEE1588 compliant switches and" << endl
         << " clocks to guarantee a low latency high precision time" << endl
         << " stamp synchronization. Otherwise the synchronization might" << endl
         << " be inaccurate." << endl
         << endl;
}

//-----------------------------------------------------------------------------
void liveThread( shared_ptr<ThreadParameter> pParameter )
//-----------------------------------------------------------------------------
{
    Device* pDev = pParameter->device();
    // establish access to the statistic properties
    Statistics statistics( pDev );
    // create an interface to the device found
    FunctionInterface fi( pDev );

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
        lock_guard<mutex> lockedScope( s_mutex );
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pDev, fi );
    // run thread loop
    const Request* pRequest = nullptr;
    const unsigned int timeout_ms = { 500 };
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // can't free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = { INVALID_ID };
    unsigned int cnt = { 0 };
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
                    lock_guard<mutex> lockedScope( s_mutex );
                    cout << "Info from " << pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
#ifdef USE_DISPLAY
                ImageDisplay& display = pParameter->displayWindow().GetImageDisplay();
                display.SetImage( pRequest );
                display.Update();
#endif // #ifdef USE_DISPLAY

                writeToStdout( "got timestamp of image " + pRequest->infoFrameID.readS() + " ( " + printHumanReadableUnixTime( pRequest->infoTimeStamp_us.read() ) + " ) " + " from " + pDev->serial.read() + ": " + printHumanReadableUnixTime( pRequest->chunkTimestamp.read() ) );
            }
            else
            {
                writeToStdout( "Error: " + pRequest->requestResult.readS() );
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
    manuallyStopAcquisitionIfNeeded( pDev, fi );

#ifdef USE_DISPLAY
    // stop the display from showing freed memory
    pParameter->displayWindow().GetImageDisplay().RemoveImage();
#endif // #ifdef USE_DISPLAY
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
}

//-----------------------------------------------------------------------------
void ConfigureCameraForPTPTestMode( Device* pDev, TBoolean isMaster )
//-----------------------------------------------------------------------------
{
    AcquisitionControl acqc( pDev );
    ChunkDataControl cdc( pDev );

    cdc.chunkModeActive.write( TBoolean::bTrue );
    cdc.chunkSelector.writeS( "Timestamp" );
    cdc.chunkEnable.write( TBoolean::bTrue );

    acqc.triggerSelector.writeS( "FrameStart" );
    acqc.triggerMode.writeS( "On" );
    acqc.triggerSource.writeS( "Line4" );
    acqc.triggerActivation.writeS( "RisingEdge" );

    if( isMaster == TBoolean::bTrue )
    {
        DigitalIOControl dioc( pDev );
        CounterAndTimerControl catc( pDev );

        catc.timerSelector.writeS( "Timer1" );
        catc.timerTriggerSource.writeS( "Timer2End" );
        catc.timerDuration.write( 1000000.0 );
        catc.timerDelay.write( 0.0 );
        catc.timerSelector.writeS( "Timer2" );
        catc.timerTriggerSource.writeS( "Timer1End" );
        catc.timerDuration.write( 10000.0 );
        catc.timerDelay.write( 0.0 );

        dioc.lineSelector.writeS( "Line0" );
        dioc.lineSource.writeS( "Timer2Active" );
    }
}

//-----------------------------------------------------------------------------
void PTPWithTwoCams( Device* pDev0, Device* pDev1 )
//-----------------------------------------------------------------------------
{
    static const int s_SECOND = 1;
    static const int s_PTP_BEST_MASTER_CLOCK_WAIT_TIME = 15;

    loadDefaultUserSet( pDev0 );
    loadDefaultUserSet( pDev1 );

    printPTPWarning();

    cout << " This example shows a simple way to set the timestamps" << endl
         << " of the cameras internal timer. It uses a drift compensation" << endl
         << " via a closed loop controller. The system time is set at" << endl
         << " the beginning on the previously determined master camera only." << endl
         << endl;
    try
    {
        Device* pDevMaster = nullptr;
        Device* pDevSlave = nullptr;

        TransportLayerControl tlc0( pDev0 );
        TransportLayerControl tlc1( pDev1 );

        // determine, which camera will be the master, therefore we simply switch on
        // ptp the first time and wait until the controllers of both cameras are stable
        tlc0.ptpEnable.write( TBoolean::bTrue );
        tlc1.ptpEnable.write( TBoolean::bTrue );

        for( int i = 0; i < s_PTP_BEST_MASTER_CLOCK_WAIT_TIME * s_SECOND; i++ )
        {
            this_thread::sleep_for( chrono::milliseconds( s_SECOND * 1000 ) );
            tlc0.ptpDataSetLatch.call();
            cout << "cam0 '" << pDev0->serial.readS() << "', '" << pDev0->product.readS() << "' has ptp state '" << tlc0.ptpStatus.readS() << "'" << endl;
            if( tlc0.ptpStatus.readS() == "Master" )
            {
                pDevMaster = pDev0;
                pDevSlave = pDev1;
                break;
            }
            cout << "cam1 '" << pDev1->serial.readS() << "', '" << pDev1->product.readS() << "' has ptp state '" << tlc1.ptpStatus.readS() << "'" << endl;
            if( tlc0.ptpStatus.readS() == "Slave" )
            {
                pDevMaster = pDev1;
                pDevSlave = pDev0;
                break;
            }
        }

        tlc0.ptpDataSetLatch.call();
        tlc1.ptpDataSetLatch.call();

        if( pDevSlave == nullptr || pDevMaster == nullptr )
        {
            cout << "we could not determine which camera is master and which one is slave" << endl;
            return;
        }

        if( tlc1.ptpStatus.readS().compare( tlc0.ptpStatus.readS() ) == 0 )
        {
            cout << "both cameras had the same state '" << tlc0.ptpStatus.readS() << "'.something went wrong!" << endl;
            return;
        }

        printPulsePTPOnLine4();
        cout << "Master is " << pDevMaster->serial.readS() << endl;
        cout << "Slave is " << pDevSlave->serial.readS() << endl;
        cout << "please contact IOs as mentioned above" << endl;
        cout << "Press [ENTER] to proceed with PTP example" << endl;
        cin.get();

        tlc0.ptpEnable.write( TBoolean::bFalse );
        tlc1.ptpEnable.write( TBoolean::bFalse );

        TransportLayerControl tlcMaster( pDevMaster );
        TransportLayerControl tlcSlave( pDevSlave );
        DeviceControl dcMaster( pDevMaster );
        DeviceControl dcSlave( pDevSlave );

        // then we set the clock of the master device to systems time and the slaves
        // clock to zero

        // get system clock and time stamp tick value
        const unsigned long long unixTimeSystem = static_cast<unsigned long long>( std::chrono::duration_cast<std::chrono::seconds>( chrono::system_clock::now().time_since_epoch() ).count() );
        const unsigned long long gevsTimeTickFrequencyValue = static_cast<unsigned long long>( tlcMaster.gevTimestampTickFrequency.read() );
        const unsigned long long cameraTimeSystem = unixTimeSystem * gevsTimeTickFrequencyValue;

        // pre-load sets the devices clock to the system time
        // set cameras clock
        dcMaster.mvTimestampResetValue.write( static_cast<int64_type>( cameraTimeSystem ) );
        dcSlave.mvTimestampResetValue.write( static_cast<int64_type>( 0 ) );
        dcMaster.timestampReset.call();
        dcSlave.timestampReset.call();

        // now we start PTP and wait until both cameras are in stable controller state
        tlcMaster.ptpEnable.write( TBoolean::bTrue );
        tlcSlave.ptpEnable.write( TBoolean::bTrue );

        for( int i = 0; i < s_PTP_BEST_MASTER_CLOCK_WAIT_TIME * s_SECOND; i++ )
        {
            this_thread::sleep_for( chrono::milliseconds( s_SECOND * 1000 ) );
            tlcMaster.ptpDataSetLatch.call();
            tlcSlave.ptpDataSetLatch.call();
            cout << "Master has ptp state '" << tlcMaster.ptpStatus.readS() << "'!" << endl;
            cout << "Slave has ptp state '" << tlcSlave.ptpStatus.readS() << "'!" << endl;
            if( ( tlcMaster.ptpStatus.readS() == "Master" ) &&
                ( tlcSlave.ptpStatus.readS() == "Slave" ) )
            {
                break;
            }
        }

        // now we use the acquisition trigger via line4 on both devices to capture
        // images which have time stamps, we can compare. Therefore we configure
        // the master and the slave properly
        ConfigureCameraForPTPTestMode( pDevMaster, TBoolean::bTrue );
        ConfigureCameraForPTPTestMode( pDevSlave, TBoolean::bFalse );

        // reduce the bandwidth of the devices when using them in switched networks
        // some switches might have troubles with high throughput in a short amount of time
        dcMaster.deviceLinkThroughputLimitMode.write( TBoolean::bTrue );
        dcMaster.deviceLinkThroughputLimit.write( dcMaster.deviceLinkSpeed.read() / 2 );
        dcSlave.deviceLinkThroughputLimitMode.write( TBoolean::bTrue );
        dcSlave.deviceLinkThroughputLimit.write( dcSlave.deviceLinkSpeed.read() / 2 );

        // now prepare the requests and the image capturing
        // store all device infos in a vector
        // and start the execution of a 'live' thread for each device.
        map<shared_ptr<ThreadParameter>, shared_ptr<thread>> threads;
        shared_ptr<ThreadParameter> pParameterSlave = make_shared<ThreadParameter>( pDevSlave );
        shared_ptr<ThreadParameter> pParameterMaster = make_shared<ThreadParameter>( pDevMaster );
#ifdef USE_DISPLAY
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        // Therefore this must be done before starting the threads
        pParameterSlave->createDisplayWindow( "mvIMPACT_acquire sample, Device " + pDevSlave->serial.read() );
        pParameterMaster->createDisplayWindow( "mvIMPACT_acquire sample, Device " + pDevMaster->serial.read() );
#endif // #ifdef USE_DISPLAY
        threads[pParameterSlave] = make_shared<thread>( liveThread, pParameterSlave );
        threads[pParameterMaster] = make_shared<thread>( liveThread, pParameterMaster );

        // now all threads will start running...
        writeToStdout( "Press [ENTER] to end the acquisition" );

        if( getchar() == EOF )
        {
            writeToStdout( "'getchar()' did return EOF..." );
        }

        // stop all threads again
        writeToStdout( "Terminating live threads..." );
        s_boTerminated = true;
        for( auto& it : threads )
        {
            it.second->join();
        }

        // disable link speed throughput because its a permanent setting
        dcMaster.deviceLinkThroughputLimitMode.write( TBoolean::bFalse );
        dcSlave.deviceLinkThroughputLimitMode.write( TBoolean::bFalse );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << endl;
        cout << " An mvIMPACT Acquire exception occurred:" << e.getErrorCodeAsString() << endl;
        cout << endl;
    }
}

//-----------------------------------------------------------------------------
void PulsePerSecond( Device* pDev, bool withPreLoad )
//-----------------------------------------------------------------------------
{
    static const int s_TIME_STEP_WIDTH_MS     = 100;
    static const int s_TEST_TIME_LOOP_COUNTER = 120 * s_TIME_STEP_WIDTH_MS / 10;

    loadDefaultUserSet( pDev );

    printPulseGeneratorWarning();
    printPulseGeneratorOnLine4();
    cout << "please contact I/Os as mentioned above" << endl
         << "The example takes about 2 minutes." << endl
         << "Press [ENTER] to proceed with PPS example" << endl;
    cin.get();

    cout << " This example shows a simple way to set the time stamps" << endl
         << " of the cameras internal timer. It uses a drift compensation" << endl
         << " via a closed loop controller. The offset is optionally set at" << endl
         << " the beginning." << endl << endl;

    try
    {
        DigitalIOControl dioc( pDev );
        EventControl evc( pDev );
        DeviceControl dc( pDev );
        TransportLayerControl tlc( pDev );

        if( dc.mvTimestampPPSSync.isValid() == false || evc.eventLine4RisingEdge.isValid() == false ||
            ( withPreLoad == true && dc.mvTimestampResetValue.isValid() == false ) || tlc.gevTimestampTickFrequency.isValid() == false ||
            dc.timestampReset.isValid() == false )
        {
            cout << " A required Property for this example is not supported by the camera" << endl;
            return;
        }

        // enable event input line 4 on rising edge
        evc.eventSelector.writeS( "Line4RisingEdge" );
        evc.eventNotification.writeS( "On" );

        // get system clock and time stamp tick value
        const unsigned long long unixTimeSystem = static_cast<unsigned long long>( std::chrono::duration_cast<std::chrono::seconds>( chrono::system_clock::now().time_since_epoch() ).count() );
        const unsigned long long gevsTimeTickFrequencyValue = static_cast<unsigned long long>( tlc.gevTimestampTickFrequency.read() );
        const unsigned long long cameraTimeSystem = unixTimeSystem * gevsTimeTickFrequencyValue;

        // pre-load sets the devices clock to the system time
        if( withPreLoad == true )
        {
            // set cameras clock
            dc.mvTimestampResetValue.write( static_cast<int64_type>( cameraTimeSystem ) );
            dc.timestampReset.call();
        }

        // enable PPS on Line 4
        dc.mvTimestampPPSSync.writeS( "Line4" );

        // print out the timestamps and the difference to the last
        for( int i = 0; i < s_TEST_TIME_LOOP_COUNTER; i++ )
        {
            this_thread::sleep_for( chrono::milliseconds( s_TIME_STEP_WIDTH_MS ) );
            static unsigned long long s_lastVal = 0;
            unsigned long long newVal = static_cast<unsigned long long>( evc.eventLine4RisingEdgeTimestamp.read() );
            if( s_lastVal != newVal )
            {
                if( s_lastVal != 0 )
                {
                    const double difference = ( ( static_cast<double>( newVal ) - static_cast<double>( s_lastVal ) ) / static_cast<double>( gevsTimeTickFrequencyValue ) - 1.0 ) * 1000000.0;
                    cout << "Got time stamp " << printHumanReadableUnixTime( newVal ) << " ( " << newVal << " ), " << " difference from 1s is: " << difference << "us" << endl;
                }
                s_lastVal = newVal;
            }
        }

        dc.mvTimestampResetValue.write( static_cast<int64_type>( 0 ) );
        dc.mvTimestampPPSSync.writeS( "Off" );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << endl;
        cout << " An mvIMPACT Acquire exception occurred:" << e.getErrorCodeAsString() << endl;
        cout << endl;
    }
}

//-----------------------------------------------------------------------------
void PreLoadSecond( Device* pDev )
//-----------------------------------------------------------------------------
{
    static const int s_TIME_STEP_WIDTH_MS     = 100;
    static const int s_TEST_TIME_LOOP_COUNTER = 120 * s_TIME_STEP_WIDTH_MS / 10;

    loadDefaultUserSet( pDev );

    printPulseGeneratorOnLine4();
    printPulseGeneratorWarning();
    cout << "please contact I/Os as mentioned above" << endl
         << "The example takes about 2 minutes." << endl
         << "Press [ENTER] to proceed with example" << endl;
    cin.get();

    cout << " This example shows a rough way to set the time stamps" << endl
         << " of the cameras internal timer. This example does neither a " << endl
         << " proper offset nor a drift compensation via a closed loop " << endl
         << " controller. It simply shows how a timer can be forced to a" << endl
         << " given time by simply using a pulsed signal with a " << endl
         << " previously transfered pre-loaded value. The drift can be seen" << endl
         << " when looking at the lower part of each time stamp. The difference" << endl
         << " is nearly zero the whole time because every second we set back the" << endl
         << " to our calculated next time value. In theory the difference is constant" << endl
         << " and zero. In reality we have some Jitter effects and uncertainties of the" << endl
         << " pulse generator." << endl << endl;

    try
    {
        DigitalIOControl dioc( pDev );
        EventControl evc( pDev );
        DeviceControl dc( pDev );
        TransportLayerControl tlc( pDev );
        AcquisitionControl acqc( pDev );

        if( evc.eventLine4RisingEdge.isValid() == false || dc.mvTimestampResetValue.isValid() == false ||
            tlc.gevTimestampTickFrequency.isValid() == false || acqc.triggerSelector.isValid() == false ||
            acqc.triggerMode.isValid() == false || acqc.triggerSource.isValid() == false )
        {
            cout << " A required Property for this example is not provided by the camera" << endl;
            return;
        }

        // get system clock and time stamp tick value
        unsigned long long unixTimeSystem = static_cast<unsigned long long>( std::chrono::duration_cast<std::chrono::seconds>( chrono::system_clock::now().time_since_epoch() ).count() );
        unsigned long long gevsTimeTickFrequencyValue = static_cast<unsigned long long>( tlc.gevTimestampTickFrequency.read() );
        //increment next clock for setting the cameras timer properly
        unsigned long long cameraTimeSystem = ( unixTimeSystem + 1 ) * gevsTimeTickFrequencyValue;
        // set cameras next clock value
        dc.mvTimestampResetValue.write( static_cast<int64_type>( cameraTimeSystem ) );

        // enable event input line 4 on rising edge
        // and enable time stamp reset on this line as well
        evc.eventSelector.writeS( "Line4RisingEdge" );
        evc.eventNotification.writeS( "On" );
        acqc.triggerSelector.writeS( "mvTimestampReset" );
        acqc.triggerMode.writeS( "On" );
        acqc.triggerSource.writeS( "Line4" );

        // print out the timestamps and the difference to the last and set new time stamp reset value for
        // next pulse signal
        for( int i = 0; i < s_TEST_TIME_LOOP_COUNTER; i++ )
        {
            this_thread::sleep_for( chrono::milliseconds( s_TIME_STEP_WIDTH_MS ) );
            static unsigned long long s_lastVal = 0;
            unsigned long long newVal = static_cast<unsigned long long>( evc.eventLine4RisingEdgeTimestamp.read() );
            if( s_lastVal != newVal )
            {
                if( s_lastVal != 0 )
                {
                    // get system clock and time stamp tick value
                    unixTimeSystem = static_cast<unsigned long long>( std::chrono::duration_cast<std::chrono::seconds>( chrono::system_clock::now().time_since_epoch() ).count() );
                    //increment next clock for setting the cameras timer properly
                    cameraTimeSystem = ( unixTimeSystem + 1 ) * gevsTimeTickFrequencyValue;
                    // set cameras next clock value
                    dc.mvTimestampResetValue.write( static_cast<int64_type>( cameraTimeSystem ) );

                    double difference = ( ( static_cast<double>( newVal ) - static_cast<double>( s_lastVal ) ) / static_cast<double>( gevsTimeTickFrequencyValue ) - 1.0 ) * 1000000.0;
                    cout << "got time stamp " << printHumanReadableUnixTime( newVal ) << " ( " << newVal << " ) " << " difference from 1s is:" << difference << "us" << endl;
                }

                s_lastVal = newVal;
            }
        }

        dc.mvTimestampResetValue.write( static_cast<int64_type>( 0 ) );
        acqc.triggerSelector.writeS( "mvTimestampReset" );
        acqc.triggerMode.writeS( "Off" );
    }
    catch( const ImpactAcquireException& e )
    {
        cout << endl;
        cout << " An mvIMPACT Acquire exception occurred:" << e.getErrorCodeAsString() << endl;
        cout << endl;
    }
}

//-----------------------------------------------------------------------------
void printNotSupported( Device* pDev )
//-----------------------------------------------------------------------------
{
    cout << "Device " << pDev->serial.read() << "(" << pDev->product << ") is not supported by this sample" << endl;
    cout << "Press [ENTER] to end the application" << endl;
    cin.get();
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    TTimestampTestMode selection = getSynchronizationModeFromUser();
    if( selection == ttmNone )
    {
        cout << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 0;
    }

    cout << "Please select the camera you want to use:" << endl;
    Device* pDev0 = getDeviceFromUserInput( devMgr );
    Device* pDev1 = nullptr;
    if( pDev0 == nullptr )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    if( !pDev0->interfaceLayout.isValid() || ( pDev0->interfaceLayout.read() != dilGenICam ) )
    {
        printNotSupported( pDev0 );
        return 1;
    }

    // if this device offers the 'GenICam' interface switch it on, as this will
    // allow are better control over GenICam compliant devices
    conditionalSetProperty( pDev0->interfaceLayout, dilGenICam );
    // if this device offers a user defined acquisition start/stop behaviour
    // enable it as this allows finer control about the streaming behaviour
    conditionalSetProperty( pDev0->acquisitionStartStopBehaviour, assbUser );
    pDev0->open();

    if( testModeRequiresSecondCamera( selection ) )
    {
        cout << "This mode requires a second camera." << endl;
        cout << "Please select the camera you want to use:" << endl;
        pDev1 = getDeviceFromUserInput( devMgr );
        if( pDev1 == nullptr )
        {
            cout << "Unable to continue! Press [ENTER] to end the application" << endl;
            cin.get();
            return 1;
        }

        if( !pDev1->interfaceLayout.isValid() || ( pDev1->interfaceLayout.read() != dilGenICam ) )
        {
            printNotSupported( pDev0 );
            return 1;
        }

        if( pDev0->serial.readS().compare( pDev1->serial.readS() ) == 0 )
        {
            cout << "Its not allowed to use the same device as second device! Press [ENTER] to end the application" << endl;
            cin.get();
            return 1;
        }

        // if this device offers the 'GenICam' interface switch it on, as this will
        // allow are better control over GenICam compliant devices
        conditionalSetProperty( pDev1->interfaceLayout, dilGenICam );
        // if this device offers a user defined acquisition start/stop behaviour
        // enable it as this allows finer control about the streaming behaviour
        conditionalSetProperty( pDev1->acquisitionStartStopBehaviour, assbUser );
        pDev1->open();
    }

    switch( selection )
    {
    case TTimestampTestMode::ttmPPSWithIOLine:
        PulsePerSecond( pDev0, false );
        break;
    case TTimestampTestMode::ttmPPSWithIOLineAndOffset:
        PulsePerSecond( pDev0, true );
        break;
    case TTimestampTestMode::ttmPreLoadEverySecond:
        PreLoadSecond( pDev0 );
        break;
    case TTimestampTestMode::ttmPTPWithTwoCams:
        PTPWithTwoCams( pDev0, pDev1 );
        break;
    default:
        cout << "Invalid selection" << endl;
        break;
    }

    cout << "Press [ENTER] to end the application" << endl;
    cin.get();
    return 0;
}
