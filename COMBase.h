#ifndef COMBASE_INCLUDED
#define COMBASE_INCLUDED

/*
	Code required to manage COM class/object creation from C code.

	-- Wim Decelle
*/

#include "Features.h"

#include "LinkedList.h"
#include "COMInits.h"
#include "BinaryTree.h"

#define _WIN32_DCOM

#include <objbase.h>
#include <OCIdl.h>
#include <Oleauto.h>
#include <olectl.h>

typedef struct COMObjectStruct COMObject;

typedef void (*Structor)(COMObject * object);

typedef struct COMClassStruct
{
	char * name;
	GUID * clsid;
	unsigned int default_iid;
	unsigned int iids_count;
	GUID ** iids;
	GUID * events_iid;//0 if no events
	unsigned int size;
	void * lpVtbl;
	Structor constructor;
	Structor destructor;
	ITypeInfo * typeinfo;
	int register_as_local_server;
	COMObject * factory;
	IUnknown * activeobject;
	BinaryTree * instances;
} COMClass;

typedef struct COMObjectStruct
{
	void * lpVtbl;
	unsigned int refcount;
	struct COMObjectStruct * parent;//if parent, then addref/release is done by the parent
	COMClass * _class;
	struct COMObjectStruct * _CPC;
	BinaryTree * weakreferences;
} COMObject;

typedef struct WeakReferenceStruct
{
	COMObject * to;
	unsigned int isValid;
} WeakReference;

WeakReference * CreateWeakReference(COMObject * to);
void DeleteWeakReference(WeakReference * wr);

void InternalAddRef();
void InternalRelease();

typedef struct COMConfigStruct
{
	char ServerName[256];
	char TypeLibFile[256];
	GUID * AppId;
	//int isLocalServer;
} COMConfig;

COMConfig * GetCOMBaseConfigurations();

//construction/destruction
void DestructObject(COMObject * todestruct);
COMObject * ConstructObject(COMClass  * fromclass);

//default IUnknown implementation
HRESULT STDMETHODCALLTYPE DefaultQueryInterface(COMObject * pThis,REFIID riid, COMObject ** ppv);
ULONG STDMETHODCALLTYPE DefaultAddRef(COMObject * pThis);
ULONG STDMETHODCALLTYPE DefaultRelease(COMObject * pThis);

typedef HRESULT (STDMETHODCALLTYPE * pQueryInterface)(IUnknown *, REFIID riid, void **);
typedef HRESULT (STDMETHODCALLTYPE * pAddRef)(IUnknown *);
typedef HRESULT (STDMETHODCALLTYPE * pRelease)(IUnknown *);

#define DEFAULT_UNKNOWN\
	pQueryInterface QueryInterface;\
	pAddRef AddRef\
	pRelease Release

#define DEFAULT_UNKNOWN_VTBL (pQueryInterface)DefaultQueryInterface, (pAddRef)DefaultAddRef, (pRelease)DefaultRelease

//default class factory implementation
HRESULT STDMETHODCALLTYPE DefaultClassCreateInstance(COMObject * pThis,IUnknown * punkOuter, REFIID vTableGuid, COMObject ** ppv);
HRESULT STDMETHODCALLTYPE DefaultClassLockServer(COMObject * pThis,BOOL flock);

typedef HRESULT (STDMETHODCALLTYPE * pClassQueryInterface)(IClassFactory *, REFIID riid, void **);
typedef HRESULT (STDMETHODCALLTYPE * pClassAddRef)(IClassFactory *);
typedef HRESULT (STDMETHODCALLTYPE * pClassRelease)(IClassFactory *);
typedef HRESULT (STDMETHODCALLTYPE * pClassCreateInstance)(IClassFactory *,IUnknown *, REFIID, void ** ppv);
typedef HRESULT (STDMETHODCALLTYPE * pClassLockServer)(IClassFactory * pThis,BOOL flock);

//default dispatch implementation
HRESULT STDMETHODCALLTYPE DefaultGetTypeInfoCount(COMObject * pThis, UINT * pCount);
HRESULT STDMETHODCALLTYPE DefaultGetTypeInfo(COMObject * pThis, UINT itinfo, LCID lcid, ITypeInfo ** pTypeInfo);
HRESULT STDMETHODCALLTYPE DefaultGetIDsOfNames(COMObject * pThis, REFIID riid, LPOLESTR * rgszNames, UINT cNames, LCID lcid, DISPID * rgdispid);
HRESULT STDMETHODCALLTYPE DefaultInvoke(COMObject * pThis, DISPID dispid, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *pexcepinfo, UINT *puArgErr);

