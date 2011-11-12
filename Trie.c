#include "Trie.h"

Trie * Tr_create(unsigned int tokensize, BTCompare token_comparator)
{
	Trie * toreturn;
	
	toreturn=alloc(sizeof(Trie));
	toreturn->count=0;
	toreturn->root=alloc(sizeof(TrieNode));
	toreturn->root->children=BT_create(token_comparator);
	toreturn->root->data=0;
	toreturn->tokensize=tokensize;
	toreturn->comparator=token_comparator;
#ifdef SYNCHRONIZED
	toreturn->TrSync=(LPCRITICAL_SECTION)alloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(toreturn->TrSync);
#endif

	return toreturn;
}

int IsNullToken(void * token, unsigned int tokensize)
{
	unsigned int idx;
	char * curptr=(char *)token;

	for(idx=0;idx<tokensize;idx++)
	{
		if((*curptr))
			return 0;
		curptr+=tokensize;
	}

	return 1;
}

void Tr_add(Trie * pThis,void * key, void * data)
{
	TrieNode * curnode, * nextnode;
	void * pCurToken;
	unsigned int i;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->TrSync);
#endif
	
	i=0;

	pCurToken=key;
	curnode=pThis->root;

	while(!IsNullToken(pCurToken,pThis->tokensize))
	{
		if(!(nextnode=BT_find(curnode->children,pCurToken)))
		{
			nextnode=alloc(sizeof(TrieNode));
			nextnode->children=BT_create(pThis->comparator);
			nextnode->data=0;
			BT_insert(curnode->children,pCurToken,nextnode);
		}
		curnode=nextnode;
		i++;
		pCurToken=(char *)key+i*(pThis->tokensize);
	}
	
	curnode->data=data;
	pThis->count++;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->TrSync);
#endif

	return;
}

void Tr_insert(Trie * pThis,void * key, unsigned int len, void * data)
{
	TrieNode * curnode, * nextnode;
	void * pCurToken;
	unsigned int i;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->TrSync);
#endif

	curnode=pThis->root;

	for(i=0;i<len;i++)
	{
		pCurToken=(char *)key+i*(pThis->tokensize);
		if(!(nextnode=BT_find(curnode->children,pCurToken)))
		{
			nextnode=alloc(sizeof(TrieNode));
			nextnode->children=BT_create(pThis->comparator);
			nextnode->data=0;
			BT_insert(curnode->children,pCurToken,nextnode);
		}
		curnode=nextnode;
	}
	
	curnode->data=data;
	pThis->count++;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->TrSync);
#endif

	return;
}

void * Tr_lookup(Trie * pThis,void * tofind)
{
	TrieNode * curnode;
	void * pCurToken;
	unsigned int i;
	void * toreturn;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->TrSync);
#endif
	
	i=0;

	pCurToken=tofind;
	curnode=pThis->root;

	while(!IsNullToken(pCurToken,pThis->tokensize))
	{
		if(!(curnode=BT_find(curnode->children,pCurToken)))
		{
#ifdef SYNCHRONIZED
			LeaveCriticalSection(pThis->TrSync);
#endif
			return 0;
		}
		i++;
		pCurToken=(char *)tofind+i*(pThis->tokensize);
	}
	
	toreturn=curnode->data;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->TrSync);
#endif

	return toreturn;
}

void * Tr_find(Trie * pThis,void * tofind, unsigned int len)
{
	TrieNode * curnode;
	void * pCurToken;
	unsigned int i;
	void * toreturn;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->TrSync);
#endif

	curnode=pThis->root;

	for(i=0;i<len;i++)
	{
		pCurToken=(char *)tofind+i*(pThis->tokensize);
		if(!(curnode=BT_find(curnode->children,pCurToken)))
		{
#ifdef SYNCHRONIZED
			LeaveCriticalSection(pThis->TrSync);
#endif
			return 0;
		}		
	}

	toreturn=curnode->data;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->TrSync);
#endif

	return toreturn;
}

void DeleteTrieNode(TrieNode * todelete)
{
	BinaryTreeEnum * btenum;
	TrieNode * curchild;

	btenum=BT_newenum(todelete->children);
	while(curchild=(TrieNode *)BT_next(btenum))
		DeleteTrieNode(curchild);

	BT_delete(todelete->children);

	clean(todelete);

	return;
}

void Tr_delete(Trie * todelete)
{
	DeleteTrieNode(todelete->root);

#ifdef SYNCHRONIZED
	DeleteCriticalSection(todelete->TrSync);
	clean(todelete->TrSync);
#endif

	clean(todelete);

	return;
}