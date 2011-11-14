#include "UOAI.h"
#include "UAC.h"
#include <stdio.h>
#include "allocation.h"
#include "Assembler.h"

#include "Process.h"
#include "PE.h"


UOAIConfig configs;
unsigned int req_admin=0;

//Try to obtain the Ultima Online Client Path from its registry entries
int ReadClientPathFromRegistry(char * buff, unsigned int buffsize)
{
	HKEY originkey;
	DWORD size;
	DWORD type;
	
	//UO SA and higher
	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE\\Electronic Arts\\EA Games\\Ultima Online Classic",0,KEY_READ,&originkey)==ERROR_SUCCESS )
	{
		size=buffsize;

		if(RegQueryValueEx(originkey,"InstallDir",0,&type,(LPBYTE)buff,&size)==ERROR_SUCCESS)
		{
			if(buff[strlen(buff)-1]!='\\')
			strcat(buff,"\\");

			RegCloseKey(originkey);
			return 1;
		}

		RegCloseKey(originkey);
	}

	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE\\Wow6432Node\\Electronic Arts\\EA Games\\Ultima Online Stygian Abyss Classic",0,KEY_READ,&originkey)==ERROR_SUCCESS )
	{
		size=buffsize;

		if(RegQueryValueEx(originkey,"InstallDir",0,&type,(LPBYTE)buff,&size)==ERROR_SUCCESS)
		{
			if(buff[strlen(buff)-1]!='\\')
			strcat(buff,"\\");

			RegCloseKey(originkey);
			return 1;
		}

		RegCloseKey(originkey);
	}

	//UO Mondain's Legacy and before...
	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE\\Origin Worlds Online\\Ultima Online\\1.0",0,KEY_READ,&originkey)==ERROR_SUCCESS )
	{
		size=buffsize;

		if(RegQueryValueEx(originkey,"InstCDPath",0,&type,(LPBYTE)buff,&size)==ERROR_SUCCESS)
		{
			if(buff[strlen(buff)-1]!='\\')
			strcat(buff,"\\");

			RegCloseKey(originkey);
			return 1;
		}

		RegCloseKey(originkey);
	}

	if( RegOpenKeyEx(HKEY_LOCAL_MACHINE,"SOFTWARE\\Origin Worlds Online\\Ultima Online Third Dawn\\1.0",0,KEY_READ,&originkey)==ERROR_SUCCESS )
	{
		size=buffsize;

		if(RegQueryValueEx(originkey,"InstCDPath",0,&type,(LPBYTE)buff,&size)==ERROR_SUCCESS)
		{
			if(buff[strlen(buff)-1]!='\\')
			strcat(buff,"\\");

			RegCloseKey(originkey);
			return 1;
		}

		RegCloseKey(originkey);
	}

	return 0;
}


//UOAI Object Construction
//
//- Debug code opens a console to allow slightly more verbose debugging. <to be removed>
//- Setup default configurations (=default client installation path and window class name, disable encryption patching, enable multi client patching
//- load stored configurations <to be added>
//- start required components using dll surrogates (<- UOAI.dll gets loaded into a surrogate process, typically dllhost.exe, when specifying CLSCTX_LOCAL_SERVER as context)
//		> ClientList object :: created with normal permissions
//		> COMInjector object :: must be created with admin permissions

void UOAI_constructor(UOAI * pThis)
{
	HRESULT hr;

	//Debug Code <to be removed>
	if(AllocConsole())
		{
			freopen("CONIN$","rb",stdin);
			freopen("CONOUT$","wb",stdout);
			freopen("CONOUT$","wb",stderr);
		}

	//default configurations
	strcpy(configs.client_exe, "client.exe");
	if(!ReadClientPathFromRegistry(configs.client_path, 256))
	{
		strcpy(configs.client_path, "c:\\Program Files\\EA Games\\Ultima Online Mondain's Legacy\\");
	}
	strcpy(configs.client_window_classname, "Ultima Online");
	
	printf("here!\n");

	hr=CoCreateInstance(&CLSID_ClientList, 0, CLSCTX_LOCAL_SERVER, &IID_IClientList, (void **)&(pThis->clientlist));
	req_admin=(IsUserAdmin()==TRUE)?1:0;

	printf("%x\n", hr);

	hr=CoCreateInstanceAsAdmin(&CLSID_COMInjector, &IID_ICOMInjector, (void **)&(pThis->injector));
	if(hr==S_OK)
		(*(pThis->injector))->Initialize((COMObject *)pThis->injector, char2bstr(configs.client_window_classname), req_admin);

	printf("%x\n", hr);

	return;
}

//UOAI Object Destruction
void UOAI_destructor(UOAI * pThis)
{
	if(pThis->injector)
		((IUnknown *)(pThis->injector))->lpVtbl->Release((IUnknown *)pThis->injector);
	if(pThis->clientlist)
		((IUnknown *)(pThis->clientlist))->lpVtbl->Release((IUnknown *)pThis->clientlist);

	return;
}

