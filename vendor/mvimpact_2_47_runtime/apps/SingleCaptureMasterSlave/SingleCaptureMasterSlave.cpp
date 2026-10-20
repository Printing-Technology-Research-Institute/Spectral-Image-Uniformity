#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#ifdef _WIN32
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

static mutex s_mutex;

//-----------------------------------------------------------------------------
class ThreadData
//-----------------------------------------------------------------------------
{
    volatile bool boTerminateThread_;
    unique_ptr<thread> pThread_;
public:
    explicit ThreadData() : boTerminateThread_( false ), pThread_() {}
    virtual ~ThreadData() {}
    bool terminated( void ) const
    {
        return boTerminateThread_;
    }
    template<class _Fn, class _Arg>
    void startThread( _Fn&& _Fx, _Arg&& _Ax )
    {
        pThread_ = unique_ptr<thread>( new thread( _Fx, _Ax ) );
    }
    void terminateThread( void )
    {
        boTerminateThread_ = true;
        if( pThread_ )
        {
            pThread_->join();
        }
    }
};

//-----------------------------------------------------------------------------
class DeviceData : public ThreadData
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    bool isMaster_;
    unique_ptr<FunctionInterface> pFI_;
    unique_ptr<IOSubSystem> pIOSS_;
    unique_ptr<Statistics> pSS_;
#ifdef USE_DISPLAY
    unique_ptr<ImageDisplayWindow> pDisplayWindow_;
#endif // #ifdef USE_DISPLAY
    int lastRequestNr_;
public:
    explicit DeviceData( Device* p ) : ThreadData(), pDev_( p ), isMaster_( false ), pFI_(), pIOSS_(), pSS_(),
#ifdef USE_DISPLAY
        pDisplayWindow_(),
#endif // #ifdef USE_DISPLAY
        lastRequestNr_( INVALID_ID ) {}
    ~DeviceData()
    {
        if( pFI_->isRequestNrValid( lastRequestNr_ ) )
        {
            pFI_->imageRequestUnlock( lastRequestNr_ );
            lastRequestNr_ = INVALID_ID;
        }
    }
    void init( const bool isMaster )
    {
        pDev_->open();
        isMaster_ = isMaster;
        pFI_ = unique_ptr<FunctionInterface>( new FunctionInterface( pDev_ ) );
        pSS_ = unique_ptr<Statistics>( new Statistics( pDev_ ) );
        pIOSS_ = unique_ptr<IOSubSystemCommon>( new IOSubSystemCommon( pDev_ ) );
        {
            lock_guard<mutex> lockedScope( s_mutex );
            cout << "Please note that there will be just one refresh for the display window, so if it is\n"
                 << "hidden under another window the result will not be visible.\n";
        }
#ifdef USE_DISPLAY
        pDisplayWindow_ = unique_ptr<ImageDisplayWindow>( new ImageDisplayWindow( string( isMaster ? "Master " : "Slave " ) + pDev_->serial.read() ) );
#endif // #ifdef USE_DISPLAY
    }
    Device* device( void ) const
    {
        return pDev_;
    }
    FunctionInterface* functionInterface( void ) const
    {
        return pFI_.get();
    }
    bool isMaster( void ) const
    {
        return isMaster_;
    }
    IOSubSystem* IOSS( void ) const
    {
        return pIOSS_.get();
    }
    Statistics* statistics( void ) const
    {
        return pSS_.get();
    }
#ifdef USE_DISPLAY
    ImageDisplayWindow* pDisp( void )
    {
        return pDisplayWindow_.get();
    }
#endif // USE_DISPLAY
};

//-----------------------------------------------------------------------------
class TriggerSignal : public ThreadData
//-----------------------------------------------------------------------------
{
    DigitalOutput* pTriggerOutput_;
    unsigned int frequency_Hz_;
public:
    explicit TriggerSignal( DigitalOutput* pTriggerOutput, unsigned int frequency_Hz ) : ThreadData(), pTriggerOutput_( pTriggerOutput ), frequency_Hz_( frequency_Hz ) {}
    DigitalOutput* triggerOutput( void ) const
    {
        return pTriggerOutput_;
    }
    unsigned int frequency_Hz( void ) const
    {
        return frequency_Hz_;
    }
};

