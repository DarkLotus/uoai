#ifndef UAC_INCLUDED
#define UAC_INCLUDED

/*
	Anything related to windows "User Account Control" and privilege management goes here.

	-- Wim Decelle
*/

#include "Features.h"
#include <windows.h>
#include "COMBase.h"

BOOL SetPrivilege(HANDLE hToken,LPCTSTR Privilege,BOOL bEnablePrivilege);//Enable or Disable a specific Privilege on the specified Token
BOOL GetPrivilege(LPCTSTR privilege);//Enable the specified Privilege on the current Thread's Token
BOOL GetDebugPrivilege();//Enable the SE_DEBUG privilege on the current Thread's Token

HRESULT CoCreateInstanceAsAdmin(REFCLSID rclsid, REFIID riid, void ** ppv);//CoCreateAsAdmin moniker <- taken from http://msdn.microsoft.com/nl-be/ms679687(en-us,VS.85).aspx

BOOL IsUserAdmin();
BOOL GetAccessPermissions_ots(SECURITY_DESCRIPTOR **ppSD);
BOOL GetLaunchActPermissionsWithIL (SECURITY_DESCRIPTOR **ppSD);

void cosec_init();

//ERRORS
#define UAC_ERROR 0x100

#endif