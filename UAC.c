#include "UAC.h"
#include "Error.h"
#include "allocation.h"
#include <windows.h>
#include <stdio.h>
#include <Sddl.h>

/*
	Anything related to windows' "User Account Control" and privilege management goes here.
	-- Wim Decelle
*/

BOOL GetDebugPrivilege()
{
	return GetPrivilege(SE_DEBUG_NAME);
}

//Enable the specified Privilege on the current Thread's Token
//http://msdn.microsoft.com/en-us/library/ff541528(VS.85).aspx
BOOL GetPrivilege(LPCTSTR privilege)
{
	HANDLE hToken;

	if(!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken))
    {
        if (GetLastError() == ERROR_NO_TOKEN)
        {
            if (!ImpersonateSelf(SecurityImpersonation))
			{
				PushError(CreateError(UAC_ERROR, "Failed to obtain a token for the current thread and user!?",0,0));
				return FALSE;
			}

            if(!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken))
			{
				PushError(CreateError(UAC_ERROR, "Failed to open the current threads token with the required access rights!?",0,0));
				return FALSE;
			}
        }
        else
		{
			PushError(CreateError(UAC_ERROR, "Failed to open the current threads token with the required access rights!?",0,0));
            return FALSE;
		}
     }

    if(!SetPrivilege(hToken, privilege, TRUE))
    {
		//no error, errror should be pushed within SetPrivilege if it fails
        CloseHandle(hToken);
        return FALSE;
    }

	CloseHandle(hToken);
	return TRUE;
}

//Enable or Disable a specific Privilege on the specified Token
//http://msdn.microsoft.com/en-us/library/ff541528(VS.85).aspx
BOOL SetPrivilege(HANDLE hToken,LPCTSTR Privilege,BOOL bEnablePrivilege)
{
    TOKEN_PRIVILEGES tp;
    LUID luid;
    TOKEN_PRIVILEGES tpPrevious;
    DWORD cbPrevious=sizeof(TOKEN_PRIVILEGES);

	if(!LookupPrivilegeValue( NULL, Privilege, &luid ))
	{
		PushError(CreateError(UAC_ERROR, "Invalid privilege value specified!?",0,0));
		return FALSE;
	}
 
    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(hToken,FALSE,&tp,sizeof(TOKEN_PRIVILEGES),&tpPrevious,&cbPrevious);

    if (GetLastError() != ERROR_SUCCESS)
	{
		PushError(CreateError(UAC_ERROR, "Failed to obtain the current token's privileges!?",0,0));
		return FALSE;
	}

    tpPrevious.PrivilegeCount       = 1;
    tpPrevious.Privileges[0].Luid   = luid;

    if(bEnablePrivilege)
	{ 
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
    }
    else
	{
        tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED & tpPrevious.Privileges[0].Attributes);
    }

    AdjustTokenPrivileges(hToken,FALSE,&tpPrevious,cbPrevious,NULL,NULL);

    if (GetLastError() != ERROR_SUCCESS)
	{
		PushError(CreateError(UAC_ERROR, "Failed to adjust the current token's privileges!?",0,0));
		return FALSE;
	}

    return TRUE;
}

//http://msdn.microsoft.com/nl-be/ms679687(en-us,VS.85).aspx
HRESULT CoCreateInstanceAsAdmin(REFCLSID rclsid, REFIID riid, void ** ppv)
{
    BIND_OPTS2 bo;
    unsigned short wszCLSID[64];
	char * cclsid;
	char cmoniker[256];
    unsigned short * wszMonikerName;
	HRESULT hr;

    StringFromGUID2(rclsid, wszCLSID, 50);
	cclsid=ole2char(wszCLSID);
	sprintf(cmoniker, "Elevation:Administrator!new:%s", cclsid);

	clean(cclsid);
	wszMonikerName=(unsigned short *)char2ole(cmoniker);
    memset(&bo, 0, sizeof(bo));
    bo.cbStruct = sizeof(bo);
    bo.dwClassContext  = CLSCTX_LOCAL_SERVER;

    hr=CoGetObject(wszMonikerName, (BIND_OPTS *)&bo, riid, ppv);
	clean(wszMonikerName);
	return hr;
}

