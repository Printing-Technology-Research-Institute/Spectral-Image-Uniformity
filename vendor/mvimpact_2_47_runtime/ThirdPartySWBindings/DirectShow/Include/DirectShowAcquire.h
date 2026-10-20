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
#ifndef DirectShowAcquireH
#define DirectShowAcquireH DirectShowAcquireH
//-----------------------------------------------------------------------------
// header file for any additional feature definition for the MATRIX VISION DirectShow acquire.

#ifdef DEFINE_GUID
#   undef DEFINE_GUID
#   define INITGUID
#endif

#include <guiddef.h>
//-----------------------------------------------------------------------------
// IKsPropertySet MATRIX VISION extension

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// {57917FF4-DD48-442c-8A0E-9288BDF27CF3}
DEFINE_GUID( DIRECT_SHOW_ACQUIRE_PROPERTYSET, 0x57917ff4, 0xdd48, 0x442c, 0x8a, 0xe, 0x92, 0x88, 0xbd, 0xf2, 0x7c, 0xf3 );

//-----------------------------------------------------------------------------
/// \brief Defines valid property identifiers for specifying the property action.
typedef enum _MVPropID
//-----------------------------------------------------------------------------
{
    /// \brief extract all properties from the internal lists and fill the MVPropertyStructs-Array with all important values.
    MVPropIDReadProperties = 0,
    /// \brief read all available strings for a property with type stringarray.
    MVPropIDReadStringArray = 1,
    /// \brief write a property.
    MVPropIDWrite = 2,
    /// \brief read actual value of a property.
    MVPropIDRead = 3
} MVPropID;

//-----------------------------------------------------------------------------
/// \brief Defines valid property types.
typedef enum _MVPropTypes
//-----------------------------------------------------------------------------
{
    /// \brief property type is 32 bit integer.
    MVPropTypesInteger = 0,
    /// \brief property type is double.
    MVPropTypesFloat = 1,
    /// \brief property type is string.
    MVPropTypesString = 2,
    /// \brief property type is string array.
    MVPropTypesStringArray = 3,
    /// \brief property type is 64 bit integer.
    MVPropTypesInteger64 = 4
} MVPropTypes;

//-----------------------------------------------------------------------------
/// \brief Defines valid property access identifiers.
typedef enum _MVPropAccess
//-----------------------------------------------------------------------------
{
    /// \brief property has write access.
    MVPropAccessWrite = 1,
    /// \brief property has read access.
    MVPropAccessRead = 2,
    /// \brief property can be invisible. Invisible properties can be stalled and have no affect. e.g. gain property
    /// will be set to invisible during automatic gain control is activ.
    MVPropAccessInvisible = 4
} MVPropAccess;

//-----------------------------------------------------------------------------
/// \brief A union to pass different values to the property module.
typedef union _MVParameter
//-----------------------------------------------------------------------------
{
    /// \brief 32 bit integer.
    int i;
    /// \brief 64 bit integer.
    __int64 i64;
    /// \brief floating point data type(double precision).
    double f;
    /// \brief A pointer to \\0 terminated string.
    char* s;
} MVParameter;

//-----------------------------------------------------------------------------
/// \brief This structure describes a property for accessing additional features.
//
/// A property will be defined by the parameters of the struct MVProperty. In the normal case we offer
/// a list with most of the properties which are defined in the ImpactACQUIRE interface. One special property
/// we offer is from type stringarray. A stringarray property contains of more than one string e.g. property
/// AutoGainControl offers two strings On / Off.
typedef struct _MVProperty
//-----------------------------------------------------------------------------
{
    const char* propertyName;       ///< the property name.
    MVParameter value;              ///< the actual value or string.
    MVParameter minVal;             ///< minimum value of this property, not defined for type string or stringarray.
    MVParameter maxVal;             ///< maximum value of this property, not defined for type string or stringarray.
    MVParameter stepWidth;          ///< the step width of this property, not defined for type string or stringarray.
    MVPropTypes type;               ///< describes the property type, integer, float, string or stringarray.
    MVPropAccess access;            ///< describes the access mode of this property, if access mode includes the invisible flag then the action this property describes could be stalled.
    unsigned int stringArraySize;   ///< if property is defined as stringarray property this variable describes how many char* pointers this property offers.
    const char** stringArray;       ///< pointer array for property with type stringarray.
    unsigned int valueCount;        ///< if property describes an array this field gives the arraysize.
    unsigned int valueIndex;        ///< if property describes an array this field defines the index this struct belongs to.
} MVProperty;

#ifdef __cplusplus
}
#endif

#endif //DirectShowAcquireH
