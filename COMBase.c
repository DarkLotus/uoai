#include "COMBase.h"
#include "BinaryTree.h"
#include "allocation.h"
#include "Process.h"
#include "UAC.h"

#include <stdio.h>

unsigned int Initialized=0;
BinaryTree * known_classes=0;//GUID->class pairs
ITypeLib * tlb=0;
COMConfig configurations;
unsigned int TotalRefCount=0;
unsigned int InternalRefCount=0;
unsigned int LockCount=0;


//default IClassFactory Implementation

IClassFactoryVtbl defClassFactoryVtbl = {
	(pClassQueryInterface)	DefaultQueryInterface,
	(pClassAddRef)			DefaultAddRef,
	(pClassRelease)			DefaultRelease,
	(pClassCreateInstance)	DefaultClassCreateInstance,
	(pClassLockServer)		DefaultClassLockServer
};

GUID * defClassFactoryInterfaces[2]={(GUID *)&IID_IUnknown, (GUID *)&IID_IClassFactory};

COMClass defClassFactory = {
	0,
	0,
	1,
	2,
	defClassFactoryInterfaces,
	0,
	sizeof(ClassFactory),
	&defClassFactoryVtbl,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};


//default enumerator (supports object-enumeration, variant-enumeration or just data-enumeration)
//	enumerator iid is specified on creation (IID_IEnumVARIANT, IID_IEnumConnections,  IID_IEnumConnectionPoints, etc.)

ILinkedListEnumeratorVtbl defLinkedListEnumeratorVtbl = {
	LLQueryInterface,
	DefaultAddRef,
	DefaultRelease,
	LLNext,
	LLSkip,
	LLReset,
	LLClone
};

GUID * defLinkedListEnumeratorInterfaces[1]={(GUID *)&IID_IUnknown};

void LinkedListEnumerator_constructor(LinkedListEnumerator * pThis);
void LinkedListEnumerator_destructor(LinkedListEnumerator * pThis);

