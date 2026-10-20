using mv.impact.acquire.examples.helper;
using mv.impact.acquire.GenICam;
using System;
using System.Collections.ObjectModel;

namespace mv.impact.acquire.examples
{
    class DigitalIOs
    {
        static void modifySyncOutput(SyncOutput p)
        {
            FeatureAccess.displayPropertyData(p.frequency_Hz);
            FeatureAccess.modifyPropertyValue(p.frequency_Hz, "", "");
            FeatureAccess.displayPropertyData(p.lowPart_pc);
            FeatureAccess.modifyPropertyValue(p.lowPart_pc, "", "");
        }

        static void displayCommonIOFeatures(IOSubSystem ioss)
        {
            // display available features
            Console.WriteLine("This device has");

            int inputCount = ioss.inputCount;
            Console.WriteLine("  {0} digital input(s)", inputCount);
            // display the state and name of each individual digital input
            for (int i = 0; i < inputCount; i++)
            {
                Console.WriteLine("   [{0}]: {1} (current state: {2})", i, ioss.input(i).description, ioss.input(i).get());
            }
            Console.WriteLine();

            if (inputCount > 0)
            {
                // read the state of all digital inputs in a single function call
                Console.WriteLine("All input registers can be queried with a single function call: Calling 'readInputRegister' returned ");
                Console.WriteLine("{0:X}", ioss.readInputRegister());
                Console.WriteLine();
                Console.WriteLine("From the LSB to the MSB a '1' in this result indicates, that this input is currently connected to a signal");
                Console.WriteLine("that is interpreted as a logical '1'. E.g. 0x13 indicates that inputs 0, 1 and 4 are currently in 'high' state.");
            }

            int outputCount = ioss.outputCount;
            Console.WriteLine("  {0} digital input(s)", outputCount);
            if (outputCount > 0)
            {
                int readOnlyAccessMask = 0;
                bool boRun = true;
                while (boRun)
                {
                    // display the state and name of each individual digital output
                    for (int j = 0; j < outputCount; j++)
                    {
                        DigitalOutput pOutput = ioss.output(j);
                        Console.WriteLine("   [{0}]: {1}(current state: {2}, {3}manually switchable)", j, pOutput.description, pOutput.get(), (pOutput.isWriteable ? "" : "NOT "));
                        if (!pOutput.isWriteable)
                        {
                            readOnlyAccessMask |= 1 << j;
                        }
                    }
                    Console.WriteLine();
                    Console.WriteLine("Enter the number of a digital output followed by [ENTER] to modify its state or 'c' followed by [ENTER] to continue.");
                    string cmd = Console.ReadLine();
                    if (cmd == "c")
                    {
                        boRun = false;
                        continue;
                    }

                    int index = 0;
                    try
                    {
                        index = Convert.ToInt32(cmd);
                    }
                    catch
                    {
                        Console.WriteLine("Invalid input:");
                        continue;
                    }

                    if (index >= outputCount)
                    {
                        Console.WriteLine("Invalid selection");
                        continue;
                    }

                    DigitalOutput Output = ioss.output(index);
                    if (!Output.isWriteable)
                    {
                        Console.WriteLine("{0} is not manually switchable.", Output.description);
                        continue;
                    }
                    Console.WriteLine("Please enter the number in front of the function that shall be called followed by [ENTER]:");
                    Console.WriteLine("  [0]: set");
                    Console.WriteLine("  [1]: reset");
                    Console.WriteLine("  [2]: flip");
                    int newMode = Convert.ToInt32(Console.ReadLine());
                    switch (newMode)
                    {
                        case 0:
                            Output.set();
                            break;
                        case 1:
                            Output.reset();
                            break;
                        case 2:
                            Output.flip();
                            break;
                        default:
                            Console.WriteLine("Invalid selection.");
                            break;
                    }
                }

                // read the state of all digital outputs in a single function call
                Console.WriteLine("All output registers can be queried with a single function call.");
                Console.WriteLine("From the LSB to the MSB a '1' in this result indicates, that this output is currently switched to 'high' or 'active' state");
                Console.WriteLine("E.g. 0x22 indicates that outputs 1 and 5 (zero-based) are currently in 'high' state.");

                uint fullOutputMask = 0;
                if (outputCount > 0)
                {
                    fullOutputMask = (uint)Math.Pow(2, outputCount) - 1;
                }

                boRun = true;
                while (boRun)
                {
                    Console.WriteLine("Calling 'readOutputRegister' returned ");
                    Console.WriteLine("0x{0}", ioss.readOutputRegister().ToString("X2"));
                    Console.WriteLine();
                    Console.WriteLine("Please enter 'y' followed by [ENTER] to modify all digital outputs with a single function");
                    Console.WriteLine("call or anything else followed by [ENTER] to continue.");
                    if (Console.ReadLine() != "y")
                    {
                        boRun = false;
                        continue;
                    }
                    Console.WriteLine("Please enter the bitmask in hex that contains the new values for the digital outputs followed by [ENTER]: ");
                    uint value = UInt32.Parse(Console.ReadLine(), System.Globalization.NumberStyles.HexNumber);
                    if ((value & ~fullOutputMask) != 0)
                    {
                        value &= fullOutputMask;
                        Console.WriteLine("WARNING: More bits than outputs specified. Bitmask truncated to ");
                        Console.WriteLine("0x{0}", value.ToString("X4"));
                        Console.WriteLine();
                    }
                    Console.WriteLine("Please enter the bitmask in hex that contains '1's for outputs that shall be affected by this operation followed by [ENTER]: ");
                    uint mask = UInt32.Parse(Console.ReadLine(), System.Globalization.NumberStyles.HexNumber);
                    if ((readOnlyAccessMask & mask) != 0)
                    {
                        Console.WriteLine("WARNING: At least one selected output is not manually switchable: Mask: ");
                        Console.WriteLine("0x{0}", mask.ToString("X4"));
                        Console.WriteLine(", read-only access mask: ");
                        Console.WriteLine("0x{0}", readOnlyAccessMask.ToString("X4"));
                        Console.WriteLine();
                        Console.WriteLine("No digital outputs have been modified.");
                        continue;
                    }
                    ioss.writeOutputRegister(value, mask);
                }
            }

            Console.WriteLine("This device also has");
            Console.WriteLine("  {0} hardware real-time controller(s).", ioss.RTCtrProgramCount);
            if (ioss.RTCtrProgramCount > 0)
            {
                Console.WriteLine("How to program the HRTC (Hardware RealTime Controller) is not part of this sample, but the manual will contain a separate chapter on this topic.");
            }

            Console.WriteLine("  {0} pulse start configuration(s).", ioss.pulseStartConfigurationCount);
        }

