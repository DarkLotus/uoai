#include "Process.h"

#include "allocation.h"
#include "Error.h"

//OpenThread and GetProcessId are required, so your windows version must support it (Windows 2000 or higher are required (so 95, 98 and old NT versions won't work)
//	however i was compiling using older Windows API headers, which did not include those functions yet.
//	To support compiling using these older headers those two functions can optionally be linked in dynamically.

#if WINVER < 0x500
//note that WINVER < 0x500 is actually not supported (i.e. the compiled code won't run)
//	but when compiling with old sdk headers the WINVER >= 0x500 features are not in the headers
//	so I link to them dynamically
//	So basically this allows compilation with WINVER < 0x500, but the compiled assembly still requires
//	Windows 2000 or higher versions to run.
//	Reason is that i've been coding using Visual Studio 6, which ships by default with the old headers.
//	Final deployed assembly is typically compiled using the latest windows SDK headers though.

HMODULE krnl=0;

typedef HANDLE (WINAPI * pOpenThread)(DWORD dwDesiredAccess,BOOL bInheritHandle,DWORD dwThreadId);
typedef DWORD (WINAPI * pGetProcessId)(HANDLE Process);
typedef DWORD (WINAPI * pGetProcessIdOfThread)(HANDLE Thread);
typedef DWORD (WINAPI * pGetThreadId)(HANDLE Thread);

pOpenThread OpenThread=0;
pGetProcessId GetProcessId=0;
pGetProcessIdOfThread GetProcessIdOfThread=0;
pGetThreadId GetThreadId=0;

void bad_api_headers_Initialize()
{
	if(!krnl)
	{
		krnl=GetModuleHandle("Kernel32.dll");
		if(!krnl)
			krnl=LoadLibrary("Kernel32.dll");
	}
	
	if(krnl)
	{
		if(!GetProcessId)
			GetProcessId=(pGetProcessId)GetProcAddress(krnl,"GetProcessId");
		if(!OpenThread)
			OpenThread=(pOpenThread)GetProcAddress(krnl,"OpenThread");
		if(!GetProcessIdOfThread)
			GetProcessIdOfThread=(pGetProcessIdOfThread)GetProcAddress(krnl,"GetProcessIdOfThread");
		if(!GetThreadId)
			GetThreadId=(pGetThreadId)GetProcAddress(krnl, "GetThreadId");
	}
	return;
}

#define INIT() bad_api_headers_Initialize()

#else//if windows api header version is large enough OpenThread and GetProcessId should be there already

#define INIT()

#endif

#ifndef TH32CS_SNAPMODULE32
#define TH32CS_SNAPMODULE32 0x00000010
#endif

int strcmp_case_insensitive(const char * a, const char * b)
{
	unsigned int i=0;
	
	while(a[i]&&b[i]&&(toupper(a[i])==toupper(b[i])))
		i++;
	
	if(a[i]&&b[i])
		return toupper(a[i])-toupper(b[i]);
	else if(a[i])
		return +1;
	else if(b[i])
		return -1;
	else
		return 0;
}

//execute a process
Process * StartProcess(char * workingpath, char * exefile, int suspended)
{
	STARTUPINFO si;
	Process * pi;
	char * fullmodulepath;
	char * filepart;

	fullmodulepath=(char *)alloc(MAX_PATH);

	if(!SearchPathA(workingpath,exefile,NULL,MAX_PATH,fullmodulepath,&filepart))
	{
		PushError(CreateError(STARTPROCESS_INVALID_PATH,"Failed to find the specified executable!",0,0));
		clean((void *)fullmodulepath);
		return 0;
	}

	pi=(PROCESS_INFORMATION  *)alloc(sizeof(Process));
	memset(&si, 0, sizeof(si));
	memset(pi, 0, sizeof(Process));
	si.cb = sizeof(si);
	if(CreateProcess(fullmodulepath, NULL, NULL, NULL, FALSE, suspended?CREATE_SUSPENDED|CREATE_NEW_CONSOLE:CREATE_NEW_CONSOLE, NULL, workingpath, &si, pi))
	{
		clean(fullmodulepath);
		
		return pi;
	}
	else
	{
		PushError(CreateError(STARTPROCESS_CREATEPROCESS_FAILURE,"CreateProcess failed, you might not have the required permissions!?",0,0));
		clean(pi);
		clean(fullmodulepath);
		return 0;
	}
}

Thread * _createThread(DWORD tid, HANDLE handle, DWORD pid)
{
	Thread * toreturn=alloc(sizeof(Thread));

	toreturn->tid=tid;
	toreturn->handle=handle;
	toreturn->pid=pid;

	return toreturn;
}

