#ifndef CLIENTLIST_INCLUDED
#define CLIENTLIST_INCLUDED

/*
	UOAI - Enumerable class of Client objects.

	Clientlist is instantiated in a dllhost.exe based LOCAL SERVER, see the UOAI_constructor for the instantation.

	-- Wim Decelle
*/

#include "COMBase.h"
#include "Client.h"
#include "BinaryTree.h"

// {BE02C37F-111E-402e-9D05-621C3D328724}
DEFINE_GUID(CLSID_ClientList, 
0xbe02c37f, 0x111e, 0x402e, 0x9d, 0x5, 0x62, 0x1c, 0x3d, 0x32, 0x87, 0x24);

// {39FB3C1B-EF1F-4e30-AF63-83CB91B9FFA8}
DEFINE_GUID(IID_IClientList, 
0x39fb3c1b, 0xef1f, 0x4e30, 0xaf, 0x63, 0x83, 0xcb, 0x91, 0xb9, 0xff, 0xa8);

// {B327CDFD-2742-42c7-A781-5084EE06DCDC}
DEFINE_GUID(IID_IClientListEvents, 
0xb327cdfd, 0x2742, 0x42c7, 0xa7, 0x81, 0x50, 0x84, 0xee, 0x6, 0xdc, 0xdc);

typedef struct ClientListStruct
{
	COMObject header;
} ClientList;

//pCOMCALL(name) generates a pointer to a COMCALL function
typedef struct ClientListVtbl
{
	DEFAULT_DISPATCH;
	pCOMCALL(GetCount)(ClientList * pThis, unsigned int * count);
	pCOMCALL(GetClient)(ClientList * pThis, long index, IUnknown ** client);
	pCOMCALL(RegisterClient)(ClientList * pThis, IUnknown * pClientDisp);
	pCOMCALL(UnregisterClient)(ClientList * pThis, IUnknown * pClientDisp);
	pCOMCALL(NewEnum)(ClientList * pThis, IUnknown ** newenum);
	pCOMCALL(FindClient)(ClientList * pThis, unsigned int pid, IUnknown ** client, VARIANT_BOOL * bSuccess);
} ClientListVtbl;

typedef struct IClientListStruct
{
	ClientListVtbl * lpVtbl;
} IClientList;

void ClientList_constructor(ClientList * pThis);
void ClientList_destructor(ClientList * pThis);

COMCALL GetCount(ClientList * pThis, unsigned int * count);
COMCALL GetClient(ClientList * pThis, long index, IUnknown ** client);
COMCALL RegisterClient(ClientList * pThis, IUnknown * pClientDisp);
COMCALL UnregisterClient(ClientList * pThis, IUnknown * pClientDisp);
COMCALL ClientListNewEnum(ClientList * pThis, IUnknown ** newenum);
COMCALL FindClient(ClientList * pThis, unsigned int pid, IUnknown ** client, VARIANT_BOOL * bSuccess);

#endif