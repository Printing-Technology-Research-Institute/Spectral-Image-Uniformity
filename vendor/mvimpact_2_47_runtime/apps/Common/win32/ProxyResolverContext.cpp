//-----------------------------------------------------------------------------
#include "../ProxyResolverContext.h"
#include "ProxyResolver.h"
#include <lmcons.h>

using namespace std;

//=============================================================================
//================= Implementation ProxyResolverContextImpl ===================
//=============================================================================
//-----------------------------------------------------------------------------
struct ProxyResolverContextImpl
//-----------------------------------------------------------------------------
{
    HINTERNET hProxyResolveSession;
    ProxyResolver* pProxyResolver;
    explicit ProxyResolverContextImpl( const wstring& userAgent ) : hProxyResolveSession( 0 ), pProxyResolver( 0 )
    {
        hProxyResolveSession = WinHttpOpen( userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,  WINHTTP_FLAG_ASYNC );
        if( hProxyResolveSession )
        {
            pProxyResolver = new ProxyResolver();
        }
    }
    ~ProxyResolverContextImpl()
    {
        delete pProxyResolver;
        if( hProxyResolveSession )
        {
            WinHttpCloseHandle( hProxyResolveSession );
        }
    }
};

//=============================================================================
//================= Implementation ProxyResolverContext =======================
//=============================================================================
//-----------------------------------------------------------------------------
ProxyResolverContext::ProxyResolverContext( const wstring& userAgent, const wstring& url ) : pImpl_( new ProxyResolverContextImpl( userAgent ) )
//-----------------------------------------------------------------------------
{
    if( pImpl_->pProxyResolver )
    {
        pImpl_->pProxyResolver->ResolveProxy( pImpl_->hProxyResolveSession, url.c_str() );
    }
}

//-----------------------------------------------------------------------------
ProxyResolverContext::~ProxyResolverContext()
//-----------------------------------------------------------------------------
{
    delete pImpl_;
}

//-----------------------------------------------------------------------------
wstring ProxyResolverContext::GetProxy( unsigned int index ) const
//-----------------------------------------------------------------------------
{
    const WINHTTP_PROXY_RESULT_ENTRY* pProxyData = pImpl_->pProxyResolver->GetProxySetting( index );
    return ( pProxyData && pProxyData->pwszProxy ) ? wstring( pProxyData->pwszProxy ) : wstring();
}

//-----------------------------------------------------------------------------
unsigned int ProxyResolverContext::GetProxyPort( unsigned int index ) const
//-----------------------------------------------------------------------------
{
    const WINHTTP_PROXY_RESULT_ENTRY* pProxyData = pImpl_->pProxyResolver->GetProxySetting( index );
    return pProxyData ? pProxyData->ProxyPort : 0;
}

//=============================================================================
//================= Implementation helper functions ===========================
//=============================================================================

