#ifndef UOAI_INCLUDED
#define UOAI_INCLUDED

/*
	Main UOAI class definitions.

	-- Wim Decelle
*/

#include "Features.h"
#include "COMBase.h"
#include "ClientList.h"
#include "COMInjector.h"

typedef enum
{
	MULTICLIENT_PATCH=2,
	ENCRYPTION_PATCH=4
} Patch;

// {61937D8B-BBCE-48b0-8ADF-F03C9443C7F2}
DEFINE_GUID(UOAI_TypeLib, 
0x61937d8b, 0xbbce, 0x48b0, 0x8a, 0xdf, 0xf0, 0x3c, 0x94, 0x43, 0xc7, 0xf2);

// {8A11E500-2101-4a27-BB16-5498C28F91DA}
DEFINE_GUID(CLSID_UOAI, 
0x8a11e500, 0x2101, 0x4a27, 0xbb, 0x16, 0x54, 0x98, 0xc2, 0x8f, 0x91, 0xda);

// {CD10E6BE-FDEE-4d59-A812-6FD08C02E8CE}
DEFINE_GUID(IID_IUOAI, 
0xcd10e6be, 0xfdee, 0x4d59, 0xa8, 0x12, 0x6f, 0xd0, 0x8c, 0x2, 0xe8, 0xce);

typedef struct UOAIStruct
{
	COMObject header;
	ClientListVtbl ** clientlist;
	COMInjectorVtbl ** injector;
} UOAI;

typedef struct UOAIVtblStruct
{
	DEFAULT_DISPATCH;
	pCOMCALL(GetClientList)(UOAI * pThis, IUnknown ** clients);
	pCOMCALL(GetClientWindowName)(UOAI * pThis, BSTR * ClientWindowName);
	pCOMCALL(SetClientWindowName)(UOAI * pThis, BSTR ClientWindowName);
	pCOMCALL(GetClienPath)(UOAI * pThis, BSTR * ClientPath);
	pCOMCALL(SetClientPath)(UOAI * pThis, BSTR ClientPath);
	pCOMCALL(GetClientExe)(UOAI * pThis, BSTR * ClientExe);
	pCOMCALL(SetClientExe)(UOAI * pThis, BSTR ClientExe);
	pCOMCALL(LaunchClient)(UOAI * pThis, IUnknown ** client, VARIANT_BOOL bApplyMultiClientPatch, VARIANT_BOOL bApplyEncryptionPatch, VARIANT_BOOL * bSuccess);
} UOAIVtbl;

void UOAI_constructor(UOAI * pThis);
void UOAI_destructor(UOAI * pThis);

COMCALL GetClientList(UOAI * pThis, IUnknown ** clients);
COMCALL GetClientWindowName(UOAI * pThis, BSTR * ClientWindowName);
COMCALL SetClientWindowName(UOAI * pThis, BSTR ClientWindowName);
COMCALL LaunchClient(UOAI * pThis, IUnknown ** client, VARIANT_BOOL bApplyMultiClientPatch, VARIANT_BOOL bApplyEncryptionPatch, VARIANT_BOOL * bSuccess);
COMCALL GetClientPath(UOAI * pThis, BSTR * ClientPath);
COMCALL SetClientPath(UOAI * pThis, BSTR ClientPath);
COMCALL GetClientExe(UOAI * pThis, BSTR * ClientExe);
COMCALL SetClientExe(UOAI * pThis, BSTR ClientExe);

typedef struct ClientListConfigStruct
{
	char client_window_classname[64];
	char client_path[256];
	char client_exe[256];
} UOAIConfig;

#endif