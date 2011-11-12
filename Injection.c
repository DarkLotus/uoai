#include "Injection.h"
#include "Streams.h"
#include "PE.h"
#include <string.h>
#include <windows.h>
#include "assembler.h"
#include "allocation.h"
#include "Error.h"

/*
	Process Injection Code : there are several methods to implement dll or code injection, one of them is implemented here.

	-- Wim Decelle	
*/

int Inject(Process * onprocess, Thread * onthread, char * dlltoload, char * startroutine, void * parameter_buffer, unsigned int buffersize)
{
	ProcessStream * _psstream;
	BufferedStream * _bstream;
	Stream ** curstream;
	void * remotebuffer;
	CONTEXT ctx;
	PEInfo * pei;
	ImportedLibrary * curlib;
	ImportedSymbol * cursymb;
	LinkedListEnum * curenum, * curenumb;

#define REQUIRED_CODE_SIZE 63

	unsigned int data_size;

	//32 bit target process is assumed!!!
	unsigned int dll_name_address;
	unsigned int dll_startroutine_address;
	unsigned int parameter_buffer_address;
	unsigned int code_address;
	unsigned int loadlibrary_address;
	unsigned int getprocaddress_address;
	unsigned int esp_backup_address;
	unsigned int handle_address;
	unsigned int pStartRoutine_address;

	int errbackup;
	int toreturn;

	HMODULE krnl32;

	//data size calculation
	data_size=4*4;//4 variables
	data_size+=strlen(dlltoload)+1;
	data_size+=(data_size%4)==0?0:4-(data_size%4);//4-byte data alignment
	data_size+=strlen(startroutine)+1;
	data_size+=(data_size%4)==0?0:4-(data_size%4);//4-byte data alignment
	data_size+=buffersize;
	data_size+=(data_size%16)==0?0:4-(data_size%16);//code following data is 16-byte aligned

	//allocate remote buffers
	remotebuffer=VirtualAllocEx(onprocess->hProcess,0,data_size+REQUIRED_CODE_SIZE, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if(remotebuffer==0)
	{
		PushError(CreateError(INJECTION_FAILURE, "Could not Inject code onto the specified process as VirtualAllocEx returned 0, do you lack the required permissions!?",0,0));
		return 0;
	}

	//setup streams
	_psstream=CreateProcessStream(onprocess);
	_bstream=CreateBufferedStream((Stream **)_psstream, 4096);
	curstream=(Stream **)_bstream;

	//suspend process
	errbackup=ErrorCount();

	SuspendProcess(onprocess);

	if(ErrorCount()>errbackup)
	{
		PushError(CreateError(INJECTION_FAILURE, "Could not Inject code as the specified process could not be suspended, do you lack the required permissions!?",0, PopError()));
		
		DeleteBufferedStream(_bstream);
		DeleteProcessStream(_psstream);

		return 0;
	}

	//get thread context
	ctx.ContextFlags=CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS | CONTEXT_EXTENDED_REGISTERS;
	if(!GetThreadContext(onthread->handle, &ctx))
	{
		PushError(CreateError(INJECTION_FAILURE, "Could not Inject code as GetThreadContext failed on the specified thread, do you lack the required permissions!?",0, 0));
		
		DeleteBufferedStream(_bstream);
		DeleteProcessStream(_psstream);

		ResumeProcess(onprocess);

		return 0;
	}

	//find LoadLibrary and GetProcAddress imports in the target Process's PE Headers
	loadlibrary_address=0;
	getprocaddress_address=0;

	errbackup=ErrorCount();

	pei=GetPEInfo(onprocess);
	curenum=LL_newenum(pei->ImportedLibraries);
	while(curlib=(ImportedLibrary *)LL_next(curenum))
	{
		if((strcmp(curlib->libname,"KERNEL32.dll")==0)||(strcmp(curlib->libname,"kernel32.dll")==0)||(strcmp(curlib->libname,"KERNEL32.DLL")==0)||(strcmp(curlib->libname,"kernel32.DLL")==0))
		{
			curenumb=LL_newenum(curlib->ImportedSymbols);
			while(cursymb=(ImportedSymbol *)LL_next(curenumb))
			{
				if((strcmp(cursymb->name,"LoadLibraryA")==0)||(strcmp(cursymb->name,"LOADLIBRARYA")==0))
					loadlibrary_address=cursymb->Address;
				else if((strcmp(cursymb->name,"GetProcAddress")==0)||(strcmp(cursymb->name,"GETPROCADDRESS")==0))
					getprocaddress_address=cursymb->Address;
			}
			LL_enumdelete(curenumb);
		}
	}
	LL_enumdelete(curenum);
	DeletePEInfo(pei);

	//silently ignore errors in PE header parsing
	while(ErrorCount()>errbackup)
		DeleteError(PopError());

	//should we have failed reading the PE headers...
	//	...then it's probably a safe bet to assume KERNEL32.dll get's loaded at the same loadaddress for every process
	//	so the LoadLibraryA and GetProcAddress functions should be at the same addresses as in the current process
	if((loadlibrary_address==0)||(getprocaddress_address==0))
	{
		krnl32=LoadLibrary("KERNEL32.dll");
		if(!loadlibrary_address)
			loadlibrary_address=(unsigned int)GetProcAddress(krnl32, "LoadLibraryA");
		if(!getprocaddress_address)
			getprocaddress_address=(unsigned int)GetProcAddress;
	}

	//all errors during writing are collected at the end
	errbackup=ErrorCount();

	//start building the remote data and code
	SSetPos(curstream, (unsigned int)((unsigned int)remotebuffer));//4 variables at the start

	//allocate space for variables and initialize them to 0
	esp_backup_address=(unsigned int)SGetPos(curstream);
	SWriteUInt(curstream, 0);

	handle_address=(unsigned int)SGetPos(curstream);
	SWriteUInt(curstream, 0);

	pStartRoutine_address=(unsigned int)SGetPos(curstream);
	SWriteUInt(curstream, 0);

	//unsed var:
	SWriteUInt(curstream, 0);

	//- write data

	//dll name (for LoadLibrary call)
	dll_name_address=(unsigned int)SGetPos(curstream);
	SWriteString(curstream, dlltoload);

	//dll startroutine name (for GetProcAddress call)
	dll_startroutine_address=(unsigned int)SGetPos(curstream);
	dll_startroutine_address+=dll_startroutine_address%4==0?0:4-(dll_startroutine_address%4);//align to 4 byte boundary
	SSetPos(curstream, (unsigned int)dll_startroutine_address);
	SWriteString(curstream, startroutine);

	//parameter buffer (for StartRoutine(pParBuff) call)
	parameter_buffer_address=(unsigned int)SGetPos(curstream);
	parameter_buffer_address+=parameter_buffer_address%4==0?0:4-(parameter_buffer_address%4);//align to 4 byte boundary
	SSetPos(curstream, (unsigned int)parameter_buffer_address);
	SWriteBytes(curstream, parameter_buffer, (unsigned int)buffersize);

	//- write code (8+31+11+8+5 = 63 bytes)
	code_address=(unsigned int)SGetPos(curstream);
	code_address+=code_address%16==0?0:16-(code_address%16);
	SSetPos(curstream, (unsigned int)code_address);

	//safety measures (6+2 = 8bytes)
	asmPushAll(curstream);//2
	asmBackupEsp(curstream, esp_backup_address);//6

	//loadlibrary, getprocaddress call to get the startup routine's address (6*5+1 = 31 bytes)
	asmPushImmediate(curstream, dll_name_address);//5
	asmCallRelative(curstream, loadlibrary_address);//5
	asmMovMemoryEax(curstream, handle_address);//5
	asmPushImmediate(curstream, dll_startroutine_address);//5
	asmPushEax(curstream);//1
	asmCallRelative(curstream, getprocaddress_address);//5
	asmMovMemoryEax(curstream, pStartRoutine_address);//5

	//build startroutine call (5+6=11 bytes)
	asmPushImmediate(curstream, parameter_buffer_address);//5
	asmCallFunctionPointer(curstream, pStartRoutine_address);//6

	//clean up stack and restore context (6+2=8 bytes)
	asmRestoreEsp(curstream, esp_backup_address);//6
	asmPopAll(curstream);//2

	//jmp back to original eip (5 bytes)
	asmJmpRelative(curstream, ctx.Eip);//5

	//flush
	BSFlush(_bstream);

	//if everything was written correctly, then we can continue
	if(ErrorCount()==errbackup)
	{
		//set thread context (eip=our code address)
		ctx.Eip=code_address;
		if(SetThreadContext(onthread->handle, &ctx))
			toreturn=1;
		else
		{
			toreturn=0;
			PushError(CreateError(INJECTION_FAILURE, "Failed to change the target thread's context, do you lack the required permissions!?", 0, 0));
		}
	}
	else
	{
		toreturn=0;
		PushError(CreateError(INJECTION_FAILURE, "Failed to inject code onto the target thread in the target process, do you lack the required permissions!?", 0, PopError()));
	}

	//resume process
	ResumeProcess(onprocess);

	//cleanup
	DeleteBufferedStream(_bstream);
	DeleteProcessStream(_psstream);

	return toreturn;
}