#include "BinaryHeap.h"
#include "allocation.h"

/*
	
	BinaryHeap : keeps minimal or maximal value on top.
		Typically used in heuristic algortihms, f.e. the shortest-path algorithim (A* pathfinding): naive approach to finding a
		shortest path on a given map is to try ALL possible paths that can be built from the starting location, by taking all possible
		combinations of steps. From the resulting set the shortest path that does reach the destination is chosen. This naïve approach
		takes for ever as soon as the map-size starts growing. The solution chosen in A* pathfinding is to built paths using a heuristic,
		that is: instead of trying all paths, paths are built step by step from the starting location, always trying next the path that
		improved the most, where a heuristic or metric is used to measure which path is currently the best. In pathfinding f.e. the sum
		of the path length and distance to the destination is used. The path that completes first (reaches the destination first) is
		considered the winner.

	-- Wim Decelle

*/

__inline void SwapNodes(BinaryHeap * pThis, unsigned int a, unsigned int b);
unsigned int FixNode(BinaryHeap * pThis, unsigned int tofix);

BinaryHeap * BH_create(BH_compare comparisonfunction,unsigned int initialsize)
{
	BinaryHeap * toreturn;
	
	toreturn=alloc(sizeof(BinaryHeap));
	toreturn->comparison_function=comparisonfunction;
	toreturn->count=0;
	toreturn->size=initialsize;
	toreturn->data=(void **)alloc(sizeof(void *)*2*initialsize);

#ifdef SYNCHRONIZED
	toreturn->bhSync=(LPCRITICAL_SECTION)alloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(toreturn->bhSync);
#endif

	return toreturn;
}

__inline void SwapNodes(BinaryHeap * pThis, unsigned int a, unsigned int b)
{
	void * tempkey, * tempvalue;

	//store node a
	tempkey=pThis->data[BH_KEY(a)];
	tempvalue=pThis->data[BH_VALUE(a)];

	//a <- b
	pThis->data[BH_KEY(a)]=pThis->data[BH_KEY(b)];
	pThis->data[BH_VALUE(a)]=pThis->data[BH_VALUE(b)];

	//b <- stored node a
	pThis->data[BH_KEY(b)]=tempkey;
	pThis->data[BH_VALUE(b)]=tempvalue;

	return;
	
}

unsigned int FixNode(BinaryHeap * pThis, unsigned int tofix)
{
	unsigned int targetchild, lchild, rchild;

	lchild=BH_LEFTCHILD(tofix);
	rchild=BH_RIGHTCHILD(tofix);

	if(lchild<pThis->count)//leftchild in tree?
	{
		//choose the child with the smallest key

		if(rchild<pThis->count)//both children in tree?
		{
			if(pThis->comparison_function((void*)pThis->data[BH_KEY(lchild)],(void*)pThis->data[BH_KEY(rchild)])>0)
				targetchild=lchild;
			else
				targetchild=rchild;
		}
		else
			targetchild=lchild;//only one child, so optional swapping will be done with this one

		//now swap if needed
		if(pThis->comparison_function((void *)pThis->data[BH_KEY(targetchild)],(void *)pThis->data[BH_KEY(tofix)])>0)
		{
			SwapNodes(pThis,targetchild,tofix);
			return targetchild;//return child node with which the swapped occurs (can never be 0)
		}
	}

	//no swapping, return 0
	return 0;
}

void BH_insert(BinaryHeap * pThis,void * key, void * data)
{
	unsigned int curidx;

#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->bhSync);
#endif

	//resize if necessary
	if(pThis->count==pThis->size)
	{
		pThis->size=BH_NEWSIZE(pThis->size);
		pThis->data=resize(pThis->data,pThis->size*2*sizeof(void *));
	}
	
	//insert at end
	pThis->data[BH_KEY(pThis->count)]=key;
	pThis->data[BH_VALUE(pThis->count)]=data;
	pThis->count++;

	//bubble up
	curidx=pThis->count-1;
	while(curidx)
	{
		curidx=BH_PARENT(curidx);
		if(!FixNode(pThis, curidx))
			break;
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->bhSync);
#endif

	return;
}

void * BH_pop(BinaryHeap * pThis)
{
	void * toreturn=0;
	unsigned int curidx;

#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->bhSync);
#endif

	if(pThis->count>1)
	{
		//store item to return
		toreturn=pThis->data[BH_VALUE(0)];
	
		//swap root with bottom
		SwapNodes(pThis,0,pThis->count-1);
	
		//decrease size
		pThis->count--;
	
		//bubble new root down to fix the tree:
		curidx=0;
		while(curidx=FixNode(pThis,curidx))
			;
	}
	else if(pThis->count==1)
	{
		//store item to return
		toreturn=pThis->data[BH_VALUE(0)];

		//decrease size
		pThis->count--;

		//tree is now empty so no further work to do
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->bhSync);
#endif

	return toreturn;
}

void BH_delete(BinaryHeap * todelete)
{
#ifdef SYNCHRONIZED
	DeleteCriticalSection(todelete->bhSync);
	clean(todelete->bhSync);
#endif
	
	clean(todelete->data);
	clean(todelete);
	
	return;
}