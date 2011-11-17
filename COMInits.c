#include "COMInits.h"

#include <INITGUID.H>

#include "UOAI.h"
#include "ClientList.h"
#include "Client.h"
#include "COMInjector.h"

// {43CB60E8-9DC4-4a1d-B0DA-2EE60D4B04B3}
DEFINE_GUID(GUID_APPID, 
0x43cb60e8, 0x9dc4, 0x4a1d, 0xb0, 0xda, 0x2e, 0xe6, 0xd, 0x4b, 0x4, 0xb3);

//UOAI Class
UOAIVtbl _UOAIVtbl = {
	DEFAULT_DISPATCH_VTBL,
	GetClientList,
	GetClientWindowName,
	SetClientWindowName,
	GetClientPath,
	SetClientPath,
	GetClientExe,
	SetClientExe,
	LaunchClient
};

GUID * UOAIGuids[3] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_IUOAI };

COMClass UOAIClass = {
	"UOAI",								//name
	(GUID *)&CLSID_UOAI,				//clsid
	2,									//default_iid;
	3,									//iids_count;
	UOAIGuids,							//iids
	0,									//events_iid
	sizeof(UOAI),						//size
	&_UOAIVtbl,							//lpVtbl
	(Structor)UOAI_constructor,			//constructor
	(Structor)UOAI_destructor,			//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

ClientListVtbl _ClientListVtbl = {
	DEFAULT_DISPATCH_VTBL,
	GetCount,
	GetClient,
	RegisterClient,
	UnregisterClient,
	ClientListNewEnum,
	FindClient
};

GUID * ClientListGuids[3] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_IClientList };

COMClass ClientListClass = {
	"ClientList",						//name
	(GUID *)&CLSID_ClientList,			//clsid
	2,									//default_iid;
	3,									//iids_count;
	ClientListGuids,					//iids
	(GUID *)&IID_IClientListEvents,		//events_iid
	sizeof(ClientList),					//size
	&_ClientListVtbl,					//lpVtbl
	(Structor)ClientList_constructor,	//constructor
	(Structor)ClientList_destructor,	//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

COMInjectorVtbl _COMInjectorVtbl = {
	DEFAULT_DISPATCH_VTBL,
	Initialize
};

GUID * COMInjectorGuids[3] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_ICOMInjector };

COMClass COMInjectorClass = {
	"COMInjector",						//name
	(GUID *)&CLSID_COMInjector,			//clsid
	2,									//default_iid;
	3,									//iids_count;
	COMInjectorGuids,					//iids
	0,									//events_iid
	sizeof(COMObject),					//size
	&_COMInjectorVtbl,					//lpVtbl
	(Structor)0,						//constructor
	(Structor)0,						//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

ClientVtbl _ClientVtbl = {
	DEFAULT_DISPATCH_VTBL,
	GetPID,
	ClientWrite,
	Bark,
	ShowYesNoGump,
	GetPlayer,
	GetItems
};

GUID * ClientGuids[3] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_IClient};

COMClass ClientClass = {
	"Client",							//name
	(GUID *)&CLSID_Client,				//clsid
	2,									//default_iid;
	3,									//iids_count;
	ClientGuids,						//iids
	(GUID *)&IID_IClientEvents,			//events_iid
	sizeof(Client),						//size
	&_ClientVtbl,						//lpVtbl
	(Structor)Client_constructor,		//constructor
	(Structor)Client_destructor,		//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

extern COMClass ItemClass;

int COMInits()
{
	COMConfig * configs;

	//base configurations:
	configs=GetCOMBaseConfigurations();
	strcpy(configs->ServerName, "UOAI");
	strcpy(configs->TypeLibFile, "UOAI.tlb");
	configs->AppId=(GUID *)&GUID_APPID;

	//server's class registrations go here:
	AddCOMClass(&UOAIClass);
	AddCOMClass(&ClientListClass);
	AddCOMClass(&ClientClass);
	AddCOMClass(&COMInjectorClass);
	//AddCOMClass(&ItemClass);

	return 1;
}
