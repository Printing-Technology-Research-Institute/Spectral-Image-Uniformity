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
#ifndef STLHelperH
#define STLHelperH STLHelperH
//-----------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

//=============================================================================
//==================== std::pair related stuff ================================
//=============================================================================
//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
const _Ty1& GetFirst( const std::pair<_Ty1, _Ty2>& data )
//-----------------------------------------------------------------------------
{
    return data.first;
}

//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
const _Ty2& GetSecond( const std::pair<_Ty1, _Ty2>& data )
//-----------------------------------------------------------------------------
{
    return data.second;
}

//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
void DeleteFirst( std::pair<_Ty1, _Ty2>& data )
//-----------------------------------------------------------------------------
{
    delete data.first;
    data.first = 0;
}

//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
void DeleteSecond( std::pair<_Ty1, _Ty2>& data )
//-----------------------------------------------------------------------------
{
    delete data.second;
    data.second = 0;
}

//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
class ContainsFirst : public std::unary_function<std::pair<_Ty1, _Ty2>, bool>
//-----------------------------------------------------------------------------
{
    std::map<_Ty1, _Ty2> m_;
public:
    explicit ContainsFirst( const std::map<_Ty1, _Ty2>& m ) : m_( m ) {}
    bool operator()( const std::pair<_Ty1, _Ty2>& x )
    {
        return m_.find( x.first ) != m_.end();
    }
};

//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
class FirstMatches : public std::unary_function<std::pair<_Ty1, _Ty2>, bool>
//-----------------------------------------------------------------------------
{
    std::pair<_Ty1, _Ty2> value_;
    FirstMatches<_Ty1, _Ty2>& operator=( const FirstMatches<_Ty1, _Ty2>& ); // do not allow assignments
public:
    explicit FirstMatches( const std::pair<_Ty1, _Ty2>& val ) : value_( val ) {}
    FirstMatches( const FirstMatches<_Ty1, _Ty2>& src ) : value_( src.value_ ) {}
    bool operator()( const std::pair<_Ty1, _Ty2>& x )
    {
        return x.first == value_.first;
    }
};

//-----------------------------------------------------------------------------
template<class _Ty1, class _Ty2>
class SecondMatches : public std::unary_function<std::pair<_Ty1, _Ty2>, bool>
//-----------------------------------------------------------------------------
{
    std::pair<_Ty1, _Ty2> value_;
    SecondMatches<_Ty1, _Ty2>& operator=( const SecondMatches<_Ty1, _Ty2>& );   // do not allow assignments
public:
    explicit SecondMatches( const std::pair<_Ty1, _Ty2>& val ) : value_( val ) {}
    SecondMatches( const FirstMatches<_Ty1, _Ty2>& src ) : value_( src.value_ ) {}
    bool operator()( const std::pair<_Ty1, _Ty2>& x )
    {
        return x.second == value_.second;
    }
};

//-----------------------------------------------------------------------------
template<typename _Ty1, typename _Ty2>
bool SecondSmaller( const std::pair<_Ty1, _Ty2>& a, const std::pair<_Ty1, _Ty2>& b )
//-----------------------------------------------------------------------------
{
    if( a.second < b.second )
    {
        return true;
    }

    if( a.second > b.second )
    {
        return false;
    }

    return ( a.first < b.first );
}

//=============================================================================
//==================== various ================================================
//=============================================================================
//-----------------------------------------------------------------------------
template<class _Ty>
void DeleteElement( _Ty& data )
//-----------------------------------------------------------------------------
{
    delete data;
    data = 0;
}

//-----------------------------------------------------------------------------
template<class _Ty>
void DeleteArrayElement( _Ty& data )
//-----------------------------------------------------------------------------
{
    delete [] data;
    data = 0;
}

//-----------------------------------------------------------------------------
/// \brief Proxy reference for ObjectDeleter copying.
template<class _Ty>
struct ObjectDeleterReference
//-----------------------------------------------------------------------------
{
    // construct from generic pointer to ObjectDeleter pointer
    explicit ObjectDeleterReference( _Ty* rhs ) : pReference_( rhs ) {}

    _Ty* pReference_;  // generic pointer to ObjectDeleter pointer
};

//-----------------------------------------------------------------------------
/// \brief Simple wrapper class to make sure an object is deleted at the end of the scope
/**
 * This of course could also be done using std::auto_ptr but as this template has been
 * declared deprecated it causes new compilers to spit out warnings/failures.
 *
 * The nice way of doing this would be by the use of std::unique_ptr these days but then
 * the client code would force people to have a C++11 or greater compliant compiler...
 */