//-----------------------------------------------------------------------------
void liveThread( DeviceData* pThreadParameter )
//-----------------------------------------------------------------------------
{
#ifdef USE_DISPLAY
    ImageDisplay& display = pThreadParameter->pDisp()->GetImageDisplay();
#endif // #ifdef USE_DISPLAY
    // establish access to the statistic properties
    Statistics* pSS = pThreadParameter->statistics();
    // create an interface to the device found
    FunctionInterface* pFI = pThreadParameter->functionInterface();

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( pFI->imageRequestSingle() ) ) == DMR_NO_ERROR ) {};
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        lock_guard<mutex> lockedScope( s_mutex );
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->device(), *pFI );
    // run thread loop
    const unsigned int timeout_ms = {200};
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // can't free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = {INVALID_ID};
    unsigned int cnt = {0};
    while( !pThreadParameter->terminated() )
    {
        // wait for results from the default capture queue
        const int requestNr = pFI->imageRequestWaitFor( timeout_ms );
        if( pFI->isRequestNrValid( requestNr ) )
        {
            const Request* pRequest = pFI->getRequest( requestNr );
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    lock_guard<mutex> lockedScope( s_mutex );
                    cout << "Info from " << pThreadParameter->device()->serial.read() << "(" << ( pThreadParameter->isMaster() ? "Master" : "Slave" ) << ")"
                         << ": " << pSS->framesPerSecond.name() << ": " << pSS->framesPerSecond.readS()
                         << ", " << pSS->errorCount.name() << ": " << pSS->errorCount.readS()
                         << ", " << pRequest->infoFrameNr
                         << ", " << pRequest->infoFrameID
                         << ", " << pSS->frameCount << endl;
                }
#ifdef USE_DISPLAY
                display.SetImage( pRequest );
                display.Update();
#endif // #ifdef USE_DISPLAY
            }
            else
            {
                lock_guard<mutex> lockedScope( s_mutex );
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }
            if( pFI->isRequestNrValid( lastRequestNr ) )
            {
                // this image has been displayed thus the buffer is no longer needed...
                pFI->imageRequestUnlock( lastRequestNr );
            }
            lastRequestNr = requestNr;
            // send a new image request into the capture queue
            pFI->imageRequestSingle();
        }
        else
        {
            //lock_guard<mutex> lockedScope( s_mutex );
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
        }
    }

    {
        lock_guard<mutex> lockedScope( s_mutex );
        cout << "Overall good frames captured from device " << pThreadParameter->device()->serial.read() << ": " << cnt << endl;
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->device(), *pFI );

#ifdef USE_DISPLAY
    // stop the display from showing freed memory
    display.RemoveImage();
#endif // #ifdef USE_DISPLAY
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( pFI->isRequestNrValid( lastRequestNr ) )
    {
        pFI->imageRequestUnlock( lastRequestNr );
    }
    // clear all queues
    pFI->imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
void triggerThread( TriggerSignal* pSignal )
//-----------------------------------------------------------------------------
{
    unsigned int cnt = {0};
    const unsigned int sleepPeriod_ms = 1000 / ( pSignal->frequency_Hz() * 2 );
    while( !pSignal->terminated() )
    {
        // generate a trigger signal
        this_thread::sleep_for( chrono::milliseconds( sleepPeriod_ms ) );
        pSignal->triggerOutput()->flip();
        this_thread::sleep_for( chrono::milliseconds( sleepPeriod_ms ) );
        pSignal->triggerOutput()->flip();
        ++cnt;
        if( cnt % 100 == 0 )
        {
            lock_guard<mutex> lockedScope( s_mutex );
            cout << "Trigger signals generated: " << cnt << endl;
        }
    }
    lock_guard<mutex> lockedScope( s_mutex );
    cout << "Overall trigger signals generated: " << cnt << endl;
}

//-----------------------------------------------------------------------------
unsigned int getNumberFromUser( void )
//-----------------------------------------------------------------------------
{
    unsigned int nr = {0};
    std::cin >> nr;
    // remove the '\n' from the stream
    std::cin.get();
    return nr;
}

//-----------------------------------------------------------------------------
bool isDeviceSupportedBySample( const Device* const pDev )
//-----------------------------------------------------------------------------
{
    return match( pDev->product.read(), string( "mvBlueCOUGAR-X*" ), '*' ) == 0;
}

