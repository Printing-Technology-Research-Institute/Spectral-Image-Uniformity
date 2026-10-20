#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <iomanip>
#include <thread>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam_CustomCommands.h>
#ifdef _WIN32
#   include <conio.h>
#   include <mvDisplay/Include/mvIMPACT_acquire_display.h>
using namespace mvIMPACT::acquire::display;
#   define USE_DISPLAY
enum keys
{
    KEY_MINUS = 45,
    KEY_PLUS = 43,
    KEY_PAGEDOWN = 81,
    KEY_PAGEUP = 73,
    KEY_UP = 72,
    KEY_DOWN = 80,
    KEY_LEFT = 75,
    KEY_RIGHT = 77,
    KEY_SPACE = 32,
    KEY_L = 108,
    KEY_R = 114,
    KEY_ENTER_CR = 13,
    KEY_ENTER_LF = 10,
    KEY_USELESS_ADDITIONAL_KEY_1 = 224,
    KEY_USELESS_ADDITIONAL_KEY_2 = 1234, // does not happen
    KEY_USELESS_ADDITIONAL_KEY_3 = 2345  // does not happen
};
#else // #ifdef _WIN32
#   include <stdio.h>
#   include <termios.h>
#   include <unistd.h>
char _getch( void )
{
    struct termios oldTerminalSettings, newTerminalSettings;
    char ch;
    tcgetattr( 0, &oldTerminalSettings ); /* grab old terminal i/o settings */
    newTerminalSettings = oldTerminalSettings; /* make new settings same as old settings */
    newTerminalSettings.c_lflag &= ~ICANON; /* disable buffered i/o */
    newTerminalSettings.c_lflag &= ~ECHO; /* set no echo mode */
    tcsetattr( 0, TCSANOW, &newTerminalSettings ); /* use these new terminal i/o settings now */
    ch = getchar();
    tcsetattr( 0, TCSANOW, &oldTerminalSettings ); /* reset to old terminal i/o settings */
    return ch;
}
enum keys
{
    KEY_MINUS = 45,
    KEY_PLUS = 43,
    KEY_PAGEDOWN = 54,
    KEY_PAGEUP = 53,
    KEY_UP = 65,
    KEY_DOWN = 66,
    KEY_LEFT = 68,
    KEY_RIGHT = 67,
    KEY_SPACE = 32,
    KEY_L = 108,
    KEY_R = 114,
    KEY_ENTER_CR = 13,
    KEY_ENTER_LF = 10,
    KEY_USELESS_ADDITIONAL_KEY_1 = 27,
    KEY_USELESS_ADDITIONAL_KEY_2 = 91,
    KEY_USELESS_ADDITIONAL_KEY_3 = 126
};
#endif // #ifdef _WIN32

using namespace std;
using namespace mvIMPACT::acquire;

//=============================================================================
//================= Data type definitions =====================================
//=============================================================================
//-----------------------------------------------------------------------------
struct SequencerSetParameter
//-----------------------------------------------------------------------------
{
    const int64_type setNr_;
    const int64_type sequencerSetNext_;
    const int64_type alternativeSetNext_;
    const double exposureTime_us_;
    explicit SequencerSetParameter( const int64_type setNr, const int64_type sequencerSetNext, const int64_type alternativeSetNext, const double exposureTime_us ) :
        setNr_( setNr ), sequencerSetNext_( sequencerSetNext ), alternativeSetNext_( alternativeSetNext ), exposureTime_us_( exposureTime_us )
    {
    }
};

