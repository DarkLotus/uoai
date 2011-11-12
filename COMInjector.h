#ifndef COMINJECTOR_INCLUDED
#define COMINJECTOR_INCLUDED

/*
	COMInjector is initiated using a COM Elevation moniker in a dllhost.
	The first time a UOAI object is created the COMInjector is started with admin rights in a dllhost.
	This object will periodically look for new ultima online clients and inject UOAI into those clients,
	onInjection a new client object is initiated within the client; which is then registered with the
	running clientlist object.
	This way, the COM subsystem takes care of all required IPC.

	-- Wim Decelle
*/

#include "Features.h"
#include "COMBase.h"

// {B25FAAF7-C0AA-4007-8C85-7693E02D59F0}
DEFINE_GUID(CLSID_COMInjector, 
0xb25faaf7, 0xc0aa, 0x4007, 0x8c, 0x85, 0x76, 0x93, 0xe0, 0x2d, 0x59, 0xf0);

// {486EC9D9-5FC9-469a-B50A-4EE7F758E0D3}
DEFINE_GUID(IID_ICOMInjector, 
0x486ec9d9, 0x5fc9, 0x469a, 0xb5, 0xa, 0x4e, 0xe7, 0xf7, 0x58, 0xe0, 0xd3);

COMCALL Initialize(COMObject * pThis, BSTR window_class_name, int parameter);

typedef struct COMInjectorVtblStruct
{
	DEFAULT_DISPATCH;
	pCOMCALL(Initialize)(COMObject * pThis, BSTR window_class_name, int parameter);
} COMInjectorVtbl;

#endif
