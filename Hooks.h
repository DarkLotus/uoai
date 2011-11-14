#ifndef HOOKS_INCLUDED
#define HOOKS_INCLUDED

#include "Streams.h"

/* SetImportHook - Hooks up the IAT in the PE headers */
void * SetImportHook(char * libraryname, char * symbolname, void * newaddress);

/* Generic hook function prototype
	- except for the IAT hooks (import hooks),
	all other hooks use a 'safe' hooking mechanism,
	i.e. stack and cpu register states are backedd up and
	restored before and after the hook is called.
	- only when skipping the original call, are the exact
	calling convention and stackframe size required
*/
#pragma pack(push, 1)
typedef struct
{
	unsigned int eflags;
	unsigned int edi;
	unsigned int esi;
	unsigned int ebp;
	unsigned int esp;
	unsigned int ebx;
	unsigned int edx;
	unsigned int ecx;
	unsigned int eax;
} RegisterStates;
#pragma pack(pop)

typedef int (*pHookFunc)(RegisterStates * regs, void * stacktop);

/* SetVtblHook - hooks up a vtbl entry ( = class member method ) */
int SetVtblHook(void * vtblentry, pHookFunc hookfunc, unsigned int stacksize);

/* SetCallHook - replaces a call xxxxx (relative call) instruction with a hook */
int SetCallHook(unsigned int calladdr, pHookFunc hookfunc, unsigned int originaltarget, unsigned int stacksize);

/* SetRandomHook - hooks up a specific code location in the assembly, typically the start of a function
				 - used mostly when there are too many different call locations to a function we want to hook
				 - this function moves a sufficient amount of instruction till after the hook code and replaces the moved instructions with a call to the hook
*/
int SetRandomHook(unsigned int address, unsigned int stacksize, pHookFunc hookfunc);


#endif