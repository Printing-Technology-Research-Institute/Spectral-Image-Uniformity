#if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wdeprecated-declarations"
#   pragma clang diagnostic ignored "-Wunused-local-typedef"
#elif defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
#   pragma warning( push )
#   pragma warning( disable : 4127 ) // 'conditional expression is constant'
#   pragma warning( disable : 4512 ) // 'class XYZ': assignment operator could not be generated'
#   pragma warning( disable : 4996 ) // ''function XYZ': was declared deprecated'
#endif
