#include "ProxyResolver.h"

#if _WIN32_WINNT < 0x0602 // This stuff became available with Windows 8
#   define WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE 0x01000000
#   define WINHTTP_CALLBACK_FLAG_GETPROXYFORURL_COMPLETE WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE
#   define API_GET_PROXY_FOR_URL         (6)
#endif // #if _WIN32_WINNT < _WIN32_WINNT_WIN8

PFNWINHTTPGETPROXYFORURLEX ProxyResolver::s_pfnWinhttpGetProxyForUrlEx = NULL;
PFNWINHTTPFREEPROXYLIST ProxyResolver::s_pfnWinhttpFreeProxyList = NULL;
PFNWINHTTPCREATEPROXYRESOLVER ProxyResolver::s_pfnWinhttpCreateProxyResolver = NULL;
PFNWINHTTPGETPROXYRESULT ProxyResolver::s_pfnWinhttpGetProxyResult = NULL;

//-----------------------------------------------------------------------------
ProxyResolver::ProxyResolver() : m_fInit( FALSE ), m_fExtendedAPI( FALSE ), m_dwError( ERROR_SUCCESS ), m_hEvent( 0 )
//-----------------------------------------------------------------------------
{
    ZeroMemory( &m_wprProxyResult, sizeof( WINHTTP_PROXY_RESULT ) );
    ZeroMemory( &m_wpiProxyInfo, sizeof( WINHTTP_PROXY_INFO ) );

    HMODULE hWinhttp = GetModuleHandle( L"winhttp.dll" );
    if( hWinhttp != NULL )
    {
        s_pfnWinhttpGetProxyForUrlEx = ( PFNWINHTTPGETPROXYFORURLEX )GetProcAddress( hWinhttp, "WinHttpGetProxyForUrlEx" );
        s_pfnWinhttpFreeProxyList = ( PFNWINHTTPFREEPROXYLIST )GetProcAddress( hWinhttp, "WinHttpFreeProxyResult" );
        s_pfnWinhttpCreateProxyResolver = ( PFNWINHTTPCREATEPROXYRESOLVER )GetProcAddress( hWinhttp, "WinHttpCreateProxyResolver" );
        s_pfnWinhttpGetProxyResult = ( PFNWINHTTPGETPROXYRESULT )GetProcAddress( hWinhttp, "WinHttpGetProxyResult" );
    }
    m_fExtendedAPI = s_pfnWinhttpGetProxyForUrlEx && s_pfnWinhttpFreeProxyList && s_pfnWinhttpCreateProxyResolver && s_pfnWinhttpGetProxyResult;
}

//-----------------------------------------------------------------------------
ProxyResolver::~ProxyResolver()
//-----------------------------------------------------------------------------
{
    if( m_wpiProxyInfo.lpszProxy != NULL )
    {
        GlobalFree( m_wpiProxyInfo.lpszProxy );
    }

    if( m_wpiProxyInfo.lpszProxyBypass != NULL )
    {
        GlobalFree( m_wpiProxyInfo.lpszProxyBypass );
    }

    if( m_fExtendedAPI )
    {
        s_pfnWinhttpFreeProxyList( &m_wprProxyResult );
    }

    if( m_hEvent != NULL )
    {
        CloseHandle( m_hEvent );
    }
}