COMClass defLinkedListEnumerator = {
	0,									//name
	0,									//clsid
	0,									//default_iid;
	1,									//iids_count;
	defLinkedListEnumeratorInterfaces,	//iids
	0,									//events_iid
	sizeof(LinkedListEnumerator),		//size
	&defLinkedListEnumeratorVtbl,		//lpVtbl
	(Structor)LinkedListEnumerator_constructor,	//constructor
	(Structor)LinkedListEnumerator_destructor,	//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};


//default IConnectionPointContainer

ConnectionPointContainerVtbl defCPCVtbl = {
	(pQueryInterface)DefaultQueryInterface,
	(pAddRef)DefaultAddRef,
	(pRelease)DefaultRelease,
	EnumConnectionPoints,
	FindConnectionPoint
};

GUID * defCPCGuids[2]={(GUID *)&IID_IUnknown, (GUID *)&IID_IConnectionPointContainer};

COMClass defConnectionPointContainer = {
	0,									//name
	0,									//clsid
	0,									//default_iid;
	2,									//iids_count;
	defCPCGuids,						//iids
	0,									//events_iid
	sizeof(ConnectionPointContainer),	//size
	&defCPCVtbl,						//lpVtbl
	(Structor)ConnectionPointContainer_constructor,	//constructor
	(Structor)ConnectionPointContainer_destructor,	//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

ConnectionPointVtbl defCPVtbl = {
	(pQueryInterface)DefaultQueryInterface,
	(pAddRef)DefaultAddRef,
	(pRelease)DefaultRelease,
	GetConnectionInterface,
	GetConnectionPointContainer,
	Advise,
	Unadvise,
	EnumConnections
};

GUID * defCPGuids[2]={(GUID *)&IID_IUnknown, (GUID *)&IID_IConnectionPoint};

COMClass defConnectionPoint = {
	0,									//name
	0,									//clsid
	0,									//default_iid;
	2,									//iids_count;
	defCPGuids,							//iids
	0,									//events_iid
	sizeof(ConnectionPoint),			//size
	&defCPVtbl,							//lpVtbl
	(Structor)ConnectionPoint_constructor,	//constructor
	(Structor)ConnectionPoint_destructor,	//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

void COMBase_cleanup(void * par);
int COMBaseInits(int reg_typelib, int unreg_typelib);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, void * lpvReserved)
{
	return TRUE;
}

int pointer_compare(unsigned int a, unsigned int b)
{
	if(a<b)
		return +1;
	else if(a>b)
		return -1;
	else
		return 0;
}

void PrintGUID(GUID * toprint)
{
	COMClass * curclass;
	unsigned short str[256];
	char * cstr;

	if(IsEqualIID(toprint, &IID_IUnknown))
		printf("IUnknown");
	else if(IsEqualIID(toprint, &IID_IDispatch))
		printf("IDispatch");
	else if(IsEqualIID(toprint, &IID_IClassFactory))
		printf("IClassFactory");
	else if(IsEqualIID(toprint, &IID_IClassFactory2))
		printf("IClassFactory2");
	else if(IsEqualIID(toprint, &IID_IConnectionPointContainer))
		printf("IConnectionPointContainer");
	else if(curclass=(COMClass *)BT_find(known_classes, toprint))
		printf(curclass->name);
	else
	{
		StringFromGUID2(toprint, str, 256);
		cstr=ole2char(str);
		printf(cstr);
		clean(cstr);		
	}
}

unsigned int olestrlen(LPOLESTR str)
{
	unsigned int i=0;
	while(str[i])
		i++;
	return i;
}

char * ole2char(LPOLESTR torewrite)
{
	short curchar;
	int i;
	char * toreturn;

	toreturn=(char *)alloc(olestrlen(torewrite)+1);
	
	i=0;
	while(curchar=torewrite[i])
	{
		toreturn[i]=(char)curchar;
		i++;
	}
	toreturn[i]='\0';

	return toreturn;
}

short * char2ole(char * toconvert)
{
	unsigned int i;
	short * toreturn;
	
	toreturn=(short *)alloc(sizeof(short)*(strlen(toconvert)+1));
	
	i=0;
	while(toconvert[i])
	{
		toreturn[i]=(short)toconvert[i];
		i++;
	}
	toreturn[i]=0;

	return toreturn;
}

BSTR char2bstr(char * toconvert)
{
	short * unicodeversion;
	BSTR bstrversion;
	
	unicodeversion=char2ole(toconvert);
	
	bstrversion=SysAllocStringLen(unicodeversion,strlen(toconvert));
	
	clean(unicodeversion);

	return bstrversion;
}

void InternalAddRef()
{
	InternalRefCount++;
	return;
}

void InternalRelease()
{
	InternalRefCount--;
	return;
}

COMConfig * GetCOMBaseConfigurations()
{
	return &configurations;
}

//construction/destruction
void DestructObject(COMObject * todestruct)
{
	WeakReference * curref;

	BT_remove(todestruct->_class->instances, todestruct);

	//if there is an event interface, destroy the connection point container (and it's connection points)
	if(todestruct->_CPC)
	{
		DefaultRelease(todestruct->_CPC);
		InternalRelease();
	}

	//cleanup weak references (if any)
	while((todestruct->weakreferences->root)&&(curref=(WeakReference *)(todestruct->weakreferences->root->data)))
	{
		curref->isValid=0;
		BT_remove(todestruct->weakreferences, curref);
	}
	BT_delete(todestruct->weakreferences);

	//cleanup this object
	if(todestruct->_class->destructor)
	{
		todestruct->_class->destructor(todestruct);
	}

	clean(todestruct);

	return;
}

COMObject * ConstructObject(COMClass  * fromclass)
{
	COMObject * newobject;

	newobject=alloc(fromclass->size);
	newobject->_class=fromclass;
	newobject->lpVtbl=fromclass->lpVtbl;
	newobject->parent=0;
	newobject->refcount=1;
	newobject->weakreferences=BT_create((BTCompare)pointer_compare);
	newobject->_CPC=0;
	
	TotalRefCount++;

	if(fromclass->events_iid)
	{
		newobject->_CPC=CreateConnectionPointContainer(newobject);
		InternalAddRef();
	}

	if(fromclass->instances==0)
		fromclass->instances=BT_create((BTCompare)pointer_compare);
	BT_insert(fromclass->instances, newobject, newobject);

	if(fromclass->constructor)
		fromclass->constructor(newobject);

	return newobject;
}

WeakReference * CreateWeakReference(COMObject * to)
{
	WeakReference * toreturn;

	toreturn=create(WeakReference);

	toreturn->isValid=1;
	toreturn->to=to;

	BT_insert(to->weakreferences, toreturn, toreturn);

	return toreturn;
}


void DeleteWeakReference(WeakReference * wr)
{	
	if(wr->isValid)
	{
		BT_remove(wr->to->weakreferences, wr);
		clean(wr);
	}	

	return;
}

int _supports_iid(COMClass * tocheck, GUID * riid)
{
	unsigned int i;
	int found=0;

	for(i=0; i<tocheck->iids_count; i++)
	{
		if(IsEqualIID(tocheck->iids[i], riid))
		{
			found=1;
			break;
		}
	}

	return found;
}

//default IUnknown Implementation
HRESULT STDMETHODCALLTYPE DefaultQueryInterface(COMObject * pThis, REFIID riid, COMObject ** ppv)
{
	if(pThis->parent)
		return DefaultQueryInterface(pThis->parent, riid, ppv);

	(*ppv)=0;
	
	if(_supports_iid(pThis->_class, (GUID *)riid))
	{
		(*ppv)=pThis;
		pThis->refcount++;	
		TotalRefCount++;
		return S_OK;
	}

	if((pThis->_CPC)&&(IsEqualIID(riid, &IID_IConnectionPointContainer)))
	{
		printf("CPC Queried\n");
		(*ppv)=pThis->_CPC;
		pThis->refcount++;		
		TotalRefCount++;
		return S_OK;
	}

	return E_NOINTERFACE;	
}

ULONG STDMETHODCALLTYPE DefaultAddRef(COMObject * pThis)
{
	if(pThis->parent)
		return DefaultAddRef(pThis->parent);

	pThis->refcount++;
	TotalRefCount++;
	return pThis->refcount;
}

ULONG STDMETHODCALLTYPE DefaultRelease(COMObject * pThis)
{
	if(pThis->parent)
		return DefaultRelease(pThis->parent);

	TotalRefCount--;
	pThis->refcount--;
	if(pThis->refcount==0)
		DestructObject(pThis);

	return pThis->refcount;
}

HRESULT STDMETHODCALLTYPE DefaultClassCreateInstance(COMObject * pThis,IUnknown * punkOuter, REFIID vTableGuid, COMObject ** ppv)
{
	COMClass * requestedclass;

	if(punkOuter)
		return CLASS_E_NOAGGREGATION;

	if(requestedclass=((ClassFactory *)pThis)->forclass)
	{
		if(requestedclass->activeobject)
			return requestedclass->activeobject->lpVtbl->QueryInterface(requestedclass->activeobject, vTableGuid, (void **)ppv);
		if((*ppv)=ConstructObject(requestedclass))
			return S_OK;
	}

	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE DefaultClassLockServer(COMObject * pThis,BOOL flock)
{
	if(flock==0)
		LockCount--;
	else
		LockCount++;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DefaultGetTypeInfoCount(COMObject * pThis, UINT * pCount)
{
	if(pThis->_class->typeinfo)
		(*pCount)=1;
	else
		(*pCount)=0;
	
	return S_OK;
}

HRESULT STDMETHODCALLTYPE DefaultGetTypeInfo(COMObject * pThis, UINT itinfo, LCID lcid, ITypeInfo ** pTypeInfo)
{
	HRESULT toreturn;

	toreturn=S_FALSE;//assume error
	(*pTypeInfo)=0;

	if(itinfo)
		toreturn=ResultFromScode(DISP_E_BADINDEX);
	else
	{
		if(pThis->_class->typeinfo->lpVtbl->QueryInterface(pThis->_class->typeinfo, &IID_ITypeInfo, pTypeInfo)!=S_OK)
			toreturn=TYPE_E_ELEMENTNOTFOUND;
		else
			toreturn=S_OK;
	}

	return toreturn;;
}

HRESULT STDMETHODCALLTYPE DefaultGetIDsOfNames(COMObject * pThis, REFIID riid, LPOLESTR * rgszNames, UINT cNames, LCID lcid, DISPID * rgdispid)
{
	HRESULT toreturn;

	toreturn=S_FALSE;

	if(pThis->_class->typeinfo==0)
		toreturn=DISP_E_UNKNOWNNAME;//or should we use TYPE_E_ELEMENTNOTFOUND?
	else
		toreturn=DispGetIDsOfNames(pThis->_class->typeinfo, rgszNames, cNames, rgdispid);

	return toreturn;
}

HRESULT STDMETHODCALLTYPE DefaultInvoke(COMObject * pThis, DISPID dispid, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *pexcepinfo, UINT *puArgErr)
{
	HRESULT toreturn;

	toreturn=S_FALSE;

	printf("invoking...\n");

	if((!IsEqualIID(riid, &IID_NULL))||(pThis->_class->typeinfo==0))
		toreturn=DISP_E_UNKNOWNINTERFACE;
	else
		toreturn=DispInvoke((void *)pThis, pThis->_class->typeinfo, dispid, wFlags, params, result, pexcepinfo, puArgErr);

	return toreturn;;
}

HRESULT PASCAL DllCanUnloadNow(void)
{
	if((LockCount>0)||(TotalRefCount>InternalRefCount))
		return S_FALSE;
	else
		return S_OK;
}

COMObject * CreateFactory(COMClass * forclass)
{
	COMObject * toreturn;
	
	toreturn=ConstructObject(&defClassFactory);
	((ClassFactory *)toreturn)->forclass=forclass;
	InternalAddRef();

	return toreturn;
}

int InitializeCOM()
{
	if(!COMBaseInits(0, 0))
		return 0;
	else
		return 1;
}

HRESULT PASCAL DllGetClassObject(REFCLSID objGuid, REFIID factoryGuid, void **factoryHandle)
{
	COMClass * requestedclass;
	COMObject * factory;

	//first time this library is used? if so..; try to initialize
	if(InterlockedCompareExchange(&Initialized, 1, 0)==0)
	{
		/*
		//debug
		if(AllocConsole())
		{
			freopen("CONIN$","rb",stdin);
			freopen("CONOUT$","wb",stdout);
			freopen("CONOUT$","wb",stderr);
		}
		*/

		CoInitializeSecurity(   NULL,
                                       -1,
                                       NULL,
                                       NULL,
                                       RPC_C_AUTHN_LEVEL_NONE,
                                       RPC_C_IMP_LEVEL_IDENTIFY,
                                       NULL,
                                       0,
                                       NULL );

		//inits
		if(!COMBaseInits(0, 0))
		{
			return E_UNEXPECTED;
		}
	}

	/*if((base_classfactory==0)||(known_classes==0))//should never happen
		return E_UNEXPECTED;*/
	if(known_classes==0)
		return E_UNEXPECTED;

	//check if a correct CLSID was passed in
	if(requestedclass=(COMClass *)BT_find(known_classes, (void *)objGuid))
	{
		if(requestedclass->factory==0)
			requestedclass->factory=CreateFactory(requestedclass);
		factory=requestedclass->factory;
		return DefaultQueryInterface(factory, factoryGuid, (COMObject **)factoryHandle);
	}
	else
	{
		return CLASS_E_CLASSNOTAVAILABLE;
	}
}

int GUID_compare(GUID * a, GUID * b)
{
	return strncmp((char *)a, (char *)b, sizeof(GUID));
}

ITypeLib * OpenTypeLib(int regtypelib, int unregtypelib)
{
	ITypeLib * _tlb;
	char curmodule_path[256];
	unsigned int i;
	short * olepath;
	TLIBATTR * attr;

	_tlb=0;

	//get path to current module
	if(!GetModuleNameFromAddress((unsigned int)OpenTypeLib, 0, 0, curmodule_path, 256))
		return 0;
	olepath=char2ole(curmodule_path);
	
	//try loading the typelibrary as a resource
	if(LoadTypeLib(olepath, &_tlb)!=S_OK)
	{
		//if that didn't work, try loading it directly
		//- build path
		for(i=strlen(curmodule_path)-1; i>=0; i--)
		{
			if(curmodule_path[i]=='\\')
			{
				curmodule_path[i]='\0';
				break;
			}
		}
		strcat(curmodule_path, "\\");
		strcat(curmodule_path, configurations.TypeLibFile);
		clean(olepath);
		olepath=char2ole(curmodule_path);
		
		//- try loading the typelibrary
		if(LoadTypeLib(olepath, &_tlb)!=S_OK)
		{
			clean(olepath);
			return 0;
		}

		//register or unregister if requested
		if(regtypelib)
			RegisterTypeLib(_tlb, olepath, 0);
		else if(unregtypelib)
		{
			if(_tlb->lpVtbl->GetLibAttr(_tlb, &attr)==S_OK)
			{
				UnRegisterTypeLib(&attr->guid, attr->wMajorVerNum, attr->wMinorVerNum, attr->lcid, attr->syskind);
				_tlb->lpVtbl->ReleaseTLibAttr(_tlb, attr);
			}
		}
	}

	clean(olepath);
	return _tlb;
}

int COMBaseInits(int reg_typelib, int unreg_typelib)
{
	BinaryTreeEnum * btenum;
	COMClass * curclass;

	//setup collections
	known_classes=BT_create((BTCompare)GUID_compare);

	//setup base_classfactory
	//base_classfactory=ConstructObject(&defClassFactory);
	//InternalAddRef();

	//call user init function to register all known classes
	if(COMInits())
	{
		//try to load the typelibrary
		if((tlb=OpenTypeLib(reg_typelib, unreg_typelib))==0)
		{
			return 0;
		}

		//initialize all typeinfos
		btenum=BT_newenum(known_classes);
		while(curclass=(COMClass *)BT_next(btenum))
		{
			if((curclass->typeinfo==0)&&((tlb->lpVtbl->GetTypeInfoOfGuid(tlb, curclass->iids[curclass->default_iid], &(curclass->typeinfo)))!=S_OK))
			{
				BT_enumdelete(btenum);
				return 0;
			}
		}
		BT_enumdelete(btenum);

		return 1;
	}

	//failed to initialize
	return 0;
}

int UnregisterCOMClass(COMClass * tounregister);
int RegisterCOMClass(COMClass * toregister)
{
	HKEY ClassesKey=0, CLSIDKey=0, GUIDKey=0,  ServerKey=0, ProgIDKey=0, ElevationKey=0, AppIDKey=0, AppIDGuidKey=0;
	unsigned short clsid_string[256];
	unsigned short appid_string[256];
	char progid[256];
	char server[256];
	char * clsid;
	char * surrogate_string="";
	char * threadingmodel="Apartment";

	char localizedname[256];
	char * appid;

	int ok;
	unsigned int do_enable;

	unsigned int dwAllowAny;

	SECURITY_DESCRIPTOR * pSD=0;

	ok=0;
	clsid=0;
	appid=0;
	do_enable=1;

	if((toregister->clsid==0)||(toregister->name==0))
		return 0;
	
	while(1)
	{
		//get path to this dll
		if(!GetModuleNameFromAddress((unsigned int)RegisterCOMClass, 0, 0, server, 256))
			break;

		localizedname[0]='\0';
		sprintf(localizedname, "@%s,-1", server);

		//AppId Guid -> appidstring
		if(StringFromGUID2((REFCLSID)configurations.AppId, appid_string, 256)==0)
			break;
		appid=ole2char(appid_string);

		//CLSID Guid -> guid string
		if(StringFromGUID2((REFCLSID)toregister->clsid, clsid_string, 256)==0)
			break;
		clsid=ole2char(clsid_string);

		//build progid
		progid[0]='\0';
		sprintf(progid, "%s.%s", configurations.ServerName, toregister->name);

		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &ClassesKey)!=ERROR_SUCCESS)
			break;

		////HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\AppID
		if(RegOpenKeyEx(ClassesKey, "AppID", 0, KEY_ALL_ACCESS, &AppIDKey)!=ERROR_SUCCESS)
			break;

		////HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\AppID\\<AppID>
		if(RegOpenKeyEx(AppIDKey, appid, 0, KEY_ALL_ACCESS, &AppIDGuidKey)!=ERROR_SUCCESS)
		{
			if(RegCreateKey(AppIDKey, appid, &AppIDGuidKey)!=ERROR_SUCCESS)
				break;
		}

		////HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\AppID\\<AppID>\\DllSurrogate=""
		if(RegSetValueEx(AppIDGuidKey, "DllSurrogate", 0, REG_SZ, surrogate_string, strlen(surrogate_string)+1)!=ERROR_SUCCESS)
			break;

		//if(toregister->support_elevation)
		//{
			if(!GetAccessPermissions_ots(&pSD))
				break;

			//allow admin<->non-admin communication
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\AppID\\<AppID>\\AccessPermission=SECURITY_DESCRIPTOR
			if(RegSetValueEx(AppIDGuidKey, "AccessPermission", 0, REG_BINARY, (BYTE *)pSD, GetSecurityDescriptorLength(pSD))!=ERROR_SUCCESS)
				break;

			if(!GetLaunchActPermissionsWithIL(&pSD))
				break;
			
			//allow CoCreateInstance from non-admin classes (even when the current class's factory is in an elevated process
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\AppID\\<AppID>\\LaunchPermission=SECURITY_DESCRIPTOR
			if(RegSetValueEx(AppIDGuidKey, "LaunchPermission", 0, REG_BINARY, (BYTE *)pSD, GetSecurityDescriptorLength(pSD))!=ERROR_SUCCESS)
				break;

			//activeobject is accessible from any elevation level
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\AppID\\<AppID>\\ROTFlags=ROTREGFLAGS_ALLOWANYCLIENT
			dwAllowAny=0x1;
			if(RegSetValueEx(AppIDGuidKey, "ROTFlags", 0, REG_DWORD, (BYTE *)&dwAllowAny, sizeof(DWORD))!=ERROR_SUCCESS)
				break;
		//}

		////HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\<ProgID>
		if(RegOpenKeyEx(ClassesKey, progid, 0, KEY_ALL_ACCESS, &ProgIDKey)!=ERROR_SUCCESS)
		{
			if(RegCreateKey(ClassesKey, progid, &ProgIDKey)!=ERROR_SUCCESS)
				break;
		}

		////HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\<ProgID>\\CLSID={clsid}
		if(RegSetValue(ProgIDKey, "CLSID", REG_SZ, clsid, strlen(clsid)+1)!=ERROR_SUCCESS)
			break;

		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID
		if(RegOpenKeyEx(ClassesKey, "CLSID", 0, KEY_ALL_ACCESS, &CLSIDKey)!=ERROR_SUCCESS)
			break;
		
		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>=<clsid>
		if(RegSetValue(CLSIDKey, clsid, REG_SZ, clsid, strlen(clsid)+1)!=ERROR_SUCCESS)
			break;

		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>
		if(RegOpenKeyEx(CLSIDKey, clsid, 0, KEY_ALL_ACCESS, &GUIDKey)!=ERROR_SUCCESS)
			break;

		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\LocalizedString=ProgID
		if(RegSetValueEx(GUIDKey, "LocalizedString", 0, REG_SZ, localizedname, strlen(localizedname)+1)!=ERROR_SUCCESS)
			break;
		
		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\AppId=AppId
		if(RegSetValueEx(GUIDKey, "AppId", 0, REG_SZ, appid, strlen(appid)+1)!=ERROR_SUCCESS)
			break;

		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\ProgID=ProgID
		if(RegSetValue(GUIDKey, "ProgID", REG_SZ, progid, strlen(progid)+1)!=ERROR_SUCCESS)
			break;

		//if(isInProc)
		//{
		//if(toregister->register_as_local_server==0)
		//{
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32=server
			if(RegSetValue(GUIDKey, "InprocServer32", REG_SZ, server, strlen(server)+1)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32
			if(RegOpenKeyEx(GUIDKey, "InprocServer32", 0, KEY_ALL_ACCESS, &ServerKey)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32\\ThreadingModel=threadingmodel
			if(RegSetValueEx(ServerKey, "ThreadingModel", 0, REG_SZ, threadingmodel, strlen(threadingmodel)+1)!=ERROR_SUCCESS)
				break;
		/*}
		else
		{
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32=server
			if(RegSetValue(GUIDKey, "LocalServer32", REG_SZ, server, strlen(server)+1)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32
			if(RegOpenKeyEx(GUIDKey, "LocalServer32", 0, KEY_ALL_ACCESS, &ServerKey)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32\\ThreadingModel=threadingmodel
			if(RegSetValueEx(ServerKey, "ThreadingModel", 0, REG_SZ, threadingmodel, strlen(threadingmodel)+1)!=ERROR_SUCCESS)
				break;
		}*/
		/*}//currently not supporting local servers
		else
		{
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\LocalServer32=path
			if(RegSetValue(GUIDKey, "LocalServer32", REG_SZ, server, strlen(server)+1)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\LocalServer32
			if(RegOpenKeyEx(GUIDKey, "LocalServer32", 0, KEY_ALL_ACCESS, &ServerKey)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\LocalServer32\\ThreadingModel=threadingmodel
			if(RegSetValueEx(ServerKey, "ThreadingModel", 0, REG_SZ, threadingmodel, strlen(threadingmodel)+1)!=ERROR_SUCCESS)
				break;
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\LocalServer32\\ServerExecutable=server
			if(RegSetValueEx(ServerKey, "ServerExecutable", 0, REG_SZ, server, strlen(server)+1)!=ERROR_SUCCESS)
				break;
		}*/

		//if(toregister->support_elevation)
		//{
			//HKEY_LOCAL_MACHINE\\Software\\Classes\CLSID\\<clsid>\\Elevation
			if(RegOpenKey(GUIDKey, "Elevation", &ElevationKey)!=ERROR_SUCCESS)
			{
				if(RegCreateKey(GUIDKey, "Elevation", &ElevationKey)!=ERROR_SUCCESS)
					break;
			}

			//HKEY_LOCAL_MACHINE\\Software\\Classes\CLSID\\<clsid>\\Elevation\\Enabled=1
			RegSetValueEx(ElevationKey, "Enabled", 0, REG_DWORD, (const unsigned char *)&do_enable, sizeof(DWORD));
		//}

		ok=1;
		break;
	}

	if(clsid)
		clean(clsid);
	if(appid)
		clean(appid);

	//HKEY ClassesKey=0, CLSIDKey=0, GUIDKey=0,  ServerKey=0, ProgIDKey=0, ElevationKey=0;
	if(ClassesKey)
	{
		if(CLSIDKey)
		{
			if(GUIDKey)
			{
				if(ElevationKey)
					RegCloseKey(ElevationKey);
				if(ServerKey)
					RegCloseKey(ServerKey);

				RegCloseKey(GUIDKey);
			}
			RegCloseKey(CLSIDKey);
		}

		if(ProgIDKey)
		{
			RegCloseKey(ProgIDKey);
		}

		if(AppIDKey)
		{
			if(AppIDGuidKey)
				RegCloseKey(AppIDGuidKey);
			RegCloseKey(AppIDKey);
		}

		RegCloseKey(ClassesKey);
	}

	if(ok==0)
		UnregisterCOMClass(toregister);

	return ok;
}

int UnregisterCOMClass(COMClass * tounregister)
{
	HKEY ClassesKey=0, CLSIDKey=0, GUIDKey=0,  ServerKey=0, ProgIDKey=0, ElevationKey=0, AppIDKey=0, AppIDGuidKey=0;
	unsigned short clsid_string[256];
	unsigned short appid_string[256];
	char progid[256];
	char server[256];
	char * clsid;
	char * appid;

	clsid=0;
	appid=0;

	if((tounregister->clsid==0)||(tounregister->name==0))
		return 0;
	
	//get path to this dll
	if(!GetModuleNameFromAddress((unsigned int)RegisterCOMClass, 0, 0, server, 256))
		return 0;

	//AppId Guid -> appid string
	if(StringFromGUID2((REFCLSID)configurations.AppId, appid_string, 256)==0)
		return 0;
	appid=ole2char(appid_string);

	//CLSID Guid -> guid string
	if(StringFromGUID2((REFCLSID)tounregister->clsid, clsid_string, 256)==0)
	{
		clean(appid);
		return 0;
	}
	clsid=ole2char(clsid_string);

	//build progid
	progid[0]='\0';
	sprintf(progid, "%s.%s", configurations.ServerName, tounregister->name);

	//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes", 0, KEY_ALL_ACCESS, &ClassesKey)==ERROR_SUCCESS)
	{
		if(RegOpenKeyEx(ClassesKey, "AppID", 0, KEY_ALL_ACCESS, &AppIDKey)==ERROR_SUCCESS)
		{
			if(RegOpenKeyEx(AppIDKey, appid, 0, KEY_ALL_ACCESS, &AppIDGuidKey)==ERROR_SUCCESS)
			{
				//delete values
				RegDeleteKey(AppIDGuidKey, "DllSurrogate");
				//if(tounregister->support_elevation)
					RegDeleteKey(AppIDGuidKey, "AccessPermission");
					RegDeleteKey(AppIDGuidKey, "LaunchPermission");
					RegDeleteKey(AppIDGuidKey, "ROTFlags");

				//delete key
				RegCloseKey(AppIDGuidKey);
				RegDeleteKey(AppIDKey, appid);
			}
			RegCloseKey(AppIDKey);
		}

		////HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\<ProgID>
		if(RegOpenKeyEx(ClassesKey, progid, 0, KEY_ALL_ACCESS, &ProgIDKey)==ERROR_SUCCESS)
		{
			//delete values
			RegDeleteKey(ProgIDKey, "CLSID");

			//delete key
			RegCloseKey(ProgIDKey);
			RegDeleteKey(ClassesKey, progid);
		}

			
		//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID
		if(RegOpenKeyEx(ClassesKey, "CLSID", 0, KEY_ALL_ACCESS, &CLSIDKey)==ERROR_SUCCESS)
		{
			//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>
			if(RegOpenKeyEx(CLSIDKey, clsid, 0, KEY_ALL_ACCESS, &GUIDKey)==ERROR_SUCCESS)
			{
				//if(tounregister->register_as_local_server==0)
				//{
					//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32
					if(RegOpenKeyEx(GUIDKey, "InprocServer32", 0, KEY_ALL_ACCESS, &ServerKey)==ERROR_SUCCESS)
					{
						//delete values
						RegDeleteKey(ServerKey, "ThreadingModel");
	
						//delete key
						RegCloseKey(ServerKey);
						RegDeleteKey(GUIDKey, "InprocServer32");
					}
				/*}
				else
				{*/
					//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\InprocServer32
					if(RegOpenKeyEx(GUIDKey, "LocalServer32", 0, KEY_ALL_ACCESS, &ServerKey)==ERROR_SUCCESS)
					{
						//delete values
						RegDeleteKey(ServerKey, "ThreadingModel");
	
						//delete key
						RegCloseKey(ServerKey);
						RegDeleteKey(GUIDKey, "LocalServer32");
					}
				/*}*/

				//HKEY_LOCAL_MACHINE\\Software\\Classes\CLSID\\<clsid>\\Elevation
				if(RegOpenKey(GUIDKey, "Elevation", &ElevationKey)==ERROR_SUCCESS)
				{
					//delete values
					RegDeleteKey(ElevationKey, "Enabled");

					//delete key
					RegCloseKey(ElevationKey);
					RegDeleteKey(GUIDKey, "Elevation");
				}

				//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\LocalizedString=ProgID
				RegDeleteKey(GUIDKey, "LocalizedString");

				//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\AppId=AppId
				RegDeleteKey(GUIDKey, "AppId");
		
				//HKEY_LOCAL_MACHINE\\SOFTWARE\\CLASSES\\CLSID\\<clsid>\\ProgID=ProgID
				RegDeleteKey(GUIDKey, "ProgID");

				//delete key
				RegCloseKey(GUIDKey);
				RegDeleteKey(CLSIDKey, clsid);

			}

			RegCloseKey(CLSIDKey);
		}

		RegCloseKey(ClassesKey);
	}

	clean(appid);
	clean(clsid);

	return 1;
}

HRESULT PASCAL DllRegisterServer()
{
	BinaryTree * done;
	BinaryTreeEnum * btenum;
	COMClass * curclass;
	int ok;

	if(ok=COMBaseInits(1, 0))
	{
		done=BT_create((BTCompare)GUID_compare);
		
		btenum=BT_newenum(known_classes);
		while(curclass=(COMClass *)BT_next(btenum))
		{
			if(BT_find(done, curclass)==0)
			{
				if((curclass->clsid!=0)&&(curclass->name!=0))
					ok&=RegisterCOMClass(curclass);
				BT_insert(done, curclass, curclass);
			}
		}
		BT_enumdelete(btenum);

		BT_delete(done);
	}	

	if(ok)
		return S_OK;
	else
		return E_UNEXPECTED;
}

HRESULT PASCAL DllUnregisterServer()
{
	BinaryTree * done;
	BinaryTreeEnum * btenum;
	COMClass * curclass;
	int ok;

	if(ok=COMBaseInits(0, 1))
	{
		done=BT_create((BTCompare)GUID_compare);
		
		btenum=BT_newenum(known_classes);
		while(curclass=(COMClass *)BT_next(btenum))
		{
			if(BT_find(done, curclass)==0)
			{
				if((curclass->clsid!=0)&&(curclass->name!=0))
					ok&=UnregisterCOMClass(curclass);
				BT_insert(done, curclass, curclass);
			}
		}
		BT_enumdelete(btenum);

		BT_delete(done);
	}	

	if(ok)
		return S_OK;
	else
		return E_UNEXPECTED;
}

void AddCOMClass(COMClass * toregister)
{
	if(toregister->instances==0)
		toregister->instances=BT_create((BTCompare)pointer_compare);

	BT_insert(known_classes, toregister->clsid, toregister);
	
	return;
}

void SetActiveObject(COMClass * _class, IUnknown * activeobject)
{
	if(_class->activeobject!=0)
	{
		_class->activeobject->lpVtbl->Release(_class->activeobject);
		InternalRelease();
	}

	_class->activeobject=activeobject;
	activeobject->lpVtbl->AddRef(activeobject);
	InternalAddRef();

	//RegisterActiveObject(activeobject, &(_class->clsid), ACTIVEOBJECT_WEAK, NULL);

	return;
}

HRESULT STDMETHODCALLTYPE LLQueryInterface(COMObject * pThis,REFIID riid, COMObject ** ppv)
{
	LinkedListEnumerator * lle_this;

	lle_this=(LinkedListEnumerator *)pThis;

	if(IsEqualIID(lle_this->_iid, riid)||IsEqualIID(lle_this->_iid, &IID_IUnknown))
	{
		(*ppv)=pThis;
		(*ppv)->refcount++;	
		TotalRefCount++;
		return S_OK;
	}

	return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE LLNext(COMObject * pThis, ULONG count, void ** arr_out, unsigned int * _count)
{
	LinkedListEnumerator * lle_this;
	unsigned int i;
	void * cur;

	lle_this=(LinkedListEnumerator *)pThis;

	i=0;
	while((i<count)&&(cur=LL_next(lle_this->_llenum)))
	{
		if(lle_this->datasize>0)
			memcpy((void *)((unsigned int)arr_out+i*lle_this->datasize), cur, lle_this->datasize);
		else if(lle_this->type==1)//Object pointer -> addref requried before returning
		{
			arr_out[i]=cur;
			DefaultAddRef((COMObject *)cur);
		}
		else if(lle_this->type==2)//VARIANT -> must be copied correctly
		{
			VariantInit((VARIANTARG *)((unsigned int)arr_out+i*sizeof(VARIANT)));
			VariantCopy((VARIANTARG *)((unsigned int)arr_out+i*sizeof(VARIANT)), (VARIANTARG *)cur);
		}
		else
			arr_out[i]=cur;
		i++;
	}

	if(_count)
		(*_count)=i;
	if(i<count)
	{
		return S_FALSE;
	}
	else
	{
		return S_OK;
	}
}

HRESULT STDMETHODCALLTYPE LLSkip(COMObject * pThis, ULONG count)
{
	LinkedListEnumerator * lle_this;
	unsigned int i;

	lle_this=(LinkedListEnumerator *)pThis;

	i=0;
	while((i<count)&&(LL_next(lle_this->_llenum)))
		i++;

	if(i<count)
		return S_FALSE;
	else
		return S_OK;
}

HRESULT STDMETHODCALLTYPE LLReset(COMObject * pThis)
{
	LinkedListEnumerator * lle_this;

	lle_this=(LinkedListEnumerator *)pThis;

	LL_reset(lle_this->_llenum);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE LLClone(COMObject * pThis, void ** ppenum)
{
	LinkedListEnumerator * lle_this;
	LinkedListEnum * newenum;

	lle_this=(LinkedListEnumerator *)pThis;

	newenum=create(LinkedListEnum);
	newenum->oflist=lle_this->_llenum->oflist;
	newenum->current=lle_this->_llenum->current;

	if((*ppenum)=(void *)CreateLinkedListEnumerator(0, newenum, lle_this->datasize, lle_this->type, lle_this->_iid))
		return S_OK;
	else
		return S_FALSE;
}

COMObject * CreateLinkedListEnumerator(LinkedList * list, LinkedListEnum * llenum, unsigned int datasize, int type, GUID * iid)
{
	LinkedListEnumerator * newllenum;
	
	newllenum=(LinkedListEnumerator *)ConstructObject(&defLinkedListEnumerator);

	newllenum->_list=list;
	newllenum->_iid=iid;
	newllenum->_llenum=llenum;
	newllenum->datasize=datasize;
	newllenum->type=type;

	if((list!=0)&&(llenum==0))
		newllenum->_llenum=LL_newenum(list);

	return (COMObject *)newllenum;
}

void LinkedListEnumerator_constructor(LinkedListEnumerator * pThis)
{
	return;
}

void LinkedListEnumerator_destructor(LinkedListEnumerator * pThis)
{
	if(pThis->_llenum)
		LL_enumdelete(pThis->_llenum);
	if(pThis->_list)
		LL_delete(pThis->_list);

	return;
}

HRESULT STDMETHODCALLTYPE EnumConnectionPoints(ConnectionPointContainer * pThis,COMObject ** ppEnum)
{
	LinkedList * newll;

	newll=LL_create();
	LL_push(newll, pThis->_cp);

	(*ppEnum)=CreateLinkedListEnumerator(newll, LL_newenum(newll), 0, 1, (GUID *)&IID_IEnumConnectionPoints);
	return S_OK;
}

HRESULT STDMETHODCALLTYPE FindConnectionPoint(ConnectionPointContainer * pThis,REFIID riid,ConnectionPoint ** ppCP)
{
	printf("finding connection point\n");

	if(IsEqualIID(pThis->_cp->_iid, riid))
		return DefaultQueryInterface((COMObject *)pThis->_cp, &IID_IConnectionPoint, (COMObject **)ppCP);

	return CONNECT_E_NOCONNECTION;
}

void ConnectionPointContainer_constructor(ConnectionPointContainer * pThis)
{
	return;
}

void ConnectionPointContainer_destructor(ConnectionPointContainer * pThis)
{
	DefaultRelease((COMObject *)pThis->_cp);
	InternalRelease();

	return;
}

HRESULT STDMETHODCALLTYPE GetConnectionInterface(ConnectionPoint * pThis,IID * pIID)
{
	memcpy(pIID, pThis->_iid, sizeof(GUID));
	return S_OK;
}

HRESULT STDMETHODCALLTYPE GetConnectionPointContainer(ConnectionPoint * pThis,ConnectionPointContainer ** ppCPC)
{
	if(pThis->_cpc->isValid)
		return DefaultQueryInterface(pThis->_cpc->to, &IID_IConnectionPointContainer, (COMObject **)ppCPC);

	return E_UNEXPECTED;
}

typedef struct _name_id_pair_struct
{
	BSTR name;
	DISPID id;
} _name_id_pair;

HRESULT STDMETHODCALLTYPE Advise(ConnectionPoint * pThis,IUnknown * pUnk,DWORD * pdwCookie)
{
	CONNECTDATA_EX * newconn;
	DISPID curid;
	LinkedList * pairs;
	IDispatch * curdisp;
	_name_id_pair * curpair;
	HRESULT hr;

	printf("advising callback interface\n");

	newconn=create(CONNECTDATA_EX);
	newconn->data.dwCookie=(DWORD)newconn;
	newconn->data.pUnk=0;

	(*pdwCookie)=0;

	if((hr=pUnk->lpVtbl->QueryInterface(pUnk, &IID_IDispatch, &curdisp))==S_OK)
	{
		newconn->data.pUnk=(IUnknown *)curdisp;

		//match dispids
		newconn->dispids=BT_create((BTCompare)pointer_compare);
		pairs=BT_pairs(pThis->name2dispid);
		while(curpair=(_name_id_pair *)LL_pop(pairs))
		{
			if(curdisp->lpVtbl->GetIDsOfNames(curdisp, &IID_NULL, &(curpair->name), 1, 0, &curid)==S_OK)
				BT_insert(newconn->dispids, (void *)curpair->id, (void *)curid);

			clean(curpair);
		}
		LL_delete(pairs);

		//insert new connection
		(*pdwCookie)=(DWORD)newconn;
		BT_insert(pThis->_connections, newconn, newconn);

		//pUnk->lpVtbl->Release(pUnk);

		return S_OK;
	}

	//pUnk->lpVtbl->Release(pUnk);

	clean(newconn);

	return E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE Unadvise(ConnectionPoint * pThis,DWORD dwCookie)
{
	CONNECTDATA_EX * curconn;

	if(curconn=(CONNECTDATA_EX *)BT_find(pThis->_connections, (void *)dwCookie))
	{
		BT_remove(pThis->_connections, (void *)dwCookie);
		curconn->data.pUnk->lpVtbl->Release(curconn->data.pUnk);
		BT_delete(curconn->dispids);
		clean(curconn);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE EnumConnections(ConnectionPoint * pThis,COMObject ** ppEnum)
{
	LinkedList * btlist;

	btlist=BT_values(pThis->_connections);
	if(btlist&&((*ppEnum)=CreateLinkedListEnumerator(btlist, LL_newenum(btlist), sizeof(CONNECTDATA), 0, (GUID *)&IID_IEnumConnections)))
		return S_OK;
	else
		return E_UNEXPECTED;
}

void ConnectionPoint_constructor(ConnectionPoint * pThis)
{
	///nothing todo here
	return;
}

void ConnectionPoint_destructor(ConnectionPoint * pThis)
{
	//cleanup and delete all connections
	CONNECTDATA_EX * curdata;
	BSTR curname;
	LinkedList * names;
	
	while((pThis->_connections->root)&&(curdata=(CONNECTDATA_EX *)(pThis->_connections->root->data)))
	{
		BT_remove(pThis->_connections, curdata);
		curdata->data.pUnk->lpVtbl->Release(curdata->data.pUnk);
		BT_delete(curdata->dispids);
		clean(curdata);
	}
	BT_delete(pThis->_connections);

	names=BT_keys(pThis->name2dispid);
	while(curname=(BSTR)LL_pop(names))
	{
		BT_remove(pThis->name2dispid, curname);
		SysFreeString(curname);
	}
	LL_delete(names);
	BT_delete(pThis->name2dispid);

	DeleteWeakReference(pThis->_cpc);

	return;
}

COMObject * CreateConnectionPointContainer(COMObject * forobject)
{
	ConnectionPointContainer * toreturn;

	toreturn=(ConnectionPointContainer *)ConstructObject(&defConnectionPointContainer);

	toreturn->header.parent=forobject;

	//setup CPC here
	toreturn->_cp=(ConnectionPoint *)CreateConnectionPoint(forobject->_class->events_iid, toreturn);
	InternalAddRef();//<- not really required

	return (COMObject *)toreturn;
}

COMObject * CreateConnectionPoint(GUID * outgoing_iid, ConnectionPointContainer * cpc)
{
	ConnectionPoint * toreturn;
	ITypeInfo * tinfo;
	TYPEATTR * tattr;
	FUNCDESC * fdesc;
	unsigned int i;
	BSTR name;
	unsigned int found;

	toreturn=(ConnectionPoint *)ConstructObject(&defConnectionPoint);

	//initialize here
	toreturn->_cpc=CreateWeakReference((COMObject *)cpc);
	toreturn->_iid=outgoing_iid;
	toreturn->_connections=BT_create((BTCompare)pointer_compare);
	toreturn->name2dispid=BT_create((BTCompare)wcscmp);

	//lookup name->dispid pairs from typeinfo
	if(tlb&&(tlb->lpVtbl->GetTypeInfoOfGuid(tlb, outgoing_iid, &tinfo)==S_OK))
	{
		if(tinfo->lpVtbl->GetTypeAttr(tinfo, &tattr)==S_OK)
		{
			for(i=0;i<tattr->cFuncs;i++)
			{
				if(tinfo->lpVtbl->GetFuncDesc(tinfo, i, &fdesc)==S_OK)
				{
					if((tinfo->lpVtbl->GetNames(tinfo, fdesc->memid, &name, 1, &found)==S_OK)&&(found>0))
						BT_insert(toreturn->name2dispid, name, (void *)fdesc->memid);
					
					tinfo->lpVtbl->ReleaseFuncDesc(tinfo, fdesc);
				}
			}

			tinfo->lpVtbl->ReleaseTypeAttr(tinfo, tattr);
		}
		tinfo->lpVtbl->Release(tinfo);
	}

	return (COMObject *)toreturn;
}

DISPPARAMS * DispStack(unsigned int stacksize)
{
	unsigned int i;
	DISPPARAMS * toreturn;

	toreturn=create(DISPPARAMS);
	if(stacksize==0)
		toreturn->rgvarg=0;
	else
		toreturn->rgvarg=(VARIANT *)alloc(sizeof(VARIANT)*stacksize);
	toreturn->rgdispidNamedArgs=0;
	toreturn->cArgs=0;
	toreturn->cNamedArgs=0;

	for(i=0;i<stacksize; i++)
		VariantInit(&(toreturn->rgvarg[i]));

	return toreturn;
}

void DispPush(DISPPARAMS * dispstack, VARIANT * arg)
{
	VariantCopy(&(dispstack->rgvarg[dispstack->cArgs]), arg);
	VariantClear(arg);
	clean(arg);
	dispstack->cArgs++;

	return;
}

VARIANT * VObject(IUnknown * pUnk)
{
	VARIANT * toreturn;

	toreturn=create(VARIANT);
	pUnk->lpVtbl->QueryInterface(pUnk, &IID_IUnknown, (void **)&(toreturn->punkVal));
	toreturn->vt=VT_UNKNOWN;

	return toreturn;
}

VARIANT * VString(char * str)
{
	VARIANT * toreturn;

	toreturn=create(VARIANT);
	toreturn->bstrVal=char2bstr(str);
	toreturn->vt=VT_BSTR;

	return toreturn;
}

VARIANT * VInt(int value)
{
	VARIANT * toreturn;

	toreturn=create(VARIANT);
	toreturn->lVal=value;
	toreturn->vt=VT_I4;

	return toreturn;
}

VARIANT * VByteArray(unsigned char * array, unsigned int size)
{
	VARIANT * toreturn;

	void HUGEP * arrdata;

	toreturn=create(VARIANT);
	toreturn->vt=VT_ARRAY|VT_UI1;
	
	if(toreturn->parray=SafeArrayCreateVector(VT_UI1,0, size))
	{
		if(SafeArrayAccessData(toreturn->parray,&arrdata)==S_OK)
		{
			memcpy(arrdata, array, size);
			SafeArrayUnaccessData(toreturn->parray);
		}
	}

	return toreturn;
}

HRESULT DoInvoke(IUnknown * onobject, DISPID methodid, DISPPARAMS * pars, VARIANT * result)
{
	HRESULT toreturn;
	IDispatch * curdispatch;
	EXCEPINFO except;
	unsigned int argnumb;
//	char * temp;

	toreturn=E_UNEXPECTED;

	if(onobject->lpVtbl->QueryInterface(onobject, &IID_IDispatch, &curdispatch)==S_OK)
	{
		
		toreturn=curdispatch->lpVtbl->Invoke(curdispatch, methodid, &IID_NULL, 0, DISPATCH_METHOD, pars, result, &except, &argnumb);
		if(toreturn!=S_OK)
		{
			printf("arg number %u bugged (code %u, hr %x)\n", argnumb, (unsigned int)except.wCode, toreturn);
		}
		curdispatch->lpVtbl->Release(curdispatch);
	}

	//cleanup
	if(pars->rgvarg)
		clean(pars->rgvarg);
	clean(pars);

	return toreturn;	
}

HRESULT DoPropGet(IUnknown * onobject, DISPID propid, DISPPARAMS * pars, VARIANT * result)
{
	HRESULT toreturn;
	IDispatch * curdispatch;

	toreturn=E_UNEXPECTED;

	if(onobject->lpVtbl->QueryInterface(onobject, &IID_IDispatch, &curdispatch)==S_OK)
	{
		toreturn=curdispatch->lpVtbl->Invoke(curdispatch, propid, &IID_NULL, 0, DISPATCH_PROPERTYGET, pars, result, 0, 0);
		curdispatch->lpVtbl->Release(curdispatch);
	}

	//cleanup
	if(pars->rgvarg)
		clean(pars->rgvarg);
	clean(pars);

	return toreturn;	
}

HRESULT DoPropSet(IUnknown * onobject, DISPID propid, DISPPARAMS * pars, VARIANT * result)
{
	HRESULT toreturn;
	IDispatch * curdispatch;

	toreturn=E_UNEXPECTED;

	if(onobject->lpVtbl->QueryInterface(onobject, &IID_IDispatch, &curdispatch)==S_OK)
	{
		toreturn=curdispatch->lpVtbl->Invoke(curdispatch, propid, &IID_NULL, 0, DISPATCH_PROPERTYPUT, pars, result, 0, 0);
		curdispatch->lpVtbl->Release(curdispatch);
	}

	//cleanup
	if(pars->rgvarg)
		clean(pars->rgvarg);
	clean(pars);

	return toreturn;	
}

HRESULT InvokeEvent(COMObject * onobject, DISPID eventid, DISPPARAMS * pars, VARIANT * result)
{
	BinaryTreeEnum * btenum;
	CONNECTDATA_EX * curdata;
	IDispatch * curdispatch;
	DISPID actualid;

	unsigned int conncount;

	HRESULT toreturn;

	conncount=0;
	toreturn=E_UNEXPECTED;

	btenum=BT_newenum(((ConnectionPointContainer *)onobject->_CPC)->_cp->_connections);
	while(curdata=(CONNECTDATA_EX *)BT_next(btenum))
	{
		conncount++;
		actualid=(DISPID)BT_find(curdata->dispids, (void *)eventid);
		if(curdata->data.pUnk->lpVtbl->QueryInterface(curdata->data.pUnk, &IID_IDispatch, &curdispatch)==S_OK)
		{
			toreturn=curdispatch->lpVtbl->Invoke(curdispatch, actualid, &IID_NULL, 0, DISPATCH_METHOD, pars, result, 0, 0);
			curdispatch->lpVtbl->Release(curdispatch);
		}
		else
			printf("dispatch query interface failure on event interface object\n");
	}
	BT_enumdelete(btenum);

	printf("conncount : %u\n", conncount);

	//cleanup
	if(pars->rgvarg)
		clean(pars->rgvarg);
	clean(pars);

	return toreturn;	
}

COMObject * CreateObject(GUID * clsid)
{
	COMClass * requestedclass;

	if(requestedclass=(COMClass *)BT_find(known_classes, clsid))
		return ConstructObject(requestedclass);
	else
		return 0;
}

COMClass * FindClass(GUID * clsid)
{
	return (COMClass *)BT_find(known_classes, clsid);
}

int doAdvise(IUnknown * onobject, GUID * callbackiid, IUnknown * callbackinterface, unsigned int * cookie)
{
	IConnectionPointContainer * cpc;
	IConnectionPoint * cp;
	int toreturn;

	(*cookie)=0;
	toreturn=0;
	if(onobject->lpVtbl->QueryInterface(onobject, &IID_IConnectionPointContainer, &cpc)==S_OK)
	{
		if(cpc->lpVtbl->FindConnectionPoint(cpc, (REFIID)callbackiid, &cp)==S_OK)
		{
			callbackinterface->lpVtbl->AddRef(callbackinterface);
			if(cp->lpVtbl->Advise(cp, callbackinterface, cookie)==S_OK)
				toreturn=1;
			cp->lpVtbl->Release(cp);
		}
		cpc->lpVtbl->Release(cpc);
	}
	return toreturn;
}

int doUnadvise(IUnknown * onobject, GUID * callbackiid, unsigned int cookie)
{
	IConnectionPointContainer * cpc;
	IConnectionPoint * cp;
	int toreturn;

	toreturn=0;
	if(onobject->lpVtbl->QueryInterface(onobject, &IID_IConnectionPointContainer, &cpc)==S_OK)
	{
		if(cpc->lpVtbl->FindConnectionPoint(cpc, (REFIID)callbackiid, &cp)==S_OK)
		{
			if(cp->lpVtbl->Unadvise(cp, cookie)==S_OK)
				toreturn=1;
			cp->lpVtbl->Release(cp);
		}
		cpc->lpVtbl->Release(cpc);
	}
	return toreturn;
}

void InternalLock()
{
	LockCount++;
	return;
}

void InternalUnlock()
{
	LockCount--;
	return;
}