//-----------------------------------------------------------------------------
void setupTriggerInput( DeviceData* pDevData )
//-----------------------------------------------------------------------------
{
    cout << "Select the digital INPUT of device(" << pDevData->device()->serial.read() << ")(as a string) that shall serve as a trigger input:" << endl;
    CameraSettingsBlueCOUGAR cs( pDevData->device() );
    DisplayPropertyDictionary<mvIMPACT::acquire::PropertyI>( cs.triggerSource );
    modifyPropertyValue( cs.triggerSource );
    cs.triggerMode.write( ctmOnRisingEdge );
    // infinite trigger timeout
    cs.imageRequestTimeout_ms.write( 0 );
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    cout << "This sample is meant for mvBlueCOUGAR-X devices only. Other devices might be installed" << endl
         << "but won't be recognized by the application." << endl
         << endl;

    DeviceManager devMgr;
    std::vector<mvIMPACT::acquire::Device*> validDevices;
    if( getValidDevices( devMgr, validDevices, isDeviceSupportedBySample ) < 2 )
    {
        cout << "This sample needs at least 2 valid devices(one master and one slave). " << validDevices.size() << " device(s) has/have been detected." << endl
             << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    cout << "Please select the MASTER device(the one that will create the trigger for all devices).\n\n";
    Device* pMaster = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample, true );
    if( pMaster == nullptr )
    {
        cout << "Master device has not been properly selected. Unable to continue!\n"
             << "Press [ENTER] to end the application\n";
        cin.get();
        return 1;
    }

    set<Device*> setOfSlaves;
    do
    {
        cout << "\nPlease select a SLAVE device(this one will be triggered by the master).\n\n";
        Device* p = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample, true );
        if( p == pMaster )
        {
            cout << "Master and slave must be different. Skipped!" << endl;
        }
        else if( p != nullptr )
        {
            if( setOfSlaves.find( p ) == setOfSlaves.end() )
            {
                setOfSlaves.insert( p );
            }
            else
            {
                cout << "ALL slaves must be different. This one has already been selected. Skipped!" << endl;
            }
        }
        cout << "\nAdd another slave device('y')? ";
    }
    while( getchar() == 'y' );

    cout << endl;
    vector<DeviceData*> devices;
    devices.push_back( new DeviceData( pMaster ) );
    for( auto* const pSlave : setOfSlaves )
    {
        devices.push_back( new DeviceData( pSlave ) );
    }
    const vector<DeviceData*>::size_type DEV_COUNT = devices.size();
    DigitalOutput* pTriggerOutput = nullptr;
    try
    {
        for( vector<DeviceData*>::size_type i = 0; i < DEV_COUNT; i++ )
        {
            cout << "Initialising device " << devices[i]->device()->serial.read() << "..." << endl;
            const bool isMaster = i == 0;
            devices[i]->init( isMaster );
            cout << endl
                 << "Setup the " << ( isMaster ? "MASTER" : "SLAVE" ) << " device:" << endl
                 << "===========================" << endl
                 << endl;
            if( isMaster )
            {
                const unsigned int digoutCount = devices[i]->IOSS()->getOutputCount();
                for( unsigned int digOut = 0; digOut < digoutCount; digOut++ )
                {
                    cout << "  [" << digOut << "]: " << devices[i]->IOSS()->output( digOut )->getDescription() << endl;
                }
                cout << endl
                     << "Select the digital OUTPUT of the MASTER device(" << devices[i]->device()->serial.read() << ") where the trigger pulse shall be generated on: ";
                pTriggerOutput = devices[i]->IOSS()->output( getNumberFromUser() );
                cout << endl;
            }
            setupTriggerInput( devices[i] );
        }
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while opening the devices(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    try
    {
        // get trigger frequency from user
        unsigned int triggerFrequency_Hz = {0};
        bool boRun = {true};
        while( boRun )
        {
            cout << "Please enter the approx. desired trigger frequency in Hz: ";
            triggerFrequency_Hz = getNumberFromUser();
            if( ( triggerFrequency_Hz >= 1 ) && ( triggerFrequency_Hz <= 100 ) )
            {
                boRun = false;
                continue;
            }
            cout << "Invalid Selection. This sample will accept values from 1 - 100." << endl;
        }
        TriggerSignal triggerSignal( pTriggerOutput, triggerFrequency_Hz );

        // start live threads
        for( auto* pDevData : devices )
        {
            pDevData->startThread( liveThread, pDevData );
        }

        // now all capture threads will start running...
        // before starting the trigger thread wait a little bit to allow the camera threads to set up completely
        this_thread::sleep_for( chrono::milliseconds( 1000 ) );
        triggerSignal.startThread( triggerThread, &triggerSignal );

        {
            lock_guard<mutex> lockedScope( s_mutex );
            cout << "Press [ENTER] to end the acquisition" << endl;
        }
        if( getchar() == EOF )
        {
            printf( "Calling '_getch()' did return EOF...\n" );
        }

        // stop all threads again
        {
            lock_guard<mutex> lockedScope( s_mutex );
            cout << "Terminating threads..." << endl;
        }

        for( auto* pDevData : devices )
        {
            pDevData->terminateThread();
        }
        triggerSignal.terminateThread();
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "An error occurred(error code: " << e.getErrorCodeAsString() << ")." << endl;
        cout << "Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }
    cout << "Press [ENTER] to end the application" << endl;
    cin.get();
    return 0;
}