//list all threads of the specified process (by pid here cause we need this when opening a process)
LinkedList * ListProcessThreads(Process * ofprocess, DWORD requestedpermissions)
{
	HANDLE snapshot=0;
	THREADENTRY32 curthread;
	HANDLE threadhandle;
	LinkedList * toreturn;

	//initializations required to get OpenThread when using out-of-date api headers (as i'm doing at the time of writing)
	INIT();

	//create a the linked list to return, if no threads can be found or accessed the list is returned empty
	toreturn=LL_create();
	
	//thread entry to use during enumeration
	curthread.dwSize=sizeof(THREADENTRY32); 
	
	//create snapshot
	snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,ofprocess->dwProcessId);
	if(!snapshot)
	{
		PushError(CreateError(THREAD_LIST_ERROR,"CreateToolhelp32Snapshot failed!",0,0));
		return toreturn;
	}
	
	//get first thread
	if(!Thread32First(snapshot,&curthread))
	{
		PushError(CreateError(THREAD_LIST_ERROR,"Process snapshot is empty: a process without any threads!?",0,0));
		CloseHandle(snapshot);
		return toreturn;
	}

	//if this thread lives on the right process
	if(curthread.th32OwnerProcessID==ofprocess->dwProcessId)
	{
		threadhandle=OpenThread(requestedpermissions,0,curthread.th32ThreadID);
		if(!threadhandle)
		{
			PushError(CreateError(THREAD_LIST_ERROR,"Thread opening failed, you might not have the required permissions!?",0,0));
			CloseHandle(snapshot);
			return toreturn;
		}
		LL_push(toreturn,(void *)_createThread(curthread.th32ThreadID, threadhandle, ofprocess->dwProcessId));
	}

	while(Thread32Next(snapshot,&curthread))
	{
		if(curthread.th32OwnerProcessID==ofprocess->dwProcessId)
		{
			threadhandle=OpenThread(requestedpermissions,0,curthread.th32ThreadID);
			if(!threadhandle)
			{
				PushError(CreateError(THREAD_LIST_ERROR,"Thread opening failed, you might not have the required permissions!?",0,0));
				CloseHandle(snapshot);
				return toreturn;
			}
			LL_push(toreturn,(void *)_createThread(curthread.th32ThreadID, threadhandle, ofprocess->dwProcessId));
		}
	}
	CloseHandle(snapshot);
	return toreturn;
}

void DeleteThreadList(LinkedList * threadlist)
{
	Thread * curthread;
	while(threadlist->itemcount)
	{
		curthread=(Thread *)LL_dequeue(threadlist);
		CloseHandle(curthread->handle);
		clean(curthread);
	}
	LL_delete(threadlist);
	return;
}

//open a process using it's pid
Process * GetProcess(DWORD pid, DWORD requestedpermissions)
{
	LinkedList * threads;
	Process * toreturn;
	HANDLE pshandle;
	Thread * curthread;
	int errorcount_backup;
	Error * innererror;

	toreturn=0;

	if(pshandle=OpenProcess(requestedpermissions,0,pid))
	{
		toreturn=alloc(sizeof(Process));
		
		toreturn->dwProcessId=pid;
		toreturn->hProcess=pshandle;
		
		innererror=0;
		errorcount_backup=ErrorCount();
		if(threads=ListProcessThreads(toreturn,THREAD_QUERY_INFORMATION))
		{
			if(ErrorCount()>errorcount_backup)
				innererror=PopError();

			if(threads->itemcount>0)
			{
				curthread=(Thread *)LL_dequeue(threads);
				
				toreturn->dwThreadId=curthread->tid;
				toreturn->hThread=curthread->handle;
				
				clean(curthread);
			}
			else
			{
				CloseHandle(toreturn->hProcess);
				clean(toreturn);
				toreturn=0;

				PushError(CreateError(GETPROCESS_NO_THREAD,"Opened process doesn't seem to have a first thread!",0,innererror));
			}
			DeleteThreadList(threads);
		}
		else
		{
			CloseHandle(toreturn->hProcess);
			clean(toreturn);
			toreturn=0;

			PushError(CreateError(GETPROCESS_OPENPROCESS_FAILURE,"OpenProcess call failed, you might not have the required permissions!?",0,0));
		}
	}

	return toreturn;
}