//-----------------------------------------------------------------------------
struct ThreadParameter
//-----------------------------------------------------------------------------
{
    Device* pDev;
    GenICam::AcquisitionControl ac;
    GenICam::ImageFormatControl ifc;
    GenICam::SequencerControl sc;
    GenICam::CustomCommandGenerator ccg;
    GenICam::DigitalIOControl dic;
    int curWidth;
    int curHeight;
    int setWidth;
    int setHeight;
    int maxSensorWidth;
    int maxSensorHeight;
    int minSensorWidth;
    int minSensorHeight;
    int maxAllowedWidth;
    int maxAllowedHeight;
    int minAllowedWidth;
    int minAllowedHeight;
    int widthStepWidth;
    int heightStepWidth;
    int numWidthSteps;
    int numHeightSteps;
    int curStepX;
    int curStepY;
    int setStepX;
    int setStepY;
    int minExpStep;
    int maxExpStep;
    int curStepExposure;
    int setStepExposure;
    bool applyInitialParameters;
    bool runRandomParameters;
    bool runLoopAndUserInputMode;
    bool loopAndUserInputModeRunning;

#ifdef USE_DISPLAY
    ImageDisplayWindow displayWindow;
#endif // #ifdef USE_DISPLAY
    explicit ThreadParameter( Device* p ) : pDev( p ), ac( p ), ifc( p ), sc( p ), ccg( p ), dic( p ), curWidth( 0 ), curHeight( 0 ), setWidth( 0 ), setHeight( 0 ), maxSensorWidth( 0 ), maxSensorHeight( 0 ), minSensorWidth( 0 ), minSensorHeight( 0 ), maxAllowedWidth( 0 ), maxAllowedHeight( 0 ), minAllowedWidth( 0 ), minAllowedHeight( 0 ), widthStepWidth( 0 ), heightStepWidth( 0 ), numWidthSteps( 0 ), numHeightSteps( 0 ), curStepX( 0 ), curStepY( 0 ), setStepX( 0 ), setStepY( 0 ), minExpStep( 0 ), maxExpStep( 0 ), curStepExposure( 0 ), setStepExposure( 0 ), applyInitialParameters( true ), runRandomParameters( true ), runLoopAndUserInputMode( false ), loopAndUserInputModeRunning( false )
#ifdef USE_DISPLAY
        // initialise display window
        // IMPORTANT: It's NOT safe to create multiple display windows in multiple threads!!!
        , displayWindow( "mvIMPACT_acquire sample, Device " + pDev->serial.read() )
#endif // #ifdef USE_DISPLAY
    {
    }
    ThreadParameter( const ThreadParameter& src ) = delete;
    ThreadParameter& operator=( const ThreadParameter& rhs ) = delete;
};

//=============================================================================
//================= static variables ==========================================
//=============================================================================
static bool s_boTerminated = false;
static array<SequencerSetParameter, 6> s_SequencerData =
{
    SequencerSetParameter( 0, 1, 8, 11000. ), // Set 0: Capture 1 frame with an exposure time of 11000 us, then jump to set 1.
    SequencerSetParameter( 1, 2, 8, 12000. ), // Set 1: Capture 1 frame with an exposure time of 12000 us, then jump to set 2.
    SequencerSetParameter( 2, 3, 8, 13000. ), // Set 2: Capture 1 frame with an exposure time of 13000 us, then jump to set 3.
    SequencerSetParameter( 3, 4, 8, 14000. ), // Set 3: Capture 1 frame with an exposure time of 14000 us, then jump to set 4.
    SequencerSetParameter( 4, 5, 8, 15000. ), // Set 4: Capture 1 frame with an exposure time of 15000 us, then jump to set 5.
    SequencerSetParameter( 5, 0, 8, 16000. )  // Set 5: Capture 1 frame with an exposure time of 16000 us, then jump back to set 0.
};
static array<SequencerSetParameter, 1> s_SequencerLoopData =
{
    SequencerSetParameter( 8, 8, 0, 8888. )  // Set 8: Loop with an exposure time of 8888 us.
};

//=============================================================================
//================= function declarations =====================================
//=============================================================================
static void configureDevice( Device* pDev, ThreadParameter* pTP );
static bool isDeviceSupportedBySample( const Device* const pDev );
static void liveThread( ThreadParameter* pTP );

//=============================================================================
//================= implementation ============================================
//=============================================================================
//-----------------------------------------------------------------------------
void printHelp( void )
//-----------------------------------------------------------------------------
{
    cout << "#############################################################################" << endl
         << "#############################################################################" << endl
         << "# This example runs " << s_SequencerData.size() << " SequencerSets, where the ExposureTime varies between" << endl
         << "# the SequencerSets. There are two modes. The example starts in random mode," << endl
         << "# where ExposureTime as well as OffsetX and OffsetY is changed every 100" << endl
         << "# images randomly. The manual mode allows to change these parameters using" << endl
         << "# keys:" << endl
         << "#         [Space]       (re)start manual mode" << endl
         << "#         [l]           (re)start loop mode" << endl
         << "#         [r]           go back to / restart random mode" << endl
         << "#" << endl
         << "#         [Left]        move AOI to the left" << endl
         << "#         [Right]       move AOI to the right" << endl
         << "#         [Up]          move AOI upwards" << endl
         << "#         [Down]        move AOI downwards" << endl
         << "#         [+]           increase AOI size" << endl
         << "#         [-]           decrease AOI size" << endl
         << "#" << endl
         << "#         [PageUp]      increase ExposureTime" << endl
         << "#         [PageDown]    decrease ExposureTime" << endl
         << "#############################################################################" << endl
         << "#############################################################################" << endl
         << "# Press   [h]           to show this help." << endl
         << "#############################################################################" << endl
         << "#############################################################################" << endl
         << "# Press   [ENTER]       to end the application." << endl
         << "#############################################################################" << endl
         << "#############################################################################" << endl;
}

