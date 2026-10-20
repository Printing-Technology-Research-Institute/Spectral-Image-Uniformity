using mv.impact.acquire.examples.helper;
using System;
using System.Threading;

namespace mv.impact.acquire.examples
{
    class GenICamI2cUsage
    {
        static void Main(string[] args)
        {
            mv.impact.acquire.LibraryPath.init(); // this will add the folders containing unmanaged libraries to the PATH variable.
            Device pDev = DeviceAccess.getDeviceFromUserInput();
            if (pDev == null)
            {
                Console.WriteLine("Unable to continue! Press any key to end the program.");
                Console.Read();
                Environment.Exit(1);
            }

            Console.WriteLine("Initialising the device. This might take some time...");
            try
            {
                pDev.open();
            }
            catch (ImpactAcquireException e)
            {
                // this e.g. might happen if the same device is already opened in another process...
                Console.WriteLine("An error occurred while opening device " + pDev.serial +
                    "(error code: " + e.Message + "). Press any key to end the application...");
                Console.ReadLine();
                Environment.Exit(1);
            }

            bool terminated = false;
            Console.WriteLine("Press [ENTER] to end the application");
            Thread thread = new Thread(delegate ()
            {
                GenICam.mvI2cInterfaceControl pI2C = null;
                try
                {
                    // Note: The following I2C addresses are locked and therefore not usable by the I2C Interface:
                    //       0x00, 0x20, 0x34, 0x36, 0x3E, 0x48, 0x60, 0x62, 0x64, 0x66, 0x6E, 0x90, 0x92, 0xA0, 0xA2, 0xA4, 0xA6, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBC, 0xBE, 0xF8
                    //       I2C address 0x30 and 0x32 are reserved for temperature sensors but are accessible for this i2c example
                    //       Do not write to other temperature sensor registers than the one used in this example, otherwise the sensor may not work properly afterwards
                    int i2cDeviceAddress = pDev.manufacturerSpecificInformation.read().Contains(";SF3") ? 0x32 : 0x30;

                    // instantiate access to the I2C Interface control
                    pI2C = new GenICam.mvI2cInterfaceControl(pDev);

                    if (!pI2C.mvI2cInterfaceEnable.isValid && !pI2C.mvI2cInterfaceWrite.isValid)
                    {
                        Console.WriteLine("ERROR: I2C properties which are essential to run this sample are not available for the selected device, press return to end the application...");
                        Console.ReadLine();
                        Environment.Exit(1);
                    }
                    // first enable I2C Interface
                    pI2C.mvI2cInterfaceEnable.write(TBoolean.bTrue);
                    // set I2C speed to 400kBaud
                    pI2C.mvI2cInterfaceSpeed.writeS("kHz_400");

                    // set I2C device address to temperature sensor
                    pI2C.mvI2cInterfaceDeviceAddress.write(i2cDeviceAddress);
                    // set I2C sub-address to resolution register 8
                    pI2C.mvI2cInterfaceDeviceSubAddress.write(0x08);
                    // write I2C sub-address 8 to sensor and look whether it is accessible
                    pI2C.mvI2cInterfaceWrite.call();

                    // set temperature resolution register 8 to 1 (resolution 0.25°C)
                    // set sub-address to 8
                    pI2C.mvI2cInterfaceDeviceSubAddress.write(0x08);
                    // write value 1 (resolution 0.25°C) to buffer
                    byte[] writeBuffer = new byte[] { 1 };
                    pI2C.mvI2cInterfaceBinaryBuffer.writeBinary(writeBuffer);
                    // send one byte from mvI2cInterfaceBinaryBuffer
                    pI2C.mvI2cInterfaceBytesToWrite.write(1);
                    // write data to i2c device (sum 3 Byte)
                    pI2C.mvI2cInterfaceWrite.call();

                    // do same as above but faster
                    // set mvI2cInterfaceDeviceSubAddress to 8 and use reminder SubAddress Byte to write value 1 (resolution 0.25°C) to sub-address 8 (0x1xxxx means 16-bit sub-address)
                    //iic.mvI2cInterfaceDeviceSubAddress.write(0x10800); // write value 0 (resolution 0.5°C)
                    pI2C.mvI2cInterfaceDeviceSubAddress.write(0x10801); // write value 1 (resolution 0.25°C)
                    //iic.mvI2cInterfaceDeviceSubAddress.write(0x10802); // write value 2 (resolution 0.125°C)
                    //iic.mvI2cInterfaceDeviceSubAddress.write(0x10803); // write value 3 (resolution 0.0625°C)
                    // do not use mvI2cInterfaceBinaryBuffer
                    pI2C.mvI2cInterfaceBytesToWrite.write(0);
                    // write data to i2c device (sum 3 Byte)
                    pI2C.mvI2cInterfaceWrite.call();
                }
                catch (Exception e)
                {
                    Console.WriteLine("ERROR! I2C properties which are essential to run this sample are not available for the selected device (error {0}), terminating...", e.Message);
                    System.Environment.Exit(1);
                }

                double lastTemperatureDisplayed = 0;
                // start temperature read loop
                while (!terminated)
                {
                    // read temperature register from temperature sensor
                    // set i2c sub-address to temperature register 5
                    pI2C.mvI2cInterfaceDeviceSubAddress.write(0x05);
                    // set i2c read counter to 2 bytes
                    pI2C.mvI2cInterfaceBytesToRead.write(0x02);
                    // read temperature from sensor
                    pI2C.mvI2cInterfaceRead.call();
                    // read temperature data from camera
                    byte[] i2cReadBinaryData = pI2C.mvI2cInterfaceBinaryBuffer.readBinary();
                    // calculate temperature
                    int temp = (i2cReadBinaryData[0] * 256) + i2cReadBinaryData[1];
                    double currentTemperature = (double)(((temp << 3) & 0xffff)) / 128;
                    // print temperature if changed
                    if (currentTemperature != lastTemperatureDisplayed)
                    {
                        Console.WriteLine("Mainboard Temperature {0} °C", currentTemperature);
                        lastTemperatureDisplayed = currentTemperature;
                    }
                    System.Threading.Thread.Sleep(1000);
                }
            });

            thread.Start();
            Console.ReadLine();
            terminated = true;
            thread.Join();
        }
    }
}