        static void mvGenICamIOAccess(Device pDev)
        {
            try
            {
                DigitalIOControl dioc = new DigitalIOControl(pDev);

                //Count the number of Digital IOs
                int IOCount = dioc.lineSelector.dictSize;
                bool boRun = true;
                while (boRun)
                {
                    Console.WriteLine();
                    Console.WriteLine("       This device has {0} DigitalIOs", IOCount);
                    Console.WriteLine(" ------------------------------------------");
                    // display information about each individual DigitalIO
                    ReadOnlyCollection<long> validLineSelectorValues = dioc.lineSelector.listOfValidValues;
                    foreach (long value in validLineSelectorValues)
                    {
                        dioc.lineSelector.write(value);
                        Console.WriteLine(" IO {0}: \t Type: {1} \t Current state: {2}", value, dioc.lineMode.readS(), Convert.ToBoolean(dioc.lineStatus.read()) ? "ON" : "OFF");
                    }
                    Console.WriteLine(" ------------------------------------------");

                    Console.WriteLine();
                    Console.WriteLine("Please enter a valid line number:");
                    Console.WriteLine("- if it is an output its value will be inverted");
                    Console.WriteLine("- if it is an input its value will be polled continuously for 10 seconds");
                    Console.Write("- or press 'c' to continue:");
                    ConsoleKeyInfo userInput = Console.ReadKey();
                    Console.WriteLine();

                    // get user input
                    if (userInput.Key == ConsoleKey.C)
                    {
                        boRun = false;
                        continue;
                    }
                    //check if key is a digit
                    else if (Char.IsDigit(userInput.KeyChar))
                    {
                        int userInputDigit = Convert.ToInt32((userInput.KeyChar.ToString()));
                        // check if selected digit is a valid IO number
                        if (userInputDigit < IOCount)
                        {
                            //Define the IO we are interested in
                            dioc.lineSelector.write(userInputDigit);
                            //check whether selected IO is an Output or an Input
                            if (dioc.lineMode.readS() == "Output")
                            {
                                if (Convert.ToBoolean(dioc.lineInverter.read()))
                                {
                                    dioc.lineInverter.write(TBoolean.bFalse);
                                }
                                else
                                {
                                    dioc.lineInverter.write(TBoolean.bTrue);
                                }
                            }
                            else if (dioc.lineMode.readS() == "Input")
                            {
                                Console.WriteLine();
                                Console.WriteLine();
                                Console.WriteLine(" ------------------------------------------");
                                Console.WriteLine("           Polling Input '{0}'", dioc.lineSelector.readS());
                                Console.WriteLine(" ------------------------------------------");
                                Console.WriteLine();
                                for (int i = 0; i < 100; i++)
                                {
                                    Console.Write("\r    Value:{0} \t    Remaining Time:{1:F1}", Convert.ToBoolean(dioc.lineStatus.read()) ? "ON" : "OFF", 10.0 - ((float)i / 10));
                                    System.Threading.Thread.Sleep(100);
                                }
                                Console.WriteLine();
                                Console.WriteLine();
                            }
                            else
                            {
                                Console.WriteLine("IO {0} is a '{1}'!", userInputDigit, dioc.lineMode.readS());
                            }
                        }
                        else
                        {
                            Console.WriteLine();
                            Console.WriteLine(" Invalid IO Number! ");
                            Console.WriteLine();
                        }
                    }
                    else
                    {
                        Console.WriteLine();
                        Console.WriteLine(" Invalid Keyboard Input! ");
                        Console.WriteLine();
                    }
                }
            }
            catch (ImpactAcquireException e)
            {
                Console.WriteLine();
                Console.WriteLine(" An mvIMPACT Acquire Exception occurred:{0}", e.errorCodeAsString);
                Console.WriteLine();
            }
        }