HRESULT STDMETHODCALLTYPE GetClientList(UOAI * pThis, IUnknown ** clients)
{
	if(pThis->clientlist==0)
		CoCreateInstance(&CLSID_ClientList, 0, CLSCTX_LOCAL_SERVER, &IID_IClientList, (void **)&(pThis->clientlist));
		
	if(pThis->clientlist)
	{
		return ((IUnknown *)pThis->clientlist)->lpVtbl->QueryInterface((IUnknown *)pThis->clientlist, &IID_IClientList, (void **)clients);
	}
	else
	{
		(*clients)=0;
		return S_FALSE;
	}
}

COMCALL GetClientWindowName(UOAI * pThis, BSTR * ClientWindowName)
{
	(*ClientWindowName)=char2bstr(configs.client_window_classname);

	return S_OK;
}

COMCALL SetClientWindowName(UOAI * pThis, BSTR ClientWindowName)
{
	char * cstr=ole2char((LPOLESTR)ClientWindowName);
	strncpy(configs.client_window_classname, cstr, 64);
	clean(cstr);
	SysFreeString(ClientWindowName);

	(*(pThis->injector))->Initialize((COMObject *)(pThis->injector), char2bstr(configs.client_window_classname), req_admin);

	return S_OK;
}

COMCALL GetClientPath(UOAI * pThis, BSTR * ClientPath)
{
	(*ClientPath) = char2bstr(configs.client_path);
	return S_OK;
}

COMCALL SetClientPath(UOAI * pThis, BSTR ClientPath)
{
	char * cpath;

	if(cpath = ole2char((LPOLESTR)ClientPath))
	{
		configs.client_path[0] = '\0';
		strncpy(configs.client_path, cpath, 256);
		
		clean(cpath);
	}

	SysFreeString(ClientPath);

	return S_OK;
}

COMCALL GetClientExe(UOAI * pThis, BSTR * ClientExe)
{
	(*ClientExe) = char2bstr(configs.client_exe);
	return S_OK;
}

COMCALL SetClientExe(UOAI * pThis, BSTR ClientExe)
{
	char * cexe;

	if(cexe = ole2char((LPOLESTR)ClientExe))
	{
		memcpy(configs.client_exe, cexe, strlen(cexe)+1);
		clean(cexe);
	}

	SysFreeString(ClientExe);

	return S_OK;
}

COMCALL LaunchClient(UOAI * pThis, IUnknown ** client, VARIANT_BOOL bApplyMultiClientPatch, VARIANT_BOOL bApplyEncryptionPatch, VARIANT_BOOL * success)
{
	Process * cproc;
	unsigned int loopcounter;
	UOCallibrations callibs;
	unsigned int i = 0;
	ProcessStream * pstream;

	memset(&callibs, 0, sizeof(UOCallibrations));

	(*success) = VARIANT_FALSE; // assume error

	if(cproc = StartProcess(configs.client_path, configs.client_exe, 1))
	{
		if( (	  
				(bApplyMultiClientPatch!=VARIANT_FALSE)
				||(bApplyEncryptionPatch!=VARIANT_FALSE)
			)
			&&
			( CallibrateClient(cproc->dwProcessId, &callibs)!=0 )
		  )
		{
			// need stream to write patch instruction
			pstream = CreateProcessStream(cproc);

			// multi client patches
			if(bApplyMultiClientPatch!=VARIANT_FALSE)
			{
				for(i=0; i<5; i++)
				{
					SSetPos((Stream **)pstream, callibs.MultiClientPatchAddresses[i]);
					asmJmpRelative((Stream **)pstream, callibs.MultiClientPatchTargets[i]);
				}
			}

			// encryption patches
			if(bApplyEncryptionPatch!=VARIANT_FALSE)
			{
				for(i=0; i<3; i++)
				{
					SSetPos((Stream **)pstream, callibs.EncryptionPatchAddresses[i]);
					asmJmpRelative((Stream **)pstream, callibs.EncryptionPatchTargets[i]);
				}
			}

			// cleanup stream
			DeleteProcessStream(pstream);
		}
		else
			MessageBox(0, "err", "err", MB_OK);
		ResumeProcess(cproc);

		loopcounter=0;
		while((*success)==VARIANT_FALSE)
		{
			Sleep(333);
			(*pThis->clientlist)->FindClient((ClientList *)pThis->clientlist, cproc->dwProcessId, client, success);
			loopcounter++;
			if(loopcounter==10)
				break;
		}

		clean(cproc);
	}

	return S_OK;
}

void testfunc()
{
	UOCallibrations callibs;
	LinkedList * clientwindows;
	Window * curwin;
	/*PEInfo * peinfo;

	LinkedListEnum * llenum;
	LinkedListEnum * llenumb;

	ImportedLibrary * il;
	ImportedSymbol * is;

	unsigned int expectedaddr;

	memset(&callibs, 0, sizeof(UOCallibrations));*/

	//list all client windows
	clientwindows=FindWindowByClassName("Ultima Online");

	//windowlist -> tree
	while(curwin=(Window *)LL_pop(clientwindows))
	{
		CallibrateClient(curwin->pid, &callibs);
	}
}