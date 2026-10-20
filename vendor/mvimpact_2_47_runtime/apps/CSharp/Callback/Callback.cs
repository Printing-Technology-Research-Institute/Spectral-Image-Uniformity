using mv.impact.acquire;
using mv.impact.acquire.examples.helper;
using System;
using System.Collections.Generic;

namespace Callback
{
    public class VinylRecord
    {
        #region private members -----------------------------------------------

        private string album_;
        private uint timesPlayed_;

        #endregion
        #region public constructors -------------------------------------------

        public VinylRecord(string album)
        {
            album_ = album;
            timesPlayed_ = 0;
        }

        #endregion
        #region public methods ------------------------------------------------

        public bool display()
        {
            Console.WriteLine("This record is called '{0}' and since creation it has been played {1} times.", album, timesPlayed);
            return true;
        }

        public void play()
        {
            timesPlayed_++;
        }

        #endregion
        #region public properties ---------------------------------------------

        public string album { get { return album_; } }
        public uint timesPlayed { get { return timesPlayed_; } }

        #endregion
    }

    public class MyCallback : ComponentCallback
    {
        #region private members -----------------------------------------------

        private uint executeHitCount_;
        private Object pMyUserData_;
        private string name_;

        #endregion
        #region public constructors -------------------------------------------

        public MyCallback(string name, Object pUserData)
            : base(pUserData)
        {
            pMyUserData_ = pUserData;
            name_ = name;
        }

        #endregion
        #region public methods ------------------------------------------------

        // Please carefully read the documentation of the base class (mv.impact.acquire.ComponentCallback) and the documentation of the 
        // mv.impact.acquire.ComponentCallback.execute function in particular to avoid unpleasant surprises!
        public override void execute(Component c, Object pUserData)
        {
            List<VinylRecord> RecordStack = new List<VinylRecord>();
            // get access to the user data previously associated with the component that just executes the callback
            RecordStack = (List<VinylRecord>)pUserData;
            // do some work with the user data object
            int recordCount = RecordStack.Count;
            Console.WriteLine("Component '{0}' did just cause a callback. This function has been called {1} times.", c.name, ++executeHitCount_);
            Console.WriteLine("This callback carries {0} good old vinyl records with it.", recordCount);
            int i = 0;
            foreach (VinylRecord Record in RecordStack)
            {
                Console.Write("  There is a record called '{0}'", Record.album);
                if (executeHitCount_ % recordCount == i)
                {
                    Console.Write(", which I will play now");
                    Record.play();
                }
                Console.WriteLine();
                ++i;
            }
        }

        #endregion
        #region static methods ------------------------------------------------

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
                Console.WriteLine("An error occurred while opening the device " + pDev.serial +
                    "(error code: " + e.Message + "). Press any key to end the application...");
                Console.ReadLine();
                Environment.Exit(1);
            }

            BasicDeviceSettings bds = new BasicDeviceSettings(pDev);
            SystemSettings ss = new SystemSettings(pDev);

            // create some user data to attach to the callback
            List<VinylRecord> vinylRecords = new List<VinylRecord>();
            vinylRecords.Add(new VinylRecord("The number of the Beast"));
            vinylRecords.Add(new VinylRecord("Holy Diver"));
            vinylRecords.Add(new VinylRecord("Master of Reality"));

            // create the callback with user data
            MyCallback myCallback = new MyCallback("", vinylRecords);
            // attach the callback to some features
            myCallback.registerComponent(bds.imageRequestTimeout_ms);
            myCallback.registerComponent(ss.requestCount);

            int vinylRecordCnt = vinylRecords.Count;
            Console.WriteLine("=== List of records BEFORE callbacks ===");
            foreach (VinylRecord record in vinylRecords)
            {
                record.display();
            }
            // provoke some callbacks by modifying the properties we just registered callbacks for.
            // Whenever one of this features is changed in any way the previously attached callback
            // handler will be called.
            for (int i = 1; i < 30; i++)
            {
                bds.imageRequestTimeout_ms.write(i);
                ss.requestCount.write(i);
            }

            // now verify that the callbacks have actually been executed by displaying the times all records have
            // been played within the callback handlers
            Console.WriteLine("=== List of records AFTER callbacks ===");
            foreach (VinylRecord record in vinylRecords)
            {
                record.display();
            }
            // clean up
            myCallback.unregisterComponent(ss.requestCount);
            myCallback.unregisterComponent(bds.imageRequestTimeout_ms);

            Console.WriteLine("Press [ENTER] to end the application");
            Console.Read();
        }

        #endregion
    }
}