//-----------------------------------------------------------------------------
// Calls the function bound to an mvIMPACT::acquire::Method object and displays
// an error message if the function call did fail.
void checkedMethodCall( Device* pDev, Method& method )
//-----------------------------------------------------------------------------
{
    const TDMR_ERROR result = static_cast<TDMR_ERROR>( method.call() );
    if( result != DMR_NO_ERROR )
    {
        std::cout << "An error was returned while calling function '" << method.displayName() << "' on device " << pDev->serial.read()
                  << "(" << pDev->product.read() << "): " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
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
void configureDevice( Device* pDev, ThreadParameter* pTP )
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
            std::cout << "An error occurred while restoring the factory default for device " << pDev->serial.read()
                      << "(error code: " << ImpactAcquireException::getErrorCodeAsString( result ) << ")." << endl;
        }

        // Auto exposure or an open shutter will not be helpful for this example thus switch it off if possible.
        GenICam::AcquisitionControl acqc( pDev );
        conditionalSetEnumPropertyByString( acqc.exposureMode, "Timed" );
        conditionalSetEnumPropertyByString( acqc.exposureAuto, "Off" );

        // Auto gain will not be helpful for this example either thus switch it off if possible.
        GenICam::AnalogControl ac( pDev );
        if( ac.gainSelector.isValid() )
        {
            // There might be more than a single 'Gain' as a 'GainSelector' is present. Iterate over all
            // 'Gain's that can be configured and switch off every 'Auto' feature detected.
            vector<string> validGainSelectorValues;
            ac.gainSelector.getTranslationDictStrings( validGainSelectorValues );
            for( const auto& gainSelectorValue : validGainSelectorValues )
            {
                conditionalSetEnumPropertyByString( ac.gainSelector, gainSelectorValue );
                conditionalSetEnumPropertyByString( ac.gainAuto, "Off" );
            }
        }
        else
        {
            // There is just a single 'Gain' turn off the 'Auto' feature if supported.
            conditionalSetEnumPropertyByString( ac.gainAuto, "Off" );
        }

        // Chunk mode is needed in order to get back all the information needed to properly check
        // if an image has been taken using the desired parameters.
        GenICam::ChunkDataControl cdc( pDev );
        cdc.chunkModeActive.write( bTrue );

        // We want to act fast, thus if e.g. Bayer-images arrive in the system do NOT convert them on the fly as depending
        // on the device speed the host system might be too slow deal with the amount of data
        ImageProcessing ip( pDev );
        ip.colorProcessing.write( cpmRaw );
        if( ip.tapSortEnable.isValid() )
        {
            ip.tapSortEnable.write( bFalse );
        }

        // Get width and height
        pTP->maxSensorWidth = static_cast<int>( pTP->ifc.widthMax.read() );
        pTP->maxSensorHeight = static_cast<int>( pTP->ifc.heightMax.read() );
        pTP->minSensorWidth = static_cast<int>( pTP->ifc.width.getMinValue() );
        pTP->minSensorHeight = static_cast<int>( pTP->ifc.height.getMinValue() );
        pTP->widthStepWidth = static_cast<int>( pTP->ifc.width.getStepWidth() );
        pTP->heightStepWidth = static_cast<int>( pTP->ifc.height.getStepWidth() );
        pTP->curWidth = pTP->maxAllowedWidth = ( min( ( pTP->maxSensorWidth - pTP->minSensorWidth ) * 2 / 3, 1600 ) / pTP->widthStepWidth ) * pTP->widthStepWidth;
        pTP->curHeight = pTP->maxAllowedHeight = ( min( ( pTP->maxSensorHeight - pTP->minSensorHeight ) * 2 / 3, 1024 ) / pTP->heightStepWidth ) * pTP->heightStepWidth;
        pTP->ifc.width.write( pTP->curWidth );
        pTP->ifc.height.write( pTP->curHeight );
        pTP->minAllowedWidth = ( max( ( pTP->maxSensorWidth - pTP->minSensorWidth ) * 1 / 10, 512 ) / pTP->widthStepWidth ) * pTP->widthStepWidth;
        pTP->minAllowedHeight = ( max( ( pTP->maxSensorHeight - pTP->minSensorHeight ) * 1 / 10, 512 ) / pTP->heightStepWidth ) * pTP->heightStepWidth;
        do
        {
            pTP->numWidthSteps = ( pTP->maxSensorWidth - pTP->curWidth ) / pTP->widthStepWidth;
            if( pTP->numWidthSteps > 100 )
            {
                pTP->widthStepWidth *= 2;
            }
        }
        while( pTP->numWidthSteps > 100 );
        do
        {
            pTP->numHeightSteps = ( pTP->maxSensorHeight - pTP->curHeight ) / pTP->heightStepWidth;
            if( pTP->numHeightSteps > 100 )
            {
                pTP->heightStepWidth *= 2;
            }
        }
        while( pTP->numHeightSteps > 100 );
        pTP->curStepX = pTP->numWidthSteps / 2;
        pTP->curStepY = pTP->numHeightSteps / 2;
        cout << "#############################################################################" << endl
             << "#############################################################################" << endl
             << "Width:  max=" << pTP->maxSensorWidth << "=" << pTP->maxAllowedWidth << " cur=" << pTP->curWidth << " minAllowed=" << pTP->minAllowedWidth << " min=" << pTP->minSensorWidth << " step=" << pTP->widthStepWidth << endl
             << "Height: max=" << pTP->maxSensorHeight << "=" << pTP->maxAllowedHeight << " cur=" << pTP->curHeight << " minAllowed=" << pTP->minAllowedHeight << " min=" << pTP->minSensorHeight << " step=" << pTP->heightStepWidth << endl
             << "numWidthSteps=" << pTP->numWidthSteps << endl
             << "numHeightSteps=" << pTP->numHeightSteps << endl
             << "curStepX=" << pTP->curStepX << endl
             << "curStepY=" << pTP->curStepY << endl
             << "#############################################################################" << endl
             << "#############################################################################" << endl;
        pTP->minExpStep = 1;
        pTP->curStepExposure = 10;
        pTP->maxExpStep = static_cast<int>( ( acqc.exposureTime.getMaxValue() / 1000 ) - 2 );
    }
    catch( const ImpactAcquireException& e )
    {
        // This e.g. might happen if the same device is already opened in another process...
        std::cout << "An error occurred while configuring the device " << pDev->serial.read()
                  << "(error code: " << e.getErrorCodeAsString() << ")." << endl
                  << "Press [ENTER] to end the application..." << endl;
        cin.get();
        exit( 1 );
    }
}

