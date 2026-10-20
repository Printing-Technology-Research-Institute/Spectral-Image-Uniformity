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
#ifndef function_castH
#define function_castH function_castH
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/// \brief Allows casting void* to a function pointer without gcc complaining.
///
/// When e.g. compiling code with the \a -pedantic option, gcc will complain about the following
/// piece of code
///
/// \code
/// void fn( void )
/// {
///   void* hMod = dlopen( "mylib", RTLD_LAZY );
///   if( hMod )
///   {
///     typedef void (*myFnPtr)( void );
///     myFnPtr p = (myFnPtr)dlsym( hMod, "foobar" ); // this will give a warning
///   }
/// }
/// \endcode
///
/// The compiler will give you the following warning:
///
/// \verbatim
/// ISO C++ forbids casting between pointer-to-function and pointer-to-object
/// \endverbatim
///
/// That is true, but there is not other way to extract symbols from shared libraries.
/// By using this template instead, gcc will be happy and will you.
///
/// \code
/// void fn( void )
/// {
///   void* hMod = dlopen( "mylib", RTLD_LAZY );
///   if( hMod )
///   {
///     typedef void (*myFnPtr)( void );
///     function_cast<myFnPtr> pFunc;
///     pFunc.pI = dlsym( hMod, "foobar" );
///     myFnPtr p = pFunc.pO;
///   }
/// }
/// \endcode
template<typename OutputPtr>
union function_cast
//-----------------------------------------------------------------------------
{
    void* pI;
    OutputPtr pO;
};

#endif // function_castH