typedef HRESULT (STDMETHODCALLTYPE * pDispatchQueryInterface)(IDispatch *, REFIID riid, void **);
typedef HRESULT (STDMETHODCALLTYPE * pDispatchAddRef)(IDispatch *);
typedef HRESULT (STDMETHODCALLTYPE * pDispatchRelease)(IDispatch *);
typedef HRESULT (STDMETHODCALLTYPE * pDispatchGetTypeInfoCount)(IDispatch * pThis, UINT * pCount);
typedef HRESULT (STDMETHODCALLTYPE * pDispatchGetTypeInfo)(IDispatch * pThis, UINT itinfo, LCID lcid, ITypeInfo ** pTypeInfo);
typedef HRESULT (STDMETHODCALLTYPE * pDispatchGetIDsOfNames)(IDispatch * pThis, REFIID riid, LPOLESTR * rgszNames, UINT cNames, LCID lcid, DISPID * rgdispid);
typedef HRESULT (STDMETHODCALLTYPE * pDispatchInvoke)(IDispatch * pThis, DISPID dispid, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *pexcepinfo, UINT *puArgErr);

#define DEFAULT_DISPATCH\
	pDispatchQueryInterface QueryInterface;\
	pDispatchAddRef AddRef;\
	pDispatchRelease Release;\
	pDispatchGetTypeInfoCount GetTypeInfoCount;\
	pDispatchGetTypeInfo GetTypeInfo;\
	pDispatchGetIDsOfNames GetIDsOfNames;\
	pDispatchInvoke Invoke

#define DEFAULT_DISPATCH_VTBL (pDispatchQueryInterface)DefaultQueryInterface, (pDispatchAddRef)DefaultAddRef, (pDispatchRelease)DefaultRelease, (pDispatchGetTypeInfoCount)DefaultGetTypeInfoCount, (pDispatchGetTypeInfo)DefaultGetTypeInfo, (pDispatchGetIDsOfNames)DefaultGetIDsOfNames, (pDispatchInvoke)DefaultInvoke

unsigned int olestrlen(LPOLESTR str);
char * ole2char(LPOLESTR torewrite);
short * char2ole(char * toconvert);
BSTR char2bstr(char * toconvert);

void AddCOMClass(COMClass * toregister);
void SetActiveObject(COMClass * _class, IUnknown * activeobject);
BinaryTree * GetKnownClasses();

HRESULT PASCAL DllRegisterServer();
HRESULT PASCAL DllUnregisterServer();
HRESULT PASCAL DllGetClassObject(REFCLSID objGuid, REFIID factoryGuid, void **factoryHandle);
HRESULT PASCAL DllCanUnloadNow(void);

typedef struct ClassFactoryStruct
{
	COMObject header;
	COMClass * forclass;
} ClassFactory;

COMObject * CreateFactory(COMClass * forclass);

//LinkedListEnumerator
typedef struct LinkedListEnumeratorStruct
{
	COMObject header;
	LinkedList * _list;
	LinkedListEnum * _llenum;//enumerator
	unsigned int datasize;//is data to be copied?
	int type;//addref before return?
	GUID * _iid;//overload of the default iid
} LinkedListEnumerator;

HRESULT STDMETHODCALLTYPE LLQueryInterface(COMObject * pThis,REFIID riid, COMObject ** ppv);
HRESULT STDMETHODCALLTYPE LLNext(COMObject * pThis, ULONG count, void ** arr_out, unsigned int * _count);
HRESULT STDMETHODCALLTYPE LLSkip(COMObject * pThis, ULONG count);
HRESULT STDMETHODCALLTYPE LLReset(COMObject * pThis);
HRESULT STDMETHODCALLTYPE LLClone(COMObject * pThis, void ** ppenum);

typedef struct ILinkedListEnumeratorVtblstruct
{
	HRESULT (STDMETHODCALLTYPE * LLQueryInterface)(COMObject * pThis,REFIID riid, COMObject ** ppv);
	ULONG (STDMETHODCALLTYPE * DefaultAddRef)(COMObject * pThis);
	ULONG (STDMETHODCALLTYPE * DefaultRelease)(COMObject * pThis);
	HRESULT (STDMETHODCALLTYPE * LLNext)(COMObject * pThis, ULONG count, void ** arr_out, unsigned int * _count);
	HRESULT (STDMETHODCALLTYPE * LLSkip)(COMObject * pThis, ULONG count);
	HRESULT (STDMETHODCALLTYPE * LLReset)(COMObject * pThis);
	HRESULT (STDMETHODCALLTYPE * LLClone)(COMObject * pThis, void ** ppenum);
} ILinkedListEnumeratorVtbl;

COMObject * CreateLinkedListEnumerator(LinkedList * list, LinkedListEnum * llenum, unsigned int datasize, int type, GUID * iid);

//ConnectionPointContainer and ConnectionPoint (automation events)
typedef struct ConnectionPointStruct ConnectionPoint;

typedef struct ConnectionPointContainerStruct
{
	COMObject header;
	ConnectionPoint * _cp;
} ConnectionPointContainer;

typedef struct ConnectionPointContainerVtblStruct
{
	pQueryInterface QueryInterface;
	pAddRef AddRef;
	pRelease Release;
	HRESULT (STDMETHODCALLTYPE * EnumConnectionPoints)(ConnectionPointContainer * pThis,COMObject ** ppEnum);
	HRESULT (STDMETHODCALLTYPE * FindConnectionPoint)(ConnectionPointContainer * pThis,REFIID riid,ConnectionPoint ** ppCP);
} ConnectionPointContainerVtbl;

