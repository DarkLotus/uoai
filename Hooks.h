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

/* thiscall wrappers:	- not really hooks, but require writing x86 instructions, so belong to the same category as the hook code
						- create_thiscall_wrapper
								A thiscall wrapper takes the 'this' parameter from the stack and moves it into the ecx register
								before performing the call. To do this instructions are written to backup and restore the return address before popping this -> ecx.
								This allows wrapping thiscalls into __stdcall (*)(void *pThis, ...rest_of_parameters...) calls.
						- create_thiscall_callback 
								A thiscall callback is the opposite of a thiscall wrapper. This creates a chunk which takes the this-pointer from the ecx register
								and pushes it onto the stack. Again the returnaddress is backed up and restored to do this.
								Example of usage is when installing YesNoGump->onYes or ->onNo callbacks. Those get the pThis pointer to the yes-no-gump in ecx.
						*/
unsigned int create_thiscall_wrapper(unsigned int towrap);
unsigned int create_thiscall_callback(unsigned int towrap);

#endif