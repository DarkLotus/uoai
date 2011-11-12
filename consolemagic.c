#include "consolemagic.h"

/*
	Just some tools to cleanup and automate console output; including automated output of some often used datastructures.
	I use a lot of console output during debugging stages, especially when debuggin injected code; so these tools come in handy.
	-- Wim Decelle
*/

#include "allocation.h"

#include <stdio.h>
#include <string.h>

void PrintCenter(char * toprint, char indentchar)
{
	unsigned int len;
	unsigned int indent;
	unsigned int i;
	unsigned int left;

	printf(" ");
	left=0;
	len=strlen(toprint);
	indent=((CONSOLEWIDTH-1-len)/2)+((CONSOLEWIDTH-1-len)%2);
	if((indent+len)<(CONSOLEWIDTH-1))
			left=(CONSOLEWIDTH-1)-(indent+len+1);

	for(i=0;i<indent;i++)
		printf("%c",indentchar);

	printf("%s",toprint);
	
	for(i=0;i<left;i++)
		printf("%c",indentchar);
}

void DrawStatus(char * trailingtext, double percentage)
{
	unsigned int i;
	unsigned int left;
	unsigned int right;

	unsigned int skip;

	printf("\r");
	if(trailingtext)
	{
		skip=strlen(trailingtext);
		printf(trailingtext);
	}
	else
	{
		skip=1;
		printf(" ");
	}

	left=(unsigned int)(percentage*(CONSOLEWIDTH-(1+skip)));
	right=(unsigned int)(CONSOLEWIDTH-(1+skip)-left);

	for(i=0;i<left;i++)
		printf("|");
	for(i=0;i<right;i++)
		printf(" ");

	return;
}

unsigned int DrawWorm(char * trailingtext, unsigned int wormstatus, unsigned int wormlen, char indentchar)
{
	unsigned int indent;
	unsigned int s, m;
	unsigned int skip;

	printf("\r");
	
	if(trailingtext)
	{
		skip=strlen(trailingtext);
		printf(trailingtext);
	}
	else
	{
		skip=1;
		printf(" ");
	}

	if(wormlen>((CONSOLEWIDTH-(1+skip))/2))
		wormlen=((CONSOLEWIDTH-(1+skip))/2);

	if(((wormstatus/(CONSOLEWIDTH-(1+skip)-(wormlen*2)))%2))
		indent=(CONSOLEWIDTH-(1+skip)-(wormlen*2))-(wormstatus%(CONSOLEWIDTH-(1+skip)-(wormlen*2)));
	else
		indent=(wormstatus%(CONSOLEWIDTH-(1+skip)-(wormlen*2)));		

	for(s=0;s<indent;s++)
		printf("%c", indentchar);

	s=wormstatus%4;
		
	if(s==3)
	{
		for(m=0;m<wormlen;m++)
			printf("\\/");
	}
	else if(s==2)
	{
		for(m=0;m<wormlen;m++)
			printf("/\\");
		//printf("----------------");
	}
	else if(s==1)
	{
		for(m=0;m<wormlen;m++)
			printf("/\\");
	}
	else if(s==0)
	{
		for(m=0;m<wormlen;m++)
			printf("\\/");
		//printf("||||||||||||||||");
	}

	for(s=skip+indent+(wormlen*2);s<(CONSOLEWIDTH-1);s++)
		printf("%c", indentchar);

	return (++wormstatus)%(2*(CONSOLEWIDTH-(1+skip)-(wormlen*2)));
}

//Trie printing :: works only for a trie with with key = char * and value = char * (so a char-token trie)

void PrintIndent(unsigned int count, char toprint)
{
	unsigned int i;
	for(i=0;i<count;i++)
		printf("%c", toprint);
	return;
}

