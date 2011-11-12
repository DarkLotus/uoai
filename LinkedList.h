#ifndef LINKEDLIST_INCLUDED
#define LINKEDLIST_INCLUDED

// General-Purpose Enumerable (Doubly) Linked List Implementation
// -- Wim Decelle

#include "Features.h"

#ifdef SYNCHRONIZED//synchronization is platform specific here :: so porting requires changes here
#include <windows.h>
#endif

typedef struct LinkedListElementStruct
{
	void * value;
	struct LinkedListElementStruct * next;
	struct LinkedListElementStruct * prev;
} LinkedListElement;

typedef struct LinkedListStruct
{
	LinkedListElement * head;
	LinkedListElement * tail;
	unsigned int itemcount;
#ifdef SYNCHRONIZED
	LPCRITICAL_SECTION LLSync;
#endif
} LinkedList;

typedef struct LinkedListEnumStruct
{
	LinkedListElement * current;
	LinkedList * oflist;
} LinkedListEnum;

LinkedList * LL_create();
void LL_push(LinkedList * onlist, void * topush);
void * LL_pop(LinkedList * fromlist);
void * LL_dequeue(LinkedList * fromlist);
void LL_delete(LinkedList * todelete);
void LL_enumdelete(LinkedListEnum * todelete);
void * LL_next(LinkedListEnum * onenum);
void * LL_previous(LinkedListEnum * onenum);
void * LL_reset(LinkedListEnum * onenum);
void * LL_end(LinkedListEnum * onenum);
LinkedListEnum * LL_newenum(LinkedList * fromlist);

#endif