//-----------------------------------------------------------------------------
/// \brief This function checks the token of the calling thread to see if the caller
/// belongs to the Administrators group.
/**
 * \return
   - true if the caller is an administrator on the local machine.
   - false otherwise
*/
bool IsCurrentUserLocalAdministrator( void )
//-----------------------------------------------------------------------------
{
    BOOL fReturn = FALSE;
    PACL pACL = NULL;
    PSID psidAdmin = NULL;
    HANDLE hToken = NULL;
    HANDLE hImpersonationToken = NULL;
    PSECURITY_DESCRIPTOR psdAdmin = NULL;


    // Determine if the current thread is running as a user that is a member of
    // the local admins group.  To do this, create a security descriptor that
    // has a DACL which has an ACE that allows only local administrators access.
    // Then, call AccessCheck with the current thread's token and the security
    // descriptor.  It will say whether the user could access an object if it
    // had that security descriptor.  Note: you do not need to actually create
    // the object.  Just checking access against the security descriptor alone
    // will be sufficient.
    const DWORD ACCESS_READ  = 1;
    const DWORD ACCESS_WRITE = 2;

    __try
    {
        // AccessCheck() requires an impersonation token.  We first get a primary
        // token and then create a duplicate impersonation token.  The
        // impersonation token is not actually assigned to the thread, but is
        // used in the call to AccessCheck.  Thus, this function itself never
        // impersonates, but does use the identity of the thread.  If the thread
        // was impersonating already, this function uses that impersonation context.
        if( !OpenThreadToken( GetCurrentThread(), TOKEN_DUPLICATE | TOKEN_QUERY, TRUE, &hToken ) )
        {
            if( GetLastError() != ERROR_NO_TOKEN )
            {
                __leave;
            }

            if( !OpenProcessToken( GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &hToken ) )
            {
                __leave;
            }
        }

        if( !DuplicateToken ( hToken, SecurityImpersonation, &hImpersonationToken ) )
        {
            __leave;
        }

        // Create the binary representation of the well-known SID that
        // represents the local administrators group.  Then create the security
        // descriptor and DACL with an ACE that allows only local admins access.
        // After that, perform the access check.  This will determine whether
        // the current user is a local admin.
        SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;
        if( !AllocateAndInitializeSid( &SystemSidAuthority, 2,
                                       SECURITY_BUILTIN_DOMAIN_RID,
                                       DOMAIN_ALIAS_RID_ADMINS,
                                       0, 0, 0, 0, 0, 0, &psidAdmin ) )
        {
            __leave;
        }

        psdAdmin = LocalAlloc( LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH );
        if( psdAdmin == NULL )
        {
            __leave;
        }

        if( !InitializeSecurityDescriptor( psdAdmin, SECURITY_DESCRIPTOR_REVISION ) )
        {
            __leave;
        }

        // Compute size needed for the ACL.
        const DWORD dwACLSize = sizeof( ACL ) + sizeof( ACCESS_ALLOWED_ACE ) + GetLengthSid( psidAdmin ) - sizeof( DWORD );
        pACL = ( PACL )LocalAlloc( LPTR, dwACLSize );
        if( pACL == NULL )
        {
            __leave;
        }

        if( !InitializeAcl( pACL, dwACLSize, ACL_REVISION2 ) )
        {
            __leave;
        }

        if( !AddAccessAllowedAce( pACL, ACL_REVISION2, ACCESS_READ | ACCESS_WRITE, psidAdmin ) )
        {
            __leave;
        }

        if( !SetSecurityDescriptorDacl( psdAdmin, TRUE, pACL, FALSE ) )
        {
            __leave;
        }

        // AccessCheck validates a security descriptor somewhat; set the group
        // and owner so that enough of the security descriptor is filled out to
        // make AccessCheck happy.
        SetSecurityDescriptorGroup( psdAdmin, psidAdmin, FALSE );
        SetSecurityDescriptorOwner( psdAdmin, psidAdmin, FALSE );

        if( !IsValidSecurityDescriptor( psdAdmin ) )
        {
            __leave;
        }

        // Initialize GenericMapping structure even though you do not use generic rights.
        GENERIC_MAPPING GenericMapping;
        GenericMapping.GenericRead    = ACCESS_READ;
        GenericMapping.GenericWrite   = ACCESS_WRITE;
        GenericMapping.GenericExecute = 0;
        GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;
        DWORD dwStructureSize = sizeof( PRIVILEGE_SET );
        DWORD dwStatus;
        PRIVILEGE_SET ps;
        if( !AccessCheck( psdAdmin, hImpersonationToken, ACCESS_READ,
                          &GenericMapping, &ps, &dwStructureSize, &dwStatus,
                          &fReturn ) )
        {
            fReturn = FALSE;
            __leave;
        }
    }
    __finally
    {
        if( pACL )
        {
            LocalFree( pACL );
        }
        if( psdAdmin )
        {
            LocalFree( psdAdmin );
        }
        if( psidAdmin )
        {
            FreeSid( psidAdmin );
        }
        if( hImpersonationToken )
        {
            CloseHandle ( hImpersonationToken );
        }
        if( hToken )
        {
            CloseHandle ( hToken );
        }
    }

    return fReturn != FALSE;
}
