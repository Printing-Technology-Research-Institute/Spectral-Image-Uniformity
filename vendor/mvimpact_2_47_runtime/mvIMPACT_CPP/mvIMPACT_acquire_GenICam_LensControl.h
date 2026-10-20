//-----------------------------------------------------------------------------
// (C) Copyright 2005 - 2022 by MATRIX VISION GmbH
//
// This software is provided by MATRIX VISION GmbH "as is"
// and any express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular purpose
// are disclaimed.
//
// In no event shall MATRIX VISION GmbH be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused and
// on any theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use of
// this software, even if advised of the possibility of such damage.

//-----------------------------------------------------------------------------
#ifndef MVIMPACT_ACQUIRE_GENICAM_LENS_CONTROL_H
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define MVIMPACT_ACQUIRE_GENICAM_LENS_CONTROL_H  MVIMPACT_ACQUIRE_GENICAM_LENS_CONTROL_H
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------
#include <mvIMPACT_CPP/mvIMPACT_acquire_GenICam.h>

#ifndef WRAP_ANY
#   include <common/auto_array_ptr.h>
#   include <string>
#   include <chrono>
#   include <thread>
#   include <cmath>
#endif // #ifndef WRAP_ANY

#ifdef _MSC_VER // is Microsoft compiler?
#   pragma warning( push )
#   if _MSC_VER < 1300 // is 'old' VC 6 compiler?
#       pragma warning( disable : 4786 ) // 'identifier was truncated to '255' characters in the debug information'
#       define __FUNCTION__ "No function name information as the __FUNCTION__ macro is not supported by this(VC 6) compiler"
#       pragma message( "WARNING: This header(" __FILE__ ") uses the __FUNCTION__ macro, which is not supported by this compiler. A default definition(\"" __FUNCTION__ "\") will be used!" )
#       pragma message( "WARNING: This header(" __FILE__ ") uses inheritance for exception classes. However this compiler can't handle this correctly. Trying to catch a specific exception by writing a catch block for a base class will not work!" )
#   endif // #if _MSC_VER < 1300
#   pragma warning( disable : 4512 ) // 'assignment operator could not be generated' (reason: assignment operators declared 'private' but not implemented)
#endif // #ifdef _MSC_VER

