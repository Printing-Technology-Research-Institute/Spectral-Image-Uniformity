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
#ifndef mvDeviceManagerH
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define mvDeviceManagerH mvDeviceManagerH
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------
#ifndef WRAP_ANY
#   include <stddef.h>
#endif // #ifndef WRAP_ANY

#include <common/mvImageBuffer.h>
#include <DriverBase/Include/mvDriverBaseEnums.h>

#ifdef __cplusplus
extern "C" {
#endif // #ifdef __cplusplus

#if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
namespace mvIMPACT
{
namespace acquire
{
#endif // #if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifdef DOXYGEN_SHOULD_SKIP_THIS
#   define __vl_base_h__ // force doxygen to include mvIMPACT related functions
#else
#   ifdef _WIN32
#       define DMR_CALL __stdcall
#       ifndef WRAP_ANY
#           define MVDMR_API
#           if defined(__BORLANDC__) // is Borland compiler?
#               pragma option push -b // force enums to size of integer
#           endif // #if defined(__BORLANDC__)
#           ifndef NO_MV_DEVICE_MANAGER_AUTOLINK
#               if defined(__BORLANDC__) // is Borland compiler?
#                   if __CODEGEARC__ >= 0X710
#                       pragma comment(lib,"mvDeviceManagerb")
#                   else
#                       pragma comment(lib,"mvDeviceManagerb.lib")
#                   endif // #if __CODEGEARC__ >= 0X710
#                   pragma message( "Automatically linking with mvDeviceManagerb.lib" )
#               elif defined(_MSC_VER) // is Microsoft compiler?
#                   pragma comment(lib,"mvDeviceManager.lib")
#                   pragma message( "Automatically linking with mvDeviceManager.lib" )
#               endif // #if defined(__BORLANDC__)
#           endif // #ifndef NO_MV_DEVICE_MANAGER_AUTOLINK
#       endif // #ifndef WRAP_ANY
#   else // #ifdef _WIN32
#       ifndef WRAP_ANY
#           if __GNUC__ >= 4
#               define MVDMR_API __attribute__ ((visibility ("default")))
#           else
#               define MVDMR_API
#           endif // #if __GNUC__ >= 4
#       endif // ifndef WRAP_ANY
#       define DMR_CALL
#   endif // #ifdef _WIN32
#endif // #ifdef DOXYGEN_SHOULD_SKIP_THIS

#ifdef __GNUC__
#   define ATTR_PACK __attribute__((packed)) __attribute__ ((aligned (8)))
#else
#define ATTR_PACK
#   pragma pack(push, 8) // 8 byte structure alignment
#endif // #ifdef __GNUC__

#if !defined(MVIMPACT_DEPRECATED_C) && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#   if !defined(MVIMPACT_ACQUIRE_H_) && !defined(NO_MVIMPACT_DEPRECATED_C_WARNINGS)
#       if defined(__GNUC__) && (((__GNUC__ * 100) + __GNUC_MINOR__ ) >= 301) // is at least GCC 3.1 compiler?
#           define MVIMPACT_DEPRECATED_C(FUNCTION) FUNCTION __attribute__ ((deprecated))
#       elif defined(_MSC_VER) && (_MSC_VER >= 1310) // is at least VC 2003 compiler?
#           define MVIMPACT_DEPRECATED_C(FUNCTION) __declspec(deprecated) FUNCTION
#       else
#           define MVIMPACT_DEPRECATED_C(FUNCTION) FUNCTION
#       endif // compiler check
#   else
#       define MVIMPACT_DEPRECATED_C(FUNCTION) FUNCTION
#   endif // #if !defined(MVIMPACT_ACQUIRE_H_) && !defined(NO_MVIMPACT_DEPRECATED_C_WARNINGS)
#endif // #if !defined(MVIMPACT_DEPRECATED_C) && !defined(DOXYGEN_SHOULD_SKIP_THIS)

#if !defined(DOXYGEN_CPP_DOCUMENTATION) && !defined(WRAP_ANY)
//-----------------------------------------------------------------------------
/// \brief This structure contains information about the image currently associated with the request object.
/// \ingroup CommonInterface
struct RequestInfo
//-----------------------------------------------------------------------------
{
    /// \brief A unique frame identifier.
    /**
     *  This parameter is returned as part of the each request object. It can be
     *  used to associate a certain image with a unique identifier.
     *  \if DOXYGEN_CPP_DOCUMENTATION
     *  When an <b>mvIMPACT::acquire::RTCtrProgramStep</b> instruction of the type <b>mvIMPACT::acquire::rtctrlProgTriggerSet</b>
     *  is executed, the \a frameID is set to the value of the property \a FrameID of a corresponding <b>mvIMPACT::acquire::RTCtrProgramStep</b> if the
     *  HRTC is used and any program step executes the setting of the ID.
     *  \endif
     *
     *  \note For some devices this property will only contain meaningful data if the device supports <b>HRTC</b> and a program is running and writing data to
     *  the property.
     *
     *  \attention
     *  with version 2.4.0 this feature internally became a 64-bit value as certain supported standards (e.g. GigE Vision 2.x) using 64-bit identifiers for
     *  tagging blocks of data. Thus in order not to get truncated data the use of this parameter
     *  from this structure is discouraged. Use direct access functions to the \a FrameID property of each request instead! How to achieve this can e.g.
     *  be seen in the source code of the function \c getRequestProp of \c exampleHelper_C.c. In this case \c FrameID should be passed as \c pPropName.
     *  Not doing so might result in wraparounds back to 0 after 2^32 blocks of data have been captured even when the internal accurate value would be
     *  0x0000000100000000.
     */
    int frameID;
    /// \brief The number of images captured since the driver has been initialised including the current image.
    /**
     *  \attention
     *  with version 2.4.0 this feature internally became a 64-bit value as certain supported standards (e.g. GigE Vision 2.x) using 64-bit identifiers for
     *  tagging blocks of data. Thus in order not to get truncated data the use of this parameter
     *  from this structure is discouraged. Use direct access functions to the \a FrameNr property of each request instead! How to achieve this can e.g.
     *  be seen in the source code of the function \c getRequestProp of \c exampleHelper_C.c. In this case \c FrameNr should be passed as \c pPropName.
     *  Not doing so might result in wraparounds back to 0 after 2^32 blocks of data have been captured even when the internal accurate value would be
     *  0x0000000100000000.
     */
    int frameNr;
    /// \brief A timestamp (in us) defining the time the device started the exposure of the image associated with this request object.
    /**
     *  This value will stay 0 if nothing is known about the time the exposure did start.
     *  In such a case the timeStamp_us parameter should be used instead.
     */
    int exposeStart_us;
    /// \brief The 'real' expose time (in us) used to generate this image.
    /**
     *  This might differ slightly from the value selected by the user via
     *  the corresponding exposure property depending on the precision
     *  available for the device or the connected camera.
     */
    int exposeTime_us;
    /// \brief The time the transaction of this image has been delayed (in us) because either the bus was blocked or the CPU was busy.
    /**
     *  Normally this value will be 0. A value larger than 0 indicates that the system
     *  can't manage the current load.
     *
     *  \note This property is not supported by every device.
     */
    int transferDelay_us;
    /// \brief The gain(in dB) this image has been taken with.
    double gain_dB;
    /// \brief A timestamp to define the exact time this image has been captured (usually either at exposure start or exposure end, depending on the device).
    /**
     *  The timestamp is independent from the FPGA and has a resolution of 1 us.
     *
     *  \attention
     *  with version 1.9.7 this feature internally became a 64-bit value thus in order not to get truncated data the use of this parameter
     *  from this structure is discouraged. Use direct access functions to the \a TimeStamp_us property of each request instead!
     *
     *  \b mvBlueFOX \b specific:
     *  The counter of the timestamp starts when the camera gets initialized. It is measured in us.
     */
    int timeStamp_us;
    /// \brief Currently unsupported.
    double imageAverage;
    /// \brief The video input channel of the device this image has been acquired from.
    /**
     *  \note This property is not supported by every device. Devices with only a
     *  single video channel will always leave this value at 0.
     */
    int videoChannel;
    /// \brief The camera output used to transmit the image to the capture device.
    /**
     *  \note This property is not supported by every device. It will contain valid data for
     *  capture devices with at least one physical video input channel.
     */
    TCameraOutput cameraOutputUsed;
    /// \brief Line number since last trigger event.
    /**
     *  Will contain
     *  - the line number since last trigger event of the first line of the snap if line counting is enabled
     *  - -1 otherwise
     *
     *  \note This property is not supported by every device.
     */
    int lineCounter;
    /// \brief The amount of data missing in the current image.
    /**
     *  The value of this property will be 0 almost always. However if a device
     *  can detect blocks of missing data and an image request has returned with
     *  \if DOXYGEN_CPP_DOCUMENTATION
     *  <b>mvIMPACT::acquire::rrFrameIncomplete</b>
     *  \else
     *  ::rrFrameIncomplete
     *  \endif
     *  to indicate that not all the data has been captured, this property will contain the amount of data missing in percent.
     */
    double missingData_pc;
} ATTR_PACK;

//-----------------------------------------------------------------------------
/// \brief Contains status information about the capture process.
/**
 *  This part of a complete request contains general status information about the
 *  request or the image currently referenced by it.
 *
 * \ingroup CommonInterface
 */
struct RequestResult
//-----------------------------------------------------------------------------
{
    /// \brief The result of this request.
    /**
     *  This parameter indicates whether a previous image acquisition has been
     *  successful or not.
     */
    TRequestResult result;
    /// \brief The current state of this request.
    /**
     *  This parameter indicates the current state of this request. A request e.g.
     *  can currently be idle. This would mean, that it is currently not used for
     *  image acquisition. Also a request can be in 'Capturing' state, which means
     *  it is currently processed by the driver.
     */
    TRequestState state;
} ATTR_PACK;
#endif // #if !defined(DOXYGEN_CPP_DOCUMENTATION) && !defined(WRAP_ANY)

//-----------------------------------------------------------------------------
/// \brief Defines valid image request parameters.
/**
 *  Some functions accept this type as the input for certain parameter related
 *  functions such as obtaining a string representation of the parameter specified.
 *
 * \ingroup CommonInterface
 */
enum TImageRequestParam
//-----------------------------------------------------------------------------
{
    /// \brief The pixel format of an image request buffer.
    irpPixelFormat = 0,
    /// \brief The Result associated with an image request.
    irpResult = 1,
    /// \brief The current state of an image request.
    irpState = 2,
    /// \brief The camera output used to transmit the image from the imaging device to the capture device.
    irpCameraOutputUsed = 3
};

//-----------------------------------------------------------------------------
/// \brief Flags to define the way an <b>mvIMPACT</b> buffer is created and handled.
/// \ingroup CommonInterface
enum TImpactBufferFlag
//-----------------------------------------------------------------------------
{
    /// \brief A dummy constant to state that none of the flags shall be specified.
    /**
     *  This flag can be used instead of writing code like this: TImpactBufferFlag(0) or
     *  static_cast<TImpactBufferFlag>(0).
     */
    ibfNone = 0x0,
    /// \brief If set no new memory will be allocated for the creation of the <b>mvIMPACT</b> buffer.
    /**
     *  This way of creating the images is fast, but modifying the image data with an image processing
     *  function will always modify the image data associated with the underlying request object.
     *
     *  \note
     *  Once the underlying request object has been unlocked, working with the image is no longer
     *  save when this flag was set during creation of the <b>mvIMPACT</b> buffer, as the memory might
     *  be freed by the driver. If you want to keep the created image do <b>NOT</b> specify this flag
     *  during creation.
     *
     *  \note
     *  Whenever a new image is acquired from a device the device might be using the memory already associated
     *  with another image thus you might end up with to IMPACT images that internally reference the
     *  same buffer. However a large DMA memory will (at least twice the size of one image) will allow
     *  to work with a double buffering scheme.
     */
    ibfUseRequestMemory = 0x1,
    /// \brief If an existing \b IPL_BUFHANDLE is passed to a function it will try to copy data in this buffer instead of freeing it.
    /**
     *  This flag can be used to allow the continuous usage of the same mvIMPACT buffer. If this
     *  flag is NOT specified whenever a valid mvIMPACT buffer handle is passed to a function
     *  accepting this type of flags it might free the existing buffer and create a new one.
     *
     *  If this flag is specified and the new buffer doesn't match the existing one in terms of
     *  the number of bands, size, etc. the function will fail and return an error code. Thus this
     *  flag can be used to optimize performance if the buffer layout will remain constant during
     *  application runtime.
     */
    ibfRecycleBufHandle = 0x2
};

//-----------------------------------------------------------------------------
/// \brief Defines valid libraries to query information from.
/// \ingroup CommonInterface
enum TLibraryQuery
//-----------------------------------------------------------------------------
{
    /// \brief Specifies the mvDeviceManager library.
    lqDeviceManager = 0,
    /// \brief Specifies the mvPropHandling library.
    lqPropHandling = 1
};

/// \brief A type for handles bound to a certain device.
/**
 * As 'C' is not a strongly typed language this simply adds an alias for \c int.
 * Internally an \c int with a unique value is used to identify properties, methods and lists. The type
 * HDEV therefore is nothing special. It is an integer value identifying the list containing the
 * device properties available before opening the device. A HDEV therefore can also be passed to all
 * functions expecting a HLIST or HOBJ type.
 *
 * \attention Functions expecting a HDRV will fail when fed with a HDEV as these functions obviously
 * expect a different feature list!
 */
typedef int HDEV;
/// \brief A type for handles bound to an interface of a certain device.
/**
 * As 'C' is not a strongly typed language this simply adds an alias for \c int.
 * Internally an \c int with a unique value is used to identify properties, methods and lists. The type
 * HDRV therefore is nothing special. It is an integer value identifying the list containing the
 * device properties becoming available after opening the device. A HDRV therefore can also be passed to all
 * functions expecting a HLIST or HOBJ type.
 *
 * \attention Functions expecting a HDEV will fail when fed with a HDRV as these functions obviously
 * expect a different feature list!
 */
typedef int HDRV;
/// \brief A type for handles bound to an object list.
/**
 * As 'C' is not a strongly typed language this simply adds an alias for \c int.
 * Internally an \c int with a unique value is used to identify properties, methods and lists. The type
 * HDEV therefore is nothing special. It is an integer value identifying a list of features. A HLIST therefore
 * can also be passed to all functions expecting a HOBJ, HDEV(when it is the correct list) or HDRV(when it is the correct list).
 *
 * \attention Functions expecting a HDRV or HDEV will fail when fed with a HLIST identifier not referencing the correct feature list!
 */
typedef int HLIST;
/// \brief A type for handles bound to an unspecified object.
/**
 * As 'C' is not a strongly typed language this simply adds an alias for \c int.
 * Internally an \c int with a unique value is used to identify properties, methods and lists. The type
 * HOBJ therefore is nothing special. It is an integer value identifying a feature (either a list of features, a property or a method).
 * A HOBJ therefore can also be passed to all functions expecting a HLIST(when referencing a list), HDEV(when it is the correct list)
 * or HDRV(when it is the correct list).
 *
 * \attention Functions expecting a HDRV or HDEV will fail when fed with a HOBJ identifier not referencing the correct feature list
 * as will a function expecting a certain feature type while the HOBJ references another or no valid feature at all!
 */
typedef int HOBJ;

/// \brief A type for handles bound to a video stream.
/**
 * As 'C' is not a strongly typed language this simply adds an alias for \c void*.
 * Internally this pointer however will be checked for validity in various ways.
 */
typedef void* HDMR_VIDEO_STREAM;

#ifndef DOXYGEN_CPP_DOCUMENTATION

/// \brief a handle to the device manager bound to the list of all devices.
typedef int HDMR;

#ifndef WRAP_ANY

typedef struct EventData EventData;
typedef struct ImageBuffer ImageBuffer;
typedef struct RequestInfo RequestInfo;
typedef struct RequestResult RequestResult;
typedef enum TImageRequestParam TImageRequestParam;
typedef enum TImpactBufferFlag TImpactBufferFlag;
typedef enum TLibraryQuery TLibraryQuery;

#ifdef __cplusplus
/// \brief A constant defining the maximum length of the strings in the <b>TDMR_DeviceInfo</b> structure.
/**
 *  \note
 *  This is used if <b>__cplusplus</b> is defined.
 */
const size_t INFO_STRING_SIZE = 38;
#else // C compatible set of constants
/// \brief A macro defining the maximum length of the strings in the <b>TDMR_DeviceInfo</b> structure.
/**
 *  \note
 *  This is used if <b>__cplusplus</b> is NOT defined.
 */
#define INFO_STRING_SIZE (38)
#endif // #ifdef __cplusplus

//-----------------------------------------------------------------------------
/// \brief Valid handle check modes.
enum TOBJ_HandleCheckMode // no_managed_type
//-----------------------------------------------------------------------------
{
    /// \brief Only the owner list of the current HOBJ is checked.
    hcmOwnerList    = 1,
    /// \brief The owner list and the object referenced by the HOBJ parameter is checked.
    hcmFull         = 2
};

//-----------------------------------------------------------------------------
/// \brief Valid string query types.
/**
 *  Certain information for an object can be queried as a string with direct
 *  construction.
 */
enum TOBJ_StringQuery // no_managed_type
//-----------------------------------------------------------------------------
{
    /// \brief The name of the object referenced by HOBJ.
    sqObjName               = 0,
    /// \brief The documentation string of the object referenced by HOBJ.
    sqObjDocString          = 1,
    /// \brief The content descriptor of the object referenced by HOBJ.
    /**
     *  \note
     *  This value is only defined if the referenced object is of type <b>::ctList</b>.
     */
    sqListContentDescriptor = 2,
    /// \brief The value of the object referenced by HOBJ.
    /**
     *  \note
     *  This value is only defined if the referenced object is of type <b>::ctProp</b>.
     */
    sqPropVal               = 3,
    /// \brief The format string of the object referenced by HOBJ.
    /**
     *  \note
     *  This value is only defined if the referenced object is of type <b>::ctProp</b>.
     */
    sqPropFormatString      = 4,
    /// \brief The parameter list of the object referenced by HOBJ.
    /**
     *  \note
     *  This value is only defined if the referenced object is of type <b>::ctMeth</b>.
     *
     *  The returned string will contain characters giving information about the parameters expected by
     *  the method object.
     *
     *  The characters have the following meaning:
     *  - i specifies a 32-bit integer value
     *  - I specifies a 64-bit integer value
     *  - s specifies a pointer to a C-string
     *  - f specifies a double precision float value
     *  - v specifies a void return value
     *
     *  Examples:
     *
     *  - 'v': This is a function returning nothing (void). It expects no parameters.
     *  - 'viis': This is a function returning nothing (void). It expects 2 integer values
     *  and one pointer to a C-string.
     *  - 'if': This function returns an integer value and expects a float value.
     */
    sqMethParamString       = 5,
    /// \brief The display name of the object referenced by HOBJ.
    /**
     *
     * \since 1.11.20
     *
     */
    sqObjDisplayName        = 6
};

//-----------------------------------------------------------------------------
/// \brief A structure for device specific information.
/**
 *  \sa
 *  <b>DMR_GetDeviceInfo()</b>
 */
struct TDMR_DeviceInfo // no_managed_type
//-----------------------------------------------------------------------------
{
    /// \brief A string representation of the serial number of this device.
    char serial[INFO_STRING_SIZE];
    /// \brief A string representation of the family this device belongs to.
    char family[INFO_STRING_SIZE];
    /// \brief A string representation of the product name of this device.
    char product[INFO_STRING_SIZE];
    /// \brief An integer value containing the current firmware version this device is programmed with.
    int firmwareVersion;
    /// \brief The device ID this device has been associated with.
    int deviceId;
} ATTR_PACK;

//-----------------------------------------------------------------------------
/// \brief Valid search modes for the function \b DMR_GetDevice() when searching for a certain device.
/**
 *  \note
 *  <b>::dmdsmUseDevID</b> can be 'ored' (|) together with
 *  all the other modes.
 */
enum TDMR_DeviceSearchMode // no_managed_type
//-----------------------------------------------------------------------------
{
    /// \brief Searches for a device with a certain serial number.
    dmdsmSerial     = 1,
    /// \brief Searches for a device belonging to a certain family.
    dmdsmFamily     = 2,
    /// \brief Searches for a devices with a certain product string identifier.
    dmdsmProduct    = 3,
    /// \brief This flag can be 'ored' (|) together with one of the other values.
    /**
     *  When <b>::dmdsmUseDevID</b> is specified, the device
     *  is located via the criteria specified above <b>AND</b> the device must have
     *  a certain ID stored into some internal memory.
     */
    dmdsmUseDevID   = 0x8000
};

//-----------------------------------------------------------------------------
/// \brief Defines valid info query types, which can be passed to the function \b DMR_GetDeviceInfoEx().
enum TDMR_DeviceInfoType
//-----------------------------------------------------------------------------
{
    /// \brief Used to query a small structure containing some information about the device.
    /**
     *  <b>DMR_GetDeviceInfoEx()</b> will expect a pointer to a <b>TDMR_DeviceInfo</b>
     *  structure when called with <b>::dmditDeviceInfoStructure</b>.
     */
    dmditDeviceInfoStructure    = 0,
    /// \brief Checks if the device is in use by an application.
    /**
     *  The output value will be a 4 byte unsigned integer. A value different from 0
     *  indicates the device is already in use. In this case the device might be in
     *  use either by the current process, by another process running on this machine
     *  or even by a process running on a different machine(e.g. when talking to a
     *  network device).
     *
     *  \since 2.0.11
     */
    dmditDeviceIsInUse          = 1,
    /// \brief Returns a handle providing access to device driver library specific features.
    /**
     *  The output value will be a HOBJ.
     *  This list does exist only once per device driver library. Changes in this list will affect all
     *  devices that are operated using this device driver library.
     *
     *  \since 2.17.0
     */
    dmdithDeviceDriver          = 2
};

//-----------------------------------------------------------------------------
/// \brief Defines valid interface list types, which can be located using the function \b DMR_FindList().
enum TDMR_ListType // no_managed_type
//-----------------------------------------------------------------------------
{
    /// \brief A placeholder for an undefined list type
    dmltUndefined = -1,
    /// \brief Specifies a certain setting.
    /**
     *  An additional string defines the name of the setting to look for.
     */
    dmltSetting = 0,
    /// \brief Specifies the list of driver owned image request objects.
    dmltRequest = 1,
    /// \brief Specifies a certain image request control.
    /**
     *  An additional string defines the name of the setting to look for.
     */
    dmltRequestCtrl = 2,
    /// \brief Specifies the driver interfaces list containing general information.
    /**
     *  This e.g. can be properties containing the driver version, the current state
     *  of the device and stuff like that.
     */
    dmltInfo = 3,
    /// \brief Specifies the driver interface list containing statistical information.
    /**
     *  This list e.g. might contain the current frame rate, the total number of images
     *  captured, etc..
     */
    dmltStatistics = 4,
    /// \brief Specifies the driver interface list containing properties, which influence the overall operation of the device.
    /**
     *  This list e.g. might contain the priority of the drivers internal worker thread,
     *  the number of request objects the driver shall work with, etc..
     */
    dmltSystemSettings = 5,
    /// \brief Specifies the driver interface list containing properties to work with any kind of I/O pin belonging to that device.
    /**
     *  Here properties addressing the digital inputs and outputs and other I/O related
     *  properties can be found.
     */
    dmltIOSubSystem = 6,
    /// \brief Specifies the driver interface list providing access to the drivers <b>H</b>ardware <b>R</b>eal-<b>T</b>ime <b>C</b>ontroller (HRTC).
    /**
     *  Here properties to control the behaviour of the HRTCs can be found.
     *  \note
     *  This feature might not be available for every device.
     */
    dmltRTCtr = 7,
    /// \brief Specifies the driver interface list providing access to the recognized camera description lists.
    /**
     *  Within this list all recognized camera descriptions can be found, each forming a sub list
     *  containing the properties describing the camera.
     *
     *  \note This feature currently is only available for frame grabber devices.
     */
    dmltCameraDescriptions = 8,
    /// \brief Specifies the driver interface list providing access to the device specific settings lists.
    /**
     *  \note This feature currently is only available for frame grabber devices.
     */
    dmltDeviceSpecificData = 9,
    // dmltEventSubSystemSettings = 10, // Removed in version 2.47.0! Do not re-use value!
    // dmltEventSubSystemResults = 11,  // Removed in version 2.47.0! Do not re-use value!
    /// \brief Specifies the driver interface list providing access to the devices memory manager list.
    /**
     *  \note This feature currently is only available for frame grabber devices.
     *
     *  This list will contain properties and lists providing access to settings related to the memory
     *  handling used by the device. E.g. the buffer size for individual DMA blocks can be configured
     *  here.
     *
     *  \note
     *  Properties in this list should only be modified by advanced users.
     */
    dmltImageMemoryManager = 12,
    /// \brief Specifies the device driver lib
    /**
     *  An additional string defines the name of the device driver lib to look for.
     *
     *  \since 2.17.0
     */
    dmltDeviceDriverLib = 13
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef enum TOBJ_HandleCheckMode TOBJ_HandleCheckMode;
typedef enum TOBJ_StringQuery TOBJ_StringQuery;
typedef struct TDMR_DeviceInfo TDMR_DeviceInfo;
typedef enum TDMR_DeviceInfoType TDMR_DeviceInfoType;
typedef enum TDMR_ListType TDMR_ListType;
typedef enum TDMR_DeviceSearchMode TDMR_DeviceSearchMode;
#endif // DOXYGEN_SHOULD_SKIP_THIS

// general library functions
MVDMR_API TDMR_ERROR DMR_CALL DMR_Init( HDMR* pHDmr );
MVDMR_API TDMR_ERROR DMR_CALL DMR_Close( void );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetDevice( HDEV* pHDev, TDMR_DeviceSearchMode searchMode, const char* pSearchString, unsigned int devNr, char wildcard );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetDeviceWithStringID( HDEV* pHDev, TDMR_DeviceSearchMode searchMode, const char* pSearchString, const char* pStringID, char wildcard );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetDeviceCount( unsigned int* pDevCnt );
MVDMR_API TDMR_ERROR DMR_CALL DMR_OpenDevice( HDEV hDev, HDRV* pHDrv );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CloseDevice( HDRV hDrv, HDEV hDev );
MVIMPACT_DEPRECATED_C( MVDMR_API TDMR_ERROR DMR_CALL DMR_GetDeviceInfo( unsigned int devNr, TDMR_DeviceInfo* pInfo, size_t infoSize ) );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetDeviceInfoEx( HDEV hDev, TDMR_DeviceInfoType infoType, void* pInfo, size_t* pInfoSize );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetDriverHandle( HDEV hDev, HDRV* pHDrv );
MVDMR_API TDMR_ERROR DMR_CALL DMR_UpdateDeviceList( unsigned int reserved, int reserved2 );
// device specific functions. These function require a handle to an initialised device
// basic operations
MVDMR_API TDMR_ERROR DMR_CALL DMR_CreateSetting( HDRV hDrv, const char* pName, const char* pParentName, HLIST* pNewID );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CreateRequestControl( HDRV hDrv, const char* pName, const char* pParentName, HLIST* pNewID, int* pRequestCtrl );
MVDMR_API TDMR_ERROR DMR_CALL DMR_DeleteList( HDRV hDrv, const char* pName, TDMR_ListType type );
MVDMR_API TDMR_ERROR DMR_CALL DMR_FindList( HDRV hDrv, const char* pName, TDMR_ListType type, unsigned int flags, HLIST* phDevList );
// image acquisition functions
MVDMR_API TDMR_ERROR DMR_CALL DMR_AcquisitionStart( HDRV hDrv );
MVDMR_API TDMR_ERROR DMR_CALL DMR_AcquisitionStop( HDRV hDrv );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestReset( HDRV hDrv, int requestCtrl, int mode );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestSingle( HDRV hDrv, int requestCtrl, int* pRequestUsed );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestUnlock( HDRV hDrv, int requestNr );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestConfigure( HDRV hDrv, int requestNr, int reserved, void* pReserved );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestWaitFor( HDRV hDrv, int timeout_ms, int queueNr, int* pRequestNr );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestResultQueueElementCount( HDRV hDrv, int queueNr, int* pResultQueueElements );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestSave( HDRV hDrv, int requestNr, const char* pFileName, TImageFileFormat format );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestBuffer( HDRV hDrv, int requestNr, ImageBuffer** ppBuffer );
// functions only needed to query certain information from other languages such as VB.
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestBufferData( HDRV hDrv, int requestNr, int* pBytesPerPixel, int* pChannelCount, int* pHeight, int* pWidth, int* pSize, TImageBufferPixelFormat* pPixelFormat, void** ppData );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestBufferChannelData( HDRV hDrv, int requestNr, int channelNr, int* pChannelOffset, int* pLinePitch, int* pPixelPitch, char* pChannelDesc, size_t channelDescSize );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestBufferImageData( HDRV hDrv, int requestNr, int xOff, int yOff, int width, int height, char* pBuf, size_t bufSize );
MVDMR_API TDMR_ERROR DMR_CALL DMR_SetImageRequestBufferImageData( HDRV hDrv, int requestNr, int xOff, int yOff, int width, int height, char* pBuf, size_t bufSize );
// functions only needed to query certain information from other languages such as VB END.
#ifdef __vl_base_h__
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImpactRequestBufferEx( HDRV hDrv, int requestNr, IPL_BUFHANDLE* pBuffer, TImpactBufferFlag flags, unsigned int reserved );
MVDMR_API TDMR_ERROR DMR_CALL DMR_BuildImpactImage( const ImageBuffer* pBuffer, IPL_BUFHANDLE* pBufHandle, TImpactBufferFlag flags, void* pReserved, size_t reservedSize );
#endif // __vl_base_h__
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestInfoEx( HDRV hDrv, int requestNr, RequestInfo* pInfo, size_t infoSize, int reserved, int reserved2 );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestResultEx( HDRV hDrv, int requestNr, RequestResult* pResult, size_t resultSize, int reserved, int reserved2 );
MVDMR_API TDMR_ERROR DMR_CALL DMR_GetImageRequestParamS( HDRV hDrv, int requestNr, TImageRequestParam param, char* pBuf, size_t bufSize );
// file I/O functions
MVDMR_API TDMR_ERROR DMR_CALL DMR_LoadRTCtrProgram( HDRV hDrv, HLIST hRTCtrList );
MVDMR_API TDMR_ERROR DMR_CALL DMR_SaveRTCtrProgram( HDRV hDrv, HLIST hRTCtrList );
MVDMR_API TDMR_ERROR DMR_CALL DMR_LoadSetting( HDRV hDrv, const char* pName, TStorageFlag storageFlags, TScope scope );
MVDMR_API TDMR_ERROR DMR_CALL DMR_SaveSetting( HDRV hDrv, const char* pName, TStorageFlag storageFlags, TScope scope );
MVDMR_API TDMR_ERROR DMR_CALL DMR_LoadSettingFromDefault( HDRV hDrv, TScope scope );
MVDMR_API TDMR_ERROR DMR_CALL DMR_SaveSettingToDefault( HDRV hDrv, TScope scope );
MVDMR_API TDMR_ERROR DMR_CALL DMR_IsSettingAvailable( const char* pName, TStorageLocation storageLocation, TScope scope );
MVDMR_API TDMR_ERROR DMR_CALL DMR_DeleteSetting( const char* pName, TStorageLocation location, TScope scope );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ExportCameraDescription( HDRV hDrv, HLIST hCameraDescList );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImportCameraDescription( HDRV hDrv, HLIST hCameraDescList );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CopyCameraDescription( HDRV hDrv, HLIST hCameraDescList, const char* pNewName );
// miscellaneous HW specific functions
MVDMR_API TDMR_ERROR DMR_CALL DMR_SetDeviceID( HDEV hDev, int newID );
MVDMR_API TDMR_ERROR DMR_CALL DMR_UpdateFirmware( HDEV hDev );
MVDMR_API TDMR_ERROR DMR_CALL DMR_UpdateDigitalInputs( HDRV hDrv );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CreateUserDataEntry( HDEV hDev, HLIST* pEntry );
MVDMR_API TDMR_ERROR DMR_CALL DMR_DeleteUserDataEntry( HDEV hDev, HLIST hEntry );
MVDMR_API TDMR_ERROR DMR_CALL DMR_WriteUserDataToHardware( HDEV hDev );

// device independent image buffer related functions
MVDMR_API TDMR_ERROR DMR_CALL DMR_AllocImageRequestBufferDesc( ImageBuffer** ppBuffer, int channelCount );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ReleaseImageRequestBufferDesc( ImageBuffer** ppBuffer );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CopyImageRequestBufferDesc( const ImageBuffer* pSrc, ImageBuffer** ppDst, int flags );
MVDMR_API TDMR_ERROR DMR_CALL DMR_AllocImageBuffer( ImageBuffer** ppBuffer, TImageBufferPixelFormat pixelFormat, int width, int height );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ReleaseImageBuffer( ImageBuffer** ppBuffer );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CopyImageBuffer( const ImageBuffer* pSrc, ImageBuffer** ppDst, int flags );
MVDMR_API TDMR_ERROR DMR_CALL DMR_LoadImageBuffer( ImageBuffer** ppBuffer, const char* pFileName, TImageFileFormat format );
MVDMR_API TDMR_ERROR DMR_CALL DMR_SaveImageBuffer( const ImageBuffer* pBuffer, const char* pFileName, TImageFileFormat format );

// video stream related functions
MVDMR_API TDMR_ERROR DMR_CALL DMR_InitVideoStreamAPI( void* pReserved, size_t reserved );
MVDMR_API TDMR_ERROR DMR_CALL DMR_OpenVideoStream( const char* pFileName, const unsigned int imageWidth, const unsigned int imageHeight, const TVideoCodec codec, const unsigned int quality_pc, const unsigned int bitrate, HDMR_VIDEO_STREAM* pHVideoStream );
MVDMR_API TDMR_ERROR DMR_CALL DMR_CloseVideoStream( HDMR_VIDEO_STREAM hVideoStream );
MVDMR_API TDMR_ERROR DMR_CALL DMR_SaveImageBufferToVideoStream( HDMR_VIDEO_STREAM hVideoStream, const ImageBuffer* pBuffer, const int64_type timestamp_us );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ImageRequestSaveToVideoStream( HDRV hDrv, int requestNr, HDMR_VIDEO_STREAM hVideoStream );
MVDMR_API TDMR_ERROR DMR_CALL DMR_PauseVideoStream( HDMR_VIDEO_STREAM hVideoStream );
MVDMR_API TDMR_ERROR DMR_CALL DMR_ResumeVideoStream( HDMR_VIDEO_STREAM hVideoStream );
MVDMR_API TDMR_ERROR DMR_CALL DMR_IsVideoStreamPaused( HDMR_VIDEO_STREAM hVideoStream );

// miscellaneous functions
MVDMR_API const char* DMR_CALL DMR_ErrorCodeToString( int errorCode );
MVDMR_API const char* DMR_CALL DMR_GetVersion( TLibraryQuery libraryQuery );
MVDMR_API TDMR_ERROR  DMR_CALL DMR_GetLastError( TDMR_ERROR* pErrorCode, char* pErrorText, size_t* pErrorTextSize );
// general object specific functions
MVIMPACT_DEPRECATED_C( MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_IsSettingAvailable( const char* pName, TStorageFlag storageFlags, TScope scope ) );
MVIMPACT_DEPRECATED_C( MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_DeleteSetting( const char* pName, TStorageFlag storageFlags, TScope scope ) );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_CheckHandle( HOBJ hObj, TOBJ_HandleCheckMode mode );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetChangedCounter( HOBJ hObj, unsigned int* pChangedCounter );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetChangedCounterAttr( HOBJ hObj, unsigned int* pChangedCounter );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetContentDesc( HOBJ hList, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetDocString( HOBJ hObj, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFlags( HOBJ hObj, TComponentFlag* pFlags );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFlagsS( HOBJ hObj, const char* pSeparator, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetName( HOBJ hObj, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetDisplayName( HOBJ hObj, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetType( HOBJ hObj, TComponentType* pType );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetTypeS( HOBJ hObj, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetVisibility( HOBJ hObj, TComponentVisibility* pVisibility );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetVisibilityS( HOBJ hObj, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_VisibilityToString( TComponentVisibility visibility, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetRepresentation( HOBJ hObj, TComponentRepresentation* pRepresentation );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetRepresentationS( HOBJ hObj, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_RepresentationToString( TComponentRepresentation representation, char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_IsDefault( HOBJ hObj, unsigned int* pResult );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_RestoreDefault( HOBJ hObj );
MVIMPACT_DEPRECATED_C( MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetHandle( HLIST hList, const char* pPathAndObjName, HOBJ* phObj ) );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetSelectedFeatures( HOBJ hObj, int index, HOBJ* phFeatures, size_t* pFeatureCount );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetSelectingFeatures( HOBJ hObj, int index, HOBJ* phFeatures, size_t* pFeatureCount );

//-----------------------------------------------------------------------------
/// \brief A prototype for a callback function that can be attached to a feature.
/**
 *  This callback will be executed whenever the feature changes
 */
typedef void ( DMR_CALL* CBOBJChanged )( /// The feature which has changed
    HOBJ hObj,
    /// A pointer to additional user data bound to this callback.
    /// This data can be passed to the callback on creation.
    /// Whenever the callback gets executed this data is passed back to the user.
    void* pUserData );
//-----------------------------------------------------------------------------
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_CreateCallback( TCallbackType type, CBOBJChanged pMeth, void* pUserData, CallbackHandle* phCallback );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_DeleteCallback( CallbackHandle hCallback );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_AttachCallback( HOBJ hObj, CallbackHandle hCallback );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_DetachCallback( HOBJ hObj, CallbackHandle hCallback );
// navigation within object hierarchies
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFirstSibling( HOBJ hObj, HOBJ* phFirstSibling );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetNextSibling( HOBJ hObj, HOBJ* phNextSibling );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetLastSibling( HOBJ hObj, HOBJ* phLastSibling );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFirstChild( HOBJ hObj, HOBJ* phFirstChild );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetParent( HOBJ hObj, HOBJ* phParent );
// property specific object functions
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetMaxValCount( HOBJ hProp, unsigned int* pValCount );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetValCount( HOBJ hProp, unsigned int* pValCount );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetValCount( HOBJ hProp, unsigned int valCount );
MVIMPACT_DEPRECATED_C( MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetBinary( HOBJ hProp, char* pBuf, unsigned int bufSize, int index ) );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetBinaryEx( HOBJ hProp, char* pBuf, unsigned int* pBufSize, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetBinary( HOBJ hProp, const char* pBuf, unsigned int bufSize, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetBinaryBufferSize( HOBJ hProp, unsigned int* pBufSize, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetBinaryBufferMaxSize( HOBJ hProp, unsigned int* pBufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetF( HOBJ hProp, double* pVal, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFArray( HOBJ hProp, double* pVal, unsigned int valCount, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetF( HOBJ hProp, double val, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetFArray( HOBJ hProp, const double* pVal, unsigned int valCount, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFDictEntry( HOBJ hProp, char* pTranslationString, size_t translationStringBufSize, double* pValue, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFDictEntries( HOBJ hProp, char** pTranslationArray, size_t sizePerTranslationBuf, double* pValArray, size_t arraySizes );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetI( HOBJ hProp, int* pVal, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetIArray( HOBJ hProp, int* pVal, unsigned int valCount, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetI( HOBJ hProp, int val, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetIArray( HOBJ hProp, const int* pVal, unsigned int valCount, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetIDictEntry( HOBJ hProp, char* pTranslationString, size_t translationStringBufSize, int* pValue, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetIDictEntries( HOBJ hObj, char** pTranslationArray, size_t sizePerTranslationBuf, int* pValArray, size_t arraySizes );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetI64( HOBJ hProp, int64_type* pVal, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetI64Array( HOBJ hProp, int64_type* pVal, unsigned int valCount, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetI64( HOBJ hProp, int64_type val, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetI64Array( HOBJ hProp, const int64_type* pVal, unsigned int valCount, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetI64DictEntry( HOBJ hProp, char* pTranslationString, size_t translationStringBufSize, int64_type* pValue, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetI64DictEntries( HOBJ hObj, char** pTranslationArray, size_t sizePerTranslationBuf, int64_type* pValArray, size_t arraySizes );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetP( HOBJ hProp, void** pVal, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetP( HOBJ hProp, void* val, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetS( HOBJ hProp, char* pVal, size_t bufSize, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_SetS( HOBJ hProp, const char* pVal, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetSFormattedEx( HOBJ hProp, char* pBuf, size_t* pBufSize, const char* pFormat, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetSArrayFormattedEx( HOBJ hProp, char* pBuf, size_t* pBufSize, const char* pFormat, const char* pDelimiters, int startIndex, int endIndex, int mode );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_RemoveVal( HOBJ hProp, int index );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_IsConstantDefined( HOBJ hProp, int constant, unsigned int* pResult );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetDictSize( HOBJ hProp, unsigned int* pDictSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetFormatString( HOBJ hProp, char* pBuf, size_t bufSize );
// list specific object functions
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetElementCount( HLIST hList, unsigned int* pElementCount );
// method specific object functions
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_Execute( HOBJ hMeth, const char* pCallParams, const char* pDelimiters, int* pResult );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetSParamList( HOBJ hMeth, char* pBuf, size_t bufSize );
// advanced object specific functions
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetHandleEx( HLIST hList, const char* pObjName, HOBJ* phObj, unsigned int searchMode, int maxSearchDepth );
/// \brief A prototype for a function pointer used for in place string construction
/**
 *  This function pointer can be used to prototype advanced efficient calls to interface functions.
 *  \sa
 *  \b OBJ_GetSWithInplaceConstruction()
 */
typedef char* ( *SCF )( const char* pBuf, size_t bufSize );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_GetSWithInplaceConstruction( HOBJ hObj, TOBJ_StringQuery sq, char** pResult, SCF constructionFunc, int mode, int reserved );
MVDMR_API TPROPHANDLING_ERROR DMR_CALL OBJ_FreeSMemory( char* pBuffer );

#endif // #ifndef WRAP_ANY

#endif // #ifndef DOXYGEN_CPP_DOCUMENTATION

#ifndef __GNUC__
#   pragma pack(pop) // restore previous structure alignment
#endif
#undef ATTR_PACK

#if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
} // namespace acquire
} // namespace mvIMPACT
#endif // #if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)

#ifdef __cplusplus
}
#endif // __cplusplus

// restore Borland compiler switch 'force enums to the size of integer'
#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
#   ifdef _WIN32
#       ifdef __BORLANDC__ // is Borland compiler?
#           pragma option pop
#       endif // #ifdef __BORLANDC__
#   endif // #ifdef _WIN32
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

#endif // mvDeviceManagerH