//CheckTokenMembership is required, so if the Windows SDK headers are too old to know about this function, 
// then we link to it dynamically
//	<- note that this implies this code won't run on windows versions < Windows 2000
#if WINVER < 0x500
//note that WINVER < 0x500 is actually not supported (i.e. the compiled code won't run)
//	but when compiling with old sdk headers the WINVER >= 0x500 features are not in the headers
//	so I link to them dynamically
//	So basically this allows compilation with WINVER < 0x500, but the compiled assembly still requires
//	Windows 2000 or higher versions to run.
//	Reason is that i've been coding using Visual Studio 6, which ships by default with the old headers.
//	Final deployed assembly is typically compiled using the latest windows SDK headers though.
HMODULE advapi=0;

typedef BOOL (WINAPI * pCheckTokenMembership)(HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember);
pCheckTokenMembership CheckTokenMembership=0;
typedef BOOL (WINAPI * pConvertStringSecurityDescriptorToSecurityDescriptorW)(LPWSTR StringSecurityDescriptor, DWORD StringSDRevision, PSECURITY_DESCRIPTOR *SecurityDescriptor, PULONG SecurityDescriptorSize);
pConvertStringSecurityDescriptorToSecurityDescriptorW ConvertStringSecurityDescriptorToSecurityDescriptorW;


void advapi_inits()
{
	if(advapi=LoadLibrary("Advapi32.dll"))
	{
		CheckTokenMembership=(pCheckTokenMembership)GetProcAddress(advapi, "CheckTokenMembership");
		ConvertStringSecurityDescriptorToSecurityDescriptorW=(pConvertStringSecurityDescriptorToSecurityDescriptorW)GetProcAddress(advapi, "ConvertStringSecurityDescriptorToSecurityDescriptorW");
	}
}
#endif

//http://msdn.microsoft.com/en-us/library/aa376389(VS.85).aspx
BOOL IsUserAdmin()
{
	BOOL b;	
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;	

#if WINVER < 0x500
	advapi_inits();
#endif
	
	b = AllocateAndInitializeSid(&NtAuthority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0, 0, 0, 0, 0, 0,&AdministratorsGroup);
	if(b) {
		if (!CheckTokenMembership(NULL, AdministratorsGroup, &b))
		{
			b = FALSE;
		}		FreeSid(AdministratorsGroup);
	}	
	return b;
}

SECURITY_DESCRIPTOR * ots_pSD=0;
SECURITY_DESCRIPTOR * il_pSD=0;

//see OTS part at: http://msdn.microsoft.com/nl-be/ms679687(en-us,VS.85).aspx
BOOL GetAccessPermissions_ots(SECURITY_DESCRIPTOR **ppSD)
{
	// Local call permissions to IU, SY
    LPWSTR lpszSDDL;
    SECURITY_DESCRIPTOR *pSD;
    
	lpszSDDL = L"O:BAG:BAD:(A;;0x3;;;IU)(A;;0x3;;;SY)";
	*ppSD = NULL;

	if(ots_pSD!=0)
	{
		(*ppSD)=ots_pSD;
		return TRUE;
	}

#if WINVER < 0x500
	advapi_inits();
#endif

    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(lpszSDDL, 1, (PSECURITY_DESCRIPTOR *)&pSD, NULL))
    {
        *ppSD = pSD;
		ots_pSD = pSD;
        return TRUE;
    }

    return FALSE;
}

BOOL GetLaunchActPermissionsWithIL (SECURITY_DESCRIPTOR **ppSD)
{
	// Allow World Local Launch/Activation permissions. Label the SD for LOW IL Execute UP
    LPWSTR lpszSDDL;
	SECURITY_DESCRIPTOR *pSD;

#if WINVER < 0x500
	advapi_inits();
#endif

	*ppSD=NULL;

	if(il_pSD!=0)
	{
		(*ppSD)=il_pSD;
		return TRUE;
	}

	lpszSDDL = L"O:BAG:BAD:(A;;0xb;;;WD)S:(ML;;NX;;;LW)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(lpszSDDL, 1, (PSECURITY_DESCRIPTOR *)&pSD, NULL))
    {
        *ppSD = pSD;
		il_pSD = pSD;
        return TRUE;
    }

	return FALSE;
}