//-----------------------------------------------------------------------------
BOOL ProxyResolver::IsRecoverableAutoProxyError( _In_ DWORD dwError )
//-----------------------------------------------------------------------------
{
    switch( dwError )
    {
    case ERROR_SUCCESS:
    case ERROR_INVALID_PARAMETER:
    case ERROR_WINHTTP_AUTO_PROXY_SERVICE_ERROR:
    case ERROR_WINHTTP_AUTODETECTION_FAILED:
    case ERROR_WINHTTP_BAD_AUTO_PROXY_SCRIPT:
    case ERROR_WINHTTP_LOGIN_FAILURE:
    case ERROR_WINHTTP_OPERATION_CANCELLED:
    case ERROR_WINHTTP_TIMEOUT:
    case ERROR_WINHTTP_UNABLE_TO_DOWNLOAD_SCRIPT:
    case ERROR_WINHTTP_UNRECOGNIZED_SCHEME:
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

//-----------------------------------------------------------------------------
VOID CALLBACK ProxyResolver::GetProxyCallBack( _In_ HINTERNET hResolver, _In_ DWORD_PTR dwContext, _In_ DWORD dwInternetStatus, _In_ PVOID pvStatusInformation, _In_ DWORD /*dwStatusInformationLength*/ )
//-----------------------------------------------------------------------------
{
    ProxyResolver* pProxyResolver = ( ProxyResolver* )dwContext;
    if( ( dwInternetStatus != WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE &&
          dwInternetStatus != WINHTTP_CALLBACK_STATUS_REQUEST_ERROR ) ||
        pProxyResolver == NULL )
    {
        return;
    }

    if( dwInternetStatus == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR )
    {
        WINHTTP_ASYNC_RESULT* pAsyncResult = ( WINHTTP_ASYNC_RESULT* )pvStatusInformation;

        if( pAsyncResult->dwResult != API_GET_PROXY_FOR_URL )
        {
            return;
        }

        pProxyResolver->m_dwError = pAsyncResult->dwError;
    }
    else if( dwInternetStatus == WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE )
    {
        pProxyResolver->m_dwError = s_pfnWinhttpGetProxyResult( hResolver, &pProxyResolver->m_wprProxyResult );
    }

    if( hResolver != NULL )
    {
        WinHttpCloseHandle( hResolver );
        hResolver = NULL;
    }

    SetEvent( pProxyResolver->m_hEvent );
}

#define CLOSE_RESOLVER_HANDLE_AND_RETURN_ERROR_CODE(HRESOLVER, ERROR_CODE) \
    if( HRESOLVER != NULL ) \
    { \
        WinHttpCloseHandle( HRESOLVER ); \
    } \
    return ERROR_CODE

//-----------------------------------------------------------------------------
DWORD ProxyResolver::GetProxyForUrlEx( _In_ HINTERNET hSession, _In_z_ PCWSTR pwszUrl, _In_ WINHTTP_AUTOPROXY_OPTIONS* pAutoProxyOptions )
//-----------------------------------------------------------------------------
{
    // Create proxy resolver handle. It's best to close the handle during call back.
    HINTERNET hResolver = NULL;
    DWORD dwError = s_pfnWinhttpCreateProxyResolver( hSession, &hResolver );
    if( dwError != ERROR_SUCCESS )
    {
        CLOSE_RESOLVER_HANDLE_AND_RETURN_ERROR_CODE( hResolver, dwError );
    }

    // Sets up a callback function that WinHTTP can call as proxy results are resolved.
    WINHTTP_STATUS_CALLBACK wscCallback = WinHttpSetStatusCallback( hResolver, GetProxyCallBack, WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_GETPROXYFORURL_COMPLETE, 0 );
    if( wscCallback == WINHTTP_INVALID_STATUS_CALLBACK )
    {
        dwError = GetLastError();
        CLOSE_RESOLVER_HANDLE_AND_RETURN_ERROR_CODE( hResolver, dwError );
    }

    // The extended API works in asynchronous mode, therefore wait until the
    // results are set in the call back function.
    dwError = s_pfnWinhttpGetProxyForUrlEx( hResolver, pwszUrl, pAutoProxyOptions, ( DWORD_PTR )this );
    if( dwError != ERROR_IO_PENDING )
    {
        CLOSE_RESOLVER_HANDLE_AND_RETURN_ERROR_CODE( hResolver, dwError );
    }

    // The resolver handle will get closed in the callback and cannot be used any longer.
    hResolver = NULL;
    dwError = WaitForSingleObjectEx( m_hEvent, INFINITE, FALSE );
    if( dwError != WAIT_OBJECT_0 )
    {
        return GetLastError();
    }
    return m_dwError;
}

//-----------------------------------------------------------------------------
_Success_( return == ERROR_SUCCESS ) DWORD ProxyResolver::GetProxyForAutoSettings( _In_ HINTERNET hSession, _In_z_ PCWSTR pwszUrl, _In_opt_z_ PCWSTR pwszAutoConfigUrl, _Outptr_result_maybenull_ PWSTR* ppwszProxy, _Outptr_result_maybenull_ PWSTR* ppwszProxyBypass )
//-----------------------------------------------------------------------------
{
    DWORD dwError = ERROR_SUCCESS;
    WINHTTP_AUTOPROXY_OPTIONS waoOptions = {};
    WINHTTP_PROXY_INFO wpiProxyInfo = {};

    *ppwszProxy = NULL;
    *ppwszProxyBypass = NULL;

    if( pwszAutoConfigUrl )
    {
        waoOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
        waoOptions.lpszAutoConfigUrl = pwszAutoConfigUrl;
    }
    else
    {
        waoOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
        waoOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    }

    // First call with no autologon. Autologon prevents the
    // session (in proc) or autoproxy service (out of proc) from caching
    // the proxy script. This causes repetitive network traffic, so it is
    // best not to do autologon unless it is required according to the
    // result of WinHttpGetProxyForUrl.
    // This applies to both WinHttpGetProxyForUrl and WinhttpGetProxyForUrlEx.
    if( m_fExtendedAPI )
    {
        m_hEvent = CreateEventEx( NULL, NULL, 0, EVENT_ALL_ACCESS );
        if( m_hEvent == NULL )
        {
            dwError = GetLastError();
            goto quit;
        }

        dwError = GetProxyForUrlEx( hSession, pwszUrl, &waoOptions );
        if( dwError != ERROR_WINHTTP_LOGIN_FAILURE )
        {
            // Unless we need to retry with auto-logon exit the function with the
            // result, on success the proxy list will be stored in m_wprProxyResult
            // by GetProxyCallBack.
            goto quit;
        }

        // Enable autologon if challenged.
        waoOptions.fAutoLogonIfChallenged = TRUE;
        dwError = GetProxyForUrlEx( hSession, pwszUrl, &waoOptions );
        goto quit;
    }

    if( !WinHttpGetProxyForUrl( hSession, pwszUrl, &waoOptions, &wpiProxyInfo ) )
    {
        dwError = GetLastError();
        if( dwError != ERROR_WINHTTP_LOGIN_FAILURE )
        {
            goto quit;
        }

        // Enable autologon if challenged.
        dwError = ERROR_SUCCESS;
        waoOptions.fAutoLogonIfChallenged = TRUE;
        if( !WinHttpGetProxyForUrl( hSession, pwszUrl, &waoOptions, &wpiProxyInfo ) )
        {
            dwError = GetLastError();
            goto quit;
        }
    }

    *ppwszProxy = wpiProxyInfo.lpszProxy;
    wpiProxyInfo.lpszProxy = NULL;

    *ppwszProxyBypass = wpiProxyInfo.lpszProxyBypass;
    wpiProxyInfo.lpszProxyBypass = NULL;

quit:

    if( wpiProxyInfo.lpszProxy )
    {
        GlobalFree( wpiProxyInfo.lpszProxy );
        wpiProxyInfo.lpszProxy = NULL;
    }

    if( wpiProxyInfo.lpszProxyBypass )
    {
        GlobalFree( wpiProxyInfo.lpszProxyBypass );
        wpiProxyInfo.lpszProxyBypass = NULL;
    }

    return dwError;
}

//-----------------------------------------------------------------------------
DWORD ProxyResolver::ResolveProxy( _In_ HINTERNET hSession, _In_z_ PCWSTR pwszUrl )
//-----------------------------------------------------------------------------
{
    DWORD dwError = ERROR_SUCCESS;
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ProxyConfig = {};
    PWSTR pwszProxy = NULL;
    PWSTR pwszProxyBypass = NULL;
    BOOL fFailOverValid = FALSE;

    if( m_fInit )
    {
        dwError = ERROR_INVALID_OPERATION;
        goto quit;
    }

    if( !WinHttpGetIEProxyConfigForCurrentUser( &ProxyConfig ) )
    {
        dwError = GetLastError();
        if( dwError != ERROR_FILE_NOT_FOUND )
        {
            goto quit;
        }

        // No IE proxy settings found, just do autodetect.
        ProxyConfig.fAutoDetect = TRUE;
        dwError = ERROR_SUCCESS;
    }

    // Begin processing the proxy settings in the following order:
    //  1) Auto-Detect if configured.
    //  2) Auto-Config URL if configured.
    //  3) Static Proxy Settings if configured.
    //
    // Once any of these methods succeed in finding a proxy we are finished.
    // In the event one mechanism fails with an expected error code it is
    // required to fall back to the next mechanism. If the request fails
    // after exhausting all detected proxies, there should be no attempt
    // to discover additional proxies.
    if( ProxyConfig.fAutoDetect )
    {
        fFailOverValid = TRUE;
        // Detect Proxy Settings.
        dwError = GetProxyForAutoSettings( hSession, pwszUrl, NULL, &pwszProxy, &pwszProxyBypass );
        if( dwError == ERROR_SUCCESS )
        {
            goto commit;
        }

        if( !IsRecoverableAutoProxyError( dwError ) )
        {
            goto quit;
        }

        // Fall back to Autoconfig URL or Static settings. An application can
        // optionally take some action such as logging, or creating a mechanism
        // to expose multiple error codes in the class.
        dwError = ERROR_SUCCESS;
    }

    if( ProxyConfig.lpszAutoConfigUrl )
    {
        fFailOverValid = TRUE;
        // Run autoproxy with AutoConfig URL.
        dwError = GetProxyForAutoSettings( hSession, pwszUrl, ProxyConfig.lpszAutoConfigUrl, &pwszProxy, &pwszProxyBypass );
        if( dwError == ERROR_SUCCESS )
        {
            goto commit;
        }

        if( !IsRecoverableAutoProxyError( dwError ) )
        {
            goto quit;
        }

        // Fall back to Static Settings. An application can optionally take some
        // action such as logging, or creating a mechanism to to expose multiple
        // error codes in the class.
        dwError = ERROR_SUCCESS;
    }

    fFailOverValid = FALSE;
    // Static Proxy Config. Failover is not valid for static proxy since
    // it is always either a single proxy or a list containing protocol
    // specific proxies such as "proxy" or http=httpproxy;https=sslproxy
    pwszProxy = ProxyConfig.lpszProxy;
    ProxyConfig.lpszProxy = NULL;

    pwszProxyBypass = ProxyConfig.lpszProxyBypass;
    ProxyConfig.lpszProxyBypass = NULL;

commit:

    if( pwszProxy == NULL )
    {
        m_wpiProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
    }
    else
    {
        m_wpiProxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    }

    m_wpiProxyInfo.lpszProxy = pwszProxy;
    pwszProxy = NULL;

    m_wpiProxyInfo.lpszProxyBypass = pwszProxyBypass;
    pwszProxyBypass = NULL;

    m_fInit = TRUE;

quit:

    if( pwszProxy != NULL )
    {
        GlobalFree( pwszProxy );
        pwszProxy = NULL;
    }

    if( pwszProxyBypass != NULL )
    {
        GlobalFree( pwszProxyBypass );
        pwszProxyBypass = NULL;
    }

    if( ProxyConfig.lpszAutoConfigUrl != NULL )
    {
        GlobalFree( ProxyConfig.lpszAutoConfigUrl );
        ProxyConfig.lpszAutoConfigUrl = NULL;
    }

    if( ProxyConfig.lpszProxy != NULL )
    {
        GlobalFree( ProxyConfig.lpszProxy );
        ProxyConfig.lpszProxy = NULL;
    }

    if( ProxyConfig.lpszProxyBypass != NULL )
    {
        GlobalFree( ProxyConfig.lpszProxyBypass );
        ProxyConfig.lpszProxyBypass = NULL;
    }

    return dwError;
}

//-----------------------------------------------------------------------------
const WINHTTP_PROXY_RESULT_ENTRY* ProxyResolver::GetProxySetting( const DWORD index ) const
//-----------------------------------------------------------------------------
{
    if( index < m_wprProxyResult.cEntries )
    {
        return &m_wprProxyResult.pEntries[index];
    }
    return 0;
}