//-----------------------------------------------------------------------------
// Configures a single 'SequencerSet' so that 'X' frames are captured using a
// certain exposure time and afterwards another sets will be used.
void configureSequencerSet( ThreadParameter* pTP, const SequencerSetParameter& ssp )
//-----------------------------------------------------------------------------
{
    pTP->sc.sequencerSetSelector.write( ssp.setNr_ );
    pTP->ac.exposureTime.write( ssp.exposureTime_us_ );
    pTP->sc.sequencerPathSelector.write( 0LL );
    pTP->sc.sequencerTriggerSource.writeS( "ExposureEnd" );
    pTP->sc.sequencerSetNext.write( ssp.sequencerSetNext_ );
    pTP->sc.sequencerPathSelector.write( 1LL );
    pTP->sc.sequencerTriggerSource.writeS( "UserOutput0" );
    pTP->sc.sequencerSetNext.write( ssp.alternativeSetNext_ );
    checkedMethodCall( pTP->pDev, pTP->sc.sequencerSetSave );
}

//-----------------------------------------------------------------------------
// This function will configure the sequencer on the device to take a sequence of
// 'X' images with different exposure times. To change the sequence edit the
// 's_SequencerData' data array and recompile the application.
void configureSequencer( ThreadParameter* pTP )
//-----------------------------------------------------------------------------
{
    try
    {
        pTP->sc.sequencerMode.writeS( "Off" );
        pTP->sc.sequencerConfigurationMode.writeS( "On" );
        pTP->sc.sequencerFeatureSelector.writeS( "ExposureTime" );
        pTP->sc.sequencerFeatureEnable.write( bTrue );
        pTP->sc.sequencerFeatureSelector.writeS( "mvImagePositionAndSize" );
        pTP->sc.sequencerFeatureEnable.write( bTrue );
        for_each( s_SequencerData.begin(), s_SequencerData.end(), [pTP]( const SequencerSetParameter & sequencerSetParameter )
        {
            configureSequencerSet( pTP, sequencerSetParameter );
        } );
        for_each( s_SequencerLoopData.begin(), s_SequencerLoopData.end(), [pTP]( const SequencerSetParameter & sequencerSetParameter )
        {
            configureSequencerSet( pTP, sequencerSetParameter );
        } );
        pTP->sc.sequencerSetStart.write( 0 );
        pTP->sc.sequencerConfigurationMode.writeS( "Off" );
        pTP->sc.sequencerMode.writeS( "On" );
    }
    catch( const ImpactAcquireException& e )
    {
        std::cout << "An error occurred while setting up the sequencer for device " << pTP->pDev->serial.read()
                  << "(error code: " << e.getErrorCodeAsString() << ")." << endl;
        s_boTerminated = true;
    }
}

