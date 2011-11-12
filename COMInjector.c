#include "COMInjector.h"
#include "allocation.h"
#include "Streams.h"
#include "OnInjection.h"
#include "Injection.h"
#include "BinaryTree.h"

unsigned int injector_initialized=0;
char * window_class_name=0;
BinaryTree * bInjected=0;
unsigned int timerid=0;
int injectionparameter=0;
unsigned int dwReg=0;
unsigned int timer_busy=0;

int _pid_compare(unsigned int a, unsigned int b)
{
	if(a<b)
		return +1;
	else if(a>b)
		return -1;
	else
		return 0;
}

//when a client is found, inject UOAI.dll into it
//	the "OnInjection method" exported by UOAI.dll will be the entrypoint on the client process
void InjectClient(Window * clientwindow)
{
	Process * cproc;
	Thread * cthread;
	char uoai_path[256];
	OnInjectionParameters pars;

	if(GetModuleNameFromAddress((unsigned int)InjectClient, 0, 0, uoai_path, 256))
	{
		memset(&pars, 0, sizeof(OnInjectionParameters));
		pars.requireadminrights=injectionparameter;
		pars.pid=clientwindow->pid;
		pars.tid=clientwindow->tid;
		pars.hwnd=(unsigned int)clientwindow->hWnd;

		printf("injecting %s\n\t\tonto client with pid %u\n", uoai_path, clientwindow->pid);

		cproc=GetProcess(clientwindow->pid, TYPICAL_PROCESS_PERMISSIONS);
		cthread=GetThread(clientwindow->tid, TYPICAL_THREAD_PERMISSIONS);

		//SuspendProcess(cproc);
		if(CallibrateClient(pars.pid, &(pars.callibrations))==0)
			printf("callibration failure!?\n");
		//ResumeProcess(cproc);

		Inject(cproc, cthread, uoai_path, "OnInjection", &pars, sizeof(OnInjectionParameters));

		//cleanup
		CloseHandle(cthread->handle);
		clean(cthread);
		CloseHandle(cproc->hThread);
		CloseHandle(cproc->hProcess);
		clean(cproc);
	}
}

//list new clients
//	-> actively looks for new clients
//	-> injects UOAI.dll onto any found client
//	-> there (see onInjection code) the clients COM interface gets initialized and registered with the running ClientList
void ClientList_Timer(HWND hWnd, unsigned int uMsg, unsigned int * idEvent, DWORD dwTime)
{
	LinkedList * clientwindows;
	Window * curwin;
	BinaryTree * new_bInjected;
	unsigned int clientcount_backup;
	
	if(InterlockedExchange(&timer_busy, 1)==0)//don't think this is required, if reantrance is a problem this would in fact hang the COMInjector surrogate
	{
		clientcount_backup=bInjected->itemcount;

		//new tree
		new_bInjected=BT_create((BTCompare)_pid_compare);

		//list all client windows
		clientwindows=FindWindowByClassName(window_class_name);

		//windowlist -> tree
		while(curwin=(Window *)LL_pop(clientwindows))
		{
			//if client corresponding to this PID is not known yet, inject it
			if(BT_find(bInjected, (void *)curwin->pid)==0)
				InjectClient(curwin);//new client, inject
			BT_insert(new_bInjected, (void *)curwin->pid, (void *)1);
			
			clean(curwin);
			
			if((clientcount_backup==0)&&(new_bInjected->itemcount==1))//first client?
				InternalLock();//lock -> from the first detected client until the last client disappears the server is to remain locked, this forces the surrogate to remain alive as long as an ultima online client and/or a UOAI based app is running.
		}
		LL_delete(clientwindows);

		//swap trees (effectively removes all client's that got shut down)
		BT_delete(bInjected);
		bInjected=new_bInjected;

		if((clientcount_backup!=0)&&(bInjected->itemcount==0))//last client disappeared?
			InternalUnlock();//unlock server :: we can shut down now if there are no outstanding references

		//release the timer lock, don't think the lock is usefull though
		InterlockedExchange(&timer_busy, 0);
	}

	return;
}

//setup configurations and start listing clients
// or change configurations
COMCALL Initialize(COMObject * pThis, BSTR bstr_window_class_name, int parameter)
{
	//DWORD temp;

	//<to be removed>
#if 0
	if(AllocConsole())
		{
			freopen("CONIN$","rb",stdin);
			freopen("CONOUT$","wb",stdout);
			freopen("CONOUT$","wb",stderr);
		}
#endif

	//copy new class name (as C string, rather than BSTR)
	if(window_class_name)
		clean(window_class_name);
	window_class_name=ole2char((LPOLESTR)bstr_window_class_name);
	SysFreeString(bstr_window_class_name);

	//parameter passed to the client after injection, currently not important
	injectionparameter=parameter;

	//initialize if required
	if(InterlockedCompareExchange(&injector_initialized, 1, 0)==0)
	{
		printf("initialized!\n");
		printf("%s\n%u\n", window_class_name, parameter);

		//inits: 
		//	- setup tree of injected clients by pid
		//	- start injection timer : actively look for new clients every 0.5 seconds
		bInjected=BT_create((BTCompare)_pid_compare);
		timerid=SetTimer(0, 0, 500, (TIMERPROC)ClientList_Timer);
	}

	//SetActiveObject(pThis->_class, pThis);
	//RegisterActiveObject(pThis, &CLSID_COMInjector, ACTIVEOBJECT_STRONG, &temp);

	return S_OK;
}