template<class _Ty>
class ObjectDeleter
//-----------------------------------------------------------------------------
{
    _Ty* pObject_;
public:
    explicit ObjectDeleter( _Ty* pObject = 0 ) : pObject_( pObject ) {}
    /// \brief Pass ownership to the new object.
    ObjectDeleter( ObjectDeleter<_Ty>& rhs ) : pObject_( rhs.release() ) {}
    ~ObjectDeleter()
    {
        DeleteElement( pObject_ );
    }
    /// \brief Pass ownership. Old buffer on the left hand side object is
    /// freed, the lhs object takes ownership of the pointer of the rhs object.
    ObjectDeleter<_Ty>& operator=( ObjectDeleter<_Ty>& rhs )
    {
        if( this != &rhs )
        {
            DeleteElement( pObject_ );
            pObject_ = rhs.release();
        }
        return *this;
    }
    template<class _Other>
    operator ObjectDeleterReference<_Other>()
    {
        // convert to compatible ObjectDeleterReference
        _Other* p = pObject_; // test implicit conversion
        ObjectDeleterReference<_Other> reference( p );
        pObject_ = 0; // pass ownership to ObjectDeleterReference
        return ( reference );
    }
    ObjectDeleter<_Ty>& operator=( ObjectDeleterReference<_Ty> rhs )
    {
        // assign compatible rhs.pReference_ (assume pointer)
        _Ty* p = rhs.pReference_;
        rhs.pReference_ = 0;
        reset( p );
        return ( *this );
    }
    operator _Ty* ()
    {
        return pObject_;
    }
    _Ty* operator->()
    {
        return pObject_;
    }
    _Ty* get( void )
    {
        return pObject_;
    }
    const _Ty* get( void ) const
    {
        return pObject_;
    }
    _Ty* release( void )
    {
        _Ty* p = pObject_;
        pObject_ = 0;
        return p;
    }
    void reset( _Ty* pObject = 0 )
    {
        if( pObject_ != pObject )
        {
            DeleteElement( pObject_ );
            pObject_ = pObject;
        }
    }
};

//-----------------------------------------------------------------------------
/// This could in theory be accomplished with a call to the for_each template:
///
/// for_each( s.begin(), s.end(), ptr_fun(DeleteElement<_Ty*>) );
///
/// But some ports of the STL always return const_iterators for sets (e.g. gcc 4.1.2)
template<class _Ty>
void ClearSetWithHeapAllocatedKeys( std::set<_Ty>& s )
//-----------------------------------------------------------------------------
{
    typename std::set<_Ty>::iterator it = s.begin();
    typename std::set<_Ty>::iterator itEnd = s.end();
    while( it != itEnd )
    {
        delete *it;
        ++it;
    }
    s.clear();
}

//-----------------------------------------------------------------------------
/// \brief Assigns a new value to a variable when this objects goes out of scope.
///
/// Can be useful if a variable must be set to a defined value at the end of a
/// code block that might rise an exception.
template<class T>
class VarScopeMod
//-----------------------------------------------------------------------------
{
    T& var_;
    T valAtEndOfScope_;
    VarScopeMod( const VarScopeMod& );              // do not allow copy constructor
    VarScopeMod& operator=( const VarScopeMod& );   // do not allow assignments
public:
    explicit VarScopeMod( T& var, T valAtEndOfScope ) : var_( var ), valAtEndOfScope_( valAtEndOfScope ) {}
    explicit VarScopeMod( T& var, T valWithinScope, T valAtEndOfScope ) : var_( var ), valAtEndOfScope_( valAtEndOfScope )
    {
        var = valWithinScope;
    }
    ~VarScopeMod()
    {
        var_ = valAtEndOfScope_;
    }
};

//-----------------------------------------------------------------------------
template<class T>
void removeDuplicates( T& container )
//-----------------------------------------------------------------------------
{
    std::sort( container.begin(), container.end() );
    typename T::iterator it = std::unique( container.begin(), container.end() );
    container.erase( it, container.end() );
}

//-----------------------------------------------------------------------------
template<class _Elem>
_Elem mv_tolower( _Elem c )
//-----------------------------------------------------------------------------
{
    return static_cast<_Elem>( tolower( c ) );
}

//-----------------------------------------------------------------------------
template<class _Elem, class _Traits, class _Ax>
void makeLowerCase(
    /// The string to convert
    std::basic_string<_Elem, _Traits, _Ax>& s )
//-----------------------------------------------------------------------------
{
    std::transform( s.begin(), s.end(), s.begin(), std::ptr_fun<_Elem, _Elem>( mv_tolower ) );
}

//-----------------------------------------------------------------------------
template<class _Elem, class _Traits, class _Ax>
std::basic_string<_Elem, _Traits, _Ax> makeLowerCase(
    /// The string to convert
    const std::basic_string<_Elem, _Traits, _Ax>& s )
//-----------------------------------------------------------------------------
{
    std::basic_string<_Elem, _Traits, _Ax> lc( s );
    std::transform( lc.begin(), lc.end(), lc.begin(), std::ptr_fun<_Elem, _Elem>( mv_tolower ) );
    return lc;
}

//-----------------------------------------------------------------------------
template<class _Elem>
_Elem mv_toupper( _Elem c )
//-----------------------------------------------------------------------------
{
    return static_cast<_Elem>( toupper( c ) );
}

//-----------------------------------------------------------------------------
template<class _Elem, class _Traits, class _Ax>
void makeUpperCase(
    /// The string to convert
    std::basic_string<_Elem, _Traits, _Ax>& s )
//-----------------------------------------------------------------------------
{
    std::transform( s.begin(), s.end(), s.begin(), std::ptr_fun<_Elem, _Elem>( mv_toupper ) );
}

//-----------------------------------------------------------------------------
template<class _Elem, class _Traits, class _Ax>
std::basic_string<_Elem, _Traits, _Ax> makeUpperCase(
    /// The string to convert
    const std::basic_string<_Elem, _Traits, _Ax>& s )
//-----------------------------------------------------------------------------
{
    std::basic_string<_Elem, _Traits, _Ax> uc( s );
    std::transform( uc.begin(), uc.end(), uc.begin(), std::ptr_fun<_Elem, _Elem>( mv_toupper ) );
    return uc;
}

#endif // STLHelperH