//-----------------------------------------------------------------------------
// This function will configure the sequencer on the device at runtime without
// stopping the acquisition of the sequencer program. This approach is much faster
// than the usual way.
void configureSequencerAtRuntime( ThreadParameter* pTP )
//-----------------------------------------------------------------------------
{
    try
    {
        if( pTP->runLoopAndUserInputMode == false && pTP->loopAndUserInputModeRunning == true )
        {
            pTP->dic.userOutputSelector.write( 0 );
            pTP->dic.userOutputValue.write( bFalse );
            pTP->dic.userOutputValue.write( bTrue );
            pTP->dic.userOutputValue.write( bFalse );
            pTP->loopAndUserInputModeRunning = false;
        }
        if( pTP->runRandomParameters )
        {
            static bool s_boApplyOriginalData = true;
            if( s_boApplyOriginalData )
            {
                for( size_t i = 0; i < s_SequencerData.size(); i++ )
                {
                    //cout << "Will set the exposure time of sequencer set " << i << " to " << s_SequencerData[i].exposureTime_us_ << "us." << endl;
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspExposureTime, s_SequencerData[i].exposureTime_us_ );
                }
            }
            else
            {
                for( size_t i = 0; i < s_SequencerData.size(); i++ )
                {
                    //cout << "Will set the exposure time of sequencer set " << i << " to " << s_SequencerData[i].exposureTime_us_ * 3 << "us." << endl;
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspExposureTime, s_SequencerData[i].exposureTime_us_ * 3 );
                }
            }
            s_boApplyOriginalData = !s_boApplyOriginalData;

            // Use random sequencer set number
            int sequencerSetNumber = rand() % s_SequencerData.size();

            // Set random OffsetX and OffsetY for partial AOIs.
            int64_t offsetX = rand() % ( pTP->maxSensorWidth - pTP->curWidth );
            offsetX -= offsetX % pTP->ifc.offsetX.getStepWidth();
            //cout << "Will set OffsetX of sequencer set " << sequencerSetNumber << " to " << offsetX << "." << endl;
            pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( sequencerSetNumber ), sspOffsetX, offsetX );
            int64_t offsetY = rand() % ( pTP->maxSensorHeight - pTP->curHeight );
            offsetY -= offsetY % pTP->ifc.offsetY.getStepWidth();
            //cout << "Will set OffsetY of sequencer set " << sequencerSetNumber << " to " << offsetY << "." << endl;
            pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( sequencerSetNumber ), sspOffsetY, offsetY );
        }
        else
        {
            if( pTP->applyInitialParameters )
            {
                if( pTP->runLoopAndUserInputMode == true && pTP->loopAndUserInputModeRunning == false )
                {
                    pTP->dic.userOutputSelector.write( 0 );
                    pTP->dic.userOutputValue.write( bFalse );
                    pTP->dic.userOutputValue.write( bTrue );
                    pTP->dic.userOutputValue.write( bFalse );
                    pTP->loopAndUserInputModeRunning = true;
                }
                pTP->curStepX = pTP->numWidthSteps / 2;
                pTP->curStepY = pTP->numHeightSteps / 2;
                pTP->curStepExposure = static_cast<int>( ( s_SequencerData[0].exposureTime_us_ / 1000 ) );
                pTP->curWidth = ( min( ( pTP->maxSensorWidth - pTP->minSensorWidth ) * 2 / 3, 1600 ) / pTP->widthStepWidth ) * pTP->widthStepWidth;
                pTP->curHeight = ( min( ( pTP->maxSensorHeight - pTP->minSensorHeight ) * 2 / 3, 1024 ) / pTP->heightStepWidth ) * pTP->heightStepWidth;
            }
            int64_t offsetX = pTP->curStepX * pTP->widthStepWidth;
            int64_t offsetY = pTP->curStepY * pTP->heightStepWidth;
            double startExposure = pTP->curStepExposure * 1000.0;
            //cout << "Will set the offsets of all sequencer sets to " << offsetX << "x" << offsetY << " and start exposure to " << startExposure << "us." << endl;
            if( pTP->loopAndUserInputModeRunning )
            {
                pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( s_SequencerLoopData[0].setNr_ ), sspExposureTime, startExposure + 880. );
                pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( s_SequencerLoopData[0].setNr_ ), sspOffsetX, offsetX );
                pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( s_SequencerLoopData[0].setNr_ ), sspOffsetY, offsetY );
                pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( s_SequencerLoopData[0].setNr_ ), sspWidth, static_cast<int64_type>( pTP->curWidth ) );
                pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( s_SequencerLoopData[0].setNr_ ), sspHeight, static_cast<int64_type>( pTP->curHeight ) );
            }
            else
            {
                for( size_t i = 0; i < s_SequencerData.size(); i++ )
                {
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspExposureTime, startExposure + ( i * 1000.0 ) );
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspOffsetX, offsetX );
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspOffsetY, offsetY );
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspWidth, static_cast<int64_type>( pTP->curWidth ) );
                    pTP->ccg.queueSequencerSetValueModification( static_cast<int64_type>( i ), sspHeight, static_cast<int64_type>( pTP->curHeight ) );
                }
            }
        }
        // send out data
        int sendCommandBufferResult = pTP->ccg.sendCommandBuffer();
        if( sendCommandBufferResult != DMR_NO_ERROR )
        {
            cout << "sendCommandBuffer failed(error code: " << sendCommandBufferResult << "(" << ImpactAcquireException::getErrorCodeAsString( sendCommandBufferResult ) << "))!" << endl;
        }
        // reset flags
        pTP->setWidth = pTP->curWidth;
        pTP->setHeight = pTP->curHeight;
        pTP->setStepX = pTP->curStepX;
        pTP->setStepY = pTP->curStepY;
        pTP->setStepExposure = pTP->curStepExposure;
        pTP->applyInitialParameters = false;
    }
    catch( const ImpactAcquireException& e )
    {
        std::cout << "An error occurred while setting up the sequencer for device " << pTP->pDev->serial.read()
                  << "(error code: " << e.getErrorCodeAsString() << ")." << endl;
        s_boTerminated = true;
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
void liveThread( ThreadParameter* pTP )
//-----------------------------------------------------------------------------
{
    pTP->ac.acquisitionFrameRateEnable.write( bTrue );
    pTP->ac.acquisitionFrameRate.write( 10.0 );

    // establish access to the statistic properties
    Statistics statistics( pTP->pDev );
    // create an interface to the device found
    FunctionInterface fi( pTP->pDev );
    configureSequencer( pTP );
    configureSequencerAtRuntime( pTP );

    // Send all requests to the capture queue. There can be more than 1 queue for some devices, but for this sample
    // we will work with the default capture queue. If a device supports more than one capture or result
    // queue, this will be stated in the manual. If nothing is mentioned about it, the device supports one
    // queue only. This loop will send all requests currently available to the driver. To modify the number of requests
    // use the property mvIMPACT::acquire::SystemSettings::requestCount at runtime or the property
    // mvIMPACT::acquire::Device::defaultRequestCount BEFORE opening the device.
    TDMR_ERROR result = DMR_NO_ERROR;
    while( ( result = static_cast<TDMR_ERROR>( fi.imageRequestSingle() ) ) == DMR_NO_ERROR )
    {
    };
    if( result != DEV_NO_FREE_REQUEST_AVAILABLE )
    {
        cout << "'FunctionInterface.imageRequestSingle' returned with an unexpected result: " << result
             << "(" << ImpactAcquireException::getErrorCodeAsString( result ) << ")" << endl;
    }

    manuallyStartAcquisitionIfNeeded( pTP->pDev, fi );
    // run thread loop
    mvIMPACT::acquire::Request* pRequest = nullptr;
    // we always have to keep at least 2 images as the displayWindow module might want to repaint the image, thus we
    // can free it unless we have a assigned the displayWindow to a new buffer.
    mvIMPACT::acquire::Request* pPreviousRequest = nullptr;
    const unsigned int timeout_ms = { 500 };
    unsigned int cnt = { 0 };
    while( !s_boTerminated )
    {
        // wait for results from the default capture queue
        int requestNr = fi.imageRequestWaitFor( timeout_ms );
        pRequest = fi.isRequestNrValid( requestNr ) ? fi.getRequest( requestNr ) : nullptr;
        if( pRequest != nullptr )
        {
            if( pRequest->isOK() )
            {
                // within this scope we have a valid buffer of data that can be an image or any other chunk of data.
                ++cnt;
                // here we can display some statistical information every 100th image
                if( cnt % 100 == 0 )
                {
                    cout << "Info from " << pTP->pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                    configureSequencerAtRuntime( pTP );
                }
                if( pTP->setStepX != pTP->curStepX || pTP->setStepY != pTP->curStepY || pTP->setStepExposure != pTP->curStepExposure || pTP->applyInitialParameters || pTP->setWidth != pTP->curWidth || pTP->setHeight != pTP->curHeight )
                {
                    configureSequencerAtRuntime( pTP );
                }
                cout << "Image #" << setw( 5 ) << pRequest->infoFrameID.read() << " captured" << ": " << setw( 4 ) << pRequest->imageOffsetX.read() << "x" << setw( 4 ) << pRequest->imageOffsetY.read()
                     << "@" << setw( 4 ) << pRequest->imageWidth.read() << "x" << setw( 4 ) << pRequest->imageHeight.read()
                     << " ExposureTime: " << setw( 7 ) << pRequest->chunkExposureTime.read() << " SequencerSetActive: " << setw( 2 ) << pRequest->chunkSequencerSetActive.read() << endl;
#ifdef USE_DISPLAY
                pTP->displayWindow.GetImageDisplay().SetImage( pRequest );
                pTP->displayWindow.GetImageDisplay().Update();
#endif // #ifdef USE_DISPLAY
            }
            if( pPreviousRequest )
            {
                // this image has been displayed thus the buffer is no longer needed...
                pPreviousRequest->unlock();
            }
            pPreviousRequest = pRequest;
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
        // The return code would be -2119(DEV_WAIT_FOR_REQUEST_FAILED) in that case. The documentation will provide
        // additional information under TDMR_ERROR in the interface reference.
        // If waiting with an infinite timeout(-1) it will be necessary to call 'imageRequestReset' from another thread
        // to force 'imageRequestWaitFor' to return when no data is coming from the device/can be captured.
        // cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr ) << ")"
        //   << ", timeout value too small?" << endl;
        //}
    }
    manuallyStopAcquisitionIfNeeded( pTP->pDev, fi );

#ifdef USE_DISPLAY
    // stop the display from showing freed memory
    pTP->displayWindow.GetImageDisplay().RemoveImage();
#endif // #ifdef USE_DISPLAY

    // In this sample all the next lines are redundant as the device driver will be
    // closed now, but in a real world application a thread like this might be started
    // several times an then it becomes crucial to clean up correctly.

    // free the last potentially locked request
    if( pRequest != nullptr )
    {
        pRequest->unlock();
    }
    // clear all queues
    fi.imageRequestReset( 0, 0 );
}

//-----------------------------------------------------------------------------
int main( void )
//-----------------------------------------------------------------------------
{
    DeviceManager devMgr;
    Device* pDev = getDeviceFromUserInput( devMgr, isDeviceSupportedBySample );
    if( pDev == nullptr )
    {
        cout << "Could not obtain a valid pointer to a device. Unable to continue!";
        cout << "Press [ENTER] to end the application" << endl;
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
        ThreadParameter threadParam( pDev );
        configureDevice( pDev, &threadParam );

        // start the execution of the 'live' thread.
        printHelp();
        thread myThread( liveThread, &threadParam );

        // now wait for key strokes to change the sequencer sets
        int c = 0;
        while( c != KEY_ENTER_CR && c != KEY_ENTER_LF )
        {
            c = _getch();
            switch( c )
            {
            case KEY_MINUS:
                if( threadParam.curWidth > threadParam.minAllowedWidth && threadParam.curHeight > threadParam.minAllowedHeight )
                {
                    threadParam.curWidth -= threadParam.widthStepWidth;
                    threadParam.curHeight -= threadParam.heightStepWidth;
                    //cout << "Smaller (" << threadParam.curWidth << "x" << threadParam.curHeight << ")" << endl;
                }
                else
                {
                    cout << "Minimal image size for this test reached!" << endl;
                }
                break;
            case KEY_PLUS:
                if( threadParam.curWidth < threadParam.maxAllowedWidth && threadParam.curHeight < threadParam.maxAllowedHeight )
                {
                    threadParam.curWidth += threadParam.widthStepWidth;
                    threadParam.curHeight += threadParam.heightStepWidth;
                    //cout << "Larger (" << threadParam.curWidth << "x" << threadParam.curHeight << ")" << endl;
                }
                else
                {
                    cout << "Maximal image size for this test reached!" << endl;
                }
                break;
            case KEY_PAGEDOWN:
                if( threadParam.curStepExposure > threadParam.minExpStep + 10 - 1 )
                {
                    threadParam.curStepExposure -= 10;
                    //cout << "Darkererer (" << threadParam.curStepExposure << ")" << endl;
                }
                else if( threadParam.curStepExposure > threadParam.minExpStep )
                {
                    threadParam.curStepExposure = threadParam.minExpStep;
                }
                else
                {
                    cout << "Darkness reached!" << endl;
                }
                break;
            case KEY_PAGEUP:
                if( threadParam.curStepExposure < threadParam.maxExpStep - 10 + 1 )
                {
                    threadParam.curStepExposure += 10;
                    //cout << "Lightererer (" << threadParam.curStepExposure << ")" << endl;
                }
                else if( threadParam.curStepExposure < threadParam.maxExpStep )
                {
                    threadParam.curStepExposure = threadParam.maxExpStep;
                }
                else
                {
                    cout << "Light reached!" << endl;
                }
                break;
            case KEY_UP:
                if( threadParam.curStepY > 1 )
                {
                    threadParam.curStepY--;
                    //cout << "Up (" << threadParam.curStepY << ")" << endl;
                }
                else
                {
                    cout << "Top reached!" << endl;
                }
                break;
            case KEY_DOWN:
                if( threadParam.curStepY < threadParam.numHeightSteps - 1 )
                {
                    threadParam.curStepY++;
                    //cout << "Down (" << threadParam.curStepY << ")" << endl;
                }
                else
                {
                    cout << "Bottom reached!" << endl;
                }
                break;
            case KEY_LEFT:
                if( threadParam.curStepX > 1 )
                {
                    threadParam.curStepX--;
                    //cout << "Left (" << threadParam.curStepX << ")" << endl;
                }
                else
                {
                    cout << "Left - End reached!" << endl;
                }
                break;
            case KEY_RIGHT:
                if( threadParam.curStepX < threadParam.numWidthSteps - 1 )
                {
                    threadParam.curStepX++;
                    //cout << "Right (" << threadParam.curStepX << ")" << endl;
                }
                else
                {
                    cout << "Right - End reached!" << endl;
                }
                break;
            case KEY_SPACE:
                cout << "Resetting..." << endl;
                threadParam.runRandomParameters = false;
                threadParam.runLoopAndUserInputMode = false;
                threadParam.applyInitialParameters = true;
                break;
            case KEY_L:
                cout << "Looping..." << endl;
                threadParam.runRandomParameters = false;
                threadParam.runLoopAndUserInputMode = true;
                threadParam.applyInitialParameters = true; // as trigger
                break;
            case KEY_R:
                cout << "Random..." << endl;
                threadParam.runRandomParameters = true;
                threadParam.runLoopAndUserInputMode = false;
                threadParam.applyInitialParameters = true; // as trigger
                break;
            case KEY_ENTER_CR:
            case KEY_ENTER_LF:
                cout << "Ending..." << endl;
                break;
            case KEY_USELESS_ADDITIONAL_KEY_1:
            case KEY_USELESS_ADDITIONAL_KEY_2:
            case KEY_USELESS_ADDITIONAL_KEY_3:
                // for some of the keys these values are entered additionally and must be ignored
                break;
            default:
                cout << "\n\n\n" << endl;
                printHelp();
                cout << endl << "unknown: -" << c << "-" << endl << endl << endl;
                break;
            }
        }
        s_boTerminated = true;
        myThread.join();
    }
    catch( const ImpactAcquireException& e )
    {
        // this e.g. might happen if the same device is already opened in another process...
        cout << "An error occurred while setting up device " << pDev->serial.read()
             << "(error code: " << e.getErrorCodeAsString() << ")." << endl
             << "Press [ENTER] to end the application..." << endl;
        cin.get();
        return 1;
    }
    return 0;
}