namespace mvIMPACT
{
namespace acquire
{
namespace GenICam
{
/// \namespace mvIMPACT::acquire::GenICam::labs This namespace contains classes and functions belonging to the GenICam specific part of the image acquisition module of this SDK that are not yet final and might change in the future without further notice. Yet still we think code within this namespace might prove useful thus we encourage you to use it if beneficial for your application. However be prepared to apply changes when needed. Feedback that helps to improve or finalize code from this namespace will be highly appreciated! A migration guide will be provided and we will perform changes only if they make sense and improve the code.
namespace labs
{

//-----------------------------------------------------------------------------
/// \brief Contains convenience functions to control features understood by generic lens adapters.
/**
 * \attention
 * Please note that this class currently is part of the mvIMPACT::acquire::GenICam::labs namespace! This means that this code
 * is not yet final and therefore might change in a future release. Feedback will be highly appreciated
 * and if changes are applied to this code a detailed migration guide can be found in the documentation!
 *
 * \since 2.38.0
 * \ingroup GenICamInterfaceDevice
 */
class LensControlBase
//-----------------------------------------------------------------------------
{
#if !DOXYGEN_SHOULD_SKIP_THIS
protected:
    PropertyIBoolean mvSerialInterfaceEnable_;
    PropertyI64 mvSerialInterfaceASCIIBuffer_;
    PropertyI64 mvSerialInterfaceBaudRate_;
    PropertyI64 mvSerialInterfaceBytesAvailableForRead_;
    PropertyI64 mvSerialInterfaceBytesToRead_;
    PropertyI64 mvSerialInterfaceBytesToWrite_;
    PropertyI64 mvSerialInterfaceMode_;
    PropertyI64 mvSerialInterfaceDataBits_;
    PropertyI64 mvSerialInterfaceStopBits_;
    PropertyI64 mvSerialInterfaceParity_;
    Method      mvSerialInterfaceRead_;
    Method      mvSerialInterfaceWrite_;

    int64_type ASCIIBufferLength_;

    double fStopMax_;
    double fStopMin_;
    unsigned int fStopCount_;
    int focalLength_;
    int focusMin_;
    int focusMax_;
#endif // #if !DOXYGEN_SHOULD_SKIP_THIS

    /// \brief Constructs a new <b>mvIMPACT::acquire::GenICam::labs::LensControlBase</b> object.
    explicit LensControlBase(
        /// [in] A pointer to a <b>mvIMPACT::acquire::Device</b> object obtained from
        /// a <b>mvIMPACT::acquire::DeviceManager</b> object.
        mvIMPACT::acquire::Device* pDev ) : mvSerialInterfaceEnable_(), mvSerialInterfaceASCIIBuffer_(), mvSerialInterfaceBaudRate_(),
        mvSerialInterfaceBytesAvailableForRead_(), mvSerialInterfaceBytesToRead_(), mvSerialInterfaceBytesToWrite_(),
        mvSerialInterfaceMode_(), mvSerialInterfaceDataBits_(), mvSerialInterfaceStopBits_(),
        mvSerialInterfaceParity_(), mvSerialInterfaceRead_(), mvSerialInterfaceWrite_(),
        ASCIIBufferLength_( 32 ), fStopMax_( 0 ), fStopMin_( 0 ), fStopCount_( 0 ), focalLength_( 0 ), focusMin_( 0 ), focusMax_( 0 )
    {
        pDev->validateInterfaceLayout( dilGenICam );
        if( !pDev->isOpen() )
        {
            pDev->open();
        }
        mvIMPACT::acquire::DeviceComponentLocator locator( pDev, dltSetting, "Base" );
        locator.bindComponent( mvSerialInterfaceEnable_, "mvSerialInterfaceEnable" );
        locator.bindComponent( mvSerialInterfaceASCIIBuffer_, "mvSerialInterfaceASCIIBuffer" );
        locator.bindComponent( mvSerialInterfaceBaudRate_, "mvSerialInterfaceBaudRate" );
        locator.bindComponent( mvSerialInterfaceBytesAvailableForRead_, "mvSerialInterfaceBytesAvailableForRead" );
        locator.bindComponent( mvSerialInterfaceBytesToRead_, "mvSerialInterfaceBytesToRead" );
        locator.bindComponent( mvSerialInterfaceBytesToWrite_, "mvSerialInterfaceBytesToWrite" );
        locator.bindComponent( mvSerialInterfaceMode_, "mvSerialInterfaceMode" );
        locator.bindComponent( mvSerialInterfaceDataBits_, "mvSerialInterfaceDataBits" );
        locator.bindComponent( mvSerialInterfaceStopBits_, "mvSerialInterfaceStopBits" );
        locator.bindComponent( mvSerialInterfaceParity_, "mvSerialInterfaceParity" );
        locator.bindComponent( mvSerialInterfaceRead_, "mvSerialInterfaceRead@i" );
        locator.bindComponent( mvSerialInterfaceWrite_, "mvSerialInterfaceWrite@i" );

        if( mvSerialInterfaceEnable_.isValid() &&
            mvSerialInterfaceASCIIBuffer_.isValid() &&
            mvSerialInterfaceBaudRate_.isValid() &&
            mvSerialInterfaceBytesAvailableForRead_.isValid() &&
            mvSerialInterfaceBytesToRead_.isValid() &&
            mvSerialInterfaceBytesToWrite_.isValid() &&
            mvSerialInterfaceMode_.isValid() &&
            mvSerialInterfaceDataBits_.isValid() &&
            mvSerialInterfaceStopBits_.isValid() &&
            mvSerialInterfaceParity_.isValid() &&
            mvSerialInterfaceRead_.isValid() &&
            mvSerialInterfaceWrite_.isValid() )
        {
            mvSerialInterfaceEnable_.write( bFalse );
            mvSerialInterfaceEnable_.write( bTrue );
            mvSerialInterfaceMode_.writeS( "Plain" );
            mvSerialInterfaceBaudRate_.writeS( "Hz_115200" );
            mvSerialInterfaceDataBits_.writeS( "Eight" );
            mvSerialInterfaceStopBits_.writeS( "One" );
            mvSerialInterfaceParity_.writeS( "None" );

            if( mvSerialInterfaceBytesToRead_.hasMaxValue() )
            {
                ASCIIBufferLength_ = mvSerialInterfaceBytesToRead_.getMaxValue();
            }
        }
        else
        {
            ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_FEATURE_NOT_AVAILABLE, "At least on crucial property for accessing the serial interface is missing therefore this class cannot operate correctly!" );
        }
    }
#if !DOXYGEN_SHOULD_SKIP_THIS
    /// \brief Returns the result of a command issued to the lens or the lens adapter.
    /**
     *  \return The string value which has been sent as answer to the issued command.
     */
    std::string getCommandResult( const std::string& cmd, const std::chrono::milliseconds maxPollingTime_ms = std::chrono::milliseconds( 5000 ), const int delimiterCount = 2 ) const
    {
        if( mvSerialInterfaceASCIIBuffer_.isWriteable() && mvSerialInterfaceBytesToWrite_.isWriteable() )
        {
            mvSerialInterfaceASCIIBuffer_.writeS( cmd );
            mvSerialInterfaceBytesToWrite_.write( cmd.length() );
        }
        if( static_cast<TDMR_ERROR>( mvSerialInterfaceWrite_.call() ) != DMR_NO_ERROR )
        {
            ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_EXECUTION_FAILED, "Error sending command " + cmd + " to lens adapter!" );
        }

