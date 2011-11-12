#ifndef CONSOLEMAGIC_INCLUDED
#define CONSOLEMAGIC_INCLUDED

/*
	Just some tools to cleanup and automate console output; including automated output of some often used datastructures.
	I use a lot of console output during debugging stages, especially when debuggin injected code; so these tools come in handy.

	-- Wim Decelle
*/

#include "Features.h"
#include "Trie.h"//for Trie printing
#include "BinaryHeap.h"//for BinaryHeap printing
#include "BinaryTree.h"//for BinaryTree printing

#define CONSOLEWIDTH 80

unsigned int DrawWorm(char * trailingtext, unsigned int wormstatus, unsigned int wormlen, char indentchar);
void DrawStatus(char * trailingtext, double percentage);

void PrintCenter(char * toprint, char indentchar);

//some printing tools for datstructure debugging:

typedef void (*PrintNodeFunc)(void * key, void * data);

void PrintTrie(Trie * toprint, PrintNodeFunc printfunction, unsigned int printwidth);//only works if key-type=char * and value type is char *, so a char-token trie (string set)
void PrintBinaryHeap(BinaryHeap * toprint, PrintNodeFunc printfunction, unsigned int printwidth);
void PrintBinaryTree(BinaryTree * toprint, PrintNodeFunc printfunction, unsigned int printwidth);

#endif