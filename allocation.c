#include "allocation.h"
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG_ALLOC

#include <windows.h> // synchronization
#include <stdio.h>	// heap state can be dumped to stdout
#include "BinaryTree.h" // heap state is maintained in a binary tree (note that allocations of this tree are not tracked!)

unsigned int allocatorbusy = 0; // reentreant allocations should not be tracked
unsigned int allocatorthread = 0; // synchronization
unsigned int allocatorinitialized = 0; // one-time initialization

BinaryTree * btAllocations = NULL; // allocation tree, stores ( address <-> description ) pairs
CRITICAL_SECTION btAllocLock; // synchronization

// allocation descriptor
typedef struct
{
	void * allocation;
	unsigned int size;
	char * filename;
	unsigned int linenumber;
} DebugAllocation;

// allocation descriptors stored by address
int address_compare(unsigned int a, unsigned int b)
{
	if(a>b)
		return -1;
	else if(a<b)
		return +1;
	else
		return 0;
}

// setup allocatoin tree and initialize critical section once
void debug_alloc_inits()
{
	

	return;
}


int debug_enter()
{ 
	unsigned int tid;

	//one-time initialization
	if(InterlockedExchange(&allocatorinitialized, 1) == 0)
	{
		InitializeCriticalSection(&btAllocLock);
		btAllocations=BT_create((BTCompare)address_compare);
	}

	// reentreant allocation must be prevented (ie allocations for the purpose of maintaining the allocation tree should not be entered into the allocation tree)
	tid =  GetCurrentThreadId();
	if( ( allocatorbusy == 0 )  || (allocatorthread !=tid) )
	{
		EnterCriticalSection(&btAllocLock);
		allocatorthread = GetCurrentThreadId();
		allocatorbusy++;
		return 1;// first time this thread enters, so it's an actual allocation
	}
	else
	{
		allocatorbusy++;
		return 0;// reentrance, so don't store this allocation
	}
}

void debug_leave()
{
	// reentreance/synchronization
	allocatorbusy--;
	if(allocatorbusy==0)
		LeaveCriticalSection(&btAllocLock);
}

// debug malloc
void * debug_allocate(unsigned int size, char * filename, unsigned int linenumber)
{	
	void * toreturn = NULL;
	DebugAllocation * da = NULL;

	// actual allocation
	toreturn = malloc(size);

	// only add allocations to the allocation tree if we are not allocating in a reentrant way
	if(debug_enter())
	{
		da = create(DebugAllocation);

		da->allocation = toreturn;
		da->filename = filename;
		da->linenumber = linenumber;
		da->size = size;

		BT_insert(btAllocations, da->allocation, da);
	}

	debug_leave();

	return toreturn;
}

// debug free
void debug_free(void * tofree)
{
	DebugAllocation * da = NULL;

	if(debug_enter())
	{
		if(da = (DebugAllocation *)BT_find(btAllocations, tofree))
		{
			BT_remove(btAllocations, tofree);
			clean(da);
		}
	}

	free(tofree);
	
	debug_leave();

	return;
}

// debug realloc
void * debug_resize(void * toresize, unsigned int newsize)
{
	DebugAllocation * da = NULL;

	void * toreturn = realloc(toresize, newsize);

	if(debug_enter())
	{
		if(da = (DebugAllocation *)BT_find(btAllocations, toresize))
		{
			da->allocation = toreturn;
			da->size = newsize;
		}
	}

	debug_leave();

	return toreturn;;
}

// dump current heap
void print_allocations()
{
	BinaryTreeEnum * btenum;
	DebugAllocation * curdesc;

	debug_enter();

	btenum=BT_newenum(btAllocations);
	while(curdesc=(DebugAllocation *)BT_next(btenum))
		printf("%x (%u)\n%s :: line %u\n\n", (unsigned int)curdesc->allocation, (unsigned int)curdesc->size, curdesc->filename, curdesc->linenumber);
	BT_enumdelete(btenum);

	debug_leave();

	return;
}

#endif

// buffer duplication
void * duplicate(void * toduplicate, unsigned int size)
{
	void * toreturn;

	toreturn=alloc(size);
	memcpy(toreturn, toduplicate, size);

	return toreturn;
}

// C-string duplication
char * duplicatestring(char * str)
{
	return (char *)duplicate(str, strlen(str)+1);
}