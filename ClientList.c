#include "ClientList.h"
#include "allocation.h"
#include "Streams.h"
#include "OnInjection.h"
#include "Injection.h"
#include "BinaryTree.h"

#include <windows.h>
#include <stdio.h>

#include "UAC.h"

void validate_clientlist();

BinaryTree * client_by_pid=0;
unsigned int refcount=0;

//ClientList is a passive list of clients
//	passive, in that clients register their COM interface themselves with this client list

int pid_compare(unsigned int a, unsigned int b)
{
	if(a<b)
		return +1;
	else if(a>b)
		return -1;
	else
		return 0;
}

void ClientList_constructor(ClientList * pThis)
{
	if(refcount==0)
	{
		//open console for debugging purposes <to be removed>

		if(AllocConsole())
		{
			freopen("CONIN$","rb",stdin);
			freopen("CONOUT$","wb",stdout);
			freopen("CONOUT$","wb",stderr);
		}

		printf("clientlist\n");

		//client list is maintained as a binary tree, with the client's PID as key
		client_by_pid=BT_create((BTCompare)pid_compare);				
	}

	//global refcount
	//synchronization is not an issue, since we are on a SINGLE THREADED APPARTMENT
	//<- ClientList lives on a seperate process (COM surrogate, typically dllhost.exe)
	refcount++;

	return;
}

void ClientList_destructor(ClientList * pThis)
{
	VARIANT * curclient;

	refcount--;

	if(refcount==0)
	{
		while((client_by_pid->root)&&(curclient=(VARIANT *)client_by_pid->root->data))
		{
			BT_remove(client_by_pid, client_by_pid->root->key);
			VariantClear(curclient);
			clean(curclient);
		}
		BT_delete(client_by_pid);

		// make sure dll host terminates
		ForceUnload();
	}

	return;
}

COMCALL GetCount(ClientList * pThis, unsigned int * count)
{
	//printf("requesting count!\n");

	validate_clientlist();

	(*count)=client_by_pid->itemcount;

	return S_OK;
}

void validate_clientlist()
{
	LinkedList * ll_clients;
	unsigned int pid;
	Process * ps;
	VARIANT * curclient;

	if(ll_clients = BT_keys(client_by_pid))
	{
		while(pid = (unsigned int)LL_pop(ll_clients))
		{
			if(ps=GetProcess(pid, MINIMAL_PROCESS_PERMISSIONS))
				clean(ps);
			else
			{
				/* invalid client in list */
				curclient = (VARIANT *)BT_remove(client_by_pid, (void *)pid);
				VariantClear((VARIANTARG *)curclient);
				clean(curclient);
				InternalAddRef(); /* client shut down without removing itself from the clientlist
									 this should imply the clientlist reference was never released
									 so we make this stale reference internal here to prevent a
									 memory leak */
			}
		}
		LL_delete(ll_clients);
	}
}

COMCALL GetClient(ClientList * pThis, long index, IUnknown ** client)
{
	BinaryTreeEnum * btenum;
	int i;
	VARIANT * curclient;
	HRESULT hr;

	validate_clientlist();

	//printf("finding client?\n");
	curclient=0;
	btenum=BT_newenum(client_by_pid);
	i=0;
	while((i<=index)&&(curclient=(VARIANT *)BT_next(btenum)))
		i++;
	BT_enumdelete(btenum);
	//printf("client list looped, index=%u, curclient=%x!, curclient->pUnkVal=%x\n", index, curclient, curclient->punkVal);

	if((i>index)&&(curclient))
	{
		//printf("querying interface\n");
		hr=curclient->punkVal->lpVtbl->QueryInterface(curclient->punkVal, &IID_IClient, (void **)client);
		//printf("interface query done\n");
		/*if(hr!=S_OK)
			printf("query interface returned %x\n", hr);*/
		return hr;
	}
	else
	{
		//printf("client not found?\n");
		(*client)=0;
		return S_FALSE;
	}

	return hr;
}

