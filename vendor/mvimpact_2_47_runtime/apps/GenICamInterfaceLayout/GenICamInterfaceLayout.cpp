#include <functional>
#include <iostream>
#include <memory>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_helper.h>
#ifdef _WIN32
#   include <conio.h>
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
#else
#   include <stdio.h>
#   include <unistd.h>
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

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
    if( pDev == nullptr )
    {
        cout << "Unable to continue! Press [ENTER] to end the application" << endl;
        cin.get();
        return 1;
    }

    try
    {
        cout << "Initialising the device. This might take some time..." << endl << endl;
        pDev->interfaceLayout.write( dilGenICam ); // This is also done 'silently' by the 'getDeviceFromUserInput' function but your application needs to do this as well so state this here clearly!
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
        // now display some SFNC(Standard Features Naming Convention) compliant features(see http://www.emva.org to find out more
        // about the standard and to download the latest SFNC document version)
        //
        // IMPORTANT:
        //
        // The SFNC unfortunately does NOT define numerical values for enumerations, thus a device independent piece of software
        // should use the enum-strings defined in the SFNC to ensure interoperability between devices. This is slightly slower
        // but should not cause problems in real world applications. When the device type AND GenICam XML file version is
        // guaranteed to be constant for a certain version of software, the driver internal code generator can be used to create
        // and interface header, that has numerical constants for enumerations as well. See device driver documentation under
        // 'Use Cases -> GenICam to mvIMPACT Acquire code generator' for details.
        mvIMPACT::acquire::GenICam::DeviceControl dc( pDev );
        displayPropertyDataWithValidation( dc.deviceVendorName, "DeviceVendorName" );
        cout << endl;
        displayPropertyDataWithValidation( dc.deviceModelName, "DeviceModelName" );
        cout << endl;

        // show the current exposure time allow the user to change it
        mvIMPACT::acquire::GenICam::AcquisitionControl ac( pDev );
        displayAndModifyPropertyDataWithValidation( ac.exposureTime, "ExposureTime" );

        // show the current pixel format, width and height and allow the user to change it
        mvIMPACT::acquire::GenICam::ImageFormatControl ifc( pDev );
        displayAndModifyPropertyDataWithValidation( ifc.pixelFormat, "PixelFormat" );
        displayAndModifyPropertyDataWithValidation( ifc.width, "Width" );
        displayAndModifyPropertyDataWithValidation( ifc.height, "Height" );

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
        cout << "An error occurred while operating the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 1;
    }
    return 0;
}
