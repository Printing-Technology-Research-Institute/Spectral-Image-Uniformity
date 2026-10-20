using System;

namespace mv.impact.acquire.examples.helper
{
    public static class DeviceAccess
    {
        #region public methods ------------------------------------------------

        //-----------------------------------------------------------------------------
        public static Device getDeviceFromUserInput(bool boAutomaticallyUseGenICamInterface)
        //-----------------------------------------------------------------------------
        {
            int devCnt = DeviceManager.deviceCount;
            if (devCnt == 0)
            {
                Console.WriteLine("No compliant device found!");
                return null;
            }

            // display every device detected
            for (int i = 0; i < devCnt; i++)
            {
                Device pDev = DeviceManager.getDevice(i);
                if (pDev != null)
                {
                    Console.Write("[{0}]: {1} ({2} , family: {3}", i, pDev.serial.read(), pDev.product.read(), pDev.family.read());
                    if (pDev.interfaceLayout.isValid)
                    {
                        if (boAutomaticallyUseGenICamInterface)
                        {
                            // if this device offers the 'GenICam' interface switch it on, as this will
                            // allow are better control over GenICam compliant devices
                            FeatureAccess.conditionalSetProperty(pDev.interfaceLayout, TDeviceInterfaceLayout.dilGenICam, true);
                        }
                        Console.Write(", interface layout: {0}", pDev.interfaceLayout.readS());
                    }
                    if (pDev.acquisitionStartStopBehaviour.isValid)
                    {
                        // if this device offers a user defined acquisition start/stop behaviour
                        // enable it as this allows finer control about the streaming behaviour
                        FeatureAccess.conditionalSetProperty(pDev.acquisitionStartStopBehaviour, TAcquisitionStartStopBehaviour.assbUser, true);
                        Console.Write(", acquisition start/stop behaviour: {0}", pDev.acquisitionStartStopBehaviour);
                    }
                    Console.WriteLine(")");
                }
            }

            // get user input
            Console.Write("Please enter the number in front of the listed device followed by [ENTER] to open it: ");
            int devNr = -1;
            try
            {
                devNr = System.Convert.ToInt32(Console.ReadLine());
            }
            catch (Exception e)
            {
                Console.WriteLine("Invalid selection! Exception message: " + e.Message + "'");
                Console.WriteLine(e.StackTrace);
                return null;
            }

            if (devNr >= devCnt)
            {
                Console.WriteLine("Invalid selection!");
                return null;
            }

            Console.WriteLine("Using device number {0}", devNr);
            return DeviceManager.getDevice(devNr);
        }

        //-----------------------------------------------------------------------------
        public static Device getDeviceFromUserInput()
        //-----------------------------------------------------------------------------
        {
            return getDeviceFromUserInput(true);
        }

        //-----------------------------------------------------------------------------
        public static void manuallyStartAcquisitionIfNeeded(Device pDev, FunctionInterface fi)
        //-----------------------------------------------------------------------------
        {
            if (pDev.acquisitionStartStopBehaviour.read() == TAcquisitionStartStopBehaviour.assbUser)
            {
                TDMR_ERROR result = (TDMR_ERROR)fi.acquisitionStart();
                if (result != TDMR_ERROR.DMR_NO_ERROR)
                {
                    Console.WriteLine("'FunctionInterface.acquisitionStart' returned with an unexpected result: {0} ({1})",
                        result, ImpactAcquireException.getErrorCodeAsString(result));
                }
            }
        }

        //-----------------------------------------------------------------------------
        public static void manuallyStopAcquisitionIfNeeded(Device pDev, FunctionInterface fi)
        //-----------------------------------------------------------------------------
        {
            if (pDev.acquisitionStartStopBehaviour.read() == TAcquisitionStartStopBehaviour.assbUser)
            {
                TDMR_ERROR result = (TDMR_ERROR)fi.acquisitionStop();
                if (result != TDMR_ERROR.DMR_NO_ERROR)
                {
                    Console.WriteLine("'FunctionInterface.acquisitionStop' returned with an unexpected result: {0} ({1})",
                        result, ImpactAcquireException.getErrorCodeAsString(result));
                }
            }
        }

        #endregion
    }
}