COMCALL ClientListNewEnum(ClientList * pThis, IUnknown ** newenum)
{
	LinkedList * clients;

	validate_clientlist();

	clients=BT_values(client_by_pid);

	if((*newenum)=(IUnknown *)CreateLinkedListEnumerator(clients, LL_newenum(clients), 0, 2, (GUID *)&IID_IEnumVARIANT))
	{
		return S_OK;
	}
	else
	{
		(*newenum)=0;
		return S_FALSE;
	}
}

COMCALL RegisterClient(ClientList * pThis, IUnknown * pClientDisp)
{
	DISPPARAMS * curstack;
	VARIANT vpid;
	VARIANT * prevclient;
	COMClass * CLClass;
	COMObject * curl;
	BinaryTreeEnum * btenum;

	curstack=DispStack(0);
	
	//printf("registering client\n");

	pClientDisp->lpVtbl->AddRef(pClientDisp);
	if(DoPropGet(pClientDisp, 1, curstack, &vpid)==S_OK)
	{
		if(VariantChangeType(&vpid, &vpid, 0, VT_UI4)==S_OK)
		{
			//insert client by pid here
			if(prevclient=(VARIANT *)BT_find(client_by_pid, (void *)vpid.ulVal))
			{
				BT_remove(client_by_pid, (void *)vpid.ulVal);
				VariantClear(prevclient);
				clean(prevclient);
			}
			
			BT_insert(client_by_pid, (void *)vpid.ulVal, (void *)VObject(pClientDisp));

			//invoke onNewClient
			if(CLClass=FindClass((GUID *)&CLSID_ClientList))
			{			
				btenum=BT_newenum(CLClass->instances);
				while(curl=(COMObject *)BT_next(btenum))
				{
					curstack=DispStack(1);
					DispPush(curstack, VObject(pClientDisp));
					InvokeEvent(curl, 1, curstack, 0);
				}
				BT_enumdelete(btenum);
			}
			
			VariantClear(&vpid);//<- not required
		}
		else
			printf("change type failed!\n");
	}
	else
		printf("do get prop on client object failed!\n");

	pClientDisp->lpVtbl->Release(pClientDisp);
	return S_OK;
}

COMCALL UnregisterClient(ClientList * pThis, IUnknown * pClientDisp)
{	
	DISPPARAMS * curstack;
	VARIANT vpid;
	VARIANT * prevclient;
	COMClass * CLClass;
	BinaryTreeEnum * btenum;
	COMObject * curl;

	printf("unregistering client\n");

	curstack=DispStack(0);
	if(DoPropGet(pClientDisp, 1, curstack, &vpid)==S_OK)
	{
		if(VariantChangeType(&vpid, &vpid, 0, VT_UI4)==S_OK)
		{
			//remove client by pid here
			if(prevclient=(VARIANT *)BT_find(client_by_pid, (void *)vpid.ulVal))
			{
				//invoke clientlist->onclientclose event
				if(CLClass=FindClass((GUID *)&CLSID_ClientList))
				{			
					btenum=BT_newenum(CLClass->instances);
					while(curl=(COMObject *)BT_next(btenum))
					{
						curstack=DispStack(1);
						DispPush(curstack, VObject(pClientDisp));
						InvokeEvent(curl, 2, curstack, 0);
					}
					BT_enumdelete(btenum);
				}

				BT_remove(client_by_pid, (void *)vpid.ulVal);
				VariantClear(prevclient);
				clean(prevclient);
			}
			
			VariantClear(&vpid);//<- not required
		}
	}
	pClientDisp->lpVtbl->Release(pClientDisp);
	return S_OK;
}

COMCALL FindClient(ClientList * pThis, unsigned int pid, IUnknown ** client, VARIANT_BOOL * bSuccess)
{
	VARIANT * prevclient;

	validate_clientlist();

	(*bSuccess)=VARIANT_FALSE;

	if(prevclient=(VARIANT *)BT_find(client_by_pid, (void *)pid))
	{
		if( prevclient->punkVal->lpVtbl->QueryInterface(prevclient->punkVal, &IID_IClient, (void **)client) == S_OK )
			(*bSuccess)=VARIANT_TRUE;
	}

	return S_OK;
}