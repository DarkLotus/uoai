#include "Hooks.h"

#include "Process.h"
#include "PE.h"
#include "Tools.h"
#include "Streams.h"
#include "Allocation.h"
#include "Assembler.h"
#include <Windows.h>
#include "BinaryTree.h"

typedef struct
{
	unsigned int hookmem;
	BinaryTree * funcs;
} HookDesc;

BinaryTree * hook_tree = NULL;

// Hooks up an imported library function and returns the original address
void * SetImportHook(char * libraryname, char * symbolname, void * newaddress)
{
	static PEInfo * peinfo=0;

	Process * curpr;
	LinkedListEnum * llenum, * llenumb;
	ImportedLibrary * curlib;
	ImportedSymbol * cursymb;
	ProcessStream * pstream;

	char * curlibname;
	char * clibraryname;
	void * toreturn = 0;

	clibraryname = duplicatestring(libraryname);
	strupper(clibraryname);

	if(curpr = GetProcess(GetCurrentProcessId(), TYPICAL_PROCESS_PERMISSIONS))
	{
		// parse PE Headers once
		if(peinfo == 0)
			peinfo = GetPEInfo(curpr);

		if(peinfo)
		{
			llenum = LL_newenum(peinfo->ImportedLibraries);
			while(curlib = (ImportedLibrary *)LL_next(llenum))
			{
				curlibname = duplicatestring(curlib->libname);
				strupper(curlibname);
				if(strcmp(curlibname, clibraryname)==0)
				{
					// got library, find symbol
					llenumb = LL_newenum(curlib->ImportedSymbols);
					while(cursymb = (ImportedSymbol *)LL_next(llenumb))
					{
						if(strcmp(cursymb->name, symbolname)==0)
						{
							if(pstream = CreateProcessStream(curpr))
							{
								toreturn = (void *)(cursymb->Address);//32 bit only
								SSetPos((Stream **)pstream, cursymb->IATAddress);
								SWriteUInt((Stream **)pstream, (unsigned int)newaddress);//32 bit only
								DeleteProcessStream(pstream);
							}
						}
					}
					LL_enumdelete(llenumb);
				}
				clean(curlibname);
			}
			LL_enumdelete(llenum);
		}
		clean(curpr);
	}
	clean(clibraryname);

	return toreturn;
}