        const std::chrono::milliseconds pollingInterval_ms = std::chrono::milliseconds( 20 );
        std::chrono::milliseconds pollingTimeExpired_ms( 0 );
        std::string commandResult;

        while( pollingTimeExpired_ms < maxPollingTime_ms )
        {
            int64_type bytesToRead = mvSerialInterfaceBytesAvailableForRead_.read();
            while( bytesToRead > 0 )
            {
                mvSerialInterfaceBytesToRead_.write( ( bytesToRead > ASCIIBufferLength_ ) ? ASCIIBufferLength_ : bytesToRead );
                mvSerialInterfaceRead_.call();
                const std::string resultFragment = mvSerialInterfaceASCIIBuffer_.readS();
                commandResult.append( resultFragment );
                bytesToRead = bytesToRead - resultFragment.length();
            }
            if( resultComplete( commandResult, delimiterCount ) )
            {
                break;
            }
            std::this_thread::sleep_for( pollingInterval_ms );
            pollingTimeExpired_ms += pollingInterval_ms;
        }
        if( pollingTimeExpired_ms >= maxPollingTime_ms )
        {
            ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_EXECUTION_FAILED, "Error! Adapter did not send the full answer in the expected time." );
        }
        return commandResult;
    }
    /// \brief Returns the command string with an appended delimiter.
    /**
     *  \return Both passed strings concatenated together.
     */
    static std::string prepareCommand(
        /// [in] A constant reference to the command string object
        const std::string& cmd,
        /// [in] The delimiter to append to \c cmd.
        const std::string& delimiter )
    {
        return cmd + delimiter;
    }
    /// \brief Splits a string by the separator parameter into the vector provided.
    static void split(
        /// [in] A constant reference to the string, which should be separated
        const std::string& stringToSplit,
        /// [in] A constant reference to the string, which should be used to split the provided string
        const std::string& separator,
        /// [in] A constant reference to the vector of strings, which should be used for the results
        std::vector<std::string>& v )
    {
        std::string::size_type i = 0;
        std::string::size_type j = stringToSplit.find( separator );
        while ( j != std::string::npos )
        {
            v.push_back( stringToSplit.substr( i, j - i ) );
            i = ++j;
            j = stringToSplit.find( separator, j );
            if ( j == std::string::npos )
            {
                if( i != stringToSplit.length() )
                {
                    v.push_back( stringToSplit.substr( i, stringToSplit.length() ) );
                }
            }
        }
    }
    /// \brief Removes unwanted characters from the provided string.
    static std::string removeCharsFromString(
        /// [in] A reference to the string, from which the characters should be removed
        std::string& str,
        /// [in] A constant pointer to the character(s), which should be removed
        const std::string& charsToRemove )
    {
        for( const auto charToRemove : charsToRemove  )
        {
            str.erase( remove( str.begin(), str.end(), charToRemove ), str.end() );
        }
        return str;
    }
    virtual bool resultComplete( const std::string& result, const int delimiterCount ) const = 0;
#endif // #if !DOXYGEN_SHOULD_SKIP_THIS
public:
    virtual ~LensControlBase() {}
};

// ----------------------------------------------------------------------------------
/// \brief Contains convenience functions to control features understood by Birger Engineering, Inc. lens adapters.
/**
 * \attention
 * Please note that this class currently is part of the mvIMPACT::acquire::GenICam::labs namespace! This means that this code
 * is not yet final and therefore might change in a future release. Feedback will be highly appreciated
 * and if changes are applied to this code a detailed migration guide can be found in the documentation!
 *
 * \since 2.38.0
 * \ingroup GenICamInterfaceDevice
 */
class LensControlBirger : public LensControlBase
//-----------------------------------------------------------------------------
{
public:
    /// \brief Constructs a new <b>mvIMPACT::acquire::GenICam::labs::LensControlBirger</b> object.
    explicit LensControlBirger(
        /// [in] A pointer to a \b mvIMPACT::acquire::Device object obtained from a \b mvIMPACT::acquire::DeviceManager object.
        mvIMPACT::acquire::Device* pDev,
        /// [in] The default timeout value in milliseconds after which a command is considered as not successful.
        unsigned int defaultCommunicationTimeout_ms = 5000 ) : LensControlBase( pDev ), delimiter_( "\r" ), okToken_( "OK" ), doneToken_( "DONE" ), defaultCommunicationTimeout_ms_( defaultCommunicationTimeout_ms )
    {
        init( defaultCommunicationTimeout_ms_ );
    }
    /// \brief Returns the serial number of the connected adapter.
    /**
     *  \return The serial number of the attached adapter as string.
     */
    std::string getSerial(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    ) const
    {
        return sendCommandAndGetResult( prepareCommand( "sn", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ) );
    }
    /// \brief Returns the maximum f-stop value of the attached lens.
    /**
     *  \return The maximum f-stop value of the attached lens as double value.
     */
    double getMaxAperture( void ) const
    {
        return fStopMax_;
    }
    /// \brief Returns the minimum f-stop value of the attached lens.
    /**
     *  \return The minimum f-stop value of the attached lens as double value.
     */
    double getMinAperture( void ) const
    {
        return fStopMin_;
    }
    /// \brief Returns the number of possible stops usable to change the opening of the aperture of the attached lens.
    /**
     *  \return The number of possible stops as unsigned int.
     */
    unsigned int getApertureStepCount( void ) const
    {
        return fStopCount_;
    }
    /// \brief Writes the new aperture to the device.
    /**
     *  \return The resulting FStop value the aperture is set to.
     */
    double setAperture(
        /// [in] The new aperture value which should be set in 1/4 stops.
        int steps,
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0 )
    {
        return returnValueToFStopValue( sendCommandAndGetResult( prepareCommand( "ma" + std::to_string( steps ), delimiter_ ), "DONE", getTimeoutToUse( timeout_ms ) ) );
    }
    /// \brief Opens the aperture to the min possible value.
    /**
     *  \return The number of steps (1/4 stops) the aperture has actually moved.
     */
    double setApertureToMin(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        return returnValueToFStopValue( sendCommandAndGetResult( prepareCommand( "mo", delimiter_ ), "DONE", getTimeoutToUse( timeout_ms ) ) );
    }
    /// \brief Closes the aperture to the max possible value.
    /**
     *  \return The number of steps (1/4 stops) the aperture has actually moved.
     */
    double setApertureToMax(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        return returnValueToFStopValue( sendCommandAndGetResult( prepareCommand( "mc", delimiter_ ), "DONE", getTimeoutToUse( timeout_ms ) ) );
    }
    /// \brief Reads the current FStop value from the device.
    /**
     *  \return The resulting f value the aperture is set to.
     */
    double getAperture(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        std::string result = sendCommandAndGetResult( prepareCommand( "pa", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ) );
        std::vector<std::string> seglist = prepareReturnValues( result, "f", "," );
        return returnValueToFStopValue( seglist[1] );
    }
    /// \brief Reads the current focus value from the device.
    /**
     *  \return The value the focus is currently set to.
     */
    int getFocus(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        return stoi( sendCommandAndGetResult( prepareCommand( "pf", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ) ) );
    }
    /// \brief Returns the maximum focus value of the attached lens.
    /**
     *  \return The maximum focus value of the attached lens as unsigned int value.
     */
    int getMaxFocus( void ) const
    {
        return focusMax_;
    }
    /// \brief Returns the minimum focus value of the attached lens.
    /**
     *  \return The minimum focus value of the attached lens as unsigned int value.
     */
    int getMinFocus( void ) const
    {
        return focusMin_;
    }
    /// \brief Moves the focus to the closest position.
    /**
     *  \return The number of steps the focus has moved.
     */
    int setFocusToZero(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        return stoi( sendCommandAndGetResult( prepareCommand( "mz", delimiter_ ), "DONE", getTimeoutToUse( timeout_ms ) ) );
    }
    /// \brief Moves the focus about the provided number of encoder steps.
    /**
     *  \return The number of steps the focus has moved. Might be positive for a movement towards the infinity position or negative for a movement towards the zero position.
     */
    int setFocus(
        /// [in] The value of encoder steps the focus should be moved. Might be positive for a movement towards the infinity position or negative for a movement towards the zero position.
        int steps,
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0 )
    {
        return stoi( sendCommandAndGetResult( prepareCommand( "mf" + std::to_string( steps ), delimiter_ ), "DONE", getTimeoutToUse( timeout_ms ), 2 ) );
    }
    /// \brief Moves the focus to the infinity position.
    /**
     *  \return The number of steps the focus has moved.
     */
    int setFocusToInfinity(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        return stoi( sendCommandAndGetResult( prepareCommand( "mi", delimiter_ ), "DONE", getTimeoutToUse( timeout_ms ) ) );
    }
private:
    std::string delimiter_;
    std::string okToken_;
    std::string doneToken_;

    unsigned int defaultCommunicationTimeout_ms_;

    /// \brief Initializes the Birger Engineering, Inc. Lens adapter and reads out the fix aperture values e.g.: max aperture, min aperture and number of increments between the maximum and the minimum.
    void init(
        /// [in] The timeout value in milliseconds after which the command is considered as not successful. In case of '0' the value of defaultCommunicationTimeout_ms will be used.
        unsigned int timeout_ms = 0
    )
    {
        try
        {
            sendCommandAndGetResult( prepareCommand( "", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ), 2 );
            sendCommandAndGetResult( prepareCommand( "rm1", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ), 2 );
        }
        catch( const ImpactAcquireException& /*e*/ )
        {
            ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_FEATURE_NOT_AVAILABLE, "Could not detect any Birger lens adapter!" );
        }

        if( !isLensPresent() )
        {
            ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_FEATURE_NOT_AVAILABLE, "Could not detect any Birger lens adapter!" );
        }

