#ifndef BINARYTREE_INCLUDED
#define BINARYTREE_INCLUDED

/* 
	 Reusable implementation of an AVL balanced binarytree, which serves well as a general
	 collection datastructure, as both insertion and searching scale as ln(n), while size
	 of the tree scales linear with the amount of data stored (n).
	 I will use this tree structure most of the time if i need a collection. At some points a
	 well-designed hash-table would perform better. But a hashtable only works if you can
	 predict the required size: too small and it will be slow, too large and it will waste
	 memory. A linked implementation of a balanced binary tree, on the other hand, dynamically
	 sizes, 'so never really wastes memory', and, though slower than a suitably large hashtable,
	 the AVL balancing algorithm reduces the worst case performance to logarithmic in the tree's
	 height, so it is practically always fast enough.
	 
	  -- Wim Decelle
*/

#include "Features.h"//synchronization of datastructures is considered a feature
#include "LinkedList.h"//required to implement the tolist conversions

#ifdef SYNCHRONIZED//synchronization is platform specific here :: so porting requires changes here
#include <windows.h>
#endif

//we need max macro
#ifndef getmax
#define getmax(a,b) (((a)>(b))?(a):(b))
#endif

typedef int (*BTCompare)(void *,void *);

typedef struct BTNodeStruct
{
	void * key;
	void * data;
	struct BTNodeStruct * left;
	struct BTNodeStruct * right;
	struct BTNodeStruct * parent;
	unsigned int height;
} BinaryTreeNode;//5*sizeof(pointer) + 1*sizeof(unsigned int) = 24 on 32bit platform and 44 on a 64bit platform

typedef struct
{
	unsigned int itemcount;
	BinaryTreeNode * root;
	BTCompare compare;
#ifdef SYNCHRONIZED
	LPCRITICAL_SECTION btsync;
#endif
} BinaryTree;

typedef struct
{
	BinaryTree * tree;
	BinaryTreeNode * curnode;
} BinaryTreeEnum;

BinaryTree * BT_create(BTCompare comparisonfunction);
int BT_insert(BinaryTree * pThis,void * key, void * data);
void * BT_find(BinaryTree * pThis,void * tofind);
void * BT_remove(BinaryTree * pThis,void * toremove);
void BT_delete(BinaryTree * todelete);

BinaryTreeEnum * BT_newenum(BinaryTree * oftree);
void * BT_next(BinaryTreeEnum * btenum);
void * BT_previous(BinaryTreeEnum * btenum);
void * BT_reset(BinaryTreeEnum * btenum);
void * BT_end(BinaryTreeEnum * btenum);
void BT_enumdelete(BinaryTreeEnum * todelete);

void * BT_leftmost(BinaryTree * tree);
void * BT_rightmost(BinaryTree * tree);

LinkedList * BT_values(BinaryTree * tree);
LinkedList * BT_keys(BinaryTree * tree);
LinkedList * BT_pairs(BinaryTree * tree);//PAIRS NEED TO BE CLEANED UP LATER ON!!

void ** BT_toarray(BinaryTree * tree, unsigned int * count);

#endif