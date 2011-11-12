#ifndef BINARYHEAP_INCLUDED
#define BINARYHEAP_INCLUDED

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

#include "Features.h"// synchronization of datastructures is considered a feature

#ifdef SYNCHRONIZED
#include <windows.h>
#endif

#define BH_NEWSIZE(originalsize) (((originalsize)*3)/2)
#define BH_PARENT(idx) (((idx)-1)/2)
#define BH_LEFTCHILD(idx) ((idx)*2+1)
#define BH_RIGHTCHILD(idx) ((idx)*2+2)
#define BH_KEY(idx) ((idx)*2)
#define BH_VALUE(idx) ((idx)*2+1)

typedef int (*BH_compare)(void *, void *);

typedef struct BinaryHeapStruct
{
	void ** data;//key-value pairs
	unsigned int size;//
	unsigned int count;
	BH_compare comparison_function;
#ifdef SYNCHRONIZED
	LPCRITICAL_SECTION bhSync;
#endif
} BinaryHeap;

BinaryHeap * BH_create(BH_compare comparisonfunction, unsigned int initialsize);
void BH_insert(BinaryHeap * pThis,void * key, void * data);
void * BH_pop(BinaryHeap * pThis);
void BH_delete(BinaryHeap * todelete);

#endif