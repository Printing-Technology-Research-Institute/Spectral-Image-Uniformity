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
#ifndef MVIMPACT_ACQUIRE_DISPLAY_H_
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#   define MVIMPACT_ACQUIRE_DISPLAY_H_ MVIMPACT_ACQUIRE_DISPLAY_H_
#endif // #ifndef DOXYGEN_SHOULD_SKIP_THIS
//-----------------------------------------------------------------------------
#ifdef SWIG
#   ifdef SWIGPYTHON
#       ifndef WRAP_PYTHON
#           define WRAP_PYTHON
#       endif
#   endif
#   ifdef SWIGJAVA
#       ifndef WRAP_JAVA
#           define WRAP_JAVA
#       endif
#   endif
#endif

#ifdef WRAP_PYTHON
#   ifndef PYTHON_ONLY
#       define PYTHON_ONLY(X) X
#   endif
#   ifndef PYTHON_CPP_SWITCH
#       define PYTHON_CPP_SWITCH(PYTHON_WRAPPER_CODE,CPP_WRAPPER_CODE) PYTHON_WRAPPER_CODE
#   endif
#   ifndef WRAP_ANY
#       define WRAP_ANY
#   endif // #ifndef WRAP_ANY
#else // #ifdef WRAP_PYTHON
#   ifndef PYTHON_ONLY
#       define PYTHON_ONLY(X)
#   endif
#   ifndef PYTHON_CPP_SWITCH
#       define PYTHON_CPP_SWITCH(PYTHON_WRAPPER_CODE,CPP_WRAPPER_CODE) CPP_WRAPPER_CODE
#   endif
#endif // #ifdef WRAP_PYTHON

#ifdef WRAP_JAVA
#   ifndef JAVA_ONLY
#       define JAVA_ONLY(X) X
#   endif
#   ifndef JAVA_CPP_SWITCH
#       define JAVA_CPP_SWITCH(JAVA_WRAPPER_CODE,CPP_WRAPPER_CODE) JAVA_WRAPPER_CODE
#   endif
#   ifndef WRAP_ANY
#       define WRAP_ANY
#   endif // #ifndef WRAP_ANY
#else // #ifdef WRAP_JAVA
#   ifndef JAVA_ONLY
#       define JAVA_ONLY(X)
#   endif
#   ifndef JAVA_CPP_SWITCH
#       define JAVA_CPP_SWITCH(JAVA_WRAPPER_CODE,CPP_WRAPPER_CODE) CPP_WRAPPER_CODE
#   endif
#endif // #ifdef WRAP_JAVA

#if defined(WRAP_PYTHON) || defined(WRAP_JAVA)
#   ifndef PYTHON_OR_JAVA_ONLY
#       define PYTHON_OR_JAVA_ONLY(X) X
#   endif
#else
#   ifndef PYTHON_OR_JAVA_ONLY
#       define PYTHON_OR_JAVA_ONLY(X)
#   endif
#endif // #if defined(WRAP_PYTHON) || defined(WRAP_JAVA)

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

#ifndef WRAP_ANY
#   include <string>
#endif // #ifdef WRAP_ANY

// C-API header will be imported into mvIMPACT::acquire::display if included from here
#include <mvDisplay/Include/mvDisplayWindow.h>
#ifdef MVIMPACT_ACQUIRE_H_
#   include <mvDisplay/Include/mvDisplayExtensions.h>
#   if defined(CPP_STANDARD_VERSION) && (CPP_STANDARD_VERSION >= 11)
#       include <memory>
#   endif // #if defined(CPP_STANDARD_VERSION) && (CPP_STANDARD_VERSION >= 11)
#endif // #ifdef MVIMPACT_ACQUIRE_H_

