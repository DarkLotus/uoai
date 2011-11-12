#include "LinkedList.h"
#include "allocation.h"

// General-Purpose Enumerable (Doubly) Linked List Implementation
// -- Wim Decelle

LinkedList * LL_create()
{
	LinkedList * toreturn;

	toreturn=alloc(sizeof(LinkedList));

	toreturn->head=0;
	toreturn->tail=0;
	toreturn->itemcount=0;

#ifdef SYNCHRONIZED
	toreturn->LLSync=alloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(toreturn->LLSync);
#endif

	return toreturn;
}

void LL_push(LinkedList * onlist, void * topush)
{
	LinkedListElement * newelement;

#ifdef SYNCHRONIZED
	EnterCriticalSection(onlist->LLSync);
#endif

	newelement=alloc(sizeof(LinkedListElement));

	newelement->next=0;
	newelement->value=topush;

	if(onlist->tail)
	{
		newelement->prev=onlist->tail;
		onlist->tail->next=newelement;
		onlist->tail=newelement;
	}
	else
	{
		newelement->prev=0;

		onlist->head=newelement;
		onlist->tail=newelement;
	}
	onlist->itemcount++;


#ifdef SYNCHRONIZED
	LeaveCriticalSection(onlist->LLSync);
#endif

	return;
}

void * LL_pop(LinkedList * fromlist)
{
	LinkedListElement * current;
	void * toreturn=0;

#ifdef SYNCHRONIZED
	EnterCriticalSection(fromlist->LLSync);
#endif

	if(fromlist->itemcount!=0)
	{
		current=fromlist->tail;

		toreturn=current->value;
		
		if(current->prev)
		{
			fromlist->tail=current->prev;
			current->prev->next=0;
			fromlist->itemcount--;
		}
		else
		{
			fromlist->tail=0;
			fromlist->head=0;
			fromlist->itemcount=0;
		}
		
		clean(current);
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(fromlist->LLSync);
#endif

	return toreturn;
}

void * LL_dequeue(LinkedList * fromlist)
{
	LinkedListElement * current;
	void * toreturn=0;

#ifdef SYNCHRONIZED
	EnterCriticalSection(fromlist->LLSync);
#endif

	if(fromlist->itemcount!=0)
	{
		current=fromlist->head;

		toreturn=current->value;
		
		if(current->next)
		{
			fromlist->head=current->next;
			current->next->prev=0;
			fromlist->itemcount--;
		}
		else
		{
			fromlist->tail=0;
			fromlist->head=0;
			fromlist->itemcount=0;
		}
		
		clean(current);
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(fromlist->LLSync);
#endif

	return toreturn;
}

void LL_delete(LinkedList * todelete)
{
	while(LL_pop(todelete))
		;
	
#ifdef SYNCHRONIZED
	DeleteCriticalSection(todelete->LLSync);
	clean(todelete->LLSync);
#endif

	clean(todelete);

	return;
}

LinkedListEnum * LL_newenum(LinkedList * fromlist)
{
	LinkedListEnum * toreturn=alloc(sizeof(LinkedListEnum));

	toreturn->current=0;
	toreturn->oflist=fromlist;

#ifdef SYNCHRONIZED
	EnterCriticalSection(fromlist->LLSync);
#endif

	return toreturn;
}

void * LL_next(LinkedListEnum * onenum)
{
	if(onenum->current==0)
		onenum->current=onenum->oflist->head;
	else if(onenum->current->next)
		onenum->current=onenum->current->next;
	else
		return 0;

	if(onenum->current)
		return onenum->current->value;
	else
		return 0;
}

void * LL_previous(LinkedListEnum * onenum)
{
	void * toreturn=0;
	if(onenum->current&&(onenum->current=onenum->current->prev))
		toreturn=onenum->current->value;
	return toreturn;
}

void * LL_reset(LinkedListEnum * onenum)
{
	onenum->current=0;
	return 0;
}

void * LL_end(LinkedListEnum * onenum)
{
	onenum->current=onenum->oflist->tail;
	if(onenum->current)
		return onenum->current->value;
	else
		return 0;
}

void LL_enumdelete(LinkedListEnum * todelete)
{
#ifdef SYNCHRONIZED
	LeaveCriticalSection(todelete->oflist->LLSync);
#endif

	clean(todelete);

	return;
}