HRESULT STDMETHODCALLTYPE EnumConnectionPoints(ConnectionPointContainer * pThis,COMObject ** ppEnum);
HRESULT STDMETHODCALLTYPE FindConnectionPoint(ConnectionPointContainer * pThis,REFIID riid,ConnectionPoint ** ppCP);
void ConnectionPointContainer_constructor(ConnectionPointContainer * pThis);
void ConnectionPointContainer_destructor(ConnectionPointContainer * pThis);

typedef struct CONNECTDATA_EX_struct
{
	CONNECTDATA data;
	BinaryTree * dispids;//original dispid -> actual dispid (<- don't know why, but C# COM wrappers generated by the interop code 
						 //don't stick to the DISPIDs specified in the typelibrary... 
						 //so we can't invoke events using dispids, but have to use names...
						 //by looking up the name->DISPID pair once whenever a sink get's advised...
						 //and storing "expected dispid -> actual dispid" pairs here, code should still run smoothely
} CONNECTDATA_EX;

typedef struct ConnectionPointStruct
{
	COMObject header;
	WeakReference * _cpc;
	GUID * _iid;
	BinaryTree * _connections;//CONNECTDATA structures
	BinaryTree * name2dispid;//name->dispid from the event guids typeinfo
} ConnectionPoint;

typedef struct ConnectionPointVtblStruct
{
	pQueryInterface QueryInterface;
	pAddRef AddRef;
	pRelease Release;
	HRESULT (STDMETHODCALLTYPE * GetConnectionInterface)(ConnectionPoint * pThis,IID * pIID);
	HRESULT (STDMETHODCALLTYPE * GetConnectionPointContainer)(ConnectionPoint * pThis,ConnectionPointContainer ** ppCPC);
	HRESULT (STDMETHODCALLTYPE * Advise)(ConnectionPoint * pThis,IUnknown * pUnk,DWORD * pdwCookie);
	HRESULT (STDMETHODCALLTYPE * Unadvise)(ConnectionPoint * pThis,DWORD dwCookie);
	HRESULT (STDMETHODCALLTYPE * EnumConnections)(ConnectionPoint * pThis,COMObject ** ppEnum);
} ConnectionPointVtbl;

HRESULT STDMETHODCALLTYPE GetConnectionInterface(ConnectionPoint * pThis,IID * pIID);
HRESULT STDMETHODCALLTYPE GetConnectionPointContainer(ConnectionPoint * pThis,ConnectionPointContainer ** ppCPC);
HRESULT STDMETHODCALLTYPE Advise(ConnectionPoint * pThis,IUnknown * pUnk,DWORD * pdwCookie);
HRESULT STDMETHODCALLTYPE Unadvise(ConnectionPoint * pThis,DWORD dwCookie);
HRESULT STDMETHODCALLTYPE EnumConnections(ConnectionPoint * pThis,COMObject ** ppEnum);
void ConnectionPoint_constructor(ConnectionPoint * pThis);
void ConnectionPoint_destructor(ConnectionPoint * pThis);

COMObject * CreateConnectionPointContainer(COMObject * forobject);
COMObject * CreateConnectionPoint(GUID * outgoing_iid, ConnectionPointContainer * cpc);

HRESULT InvokeEvent(COMObject * onobject, DISPID eventid, DISPPARAMS * pars, VARIANT * result);
VARIANT * VByteArray(unsigned char * array, unsigned int size);
VARIANT * VInt(int value);
VARIANT * VString(char * str);
VARIANT * VObject(IUnknown * pDisp);
VARIANT * VDispObject(IDispatch * pUnk);
void DispPush(DISPPARAMS * dispstack, VARIANT * arg);
DISPPARAMS * DispStack(unsigned int stacksize);

COMObject * CreateObject(GUID * clsid);

#define COMCALL HRESULT STDMETHODCALLTYPE
#define pCOMCALL(name) HRESULT (STDMETHODCALLTYPE * name)

#define TO_VARIANT_BOOL(a) (a==0?VARIANT_FALSE:VARIANT_TRUE)
#define FROM_VARIANT_BOOL(a) (a==VARIANT_FALSE?0:1)

COMClass * FindClass(GUID * clsid);

HRESULT DoInvoke(IUnknown * onobject, DISPID methodid, DISPPARAMS * pars, VARIANT * result);
HRESULT DoPropGet(IUnknown * onobject, DISPID propid, DISPPARAMS * pars, VARIANT * result);
HRESULT DoPropSet(IUnknown * onobject, DISPID propid, DISPPARAMS * pars, VARIANT * result);

int InitializeCOM();//don't call this unless you know why... inits are typically done automatically from DllGetClassObject
					//... however i need it to setup COM after injection (when the dll wasn't loaded through a CoCreateInstance call)

int doUnadvise(IUnknown * onobject, GUID * callbackiid, unsigned int cookie);
int doAdvise(IUnknown * onobject, GUID * callbackiid, IUnknown * callbackinterface, unsigned int * cookie);

void InternalLock();
void InternalUnlock();

void ForceUnload();

#endif