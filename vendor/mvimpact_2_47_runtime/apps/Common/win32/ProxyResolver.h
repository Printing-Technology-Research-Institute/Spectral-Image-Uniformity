#pragma once
#include <windows.h>
#include <winhttp.h>

#if _WIN32_WINNT < 0x0602 // This stuff became available with Windows 8
typedef struct _WINHTTP_PROXY_RESULT_ENTRY
{
    BOOL            fProxy;                // Is this a proxy or DIRECT?
    BOOL            fBypass;               // If DIRECT, is it bypassing a proxy (intranet) or is all traffic DIRECT (internet)
    INTERNET_SCHEME ProxyScheme;           // The scheme of the proxy, SOCKS, HTTP (CERN Proxy), HTTPS (SSL through Proxy)
    PWSTR           pwszProxy;             // Hostname of the proxy.
    INTERNET_PORT   ProxyPort;             // Port of the proxy.
} WINHTTP_PROXY_RESULT_ENTRY;

typedef struct _WINHTTP_PROXY_RESULT
{
    DWORD cEntries;
    WINHTTP_PROXY_RESULT_ENTRY* pEntries;
} WINHTTP_PROXY_RESULT;
#endif // #if _WIN32_WINNT < 0x0602

typedef DWORD ( WINAPI* PFNWINHTTPGETPROXYFORURLEX )( HINTERNET, PCWSTR, WINHTTP_AUTOPROXY_OPTIONS*, DWORD_PTR );
typedef DWORD ( WINAPI* PFNWINHTTPFREEPROXYLIST )( WINHTTP_PROXY_RESULT* );
typedef DWORD ( WINAPI* PFNWINHTTPCREATEPROXYRESOLVER )( HINTERNET, HINTERNET* );
typedef DWORD ( WINAPI* PFNWINHTTPGETPROXYRESULT )( HINTERNET, WINHTTP_PROXY_RESULT* );

class ProxyResolver
{
    BOOL m_fInit;
    BOOL m_fExtendedAPI;
    DWORD m_dwError;
    HANDLE m_hEvent;
    WINHTTP_PROXY_INFO m_wpiProxyInfo;
    WINHTTP_PROXY_RESULT m_wprProxyResult;

    BOOL IsRecoverableAutoProxyError( _In_ DWORD dwError );
    VOID static CALLBACK GetProxyCallBack( _In_  HINTERNET hResolver, _In_ DWORD_PTR dwContext, _In_ DWORD dwInternetStatus, _In_ PVOID pvStatusInformation, _In_ DWORD dwStatusInformationLength );
    DWORD GetProxyForUrlEx( _In_ HINTERNET hSession, _In_z_ PCWSTR pwszUrl, _In_ WINHTTP_AUTOPROXY_OPTIONS* pAutoProxyOptions );
    _Success_( return == ERROR_SUCCESS ) DWORD GetProxyForAutoSettings( _In_ HINTERNET hSession, _In_z_ PCWSTR pwszUrl, _In_opt_z_ PCWSTR pwszAutoConfigUrl, _Outptr_result_maybenull_ PWSTR* ppwszProxy, _Outptr_result_maybenull_ PWSTR* ppwszProxyBypass );

    static PFNWINHTTPGETPROXYFORURLEX s_pfnWinhttpGetProxyForUrlEx;
    static PFNWINHTTPFREEPROXYLIST s_pfnWinhttpFreeProxyList;
    static PFNWINHTTPCREATEPROXYRESOLVER s_pfnWinhttpCreateProxyResolver;
    static PFNWINHTTPGETPROXYRESULT s_pfnWinhttpGetProxyResult;

public:
    ProxyResolver();
    ~ProxyResolver();

    DWORD ResolveProxy( _In_ HINTERNET hSession, _In_z_ PCWSTR pwszUrl );
    const WINHTTP_PROXY_RESULT_ENTRY* GetProxySetting( const DWORD index ) const;
};
