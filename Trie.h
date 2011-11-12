#ifndef TRIE_INCLUDED
#define TRIE_INCLUDED

/*

  Trie : during parsing, given a sequence of input bytes (or tokens) we might want to lookup the first sequence of n tokens
	in a set of possible token-lists. By using each byte (each token) as an index on each level, we can quickly lookup the presence
	of a certain tokenlist in the set.
	So in summary: represents a set of variable-length token lists (though tokens are most likely fixed size);
	Each level is just an AVL balanced binary tree (BinaryTree.h)
	
	So in summary, this is a datastructure used mostly when implementing a parser.

	-- Wim Decelle
*/

#include "Features.h"
#include "BinaryTree.h"
#include "allocation.h"

#ifdef SYNCHRONIZED
#include <windows.h>
#endif

typedef struct TrieNodeStruct
{
	BinaryTree * children;
	void * data;
} TrieNode;

typedef struct TrieStruct
{
	TrieNode * root;
	unsigned int count;
	unsigned int tokensize;
	BTCompare comparator;
#ifdef SYNCHRONIZED
	LPCRITICAL_SECTION TrSync;
#endif
} Trie;

Trie * Tr_create(unsigned int tokensize, BTCompare token_comparator);
void Tr_add(Trie * pThis,void * key, void * data);//assumes 0 termination
void Tr_insert(Trie * pThis,void * key, unsigned int len, void * data);
void * Tr_find(Trie * pThis,void * tofind, unsigned int len);
void * Tr_lookup(Trie * pThis,void * tofind);
void Tr_delete(Trie * todelete);

#endif