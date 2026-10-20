#ifdef _MSC_VER // is Microsoft compiler?
#   if _MSC_VER < 1300  // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#   endif // #if _MSC_VER < 1300
#endif // #ifdef _MSC_VER
#include <conio.h>
#include <windows.h>
#include <process.h>
#include <iostream>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>
#include <mvDisplay/Include/mvIMPACT_acquire_display.h>

using namespace std;
using namespace mvIMPACT::acquire;
using namespace mvIMPACT::acquire::display;

//-----------------------------------------------------------------------------
/// to get thread safe access to the std out and the display module
class CriticalSection
//-----------------------------------------------------------------------------
{
    CRITICAL_SECTION m_criticalSection;
public:
    CriticalSection()
    {
        InitializeCriticalSection( &m_criticalSection );
    }
    ~CriticalSection()
    {
        DeleteCriticalSection( &m_criticalSection );
    }
    void lock( void )
    {
        EnterCriticalSection( &m_criticalSection );
    }
    void unlock( void )
    {
        LeaveCriticalSection( &m_criticalSection );
    }
} g_critSect;

//-----------------------------------------------------------------------------
class LockedScope
//-----------------------------------------------------------------------------
{
    CriticalSection c_;
public:
    explicit LockedScope( CriticalSection& c ) : c_( c )
    {
        c_.lock();
    }
    ~LockedScope()
    {
        c_.unlock();
    }
};

//-----------------------------------------------------------------------------
class ThreadData
//-----------------------------------------------------------------------------
{
    volatile bool boTerminateThread_;
public:
    explicit ThreadData() : boTerminateThread_( false ) {}
    virtual ~ThreadData() {}
    bool terminated( void ) const
    {
        return boTerminateThread_;
    }
    void terminateThread( void )
    {
        boTerminateThread_ = true;
    }
};

//-----------------------------------------------------------------------------
class DeviceData : public ThreadData
//-----------------------------------------------------------------------------
{
    Device* pDev_;
    FunctionInterface* pFI_;
    IOSubSystem* pIOSS_;
    Statistics* pSS_;
    ImageDisplayWindow* pDisplayWindow_;
    int lastRequestNr_;
public:
    explicit DeviceData( Device* p ) : ThreadData(), pDev_( p ), pFI_( 0 ), pIOSS_( 0 ), pSS_( 0 ), pDisplayWindow_( 0 ), lastRequestNr_( INVALID_ID ) {}
    ~DeviceData()
    {
        if( pFI_->isRequestNrValid( lastRequestNr_ ) )
        {
            pFI_->imageRequestUnlock( lastRequestNr_ );
            lastRequestNr_ = INVALID_ID;
        }
        delete pFI_;
        delete pSS_;
        delete pIOSS_;
        delete pDisplayWindow_;
    }
    void init( const std::string& windowName )
    {
        pDev_->open();
        pFI_ = new FunctionInterface( pDev_ );
        pSS_ = new Statistics( pDev_ );
        pIOSS_ = new IOSubSystem( pDev_ );
        {
            LockedScope lockedScope( g_critSect );
            cout << "Please note that there will be just one refresh for the display window, so if it is" << endl
                 << "hidden under another window the result will not be visible." << endl;
        }
        pDisplayWindow_ = new ImageDisplayWindow( windowName );
    }
    Device* device( void ) const
    {
        return pDev_;
    }
    FunctionInterface* functionInterface( void ) const
    {
        return pFI_;
    }
    IOSubSystem* IOSS( void ) const
    {
        return pIOSS_;
    }
    Statistics* statistics( void ) const
    {
        return pSS_;
    }
    ImageDisplayWindow* pDisp( void )
    {
        return pDisplayWindow_;
    }
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
unsigned int __stdcall liveThread( void* pData )
//-----------------------------------------------------------------------------
{
    DeviceData* pThreadParameter = reinterpret_cast<DeviceData*>( pData );
    unsigned int cnt = 0;

    ImageDisplay& display = pThreadParameter->pDisp()->GetImageDisplay();
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
        LockedScope lockedScope( g_critSect );
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pThreadParameter->device(), *pFI );
    // run thread loop
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 200;
    int requestNr = INVALID_ID;
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // can't free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
    while( !pThreadParameter->terminated() )
    {
        // wait for results from the default capture queue
        requestNr = pFI->imageRequestWaitFor( timeout_ms );
        if( pFI->isRequestNrValid( requestNr ) )
        {
            pRequest = pFI->getRequest( requestNr );
            if( pRequest->isOK() )
            {
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    LockedScope lockedScope( g_critSect );
                    cout << "Info from " << pThreadParameter->device()->serial.read()
                         << ": " << pSS->framesPerSecond.name() << ": " << pSS->framesPerSecond.readS()
                         << ", " << pSS->errorCount.name() << ": " << pSS->errorCount.readS()
                         << ", " << pRequest->infoFrameNr
                         << ", " << pRequest->infoFrameID
                         << ", " << pSS->frameCount << endl;
                }
                display.SetImage( pRequest );
                display.Update();
            }
            else
            {
                LockedScope lockedScope( g_critSect );
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
            //LockedScope lockedScope( g_critSect );
            //// If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            //// additional information under TDMR_ERROR in the interface reference (
            //cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ", device " << pThreadParameter->device()->serial.read() << ")"
            //  << ", timeout value too small?" << endl;
        }
    }

    {
        LockedScope lockedScope( g_critSect );
        cout << "Overall good frames captured from device " << pThreadParameter->device()->serial.read() << ": " << cnt << endl;
    }
    manuallyStopAcquisitionIfNeeded( pThreadParameter->device(), *pFI );

    // stop the display from showing freed memory
    display.RemoveImage();
    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( pFI->isRequestNrValid( requestNr ) )
    {
        pFI->imageRequestUnlock( requestNr );
    }
    // clear all queues
    pFI->imageRequestReset( 0, 0 );
    return 0;
}