/* 32, 64 or 128 byte blocks are used to write safe hook code at runtime -- blocks are never freed */
void * AllocateHookBlock(unsigned int sizeidx)
{
	/* 32, 64, 128 bytes correspond to sizeidx 0, 1, 2 */
	static unsigned int hookpage[3] = { 0, 0, 0 };
	static unsigned int next_hook[3] = { 0, 0, 0 };
	static unsigned int blocks_per_page[3] = { 4096/32, 4096/64, 4096/128 };
	static unsigned int block_sizes[3] = { 32, 64, 128 };

	void * toreturn = NULL;

	if( (hookpage[sizeidx] == 0) || (next_hook[sizeidx] == blocks_per_page[sizeidx]) )
	{
		hookpage[sizeidx] = (unsigned int)VirtualAlloc(NULL, 4096, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		next_hook[sizeidx] = 0;
	}

	if(hookpage[sizeidx] != 0)
	{
		toreturn = (void *)(hookpage[sizeidx] + next_hook[sizeidx]*block_sizes[sizeidx]);
		next_hook[sizeidx]++;
	}

	return toreturn;
}

int __stdcall internal_hook(unsigned int hookkey)
{
	unsigned int espbackup;
	int retval ;
	
	void * stacktop;
	RegisterStates * regstates;

	HookDesc * curhook;
	BinaryTreeEnum * btenum;
	pHookFunc curhookfunc;

	retval = 1;

	if(curhook = (HookDesc *)BT_find(hook_tree, (void *)hookkey))
	{
		espbackup = *((unsigned int *)(curhook->hookmem + 64 - 4));
		regstates = (RegisterStates *)(espbackup - sizeof(RegisterStates));
		stacktop = (void *)(espbackup - sizeof(RegisterStates) - 4);
	
		btenum = BT_newenum(curhook->funcs);
		while(curhookfunc = (pHookFunc)BT_next(btenum))
			retval &= curhookfunc(regstates, stacktop);
	}

	return retval;
}

int def_compare(unsigned int a, unsigned int b)
{
	if(a>b)
		return 1;
	else if(a<b)
		return -1;
	else
		return 0;
}

/* SetVtblHook - hooks up a vtbl entry ( = class member method ) */
int SetVtblHook(void * vtblentry, pHookFunc hookfunc, unsigned int stacksize)
{
	unsigned int hookmem;
	Process * curps;
	ProcessStream * pstream;
	Stream ** curstream;
	HookDesc * curdesc;

	int toreturn = 0;

	unsigned int originalVtblEntry, espbackuplocation, skipcalllabel;

	if( (hook_tree == NULL)
		&& ((hook_tree = BT_create((BTCompare)def_compare)) == NULL) )
		return 0;

	/*
		a. double check if we already have this vtbl entry, if so just insert the hookfunc if not already present
	*/
	if(curdesc = (HookDesc *)BT_find(hook_tree, vtblentry))
	{
		if(!BT_find(curdesc->funcs, (void *)hookfunc))
			BT_insert(curdesc->funcs, (void *)hookfunc, (void *)hookfunc);

		return 1;
	}

	/*
		b.  new hook - write hook code
	*/

	if( curps = GetProcess(GetCurrentProcessId(), TYPICAL_PROCESS_PERMISSIONS) )
	{
		if(pstream = CreateProcessStream(curps))
		{
			if(hookmem = (unsigned int)AllocateHookBlock(1))
			{
				curstream = (Stream **)pstream;

				espbackuplocation = hookmem + 64 - 4; /* put espbackup at the end of this 64 byte block */
				skipcalllabel = hookmem + 35; 

				SSetPos(curstream, (unsigned int)vtblentry);
				originalVtblEntry = SReadUInt(curstream);

				SSetPos(curstream, hookmem);

				/* backup cpu registers and stack pointer */
/* 02 bytes */	asmPushAll(curstream);												//pusha							<- backup cpu registers
/* 06 bytes */	asmBackupEsp(curstream, espbackuplocation);							//mov [esp_backup], esp			<- store stack pointer at fixed address so we can safely restore the stack pointer and build a 'stack-stream'
				
				/* perform early hook call */
/* 05 bytes */	asmPushImmediate(curstream, (unsigned int)vtblentry);				//push vtblentry				<- vtblentry is used to find the hook descriptor and all callbacks
/* 05 bytes */	asmCallRelative(curstream, (unsigned int)internal_hook);			//call internal_vtbl_hook		<- handler that looks up the hookmem_address and performs the hook call
/* 02 bytes */	asmTestEaxEax(curstream);											//test eax, eax					<- if hook returned 0, skip the actual call
/* 02 bytes */	asmJzRelativeShort(curstream, skipcalllabel);						//jz skipcalllabel:				<- go to the skip location

				/* perform original function call */
/* 06 bytes */	asmRestoreEsp(curstream, espbackuplocation);						//mov esp, [esp_backup]			<- ensure correct stack state
/* 02 bytes */	asmPopAll(curstream);												//popa							<- restore cpu registers
/* 05 bytes */	asmJmpRelative(curstream, originalVtblEntry);						//jmp original_vtbl_entry		<- jmp to the original method
				
/* 35 bytes -- skipcall label */

				/* return without calling */
/* 06 bytes */	asmRestoreEsp(curstream, espbackuplocation);						//mov esp, [esp_backup]			<- ensure correct stack state
/* 02 bytes */	asmPopAll(curstream);												//popa							<- restore cpu registers
				if(stacksize == 0)
/* 01 bytes */		asmRtn(curstream);												//rtn
				else
/* 03 bytes */		asmRtnStackSize(curstream, stacksize);							//rtn stacksize
				
/* max 50 bytes */

				/* write vtbl entry */
				SSetPos(curstream, (unsigned int)vtblentry);
				SWriteUInt(curstream, hookmem);

				/* insert hook into hook table */
				curdesc = create(HookDesc);
				curdesc->hookmem = hookmem;
				curdesc->funcs = BT_create((BTCompare)def_compare);
				BT_insert(curdesc->funcs, (void *)hookfunc, (void *)hookfunc);
				BT_insert(hook_tree, (void *)vtblentry, (void *)curdesc);

				/* done */
				toreturn = 1;
			}

			DeleteProcessStream(pstream);
		}

		clean(curps);
	}

	return toreturn;
}

/* SetCallHook - replaces a call xxxxx (relative call) instruction with a hook 
	-- identical to vtbl-hook except for the use of a call instruction rather than a vtbl-entry
*/
int SetCallHook(unsigned int calladdr, pHookFunc hookfunc, unsigned int originaltarget, unsigned int stacksize)
{
	unsigned int hookmem;
	Process * curps;
	ProcessStream * pstream;
	Stream ** curstream;
	HookDesc * curdesc;

	int toreturn = 0;

	unsigned int espbackuplocation, skipcalllabel;

	if( (hook_tree == NULL)
		&& ((hook_tree = BT_create((BTCompare)def_compare)) == NULL) )
		return 0;

	/*
		a. double check if we already have this vtbl entry, if so just insert the hookfunc if not already present
	*/
	if(curdesc = (HookDesc *)BT_find(hook_tree, (void *)calladdr))
	{
		if(!BT_find(curdesc->funcs, (void *)hookfunc))
			BT_insert(curdesc->funcs, (void *)hookfunc, (void *)hookfunc);

		return 1;
	}

	/*
		b.  new hook - write hook code
	*/

	if( curps = GetProcess(GetCurrentProcessId(), TYPICAL_PROCESS_PERMISSIONS) )
	{
		if(pstream = CreateProcessStream(curps))
		{
			if(hookmem = (unsigned int)AllocateHookBlock(1))
			{
				curstream = (Stream **)pstream;

				espbackuplocation = hookmem + 64 - 4; /* put espbackup at the end of this 64 byte block */
				skipcalllabel = hookmem + 35; 

				SSetPos(curstream, hookmem);

				/* backup cpu registers and stack pointer */
/* 02 bytes */	asmPushAll(curstream);												//pusha							<- backup cpu registers
/* 06 bytes */	asmBackupEsp(curstream, espbackuplocation);							//mov [esp_backup], esp			<- store stack pointer at fixed address so we can safely restore the stack pointer and build a 'stack-stream'
				
				/* perform early hook call */
/* 05 bytes */	asmPushImmediate(curstream, (unsigned int)calladdr);				//push vtblentry				<- vtblentry is used to find the hook descriptor and all callbacks
/* 05 bytes */	asmCallRelative(curstream, (unsigned int)internal_hook);			//call internal_vtbl_hook		<- handler that looks up the hookmem_address and performs the hook call
/* 02 bytes */	asmTestEaxEax(curstream);											//test eax, eax					<- if hook returned 0, skip the actual call
/* 02 bytes */	asmJzRelativeShort(curstream, skipcalllabel);						//jz skipcalllabel:				<- go to the skip location

				/* perform original function call */
/* 06 bytes */	asmRestoreEsp(curstream, espbackuplocation);						//mov esp, [esp_backup]			<- ensure correct stack state
/* 02 bytes */	asmPopAll(curstream);												//popa							<- restore cpu registers
/* 05 bytes */	asmJmpRelative(curstream, originaltarget);						//jmp original_vtbl_entry		<- jmp to the original method
				
/* 35 bytes -- skipcall label */

				/* return without calling */
/* 06 bytes */	asmRestoreEsp(curstream, espbackuplocation);						//mov esp, [esp_backup]			<- ensure correct stack state
/* 02 bytes */	asmPopAll(curstream);												//popa							<- restore cpu registers
				if(stacksize == 0)
/* 01 bytes */		asmRtn(curstream);												//rtn
				else
/* 03 bytes */		asmRtnStackSize(curstream, stacksize);							//rtn stacksize
				
/* max 50 bytes */

				/* write call instruction */
				SSetPos(curstream, (unsigned int)calladdr);
				asmCallRelative(curstream, hookmem);

				/* insert hook into hook table */
				curdesc = create(HookDesc);
				curdesc->hookmem = hookmem;
				curdesc->funcs = BT_create((BTCompare)def_compare);
				BT_insert(curdesc->funcs, (void *)hookfunc, (void *)hookfunc);
				BT_insert(hook_tree, (void *)calladdr, (void *)curdesc);

				/* done */
				toreturn = 1;
			}

			DeleteProcessStream(pstream);
		}

		clean(curps);
	}

	return toreturn;
}

/* SetRandomHook - hooks up a specific code location in the assembly, typically the start of a function
				 - used mostly when there are too many different call locations to a function we want to hook
				 - this function moves a sufficient amount of instruction till after the hook code and replaces the moved instructions with a call to the hook
*/
int SetRandomHook(unsigned int address, pHookFunc hookfunc)
{
	return 0;
}