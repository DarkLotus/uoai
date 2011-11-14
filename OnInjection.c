#include "OnInjection.h"
#include <windows.h>
#include <stdio.h>

#include "UAC.h"
#include "Client.h"
#include "ClientList.h"
//#include "COMBase.h"

#include "COMInjector.h"

//The onInjection function below is exported and used as entrypoint after injection on a remote process

volatile unsigned int initialized=0;
HHOOK hMsgHook=0;
IUnknown * clientlist_object=0;

LRESULT CALLBACK MsgHook(int code, WPARAM wParam, LPARAM lParam)
{
	MSG * pMsg;
	COMClass * clientclass;
	DISPPARAMS * pars;
	
	pMsg=(MSG *)lParam;

	switch(pMsg->message)
	{
	case WM_QUIT:
		if((clientclass=FindClass((GUID *)&CLSID_Client))&&(clientclass->activeobject!=0))
		{
			//invoke onClose event
			pars=DispStack(1);
			DispPush(pars, VObject(clientclass->activeobject));
			InvokeEvent((COMObject *)(clientclass->activeobject), 1, pars, 0);

			//get client list, unregister self
			if(clientlist_object)
			{
				//Invoke ClientList->UnregisterClient(...)
				pars=DispStack(1);
				DispPush(pars, VObject((IUnknown *)clientclass->activeobject));
				DoInvoke(clientlist_object, 3, pars, 0);

				//release ClientList
				clientlist_object->lpVtbl->Release(clientlist_object);
				clientlist_object=0;
			}
		}
		break;
	default:
		break;
	}

	return CallNextHookEx(0, code, wParam, lParam);
}


//Entry Point after injection of this dll into a client's process:
//	- we need to setup the Client's COM interface
//	- callibration is done remotely, and passed through the parametr of this function
//	- the Client interface is to be an active object (we don't want multiple instances, we are on one specific client process at the moment)
//	- we need to register that interface with the client list, so we obtain the client list object (which is a remote one, on a surrogate process)
//	- 
void OnInjection(OnInjectionParameters * parameters)
{
	COMClass * clientclass;
	Client * clientobject;
	DISPPARAMS * pars;
	HRESULT hr;

	if(InterlockedCompareExchange(&initialized, 1, 0)==0)
	{
		//initialize COM
		if(InitializeCOM())
		{
//#if 0
			//debug version : open a console and print the parameters
			if(AllocConsole())
			{
				freopen("CONIN$","rb",stdin);
				freopen("CONOUT$","wb",stdout);
				freopen("CONOUT$","wb",stderr);
			}
//#endif

			if(clientclass=FindClass((GUID *)&CLSID_Client))
			{
				SetCallibrations(&(parameters->callibrations));
				//setup injected client COM object
				clientobject=(Client *)ConstructObject(clientclass);
				SetActiveObject(clientclass, (IUnknown *)clientobject);
				DefaultRelease((COMObject *)clientobject);//as activeobject it is kept alive

				//get clientlist, register self, note that if the app accessing the client is running as admin, we need to confirm the admin rights here (can be annoying)
				if((parameters->requireadminrights)&&(IsUserAdmin()==0))
					hr=CoCreateInstanceAsAdmin(&CLSID_ClientList, &IID_IUnknown, (void **)&clientlist_object);
				else
					hr=CoCreateInstance(&CLSID_ClientList, 0, CLSCTX_LOCAL_SERVER, &IID_IUnknown, (void **)&clientlist_object);

				if(hr==S_OK)
				{
					//invoke ClientList->RegisterClient(...)
					pars=DispStack(1);
					DispPush(pars, VObject((IUnknown *)clientobject));
					DoInvoke(clientlist_object, 2, pars, 0);
				}
				else
					printf("clientlist could not be obtained!\n");
				
				//need msg-hook (for WM_CLOSE)
				hMsgHook=SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)MsgHook, 0, parameters->tid);
			}
		}
		else
			printf("COM initialization failure!\n");
	}
	else
		printf("duplicate injection\n");

	return;
}