Thread * GetThread(DWORD tid, DWORD permissions)
{
	HANDLE threadhandle;
	Thread * newthread;

	if(threadhandle=OpenThread(permissions|THREAD_QUERY_INFORMATION,0,tid))
	{
		newthread=(Thread *)alloc(sizeof(Thread));

		newthread->handle=threadhandle;
		newthread->tid;
		newthread->pid=GetProcessIdOfThread(threadhandle);
		
		return newthread;
	}

	PushError(CreateError(GETTHREAD_OPENTHREAD_FAILURE,"OpenThread call failed, you might lack the required permissions!?",0,0));

	return 0;
}

Window * _CreateWindow(HWND hWnd, DWORD pid, DWORD tid)
{
	Window * toreturn;

	toreturn=alloc(sizeof(Window));
	toreturn->hWnd=hWnd;
	toreturn->pid=pid;
	toreturn->tid=tid;

	return toreturn;
}

Window * OpenWindow(HWND hWnd)
{
	DWORD pid, tid;
	tid=GetWindowThreadProcessId(hWnd,&pid);
	return _CreateWindow(hWnd,pid,tid);
}

LinkedList * EnumerateProcesses(DWORD permissions)
{
	HANDLE snapshot;
	PROCESSENTRY32 curprocess;
	LinkedList * toreturn;

	toreturn=LL_create();

	curprocess.dwSize=sizeof(PROCESSENTRY32);

	snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPALL,0);
	if(!snapshot)
	{
		PushError(CreateError(FINDPROCESS_NO_PROCESSES,"Failed to create a toolhelp snapshot!",0,0));
		return toreturn;
	}

	if(!Process32First(snapshot,&curprocess))
	{
		PushError(CreateError(FINDPROCESS_NO_PROCESSES,"Failed to create a toolhelp snapshot!",0,0));
		CloseHandle(snapshot);
		return toreturn;
	}
	
	LL_push(toreturn,(void *)GetProcess(curprocess.th32ProcessID,permissions));
	
	while(Process32Next(snapshot,&curprocess))
		LL_push(toreturn,(void *)GetProcess(curprocess.th32ProcessID,permissions));

	return toreturn;
}

LinkedList * FindProcess(char * key, DWORD permissions)
{
	HANDLE snapshot;
	PROCESSENTRY32 curprocess;
	LinkedList * toreturn;

	toreturn=LL_create();

	curprocess.dwSize=sizeof(PROCESSENTRY32);

	snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPALL,0);
	if(!snapshot)
	{
		PushError(CreateError(FINDPROCESS_NO_PROCESSES,"Failed to create a toolhelp snapshot!",0,0));
		return toreturn;
	}

	if(!Process32First(snapshot,&curprocess))
	{
		PushError(CreateError(FINDPROCESS_NO_PROCESSES,"Failed to create a toolhelp snapshot!",0,0));
		CloseHandle(snapshot);
		return toreturn;
	}

	if(strcmp(curprocess.szExeFile,key)==0)
		LL_push(toreturn,(void *)GetProcess(curprocess.th32ProcessID,permissions));
	
	while(Process32Next(snapshot,&curprocess))
	{
		if(strcmp(curprocess.szExeFile,key)==0)
			LL_push(toreturn,(void *)GetProcess(curprocess.th32ProcessID,permissions));
	}

	return toreturn;
}

typedef struct fwstruct
{
	DWORD pid;
	DWORD threadid;
	HWND hwnd;
	struct fwstruct * next;
} foundwindow;

typedef struct
{
	char * tofind;
	LinkedList * results;
} findwindowpar;

typedef struct
{
	 DWORD pid;
	 LinkedList * results;
} findwindowparb;

BOOL CALLBACK myFindWindow_bypid(HWND hwnd,LPARAM lParam)
{
	findwindowparb * fwp;
	DWORD tid, pid;
	
	fwp=(findwindowparb *)lParam;	
	
	tid=GetWindowThreadProcessId(hwnd,&pid);

	if(pid==fwp->pid)
		LL_push(fwp->results,(void *)_CreateWindow(hwnd,pid,tid));

	return TRUE;
}

BOOL CALLBACK myFindWindow_byclassname(HWND hwnd,LPARAM lParam)
{
	char cname[1024];
	findwindowpar * fwp;
	DWORD tid, pid;
	
	fwp=(findwindowpar *)lParam;	

	if(GetClassNameA(hwnd,cname,1024))
	{
		if(strcmp(cname,fwp->tofind)==0)
		{
			tid=GetWindowThreadProcessId(hwnd,&pid);
			LL_push(fwp->results,(void *)_CreateWindow(hwnd,pid,tid));
		}
	}

	return TRUE;
}

