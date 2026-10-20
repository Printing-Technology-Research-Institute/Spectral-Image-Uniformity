#if defined(__clang__)
#   pragma clang diagnostic pop
#elif defined(_MSC_VER) && (_MSC_VER >= 1400) // is at least VC 2005 compiler?
#   pragma warning( pop )
#endif
