#if defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // 'conditional expression is constant'
#   pragma warning( disable : 4244 ) // 'conversion from 'Bla' to 'Blub', possible loss of data
#   pragma warning( disable : 4251 ) // 'class 'Bla' needs to have dll-interface to be used by clients of class 'Blub''
#   pragma warning( disable : 4800 ) // 'int' : forcing value to bool 'true' or 'false' (performance warning)
#endif
