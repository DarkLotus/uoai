#ifndef PROCESS_INCLUDED
#define PROCESS_INCLUDED

#include "Features.h"
#include <windows.h>
#include <Tlhelp32.h>
#include "LinkedList.h"

/*

  Some wrappers around windows process handling; mainly for the purpose of handling (read hacking) remote process.
  Includes:
	- Process, Thread and Window structures
	- Thread and Window enumeration.per process
	- Window search functions

  -- Wim Decelle

*/

//reuse the  windows PROCESS_INFORMATION structure as a handle (has both pid, windows handle, tid and thread handle) so should do the job
typedef PROCESS_INFORMATION Process;

//custom thread structure
typedef struct ThreadStruct
{
	DWORD tid;
	HANDLE handle;
	DWORD pid;
} Thread;

typedef struct WindowStruct
{
	HWND hWnd;
	DWORD pid;
	DWORD tid;
} Window;

Process * StartProcess(char * workingpath, char * exefile, int suspended);//execute a process optionally suspended
LinkedList * ListProcessThreads(Process * ps, DWORD threadpermissions);//list a process's threads
void DeleteThreadList(LinkedList * threadlist);//dummy
Process * GetProcess(DWORD pid, DWORD permissions);//open a process using it's pid
Thread * GetThread(DWORD tid, DWORD permissions);//open a thread using it's id
LinkedList * ListProcessWindows(Process * ps);
Window * OpenWindow(HWND hWnd);
LinkedList * FindWindowByClassName(char * classname);
LinkedList * FindWindowByTitle(char * title);
LinkedList * EnumerateProcesses(DWORD permissions);//don't use this :) opens all processes it can find!
LinkedList * FindProcess(char * key, DWORD permissions);//find process by it's exe file name and opens it with the specified permissions
DWORD GetBaseAddress(Process * ofprocess);//Never fails, returns 0x400000 if it can't access the moduleentry of this process
DWORD GetModuleBaseAddress(Process * onprocess, char * modulename);
MODULEENTRY32 * GetProcessInformation(Process * ofprocess);//get MODULEENTRY32 structure for this process (assuming it's an exe!)
MODULEENTRY32 * GetModuleInformation(Process * ofprocess, char * modulename);//get MODULEENTRY32 structure for the specified module on the specified process (name must match exactly, case-sensitive)
void ResumeProcess(Process * toresume);
void SuspendProcess(Process * tosuspend);
int GetModuleNameFromAddress(unsigned int targetaddress, char * modulename, unsigned int namesize, char * fullpath, unsigned int pathsize);//given an address within a module the name for that module is returned <- used as a way to get a handle to the current module

//ERRORS
#define PROCESS_ERROR_BASE 0x10
#define THREAD_LIST_ERROR PROCESS_ERROR_BASE+1
#define STARTPROCESS_INVALID_PATH PROCESS_ERROR_BASE+2
#define STARTPROCESS_CREATEPROCESS_FAILURE PROCESS_ERROR_BASE+3
#define GETPROCESS_NO_THREAD PROCESS_ERROR_BASE+4
#define GETPROCESS_OPENPROCESS_FAILURE PROCESS_ERROR_BASE+5
#define GETTHREAD_OPENTHREAD_FAILURE PROCESS_ERROR_BASE+6
#define FINDPROCESS_NO_PROCESSES PROCESS_ERROR_BASE+7
#define GETMODULEBASEADDRESS_ERROR PROCESS_ERROR_BASE+8
#define GETINFORMATION_ERROR PROCESS_ERROR_BASE+9

//typical permissions used when accessing another process
//	note that this requires admin rights on vista and windows 7
#define TYPICAL_PROCESS_PERMISSIONS PROCESS_CREATE_THREAD|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION|PROCESS_QUERY_INFORMATION
#define TYPICAL_THREAD_PERMISSIONS THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME|THREAD_QUERY_INFORMATION

#endif