#if defined(MVIMPACT_ACQUIRE_HELPER_H) && (!defined(CPP_STANDARD_VERSION) || ( defined(CPP_STANDARD_VERSION) && (CPP_STANDARD_VERSION < 11)))
#   error "Combining the mvIMPACT_acquire_helper.h and mvIMPACT_acquire_display.h requires 'CPP_STANDARD_VERSION' being defined and set to a minimal value of 11! See documentation of the class mvIMPACT::acquire::helper::RequestProvider for details."
#endif // #if defined(MVIMPACT_ACQUIRE_HELPER_H) && (!defined(CPP_STANDARD_VERSION) || ( defined(CPP_STANDARD_VERSION) && (CPP_STANDARD_VERSION < 11)))

namespace mvIMPACT
{
namespace acquire
{
/// \namespace mvIMPACT::acquire::display This namespace contains classes and functions that can be used to display images.
namespace display
{

/// \defgroup Display Display functions
/// \brief Classes and functions that can be used to display images.
/**
 *  This group contains classes and functions that can be used to display images.
 */
/// @{

class ImageDisplayWindow;

//-----------------------------------------------------------------------------
/// \brief A class that can be used for displaying images within existing windows or GUI elements that can provide a window handle.
/**
 *  Instances of this class can be associated with an existing window object of
 *  which the window handle is known and can be passed to the constructor of this
 *  class.
 *
 *  \image html Display_Window.png
 *
 *  The full client area of a window will be consumed by a display object. Where the actual image shall be display relative
 *  to the upper left corner of the windows client area can be defined by calling \b mvIMPACT::acquire::display::ImageDisplay::SetDisplayRectangle(). This function
 *  will also limit the area where image data will be displayed. The area outside the display rectangle but within the
 *  windows client area can be filled by defining a background brush and then attach this brush to the display by calling
 *  \b mvIMPACT::acquire::display::ImageDisplay::SetBackgroundBrush().
 *
 *  If the actual image size differs from the area defined by \b mvIMPACT::acquire::display::ImageDisplay::SetDisplayRectangle(), it depends on the display mode
 *  if the complete image is scaled, cropped of just drawn in the top left corner of the display rectangle. The display mode
 *  can be modified by calling \b mvIMPACT::acquire::display::ImageDisplay::SetDisplayMode().
 *
 *  In addition to that only a part of the actual image can be selected for drawing by the function \b mvIMPACT::acquire::display::ImageDisplay::SetBitmapRectangle().
 *  For this region the same scaling and cropping regions will be applied then.
 *
 * \if DOXYGEN_CPP_DOCUMENTATION
 *  \note
 *  This class can be used without using the rest of %mvIMPACT Acquire. This is achieved by using a pre-processor check
 *  for the presence of a certain include guard that is defined by %mvIMPACT Acquire. Now in order to be able to use those
 *  functions, that can directly work on data types as defined by %mvIMPACT Acquire, the main %mvIMPACT Acquire header
 *  must be included \b BEFORE including this header:
 *
 * \code
 *  #include <mvIMPACT_CPP/mvIMPACT_acquire.h>
 *  #include <mvDisplay/Include/mvIMPACT_acquire_display.h>
 * \endcode
 * \endif
 *
 *  \sa
 *  <b>mvIMPACT::acquire::display::ImageDisplay::SetDisplayRectangle(),<br>
 *     mvIMPACT::acquire::display::ImageDisplay::SetBitmapRectangle(),<br>
 *     mvIMPACT::acquire::display::ImageDisplay::SetBackgroundBrush(),<br>
 *     mvIMPACT::acquire::display::ImageDisplay::SetDisplayMode()</b>
 */
class ImageDisplay
//-----------------------------------------------------------------------------
{
    friend class ImageDisplayWindow;
    TDisp* pDisp_;
    bool boMustFree_;
#if defined(MVIMPACT_ACQUIRE_H_)
#   if defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 )
    std::shared_ptr<Request> pCurrentRequest_;
#   endif // #if defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 )
#endif // #if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
    explicit ImageDisplay() : pDisp_( 0 ), boMustFree_( false ) {}
public:
    /// \brief Create a new object that can be used for displaying images
    explicit ImageDisplay(
        /// A handle to the window which should be used for the displaying.
        WindowHandle hwnd ) : pDisp_( 0 ), boMustFree_( true )
    {
        pDisp_ = mvDispInit( hwnd );
    }
    /// \brief Frees the resources previously allocated.
    ~ImageDisplay()
    {
        if( boMustFree_ )
        {
            mvDispDeinit( &pDisp_ );
        }
    }
    /// \brief Clears the display.
    /**
     *  Either the windows default background will be displayed afterwards or if a
     *  background brush has been defined the background brush will be used to fill
     *  the complete client area.
     *
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplay::SetBackgroundBrush()</b>
     */
    void Clear( void ) const
    {
        mvDispClear( pDisp_ );
    }
    /// \brief Sets the position and dimension of bitmap data region.
    /**
     *  This defines the area relative within the area consumed by the window associated with
     *  the window handle that is bound to this display. See \b mvIMPACT::acquire::display::ImageDisplay for a detailed
     *  description on how to define the different rectangles and areas for drawing.
     *
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplay<br>
     *     mvIMPACT::acquire::display::ImageDisplay::SetDisplayRectangle()</b>
     */
    void SetBitmapRectangle(
        /// [in] The left offset of the rectangle.
        int left,
        /// [in] The top offset of the rectangle.
        int top,
        /// [in] The width of the bitmap rectangle.
        int width,
        /// [in] The height of the rectangle.
        int height )
    {
        mvDispSetBitmapRectangle( pDisp_, left, top, width, height );
    }
    /// \brief Defines the rectangle used for drawing within the window associated with the display structure.
    /**
     *  This function can be used to define a rectangle within the defined window
     *  Only this then will be used for drawing. This window can be smaller than the image
     *  defined by \b mvIMPACT::acquire::display::ImageDisplay::SetImage(). Then just a part of the image
     *  will be displayed when in \b mvIMPACT::acquire::display::DM_Fastest or the whole image will be scaled
     *  down to fit into the rectangle when in \b mvIMPACT::acquire::display::DM_Default mode.
     *
     *  When the display rectangle is larger than the image the image will be placed in the lower left
     *  corner of the display rectangle (either scaled or unscaled). See \b ImageDisplay for a detailed
     *  description on how to define the different rectangles and areas for drawing.
     *
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplay,<br>
     *     mvIMPACT::acquire::display::ImageDisplay::SetBitmapRectangle</b>
     */
    void SetDisplayRectangle(
        /// [in] The left offset within the window.
        int left,
        /// [in] The top offset within the window.
        int top,
        /// [in] The width of the rectangle to use for drawing.
        int width,
        /// [in] The height of the rectangle to use for drawing.
        int height )
    {
        mvDispSetDisplayRectangle( pDisp_, left, top, width, height );
    }
    /// \brief Gets the parameters of the current data block associated with the display.
    /**
     *  If a parameter is not needed it might be 0(NULL).
     *
     *  The data returned will always be in packed format.
     */
    void GetImage(
        /// [out] A pointer to a variable to receive the storage location of the pixel data.
        /// This can be 0 if the value is not needed.
        const void** ppData,
        /// [out] A pointer to a variable to receive the width of the current image.
        /// This can be 0 if the value is not needed.
        int* pWidth,
        /// [out] A pointer to a variable to receive the height of the current image.
        /// This can be 0 if the value is not needed.
        int* pHeight,
        /// [out] A pointer to a variable to receive the bits per pixel of the current image.
        /// This can be 0 if the value is not needed.
        int* pBitsPerPixel,
        /// [out] A pointer to a variable to receive the pitch (bytes per pixel * width in pixel)of the current image.
        /// This can be 0 if the value is not needed.
        int* pPitch )
    {
        mvDispGetImage( pDisp_, ppData, pWidth, pHeight, pBitsPerPixel, pPitch );
    }
    /// \brief Sets the next image to display.
    /**
     *  This function can deal with RGB888x packed images, 8 bit grey-scale images
     *  and 16 bit Packed RGB images. For other formats overloaded versions of this function
     *  must be used.
     */
    void SetImage(
        /// [in] A pointer to the address of the pixel data.
        const void* pData,
        /// [in] The width of the image.
        int width,
        /// [in] The height of the image.
        int height,
        /// [in] The number of bits per pixel.
        int bitsPerPixel,
        /// [in] The pitch in bytes of one line of pixel data (bytes per pixel * width in pixel per line).
        int pitch )
    {
        mvDispSetImage( pDisp_, pData, width, height, bitsPerPixel, pitch );
    }
    /// \brief Sets the next image to display.
    void SetImage(
        /// [in] An array of pointers that point to the actual image data
        /// For planar RGB images e.g. this can be 3 pointers each pointing
        /// to one color plane.
        const void** ppData,
        /// [in] The number of pointers passed via \a ppData.
        size_t ppDataArraySize,
        /// [in] The pixel format as defined by \b mvIMPACT::acquire::display::TFormatFlags.
        TFormatFlags format,
        /// [in] The width of the image.
        int width,
        /// [in] The height of the image.
        int height,
        /// [in] The number of bits per pixel.
        int bitsPerPixel,
        /// [in] The pitch in bytes of one line of pixel data (bytes per pixel * width in pixel per line).
        int pitch )
    {
        mvDispSetImageEx( pDisp_, ppData, ppDataArraySize, format, width, height, bitsPerPixel, pitch );
    }
#if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
    /// \brief Sets the next image to display.
    /**
     *  This function can deal with any pixel format supported by %mvIMPACT Acquire.
     */
    void SetImage(
        /// [in] The image buffer object that shall be displayed.
        const mvIMPACT::acquire::ImageBuffer* pBuf )
    {
        mvDispSetImageFromImageBuffer( pDisp_, pBuf );
    }
    /// \brief Sets the next image to display.
    /**
     *  This function can deal with any pixel format supported by %mvIMPACT Acquire.
     */
    void SetImage(
        /// [in] The \b mvIMPACT::acquire::Request object whose image data shall be displayed.
        const mvIMPACT::acquire::Request* pRequest )
    {
        mvDispSetImageFromRequest( pDisp_, pRequest );
    }
#   if (defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 )) || defined(DOXYGEN_CPP_DOCUMENTATION)
    /// \brief Sets the next image to display.
    /**
     *  This function can deal with any pixel format supported by %mvIMPACT Acquire.
     */
    void SetImage(
        /// [in] The \b mvIMPACT::acquire::Request object whose image data shall be displayed.
        std::shared_ptr<Request> pRequest )
    {
        pCurrentRequest_ = pRequest;
        mvDispSetImageFromRequest( pDisp_, pRequest.get() );
    }
#   endif // #if (defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 )) || defined(DOXYGEN_CPP_DOCUMENTATION)
#endif // #if defined(MVIMPACT_ACQUIRE_H_) || defined(DOXYGEN_CPP_DOCUMENTATION)
    /// \brief Removes the current image from the display.
    /**
     *  This function will remove the current image attached to the display from the display again.
     *  This is mainly meant to be able to free the memory associated with the image buffer without the risk that a
     *  later call to an update function or a repaint message would cause another redraw of then
     *  freed memory. As only one buffer can be attached to a display at any given time calling
     *  \b mvIMPACT::acquire::display::ImageDisplay::SetImage will effectively remove the previous
     *  image as well so this function usually is only needed when ending e.g. a live display.
     */
    void RemoveImage( void )
    {
        mvDispSetImage( pDisp_, 0, 0, 0, 0, 0 );
#if defined(MVIMPACT_ACQUIRE_H_)
#   if defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 )
        pCurrentRequest_.reset();
#   endif // #if defined( CPP_STANDARD_VERSION ) && ( CPP_STANDARD_VERSION >= 11 )
#endif // #if defined(MVIMPACT_ACQUIRE_H_)
    }
    /// \brief Immediately redraws the current image.
    void Update( void ) const
    {
        mvDispUpdate( pDisp_ );
    }
    /// \brief Returns the current display mode.
    /**
     *  Valid display modes are defined by \b mvIMPACT::acquire::display::TDisplayMode.
     *  \return
     *  The current display mode.
     */
    TDisplayMode GetDisplayMode( void ) const
    {
        return mvDispGetDisplayMode( pDisp_ );
    }
    /// \brief Switches to a different display mode.
    void SetDisplayMode(
        /// [in] The new display mode.
        TDisplayMode mode )
    {
        mvDispSetDisplayMode( pDisp_, mode );
    }
    /// \brief Gets the current interpolation mode that will be used for scaling if display window rectangle is different to input image rectangle.
    /**
     *  Valid interpolation modes are defined by \b mvIMPACT::acquire::display::TInterpolationMode.
     *  \return The current interpolation mode.
     */
    TInterpolationMode GetInterpolationMode( void ) const
    {
        return mvDispGetInterpolationMode( pDisp_ );
    }
    /// \brief Sets the new interpolation mode that will be used for scaling if display window rectangle is different to input image rectangle.
    void SetInterpolationMode(
        /// [in] The new interpolation mode.
        TInterpolationMode mode )
    {
        mvDispSetInterpolationMode( pDisp_, mode );
    }
    /// \brief Returns the current shift value as defined by the application.
    /**
     *  This function returns the current shift value as defined by the application by previous
     *  calls to \b mvIMPACT::acquire::display::ImageDisplay::SetShift. See \b mvIMPACT::acquire::display::ImageDisplay::SetShift for a detailed explanation
     *  about the display behaviour when applying custom shift values.
     *
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplay::SetShift,<br>
     *     mvIMPACT::acquire::display::ImageDisplay::GetAppliedShift</b>
     *  \since 2.4.0
     *  \return The current shift value from previous calls to \b mvIMPACT::acquire::display::ImageDisplay::SetShift.
     */
    int GetShift( void ) const
    {
        return mvDispGetShift( pDisp_ );
    }
    /// \brief Sets the shift value that shall be subtracted from the shift value needed to display the 8 msb of a pixel.
    /**
     *  This function will allow to select which 8 bits out of a multi-byte pixel format shall be displayed the
     *  next time \b mvIMPACT::acquire::display::ImageDisplay::Update is called. When the \a shift value is 0 the 8 msb of each pixel will be displayed
     *  so e.g. for a 12 bit format bits 11 to 4 will be displayed by default.
     *
     *  Consider the typical layout of 12 mono pixel data in memory:
     *
     *  \image html Mono12_01.png
     *
     *  So with an application defined \a shift value of 0 (the default), the display module will shift each
     *  2 byte pixel by 4 positions to the right. This will remove the 4 lsb from the data. Afterwards the now
     *  empty upper byte of each pixel is removed from the data resulting in the following memory layout which is
     *  then displayed on the canvas:
     *
     *  \image html Mono12_02.png
     *
     *  Now sometimes it is required to display other bits from the image e.g. for analysis purposes. As most operating
     *  systems only support to display 8 bits per color component this requires to select a different range of pixels to
     *  be sent to the canvas. This can be done by calling this function and passing a custom \a shift value to it. The
     *  custom \a shift value will be subtracted from the value that would be needed to display the 8 msb for a given format.
     *
     *  So to display the 8 lsb for a 12 bit mono format, \a shift must be set to 4. This then results in the display module to
     *  use a actual shift value of 4(required to display the 8 msb) - 4(defined by calling this function) = 0. Then removing
     *  the upper byte from each pixel results in only the 8 lsb of each pixel being displayed.
     *
     *  When e.g. setting \a shift to 3 for a 12 bit mono for would result in bits 8 - 1 to be displayed:
     *
     *  \image html Mono12_03.png
     *
     *  The shift value that has actually been applied the last time an image has been displayed (thus '1' when taking the
     *  example in the image above (4(required) - 3(defined))) can be queried by calling \b mvIMPACT::acquire::display::ImageDisplay::GetAppliedShift.
     *
     *  \note
     *  During the conversion from a multi-byte to a single byte format the pixel data will be clipped to 255 if
     *  a pixel value is larger than what can be stored in a single byte after the shift operation.
     *
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplay::GetShift,<br>
     *     mvIMPACT::acquire::display::ImageDisplay::GetAppliedShift</b>
     *  \since 2.4.0
     */
    void SetShift(
        /// [in] The shift value to apply to the displayed images. The maximum value for
        /// this function is 8, the minimum 0. Values out of this range will be ignored.
        /// It is \b NOT possible to shift pixel data in such a way that less than 8 bits contain valid
        /// data, thus e.g. a shift value of 2 applied to 8 bit mono data will be ignored.
        int shift )
    {
        mvDispSetShift( pDisp_, shift );
    }
    /// \brief Returns the current shift value that has been applied to the last image that has been displayed.
    /**
     *  This function returns the shift value that has been applied to the last image that has been displayed.
     *  See \b mvIMPACT::acquire::display::ImageDisplay::SetShift for a detailed explanation about the display behaviour when applying custom shift values.
     *
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplay::SetShift,<br>
     *     mvIMPACT::acquire::display::ImageDisplay::GetShift</b>
     *  \since 2.4.0
     *  \return The current shift value that has been applied to the last image that has been displayed.
     */
    int GetAppliedShift( void ) const
    {
        return mvDispGetAppliedShift( pDisp_ );
    }
    /// \brief Returns the last error and clears it.
    /**
     *  If an error has occurred it will not be overwritten by subsequent
     *  errors till the error is queried and cleared. After calling this
     *  function a second call would return \b mvIMPACT::acquire::display::IDE_NoError until another error
     *  occurs.
     *
     *  \return The last error
     */
    int GetLastError( void ) const
    {
        return mvDispGetError( pDisp_ );
    }
    /// \brief Returns the current window handle associated with the display object.
    /**
     *  Returns the current window handle associated with the display object.
     *  \return The current window handle associated with the display object.
     */
    WindowHandle GetWindowHandle( void ) const
    {
        return mvDispGetWindowHandle( pDisp_ );
    }
    /// \brief Assigns a new destination window handle to a display object.
    void SetWindowHandle(
        /// [in] The handle of the new destination window.
        WindowHandle hwnd )
    {
        mvDispSetWindowHandle( pDisp_, hwnd );
    }
    /// \brief Associates a brush to be used for painting the background within the client area.
    /**
     *  See \b mvDispInit() for a detailed description on which regions will be painted using
     *  the background brush.
     *
     *  \attention This only stores the handle to the brush object, not the actual brush, so
     *  as long as this handle is set and the display object is used, the referenced
     *  brush must \b NOT be deleted again.
     */
    void SetBackgroundBrush(
        /// [in] Handle to the brush to be used to paint the background.
        /// This brush then will be used to paint the regions of the rectangle
        /// used for drawing that will not painted with the image itself.
        BrushHandle hBrush )
    {
        mvDispSetBackgroundBrush( pDisp_, hBrush );
    }
    /// \brief Defines the key color for the DirectDraw&reg; overlay
    void SetDDrawOverlayKeyColor(
        /// [in] The key color to be used for the DirectDraw&reg; overlay.
        ColorValue keyColor )
    {
        mvDispSetDDrawOverlayKeyColor( pDisp_, keyColor );
    }
#ifndef WRAP_ANY
    /// \brief Installs an overlay callback function.
    /**
     *  This function will be called when operated in \b mvIMPACT::acquire::display::DM_Default mode
     *  and can be used to draw a user defined overlay on top of the image before it
     *  is displayed.
     */
    void SetOverlayCallbackFunction(
        /// [in] The address of the function to be called for a user
        /// defined overlay
        TImageDisplayOverlayFunction fctOverlay,
        /// [in] A pointer to a user defined parameter
        void* pUserParam )
    {
        mvDispSetOverlayCallbackFunction( pDisp_, fctOverlay, pUserParam );
    }
#endif // #ifndef WRAP_ANY
};

//-----------------------------------------------------------------------------
/// \brief A class that can be used to display images in a window.
/**
 *  Every instance of this class will create and display its own independent window.
 *  Internally it uses an instance of <b>mvIMPACT::acquire::display::ImageDisplay</b>
 *  to display image data. The internal display object can be accessed by calling
 *  <b>mvIMPACT::acquire::display::ImageDisplayWindow::GetImageDisplay()</b>.
 */
class ImageDisplayWindow
//-----------------------------------------------------------------------------
{
    HDISP hDisp_;
    ImageDisplay imageDisplay_;

    ImageDisplayWindow( const ImageDisplayWindow& );            // do not allow copy constructor
    ImageDisplayWindow& operator=( const ImageDisplayWindow& ); // do not allow assignments
public:
    /// \brief Creates a new window that can be used to display image data and displays it.
    /**
     *  This is an example of how to use the GetImageDisplay class.
     *  More details about this example.
     */
    explicit ImageDisplayWindow(
        /// [in] The title of the window (will be displayed in the windows title bar).
        const std::string& title ) : hDisp_( 0 )
    {
        hDisp_ = mvDispWindowCreate( title.c_str() );
        mvDispWindowShow( hDisp_ );
        imageDisplay_.pDisp_ = mvDispWindowGetDisplayHandle( hDisp_ );
    }
    /// \brief destroys the display window and frees resources.
    ~ImageDisplayWindow()
    {
        mvDispWindowDestroy( hDisp_ );
        hDisp_ = 0;
    }
    /// \brief Returns a reference to the actual display object associated with this window.
    ImageDisplay& GetImageDisplay( void )
    {
        return imageDisplay_;
    }
    /// \brief Defines the refresh time in ms
    /**
     *  This function can be used to define a refresh period that - when elapsed -
     *  will automatically repaint the window.
     */
    void SetRefreshTime(
        /// [in] The refresh period for the window in ms. Passing \a 0 will
        /// disable the automatic repaint behaviour.
        int time_ms )
    {
        mvDispWindowSetRefreshTime( hDisp_, time_ms );
    }
#ifndef WRAP_ANY
    /// \brief Installs a callback to handle the window messages.
    /**
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplayWindow::GetMessageHandler()</b>
     */
    void SetMessageHandler(
        /// [in] Pointer to a function which is called by the windows
        /// procedure before the display update is done.
        /// If zero is passed here even the internal default handler
        /// will be disabled and \a WM_SIZE and \a WM_LBUTTONDBLCLK will no longer
        /// be handled internally.
        TPreProcessMessage handler )
    {
        mvDispWindowSetMessageHandler( hDisp_, handler );
    }
    /// \brief Returns the current message handler for this window.
    /**
     *  \sa
     *  <b>mvIMPACT::acquire::display::ImageDisplayWindow::SetMessageHandler()</b>
     *  \return The current message handler for this window.
     */
    TPreProcessMessage GetMessageHandler( void ) const
    {
        return mvDispWindowGetMessageHandler( hDisp_ );
    }
#endif // #ifndef WRAP_ANY // The concept of callback functions as used here is rather C/C++ specific.
};

/// @}

} // namespace display
} // namespace acquire
} // namespace mvIMPACT

#if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)
#   ifndef MVIMPACT_USE_NAMESPACES
using namespace mvIMPACT::acquire::display;
#   endif // #ifndef MVIMPACT_USE_NAMESPACES
#endif // #if !defined(DOXYGEN_SHOULD_SKIP_THIS) && !defined(WRAP_ANY)

#endif //MVIMPACT_ACQUIRE_DISPLAY_H_
