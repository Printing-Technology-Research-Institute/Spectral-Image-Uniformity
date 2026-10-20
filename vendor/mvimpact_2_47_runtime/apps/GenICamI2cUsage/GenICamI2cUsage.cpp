#include <algorithm>
#include <apps/Common/exampleHelper.h>
#include <functional>
#include <memory>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <thread>

using namespace std;
using namespace mvIMPACT::acquire;

static bool s_boTerminated = false;

//-----------------------------------------------------------------------------
int threadFn( Device* pDev )
//-----------------------------------------------------------------------------
{
    // Note: The following I2C addresses are locked and therefore not usable by the I2C Interface:
    //       0x00, 0x20, 0x34, 0x36, 0x3E, 0x48, 0x60, 0x62, 0x64, 0x66, 0x6E, 0x90, 0x92, 0xA0, 0xA2, 0xA4, 0xA6, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBC, 0xBE, 0xF8
    //       I2C address 0x30 and 0x32 are reserved for temperature sensors but are accessible for this i2c example
    //       Do not write to other temperature sensor registers than the one used in this example, otherwise the sensor may not work properly afterwards
    const int i2cDeviceAddress = pDev->manufacturerSpecificInformation.read().find( ";SF3" ) ? 0x32 : 0x30;
    unique_ptr<GenICam::mvI2cInterfaceControl> pI2C;
    try
    {
        // instantiate access to the I2C Interface control
        pI2C = unique_ptr<GenICam::mvI2cInterfaceControl>( new GenICam::mvI2cInterfaceControl( pDev ) );
        // first enable I2C Interface
        pI2C->mvI2cInterfaceEnable.write( bTrue );
        // set I2C speed to 400kBaud
        pI2C->mvI2cInterfaceSpeed.writeS( "kHz_400" );
        // set I2C device address to temperature sensor
        pI2C->mvI2cInterfaceDeviceAddress.write( i2cDeviceAddress );
        // set I2C sub-address to resolution register 8
        pI2C->mvI2cInterfaceDeviceSubAddress.write( 0x08 );
        // write I2C sub-address 8 to sensor and look whether it is accessible
        pI2C->mvI2cInterfaceWrite.call();

        // set temperature resolution register 8 to 1 (resolution 0.25  degrees Celsius)
        // set sub-address to 8
        pI2C->mvI2cInterfaceDeviceSubAddress.write( 0x08 );
        // write value 1 (resolution 0.25  degrees Celsius) to buffer
        const char writeBuf[1] = { 1 };
        pI2C->mvI2cInterfaceBinaryBuffer.writeBinary( string( writeBuf, sizeof( writeBuf ) / sizeof( writeBuf[0] ) ) );
        // send one byte from mvI2cInterfaceBinaryBuffer
        pI2C->mvI2cInterfaceBytesToWrite.write( 1 );
        // write data to i2c device (sum 3 Byte)
        pI2C->mvI2cInterfaceWrite.call();
        // do same as above but faster
        // set mvI2cInterfaceDeviceSubAddress to 8 and use reminder SubAddress Byte to write value 1 (resolution 0.25 degrees Celsius) to sub-address 8 (0x1xxxx means 16-bit sub-address)
        //pI2C->mvI2cInterfaceDeviceSubAddress.write(0x10800); // write value 0 (resolution 0.5 degrees Celsius)
        pI2C->mvI2cInterfaceDeviceSubAddress.write( 0x10801 ); // write value 1 (resolution 0.25 degrees Celsius)
        //pI2C->mvI2cInterfaceDeviceSubAddress.write(0x10802); // write value 2 (resolution 0.125 degrees Celsius)
        //pI2C->mvI2cInterfaceDeviceSubAddress.write(0x10803); // write value 3 (resolution 0.0625  degrees Celsius)
        // do not use mvI2cInterfaceBinaryBuffer
        pI2C->mvI2cInterfaceBytesToWrite.write( 0 );
        // write data to i2c device (sum 3 Byte)
        pI2C->mvI2cInterfaceWrite.call();
    }
    catch( const ImpactAcquireException& e )
    {
        cout << "ERROR! I2C properties which are essential to run this sample are not available for the selected device (error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")), press [ENTER] to end the application..." << endl;
        return 1;
    }

    double lastTemperatureDisplayed = {0.};
    // start temperature read loop
    while( !s_boTerminated )
    {
        // read temperature register from temperature sensor
        // set i2c sub-address to temperature register 5
        pI2C->mvI2cInterfaceDeviceSubAddress.write( 0x05 );
        // set i2c read counter to 2 bytes
        pI2C->mvI2cInterfaceBytesToRead.write( 0x02 );
        // read temperature from sensor
        pI2C->mvI2cInterfaceRead.call();
        // read temperature data from camera
        const string i2cReadBinaryData = pI2C->mvI2cInterfaceBinaryBuffer.readBinary();

        // calculate temperature
        const int temp = ( i2cReadBinaryData[0] * 256 ) + i2cReadBinaryData[1];
        const double currentTemperature = static_cast<double>( ( ( temp << 3 ) & 0xffff ) ) / 128.;
        // print temperature if changed
        if( currentTemperature != lastTemperatureDisplayed )
        {
            cout << "Mainboard Temperature: " << currentTemperature << "\370C" << endl;
            lastTemperatureDisplayed = currentTemperature;
        }
        this_thread::sleep_for( chrono::milliseconds( 1000 ) );
    }
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

    // since there is no possibility to check if the device supports the
    // mvI2cInterfaceControl functionality without opening the device
    // this check has to be done later on
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
    cout << "Press [ENTER] to end the application" << endl;
    s_boTerminated = false;
    thread myThread( threadFn, pDev );
    cin.get();
    s_boTerminated = true;
    myThread.join();
    return 0;
}
