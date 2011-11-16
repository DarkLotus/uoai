#include "UOAITest.h"

typedef void (*pDllRegisterServer)();

#include <windows.h>

#include <objbase.h>
#include <OCIdl.h>
#include <Oleauto.h>
#include <olectl.h>

#include <InitGuid.h>

DEFINE_GUID(CLSID_UOAI, 
0x8a11e500, 0x2101, 0x4a27, 0xbb, 0x16, 0x54, 0x98, 0xc2, 0x8f, 0x91, 0xda);

DEFINE_GUID(IID_IUOAI, 
0xcd10e6be, 0xfdee, 0x4d59, 0xa8, 0x12, 0x6f, 0xd0, 0x8c, 0x2, 0xe8, 0xce);

DEFINE_GUID(CLSID_ClientList, 
0xbe02c37f, 0x111e, 0x402e, 0x9d, 0x5, 0x62, 0x1c, 0x3d, 0x32, 0x87, 0x24);

DEFINE_GUID(IID_IClientList, 
0x39fb3c1b, 0xef1f, 0x4e30, 0xaf, 0x63, 0x83, 0xcb, 0x91, 0xb9, 0xff, 0xa8);

DEFINE_GUID(IID_IClientListEvents, 
0xb327cdfd, 0x2742, 0x42c7, 0xa7, 0x81, 0x50, 0x84, 0xee, 0x6, 0xdc, 0xdc);

int main()
{
	HMODULE hmod;
	pDllRegisterServer regsvr;
	pDllRegisterServer unregsvr;
	void (*testfunc)();

	if(hmod=LoadLibrary("UOAI.dll"))
	{
		if(testfunc = (void (*)())GetProcAddress(hmod, "testfunc"))
		{
			testfunc();
		}

		if(unregsvr=(pDllRegisterServer)GetProcAddress(hmod, "DllUnregisterServer"))
		{
			unregsvr();
		}

		if(regsvr=(pDllRegisterServer)GetProcAddress(hmod, "DllRegisterServer"))
		{
			regsvr();
		}
	}

	return 0;
}