//-----------------------------------------------------------------------------
unsigned int __stdcall triggerThread( void* pData )
//-----------------------------------------------------------------------------
{
    TriggerSignal* pSignal = reinterpret_cast<TriggerSignal*>( pData );
    unsigned int cnt = 0;
    const unsigned int sleepPeriod_ms = 1000 / ( pSignal->frequency_Hz() * 2 );
    while( !pSignal->terminated() )
    {
        // generate a trigger signal
        Sleep( sleepPeriod_ms );
        pSignal->triggerOutput()->flip();
        Sleep( sleepPeriod_ms );
        pSignal->triggerOutput()->flip();
        ++cnt;
        if( cnt % 100 == 0 )
        {
            LockedScope lockedScope( g_critSect );
            cout << "Trigger signals generated: " << cnt << endl;
        }
    }
    LockedScope lockedScope( g_critSect );
    cout << "Overall trigger signals generated: " << cnt << endl;
    return 0;
}

//-----------------------------------------------------------------------------
unsigned int getNumberFromUser( void )
//-----------------------------------------------------------------------------
{
    unsigned int nr = 0;
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
    if( !pMaster )
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
        else if( p )
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
    while( _getch() == 'y' );

    cout << endl;
    vector<DeviceData*> devices;
    devices.push_back( new DeviceData( pMaster ) );
    set<Device*>::const_iterator it = setOfSlaves.begin();
    set<Device*>::const_iterator itEND = setOfSlaves.end();
    while( it != itEND )
    {
        devices.push_back( new DeviceData( *it ) );
        ++it;
    }

    const vector<DeviceData*>::size_type DEV_COUNT = devices.size();
    DigitalOutput* pTriggerOutput = 0;
    try
    {
        for( vector<DeviceData*>::size_type i = 0; i < DEV_COUNT; i++ )
        {
            cout << "Initialising device " << devices[i]->device()->serial.read() << "..." << endl;
            devices[i]->init( ( ( i == 0 ) ? string( "Master " ) : string( "Slave " ) ) + devices[i]->device()->serial.read() );
            cout << endl
                 << "Setup the " << ( ( i == 0 ) ? "MASTER" : "SLAVE" ) << " device:" << endl
                 << "===========================" << endl
                 << endl;
            if( i == 0 )
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
        unsigned int triggerFrequency_Hz = 0;
        bool boRun = true;
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

        HANDLE* pHandles = new HANDLE[DEV_COUNT];
        // start live threads
        for( unsigned int j = 0; j < DEV_COUNT; j++ )
        {
            unsigned int dwThreadID;
            pHandles[j] = ( HANDLE )_beginthreadex( 0, 0, liveThread, ( LPVOID )devices[j], 0, &dwThreadID );
        }

        // now all capture threads will start running...
        // before starting the trigger thread wait a little bit to allow the camera threads to set up completely
        Sleep( 1000 );

        HANDLE hTriggerThread;
        unsigned int dwTriggerThreadID;
        hTriggerThread = ( HANDLE )_beginthreadex( 0, 0, triggerThread, ( LPVOID )&triggerSignal, 0, &dwTriggerThreadID );

        {
            LockedScope lockedScope( g_critSect );
            cout << "Press [ENTER] to end the acquisition" << endl;
        }
        if( _getch() == EOF )
        {
            printf( "Calling '_getch()' did return EOF...\n" );
        }

        // stop all threads again
        {
            LockedScope lockedScope( g_critSect );
            cout << "Terminating threads..." << endl;
        }

        for( unsigned int k = 0; k < DEV_COUNT; k++ )
        {
            devices[k]->terminateThread();
        }
        triggerSignal.terminateThread();

        // wait until each live thread has terminated...
        WaitForMultipleObjects( static_cast<DWORD>( DEV_COUNT ), pHandles, true, INFINITE );
        for( unsigned int l = 0; l < DEV_COUNT; l++ )
        {
            CloseHandle( pHandles[l] );
        }
        // ... and then for the trigger thread to terminate
        WaitForSingleObject( hTriggerThread, INFINITE );
        CloseHandle( hTriggerThread );
        hTriggerThread = 0;
        delete [] pHandles;
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