        static void mvBlueFOXIOAccess(Device pDev)
        {
            IOSubSystemBlueFOX ioss = new IOSubSystemBlueFOX(pDev);
            displayCommonIOFeatures(ioss);
            if (ioss.digitalInputThreshold.isValid)
            {
                Console.WriteLine("This device also supports the '{0}' property.", ioss.digitalInputThreshold.name);
                FeatureAccess.displayPropertyData(ioss.digitalInputThreshold);
            }
            CameraSettingsBlueFOX cs = new CameraSettingsBlueFOX(pDev);
            Console.WriteLine("To use a digital output in 'expose active' mode, the property '{0}' can be used.", cs.flashMode.name);
            Console.WriteLine("If a delay between switching the output and starting the frame exposure is needed, this can be achieved by ");
            Console.WriteLine("writing to the property '{0}'.", cs.flashToExposeDelay_us.name);
        }

        static void frameGrabberIOAccess(Device pDev)
        {
            IOSubSystemFrameGrabber ioss = new IOSubSystemFrameGrabber(pDev);
            displayCommonIOFeatures(ioss);

            int HDOutputCount = ioss.HDOutputCount;
            int VDOutputCount = ioss.VDOutputCount;
            if ((HDOutputCount > 0) || (VDOutputCount > 0))
            {
                Console.WriteLine("This device also offers");
                bool boRun = true;
                while (boRun)
                {
                    Console.WriteLine("  {0} HD output(s)", HDOutputCount);
                    for (int i = 0; i < HDOutputCount; i++)
                    {
                        SyncOutput p = ioss.HDOutput(i);
                        Console.WriteLine("   [{0}]: {1}({2}, {3})", p.description, p.frequency_Hz, p.lowPart_pc);
                    }
                    Console.WriteLine();

                    Console.WriteLine("  {0} VD output(s)", VDOutputCount);
                    for (int j = 0; j < VDOutputCount; j++)
                    {
                        SyncOutput p = ioss.VDOutput(j);
                        Console.WriteLine("   [{0}]: {1}({2}, {3})", p.description, p.frequency_Hz, p.lowPart_pc);
                    }
                    Console.WriteLine("HD and VD output signals are currently switched to '{0}' mode.", ioss.syncOutputMode.readS());

                    Console.WriteLine("Please enter the number in front of the function that shall be called followed by [ENTER] or 'c' followed by [ENTER] to continue:");
                    Console.WriteLine("  [0]: modify the properties a HD output");
                    Console.WriteLine("  [1]: modify the properties a VD output");
                    Console.WriteLine("  [2]: modify the {0} property.", ioss.syncOutputMode.name);
                    string cmd = Console.ReadLine();
                    if (cmd == "c")
                    {
                        boRun = false;
                        continue;
                    }

                    int index = 0;
                    try
                    {
                        index = Convert.ToInt32(cmd);
                    }
                    catch
                    {
                        Console.WriteLine("Invalid input");
                        continue;
                    }

                    if (index > 2)
                    {
                        Console.WriteLine("Invalid selection");
                        continue;
                    }
                    switch (index)
                    {
                        case 0:
                            {
                                Console.WriteLine("Please enter the index of the HD-output you want to modify followed by [ENTER]: ");
                                cmd = Console.ReadLine();
                                try
                                {
                                    index = Convert.ToInt32(cmd);
                                }
                                catch
                                {
                                    Console.WriteLine("Invalid input");
                                    index = 0;
                                    continue;
                                }

                                if (index >= HDOutputCount)
                                {
                                    Console.WriteLine("Invalid selection");
                                    continue;
                                }
                                modifySyncOutput(ioss.HDOutput(index));
                            }
                            break;
                        case 1:
                            {
                                Console.WriteLine("Please enter the index of the VD-output you want to modify followed by [ENTER]: ");
                                cmd = Console.ReadLine();
                                try
                                {
                                    index = Convert.ToInt32(cmd);
                                }
                                catch
                                {
                                    Console.WriteLine("Invalid input");
                                    index = 0;
                                    continue;
                                }
                                if (index >= VDOutputCount)
                                {
                                    Console.WriteLine("Invalid selection");
                                    continue;
                                }
                                modifySyncOutput(ioss.VDOutput(index));
                            }
                            break;
                        case 2:
                            FeatureAccess.displayPropertyData(ioss.syncOutputMode);
                            FeatureAccess.modifyPropertyValue(ioss.syncOutputMode, "", "");
                            break;
                        default:
                            break;
                    }
                }
            }
            else
            {
                Console.WriteLine("HD and VD output signals are not supported by this device.");
            }

            int outputCount = ioss.outputCount;
            if (outputCount > 0)
            {
                // show how to define and undefine certain output signals for frame grabber devices
                OutputSignalGeneratorFrameGrabber osg = new OutputSignalGeneratorFrameGrabber(pDev);
            }
        }

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

            if (pDev.interfaceLayout.isValid && pDev.interfaceLayout.read() == TDeviceInterfaceLayout.dilGenICam)
            {
                mvGenICamIOAccess(pDev);
            }
            else if (pDev.family.readS() == "mvBlueFOX")
            {
                mvBlueFOXIOAccess(pDev);
            }
            else if (pDev.deviceClass.read() == TDeviceClass.dcFrameGrabber)
            {
                frameGrabberIOAccess(pDev);
            }
            else
            {
                Console.WriteLine("Device {0} ({1}) is not supported by this sample", pDev.serial, pDev.product);
                Console.WriteLine("Press [ENTER] to end the application");
                Console.Read();
                Environment.Exit(1);
            }
            Console.WriteLine();
            Console.WriteLine("Press [ENTER] to end the application");
            Console.Read();
        }
    }
}
