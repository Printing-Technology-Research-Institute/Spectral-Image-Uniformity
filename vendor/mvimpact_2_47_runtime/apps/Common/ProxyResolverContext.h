//-----------------------------------------------------------------------------
#ifndef ProxyResolverContextH
#define ProxyResolverContextH ProxyResolverContextH
//-----------------------------------------------------------------------------
#include <string>

//-----------------------------------------------------------------------------
class ProxyResolverContext
//-----------------------------------------------------------------------------
{
    struct ProxyResolverContextImpl* pImpl_;
public:
    explicit ProxyResolverContext( const std::wstring& userAgent = std::wstring(), const std::wstring& url = std::wstring() );
    ~ProxyResolverContext();

    std::wstring GetProxy( unsigned int index ) const;
    unsigned int GetProxyPort( unsigned int index ) const;
};

bool IsCurrentUserLocalAdministrator( void );

#endif // ProxyResolverContextH