void PrintTrieNode(TrieNode * toprint, unsigned int indentlevel, unsigned int curlevel, PrintNodeFunc printfunction, unsigned int printwidth)
{
	LinkedListEnum * llenum;
	LinkedList * btpairs;
	void ** curpair;
	void ** nextpair;
	TrieNode * curnode;

	if(toprint->children->itemcount>0)
	{	
		btpairs=BT_pairs(toprint->children);
		llenum=LL_newenum(btpairs);
		curpair=(void **)LL_next(llenum);

		while(curpair)
		{
			nextpair=(void **)LL_next(llenum);
			
			curnode=(TrieNode *)curpair[1];

			printfunction(curpair[0], curnode->data);

			if(nextpair)
			{
				if(toprint->children->itemcount>1)
					PrintTrieNode(curnode,curlevel,curlevel+1, printfunction, printwidth);
				else
					PrintTrieNode(curnode, indentlevel, curlevel+1, printfunction, printwidth);
			}
			else
				PrintTrieNode(curnode, indentlevel, curlevel+1, printfunction, printwidth);
			
			curpair=nextpair;
		}

		LL_enumdelete(llenum);
		LL_delete(btpairs);
	}
	else
	{
		printf("\n");
		if(indentlevel)
			PrintIndent(indentlevel*printwidth, ' ');
	}

	return;
}

void PrintTrie(Trie * toprint, PrintNodeFunc printfunction, unsigned int printwidth)
{
	PrintTrieNode(toprint->root,0,0, printfunction, printwidth);
	return;
}

//Heap printing

void PrintBinaryHeapNode(BinaryHeap * toprint, unsigned int idx, PrintNodeFunc printfunction)
{
	printfunction(toprint->data[BH_KEY(idx)], toprint->data[BH_VALUE(idx)]);
	return;
}

void PrintBinaryHeapLevel(BinaryHeap * toprint, unsigned int startidx, unsigned int endidx, unsigned int tabcount, PrintNodeFunc printfunction, unsigned int printwidth)
{
	unsigned int curidx;

	for(curidx=startidx;curidx<=endidx;curidx++)
	{
		if(curidx>=toprint->count)
			break;

		PrintBinaryHeapNode(toprint,curidx, printfunction);
		if(tabcount>1)
			PrintIndent((tabcount-1)*printwidth, ' ');
	}
	printf("\n");

	if((endidx+1)<toprint->count)
		PrintBinaryHeapLevel(toprint, startidx*2+1, endidx*2+2, tabcount/2, printfunction, printwidth);

	return;
}

unsigned int GetMaxCount(BinaryHeap * toprint, unsigned int startidx, unsigned int endidx)
{
	if((endidx+1)>=toprint->count)
		return 1+endidx-startidx;
	else
		return GetMaxCount(toprint,startidx*2+1,endidx*2+2);
}

void PrintBinaryHeap(BinaryHeap * toprint, PrintNodeFunc printfunction, unsigned int printwidth)
{
	PrintBinaryHeapLevel(toprint,0,0, GetMaxCount(toprint, 0, 0), printfunction, printwidth);
}

//Binary Tree Printing (AVL Balanced and linked version)
//	- basically the same as the binary heap, except that we need to use an enumerator to overcome the fact that the tree is constructed in a linked fashion
void PrintBinaryTreeLevel(unsigned int count, void ** asarray, unsigned int startidx, unsigned int endidx, unsigned int tabcount, PrintNodeFunc printfunction, unsigned int printwidth)
{
	unsigned int curidx;

	for(curidx=startidx;curidx<=endidx;curidx++)
	{
		if(curidx>=count)
			break;

		printfunction(asarray[curidx*2],asarray[curidx*2+1]);
		
		if(tabcount>1)
			PrintIndent((tabcount-1)*printwidth, ' ');
	}
	printf("\n");

	if((endidx+1)<count)
		PrintBinaryTreeLevel(count, asarray, startidx*2+1, endidx*2+2, tabcount/2, printfunction, printwidth);

	return;
}

unsigned int BTGetMaxCount(unsigned int count, unsigned int startidx, unsigned int endidx)
{
	if((endidx+1)>=count)
		return 1+endidx-startidx;
	else
		return BTGetMaxCount(count,startidx*2+1,endidx*2+2);
}

void PrintBinaryTree(BinaryTree * toprint, PrintNodeFunc printfunction, unsigned int printwidth)
{
	void ** toprint_asarray;
	unsigned int count;

	toprint_asarray=BT_toarray(toprint, &count);

	PrintBinaryTreeLevel(count,toprint_asarray,0,0, BTGetMaxCount(count, 0, 0), printfunction, printwidth);

	clean(toprint_asarray);

	return;
}