        sendCommandAndGetResult( prepareCommand( "in", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ) );
        readApertureLimits( focalLength_, fStopMin_, fStopMax_, fStopCount_, ( timeout_ms == 0 ) ? timeout_ms = defaultCommunicationTimeout_ms_ : timeout_ms  );
        sendCommandAndGetResult( prepareCommand( "la0", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ), 3 );
        readFocusLimits( focusMin_, focusMax_, ( timeout_ms == 0 ) ? defaultCommunicationTimeout_ms_ : timeout_ms  );
    }
    std::chrono::milliseconds getTimeoutToUse( int timeout_ms ) const
    {
        return std::chrono::milliseconds( ( timeout_ms == 0 ) ? defaultCommunicationTimeout_ms_ : timeout_ms );
    }
    std::vector<std::string> parseResult( const std::string& resultToParse ) const
    {
        std::vector<std::string> seglist;
        split( resultToParse, delimiter_, seglist );
        return seglist;
    }
    std::vector<std::string> prepareReturnValues( std::string& stringToPrepare, const std::string& charsToRemove, const std::string& separator ) const
    {
        removeCharsFromString( stringToPrepare, charsToRemove );
        std::vector<std::string> segments;
        split( stringToPrepare, separator, segments );
        return segments;
    }
    double returnValueToFStopValue( const std::string& valueToConvert ) const
    {
        return round( stod( valueToConvert ) * 100 ) / 1000;
    }
    virtual bool resultComplete( const std::string& result, const int delimiterCount ) const
    {
        return ( std::count( result.begin(), result.end(), delimiter_.at( 0 ) ) == delimiterCount );
    }
    std::string sendCommandAndGetResult( const std::string& cmd, const std::string& expectedResult, std::chrono::milliseconds timeout_ms, const int delimiterCount = 3 ) const
    {
        std::string result = "";
        std::vector<std::string> resultVector = parseResult( getCommandResult( cmd, timeout_ms, delimiterCount ) );
        size_t okIndex = 0;
        if( resultVector.empty() )
        {
            ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_EXECUTION_FAILED, "No valid answer on command " + cmd + " received!" );
        }
        else
        {
            std::vector<std::string>::iterator it = find( resultVector.begin(), resultVector.end(), "ERR2" );
            if( it != resultVector.end() )
            {
                ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_EXECUTION_FAILED, "Lens is in manual focus mode, therefore focus settings can not be applied!" );
            }

            it = find( resultVector.begin(), resultVector.end(), okToken_ );
            if( it == resultVector.end() )
            {
                ExceptionFactory::raiseException( MVIA_FUNCTION, __LINE__, DMR_EXECUTION_FAILED, "No valid answer on command " + cmd + " received!" );
            }
            else
            {
                if( expectedResult == okToken_ )
                {
                    result = resultVector[resultVector.size() - 1];
                }
                else
                {
                    okIndex = distance( resultVector.begin(), it );
                }

                if( expectedResult == doneToken_ && ( ( resultVector.size() - 1 ) == okIndex ) )
                {
                    result = "0";
                }
                else if( ( expectedResult == doneToken_ ) && ( resultVector.size() >= okIndex ) )
                {
                    const size_t cnt = resultVector.size();
                    for( size_t i = 0; i < cnt; i++ )
                    {
                        if( resultVector[i].find( doneToken_ ) == 0 )
                        {
                            resultVector[i] = resultVector[i].substr( doneToken_.size() );
                            std::vector<std::string> tmpVector;
                            split( resultVector[i], ",", tmpVector );
                            result = tmpVector[0];
                        }
                    }
                }
            }
        }
        return result;
    }
    bool isLensPresent( unsigned int timeout_ms = 0 ) const
    {
        return( sendCommandAndGetResult( prepareCommand( "lp", delimiter_ ), "OK", getTimeoutToUse( timeout_ms ) ) == "1" );
    }
    void readApertureLimits( int& focalLength, double& fStopMin, double& fStopMax, unsigned int& fStopStepCount, unsigned int timeout_ms ) const
    {
        std::string resultToParse = sendCommandAndGetResult( prepareCommand( "ls", delimiter_ ), "OK", std::chrono::milliseconds( timeout_ms ) );
        std::vector<std::string> seglist = prepareReturnValues( resultToParse, "fm:@", "," );

        focalLength = stoi( seglist[0] );
        fStopMin = returnValueToFStopValue( seglist[1] );
        fStopStepCount = stoi( seglist[2] );
        fStopMax = returnValueToFStopValue( seglist[3] );
    }
    /// \brief Reads the current focus value from the device.
    /**
     *  \return The value the focus is currently set to.
     */
    void readFocusLimits( int& focusMin, int& focusMax, unsigned int timeout_ms ) const
    {
        std::string resultToParse = sendCommandAndGetResult( prepareCommand( "fp", delimiter_ ), "OK", std::chrono::milliseconds( timeout_ms ) );
        std::vector<std::string> seglist = prepareReturnValues( resultToParse, "fminaxcuret:", " " );
        focusMin = stoi( seglist[0] );
        focusMax = stoi( seglist[2] );
    }
};

} // namespace labs
} // namespace GenICam
} // namespace acquire
} // namespace mvIMPACT

#endif // #ifndef MVIMPACT_ACQUIRE_GENICAM_LENS_CONTROL_H