BOOL CALLBACK myFindWindow_bytitle(HWND hwnd,LPARAM lParam)
{
	char cname[1024];
	findwindowpar * fwp;
	DWORD tid, pid;
	
	fwp=(findwindowpar *)lParam;	

	if(GetWindowTextA(hwnd,cname,1024))
	{
		if(strcmp(cname,fwp->tofind)==0)
		{
			tid=GetWindowThreadProcessId(hwnd,&pid);
			LL_push(fwp->results,(void *)_CreateWindow(hwnd,pid,tid));
		}
	}

	return TRUE;
}

LinkedList * FindWindowByClassName(char * classname)
{
	findwindowpar mywinlist;

	mywinlist.results=LL_create();
	mywinlist.tofind=classname;
	
	EnumWindows(myFindWindow_byclassname,(LPARAM)&mywinlist);

	return mywinlist.results;
}

LinkedList * FindWindowByTitle(char * title)
{
	findwindowpar mywinlist;

	mywinlist.results=LL_create();
	mywinlist.tofind=title;
	
	EnumWindows(myFindWindow_bytitle,(LPARAM)&mywinlist);

	return mywinlist.results;
}

LinkedList * ListProcessWindows(Process * ps)
{
	findwindowparb mywinlist;

	mywinlist.results=LL_create();
	mywinlist.pid=ps->dwProcessId;
	
	EnumWindows(myFindWindow_bypid,(LPARAM)&mywinlist);

	return mywinlist.results;
}

DWORD GetBaseAddress(Process * ofprocess) 
{ 
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
  MODULEENTRY32 me32; 
  
  hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, ofprocess->dwProcessId); 
  if(hModuleSnap==INVALID_HANDLE_VALUE)
	  return 0x400000;//assuming a baseaddress of 0x400000 for an exe should be ok;
 
  me32.dwSize = sizeof( MODULEENTRY32 ); 
  if(!Module32First(hModuleSnap,&me32)) 
  { 
    CloseHandle( hModuleSnap );
	return 0x400000;//assuming a baseaddress of 0x400000 for an exe should be ok
  } 
 
  do 
  { 
	  if((strcmp(me32.szExePath+strlen(me32.szExePath)-3,"exe")==0)||(strcmp(me32.szExePath+strlen(me32.szExePath)-3,"EXE")==0))
	  {
		  CloseHandle( hModuleSnap ); 
		  return (unsigned int)me32.modBaseAddr;
	  }
  } while( Module32Next( hModuleSnap, &me32 ) ); 
 
  CloseHandle( hModuleSnap ); 
  return 0x400000;//assuming a baseaddress of 0x400000 for an exe should be ok 
}

MODULEENTRY32 * GetProcessInformation(Process * ofprocess) 
{ 
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
  MODULEENTRY32 * me32; 

  hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, ofprocess->dwProcessId); 
  if(hModuleSnap==INVALID_HANDLE_VALUE)
  {
	  PushError(CreateError(GETINFORMATION_ERROR, "Couldn't list modules for the specified process!?",0,0));
	  return 0;
  }
 
  me32=alloc(sizeof(MODULEENTRY32));
  me32->dwSize = sizeof(MODULEENTRY32); 
  if(!Module32First(hModuleSnap,me32)) 
  { 
	  PushError(CreateError(GETINFORMATION_ERROR, "No modules found for the specified process!?",0,0));
	  clean(me32);
	  CloseHandle( hModuleSnap );
	  return 0;
  } 
 
  do 
  { 
	  if((strcmp(me32->szExePath+strlen(me32->szExePath)-3,"exe")==0)||(strcmp(me32->szExePath+strlen(me32->szExePath)-3,"EXE")==0))
	  {
		  CloseHandle( hModuleSnap ); 
		  return me32;
	  }
  } while( Module32Next( hModuleSnap, me32 ) ); 
 
  PushError(CreateError(GETINFORMATION_ERROR, "Process does not seem to contain an EXE module!?",0,0));
  clean(me32);
  CloseHandle( hModuleSnap ); 
  return 0; 
}

DWORD GetModuleBaseAddress(Process * onprocess, char * modulename)
{ 
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
  MODULEENTRY32 me32; 

  hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, onprocess->dwProcessId); 
  if(hModuleSnap==INVALID_HANDLE_VALUE)
  {
	  PushError(CreateError(GETMODULEBASEADDRESS_ERROR,"Failed to create a snapshot of all modules loaded on the specified process!?",0,0));
	  return 0;
  }
 
  me32.dwSize = sizeof( MODULEENTRY32 ); 
  if(!Module32First(hModuleSnap,&me32)) 
  { 
	PushError(CreateError(GETMODULEBASEADDRESS_ERROR,"Couldn't find any modules loaded on the specified process!?",0,0));
    CloseHandle( hModuleSnap );
	return 0;
  } 
 
  do 
  { 
	  //if(strcmp(me32.szModule,modulename)==0)
	  if(strcmp_case_insensitive(me32.szModule,modulename)==0)
	  {
		  CloseHandle( hModuleSnap ); 
		  return (unsigned int)me32.modBaseAddr;
	  }
  } while( Module32Next( hModuleSnap, &me32 ) ); 
 
  PushError(CreateError(GETMODULEBASEADDRESS_ERROR,"Specified module does not seem to be loaded on the specified process!?",0,0));
  CloseHandle( hModuleSnap ); 
  return 0;//assuming a baseaddress of 0x400000 for an exe should be ok 
}

