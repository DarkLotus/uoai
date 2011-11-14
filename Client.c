#include "Client.h"
#include "allocation.h"
#include <windows.h>
#include <stdio.h>
#include <WinSock.h>
#include "PE.h"
#include "Process.h"
#include <string.h>
#include "Tools.h"
#include "Hooks.h"

UOCallibrations * callibrations=0;

typedef int (__stdcall *pConnect)(SOCKET s, const struct sockaddr * name, int namelen);
typedef void (__stdcall *pConnectionLoss)();

pConnect _original_connect = 0;
pConnectionLoss _original_connection_loss_handler = 0;

void __stdcall onConnectionLoss()
{
	void * thispointer;
	_asm
	{
		mov thispointer, ecx
	}

	callibrations->ShowMessage(5555, 0, "connection loss");

	if(_original_connection_loss_handler)
	{
		_asm
		{
			mov ecx, thispointer
		}
		_original_connection_loss_handler();
	}
}

int __stdcall connect_hook(SOCKET s, const struct sockaddr *name, int namelen)
{
	char msg[256] = {'\0'};

	if(_original_connect!=0)
	{
		if(callibrations)
		{
			sprintf(msg, "Connecting to ");
			sprintip(&msg[strlen(msg)], ((struct sockaddr_in *)name)->sin_addr.S_un.S_addr);
			callibrations->ShowMessage(5555, 0, msg);
		}
		return _original_connect(s, name, namelen);
	}
	else
		return connect(s, name, namelen);
}

int SendPacketHook(RegisterStates * regs, void * stacktop)
{
	char * pPacket = *((char **)stacktop);
	unsigned int cmd;
	//printf("in packet hook!\n");
	//callibrations->ShowMessage(3, 0, "in packethook!\n");

	/* debug -- check if hook is set correctly on some well-known packets */
	if(pPacket!=NULL)
	{
		cmd = (unsigned int)(pPacket[0])&0xFF;
		printf("pPacket[0] == %x\n", cmd);
		if(cmd == 0xAD)
			return 0;
		if(cmd == 0x02)
			callibrations->ShowMessage(3, 0, "walk request");
	}

	return 1;
}

void Client_constructor(Client * pThis)
{
	DWORD oldprotect;

	_original_connect = (pConnect)SetImportHook("WSOCK32.dll", "connect", (void *)connect_hook);

	if(SetRandomHook((unsigned int)(callibrations->SendPacket), 4, SendPacketHook)==0)
	{
		printf("failed to set hook!\n");
	}
	else
		printf("hook set (%x)!\n", (unsigned int)(callibrations->SendPacket));

	/*
	_original_connection_loss_handler = callibrations->NetworkObjectVtbl->onConnectionLoss;

	VirtualProtect((void *)callibrations->NetworkObjectVtbl, sizeof(NetworkObjectClassVtbl), PAGE_READWRITE, &oldprotect);
	callibrations->NetworkObjectVtbl->onConnectionLoss = onConnectionLoss;
	VirtualProtect((void *)callibrations->NetworkObjectVtbl, sizeof(NetworkObjectClassVtbl), oldprotect, &oldprotect);
	*/

	if(callibrations)
	{
		callibrations->ShowMessage(5555, 0, "-- UOAI enabled client --");
	}
}

void Client_destructor(Client * pThis)
{
	printf("client destruction\n");
}

HRESULT STDMETHODCALLTYPE GetPID(Client * pThis, unsigned int * pPID)
{
	(*pPID)=GetCurrentProcessId();
	return S_OK;
}

HRESULT STDMETHODCALLTYPE ClientWrite(Client * pThis, unsigned int color, unsigned int font, BSTR message)
{
	char * cmessage;

	cmessage=ole2char(message);

	callibrations->ShowMessage(color, font, cmessage);

	clean(cmessage);

	SysFreeString(message);

	return S_OK;
}

void * curyngump = NULL;

void closecurgump()
{
	if(curyngump == NULL)
		return;

	__asm
	{
		mov ecx, curyngump
		mov eax, [ecx]
		mov edx, [eax]
		push 1
		call edx
	}

	curyngump = NULL;

	return;
}

void __stdcall onyes()
{
	callibrations->ShowMessage(5555, 0, "Yes!");

	closecurgump();

	return;
}

void __stdcall onno()
{
	callibrations->ShowMessage(5555, 0, "No!");

	closecurgump();

	return;
}

HRESULT STDMETHODCALLTYPE ShowYesNoGump(Client * pThis, BSTR question)
{
	char * cquestion;
	void * pgump;

	Gump * p_gump;

	unsigned int ync;
	unsigned int sg;

	ync = (unsigned int)callibrations->YesNoGumpConstructor;
	sg = (unsigned int)callibrations->ShowGump;
	
	if(cquestion = ole2char(question))
	{
		printf("allocating...\n");
		pgump = callibrations->Allocator(callibrations->YesNoGumpSize);

		if(pgump!=0)
		{
			closecurgump();
			curyngump = pgump;

			printf("constructing...\n");
			_asm
			{
				push 0
				push onno
				push onyes
				push cquestion
				mov ecx, pgump
				call ync
			}

			printf("showing...\n");
			_asm
			{
				push 0
				push 0
				mov ecx, pgump
				call sg
			}
		}
		else
			printf("gump allocation failed!\n");

		clean(cquestion);
	}

	SysFreeString(question);
	
	return S_OK;
}

HRESULT STDMETHODCALLTYPE Bark(Client * pThis, BSTR speech)
{
	char * cspeech;
	unsigned int pPlayer;
	unsigned int pBark;

	pPlayer = *((unsigned int *)callibrations->Player);
	pBark = (unsigned int)callibrations->Bark;

	if(cspeech = ole2char(speech))
	{
		_asm
		{
			push 0
			push 0
			push 0
			push 5555
			push cspeech
			mov ecx, pPlayer
			call pBark
		}

		clean(cspeech);
	}

	SysFreeString(speech);
	
	return S_OK;
}

void SetCallibrations(UOCallibrations * callibs)
{
	if(callibrations)
		clean(callibrations);
	callibrations=callibs;
	return;
}

UOCallibrations * GetCallibrations()
{
	return callibrations;
}