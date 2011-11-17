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

unsigned int sbo(unsigned int toswap)
{
	unsigned char * bytes = (unsigned char *)toswap;
	unsigned char temp;
	temp = bytes[0];
	bytes[0] = bytes[3];
	bytes[3] = temp;
	temp = bytes[1];
	bytes[1] = bytes[2];
	bytes[2] = temp;

	return *((unsigned int *)bytes);
}

int HandlePacketHook(RegisterStates * regs, void * stacktop)
{
	char * pPacket = *((char **)stacktop);
	unsigned int cmd;

	if(pPacket!=NULL)
	{
		cmd = (unsigned int)(pPacket[0])&0xFF;
		printf("received packet %x\n", cmd);

		/* test */
		if(cmd == 0xAE)
		{
			unsigned int * pID;
			unsigned int ID, i;
			char name[30];
			unsigned short * speech;
			unsigned short uspeech[256];
			char * cSpeech;
			pID = (unsigned int *)&pPacket[3];
			ID = htonl(*pID);
			memset(name, 0, 30);
			strncpy(name, &pPacket[18], 30);
			speech = (unsigned short *)&pPacket[48];
			i=0;
			memset(uspeech, 0, sizeof(unsigned short)*256);
			while((speech[i]!=0)&&(i<255))
			{
				uspeech[i] = ntohs(speech[i]);
				i++;
			}
			cSpeech = ole2char((LPOLESTR)uspeech);
			printf("%s (%u) : %s\n", name, ID, cSpeech);
			clean(cSpeech);
		}
	}

	return 1;
}

int SendPacketHook(RegisterStates * regs, void * stacktop)
{
	char * pPacket = *((char **)stacktop);
	unsigned int cmd;

	if(pPacket!=NULL)
	{
		cmd = (unsigned int)(pPacket[0])&0xFF;
		printf("sent packet %x\n", cmd);
		
		/* tests */
		//if(cmd == 0xAD)
		//	return 0;
		if(cmd == 0x02)
			callibrations->ShowMessage(3, 0, "walk request");
	}

	return 1;
}

void Client_constructor(Client * pThis)
{
	_original_connect = (pConnect)SetImportHook("WSOCK32.dll", "connect", (void *)connect_hook);

	if(SetRandomHook((unsigned int)(callibrations->SendPacket), 4, SendPacketHook)==0)
		printf("failed to set sendpacket hook!\n");

	if(SetVtblHook((void *)&(callibrations->NetworkObjectVtbl->HandlePacket), HandlePacketHook, 4))
		printf("failed to set handlepacket hook!\n");

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

void closegump(void * pGump)
{
	if(pGump == NULL)
		return;

	__asm
	{
		mov ecx, pGump
		mov eax, [ecx]
		mov edx, [eax]
		push 1
		call edx
	}

	return;
}

void __stdcall onyes(void * pThis)
{
	callibrations->ShowMessage(5555, 0, "Yes!");

	closegump(pThis);

	return;
}

void __stdcall onno(void * pThis)
{
	callibrations->ShowMessage(5555, 0, "No!");

	closegump(pThis);

	return;
}

typedef void (__stdcall * pYesNoGumpConstructor_wrapper)(void * pThis, char * text, pYesNoGumpCallback onYes, pYesNoGumpCallback onNo, unsigned int unknown); 
typedef void (__stdcall * pShowGump_wrapper)(void * pThis, void * parent, int bUnknown);//thiscall gump->Show(parent_gump)

HRESULT STDMETHODCALLTYPE ShowYesNoGump(Client * pThis, BSTR question)
{
	static pYesNoGumpCallback _onYesCallback = 0;
	static pYesNoGumpCallback _onNoCallback = 0;
	static pYesNoGumpConstructor_wrapper YNGump_constructor = NULL;
	static pShowGump_wrapper ShowGump = NULL;

	char * cquestion;
	void * pgump;

	if(_onYesCallback == 0)
		_onYesCallback = (pYesNoGumpCallback)create_thiscall_callback((unsigned int)onyes);
	if(_onNoCallback == 0)
		_onNoCallback = (pYesNoGumpCallback)create_thiscall_callback((unsigned int)onno);
	if(YNGump_constructor == NULL)
		YNGump_constructor = (pYesNoGumpConstructor_wrapper)create_thiscall_wrapper((unsigned int)callibrations->YesNoGumpConstructor);
	if(ShowGump == NULL)
		ShowGump = (pShowGump_wrapper)create_thiscall_wrapper((unsigned int)callibrations->ShowGump);

	/*
	ync = (unsigned int)callibrations->YesNoGumpConstructor;
	sg = (unsigned int)callibrations->ShowGump;
	*/
	
	if(cquestion = ole2char(question))
	{
		//printf("allocating...\n");
		pgump = callibrations->Allocator(callibrations->YesNoGumpSize);

		if(pgump!=0)
		{
			//printf("constructing...\n");
			/*
			_asm
			{
				push 0
				push onNoCallback
				push onYesCallback
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
			*/

			YNGump_constructor(pgump, cquestion, _onYesCallback, _onNoCallback, 0);
			ShowGump(pgump, NULL, 0);
		}
		/*
		else
			printf("gump allocation failed!\n");
			*/

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

HRESULT STDMETHODCALLTYPE GetPlayer(Client * pThis, Item ** pPlayer)
{
	(*pPlayer) = GetItemByOffset(*((unsigned int *)(callibrations->Player)));
	return S_OK;
}

HRESULT STDMETHODCALLTYPE GetItems(Client * pThis, void ** pItems)
{
	(*pItems) = (void *)create_itemlist(0);
	return S_OK;
}