MODULEENTRY32 * GetModuleInformation(Process * ofprocess, char * modulename) 
{ 
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
  MODULEENTRY32 * me32; 

  hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, ofprocess->dwProcessId); 
  if(hModuleSnap==INVALID_HANDLE_VALUE)
  {
	  PushError(CreateError(GETINFORMATION_ERROR, "Couldn't list modules for the specified process!?",0,0));
	  return 0;
  }
 
  me32=alloc(sizeof(MODULEENTRY32));
  me32->dwSize = sizeof(MODULEENTRY32); 
  if(!Module32First(hModuleSnap,me32)) 
  { 
	  PushError(CreateError(GETINFORMATION_ERROR, "No modules found for the specified process!?",0,0));
	  clean(me32);
	  CloseHandle( hModuleSnap );
	  return 0;
  } 
 
  do 
  { 
	  //if(strcmp(me32->szModule,modulename)==0)
	  if(strcmp_case_insensitive(me32->szModule,modulename)==0)
	  {
		  CloseHandle( hModuleSnap ); 
		  return me32;
	  }
  } while( Module32Next( hModuleSnap, me32 ) ); 
 
  PushError(CreateError(GETINFORMATION_ERROR, "Specified Module doesn't seem to be loaded onto this process!?",0,0));
  clean(me32);
  CloseHandle( hModuleSnap ); 
  return 0; 
}

void SuspendProcess(Process * tosuspend)
{
	LinkedList * threads;
	Thread * curthread;

	if(threads=ListProcessThreads(tosuspend, TYPICAL_THREAD_PERMISSIONS))
	{
		while(curthread=(Thread  *)LL_pop(threads))
		{
			SuspendThread(curthread->handle);
			CloseHandle(curthread->handle);
			clean(curthread);
		}
		DeleteThreadList(threads);
	}

	return;
}

void ResumeProcess(Process * tosuspend)
{
	LinkedList * threads;
	Thread * curthread;

	if(threads=ListProcessThreads(tosuspend, TYPICAL_THREAD_PERMISSIONS))
	{
		while(curthread=(Thread  *)LL_pop(threads))
		{
			ResumeThread(curthread->handle);
			CloseHandle(curthread->handle);
			clean(curthread);
		}
		DeleteThreadList(threads);
	}

	return;
}

int uintcompare(unsigned int a, unsigned int b)
{
	if(a>b)
		return +1;
	else if(a<b)
		return -1;
	else
		return 0;
}

int GetModuleNameFromAddress(unsigned int targetaddress, char * modulename, unsigned int namesize, char * fullpath, unsigned int pathsize)
{ 
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE; 
  MODULEENTRY32 curmodule, closestmodule;
  unsigned int prevdist, curdist;

  hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32, GetCurrentProcessId()); 
  if(hModuleSnap==INVALID_HANDLE_VALUE)
	  return 0;
 
  closestmodule.dwSize=sizeof(MODULEENTRY32);
  if(!Module32First(hModuleSnap,&closestmodule)) 
  { 
	  CloseHandle( hModuleSnap );
	  return 0;
  } 

  prevdist=targetaddress-((unsigned int)closestmodule.modBaseAddr);

  curmodule.dwSize=sizeof(MODULEENTRY32);
  while( Module32Next( hModuleSnap, &curmodule ) )
  {
	  curdist=targetaddress-((unsigned int)curmodule.modBaseAddr);
	  if((curdist>=0)&&((prevdist<0)||(curdist<prevdist)))
	  {
		  //swap
		  memcpy(&closestmodule,&curmodule,sizeof(MODULEENTRY32));
		  prevdist=curdist;
	  }
  }

  //set return values
  if(prevdist>0)
  {
	  if(modulename)
		  strncpy(modulename,closestmodule.szModule,namesize);
	  
	  if(fullpath)
		  strncpy(fullpath, closestmodule.szExePath, pathsize);
  }

  //cleanup
  CloseHandle(hModuleSnap);

  return (prevdist>=0);
}