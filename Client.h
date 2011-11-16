#ifndef CLIENT_INCLUDED
#define CLIENT_INCLUDED

/*
	UOAI - Ultima Online Client Automation object

	Client objects are created from within the ultima online client proccess after Injection of the UOAI dll (see onInjection).

	-- Wim Decelle
*/

#include "Features.h"
#include "COMBase.h"
#include "UOCallibrations.h"
#include "UOItem.h"

// {BFC0EE0A-2B29-41d9-83DA-4B6B61EF54F9}
DEFINE_GUID(CLSID_Client, 
0xbfc0ee0a, 0x2b29, 0x41d9, 0x83, 0xda, 0x4b, 0x6b, 0x61, 0xef, 0x54, 0xf9);

// {45A784D5-0E63-49c9-ACBA-46E4EDBD7D29}
DEFINE_GUID(IID_IClient, 
0x45a784d5, 0xe63, 0x49c9, 0xac, 0xba, 0x46, 0xe4, 0xed, 0xbd, 0x7d, 0x29);

// {EC60659C-611F-467c-B1F8-2CDB15BD257E}
DEFINE_GUID(IID_IClientEvents, 
0xec60659c, 0x611f, 0x467c, 0xb1, 0xf8, 0x2c, 0xdb, 0x15, 0xbd, 0x25, 0x7e);

typedef struct ClientStruct
{
	COMObject header;
} Client;

typedef struct ClientVtblStruct
{
	DEFAULT_DISPATCH;
	HRESULT (STDMETHODCALLTYPE * GetPID)(Client * pThis, unsigned int * pPID);
	HRESULT (STDMETHODCALLTYPE * Write)(Client * pThis, unsigned int color, unsigned int font, BSTR message);
	HRESULT (STDMETHODCALLTYPE * Bark)(Client * pThis, BSTR speech);
	HRESULT (STDMETHODCALLTYPE * ShowYesNoGump)(Client * pThis, BSTR question);
	HRESULT (STDMETHODCALLTYPE * GetPlayer)(Client * pThis, Item * pPlayer);
} ClientVtbl;

void Client_constructor(Client * pThis);
void Client_destructor(Client * pThis);

HRESULT STDMETHODCALLTYPE GetPID(Client * pThis, unsigned int * pPID);
HRESULT STDMETHODCALLTYPE ClientWrite(Client * pThis, unsigned int color, unsigned int font, BSTR message);
HRESULT STDMETHODCALLTYPE Bark(Client * pThis, BSTR speech);
HRESULT STDMETHODCALLTYPE ShowYesNoGump(Client * pThis, BSTR question);
HRESULT STDMETHODCALLTYPE GetPlayer(Client * pThis, Item * pPlayer);

void SetCallibrations(UOCallibrations * callibs);
UOCallibrations * GetCallibrations();

#endif