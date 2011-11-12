#ifndef ALLOCATION_INCLUDED
#define ALLOCATION_INCLUDED

/*
	
	  Instead of consistently relying on malloc, realloc and free, this file defines macros that allow
	  usage of other allocation methods, f.e. debug-methods that allow for memory leak detection.

	  -- Wim Decelle

*/

#include "Features.h"
#include <stdlib.h>

// buffer duplication
void * duplicate(void * toduplicate, unsigned int size);

// string duplication
char * duplicatestring(char * str);

#ifndef DEBUG_ALLOC

// allocation interface defines 
//	- use stdlib malloc/free when not debugging the heap
#define alloc(size) malloc(size)
#define resize(obj, size) realloc(obj, size)
#define clean(obj) free((void *)obj)
#define create(objtype) (objtype *)alloc(sizeof(objtype))
#define dump_heap()

#else

// debug malloc
void * debug_allocate(unsigned int size, char * filename, unsigned int linenumber);

// debug free
void debug_free(void * tofree);

//debug realloc
void * debug_resize(void * toresize, unsigned int newsize);

// dump current heap
void print_allocations();

// allocation interface defines
//	- use debug allocation fubnctions to trace memoryleaks
#define alloc(size) debug_allocate(size, __FILE__, __LINE__)
#define resize(Obj, size) debug_resize(Obj, size)
#define clean(Obj) debug_free(Obj)
#define create(ObjType) (ObjType *)alloc(sizeof(ObjType))
#define dump_heap() print_allocations()

#endif

#endif