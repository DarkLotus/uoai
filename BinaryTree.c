#include "BinaryTree.h"
#include "allocation.h"

//#include <stdio.h>

/* 
	 Reusable implementation of an AVL balanced binarytree, which serves well as a general
	 collection datastructure, as both insertion and searching scale as ln(n), while size
	 of the tree scales linear with the amount of data stored (n).
	 I will use this tree structure most of the time if i need a collection. At some points a
	 well-designed hash-table would perform better. But a hashtable only works if you can
	 predict the required size: too small and it will be slow, too large and it will waste
	 memory. A linked implementation of a balanced binary tree, on the other hand, dynamically
	 sizes, so never really wastes memory, and, though slower than a suitably large hashtable,
	 the AVL balancing algorithm reduces the worst case performance to logarithmic in the trees
	 height, so it is practically always fast enough.
	 
	  -- Wim Decelle
*/

BinaryTreeNode * findinsertionpoint(BinaryTree * intree,void * tofind,int * lastcomparison);
BinaryTreeNode * createnode(BinaryTreeNode * parent,void * key, void * data);
void balanceup(BinaryTree * tree,BinaryTreeNode * fromnode);
void recalculateheight(BinaryTreeNode * curnode);
BinaryTreeNode * rotateleft(BinaryTreeNode * torotate);
BinaryTreeNode * rotateright(BinaryTreeNode * torotate);
int getbalance(BinaryTreeNode * ofnode);
BinaryTreeNode * findreplacement(BinaryTreeNode * ofnode);
void swapnodes(BinaryTreeNode * nodea,BinaryTreeNode * nodeb);
void DeleteLeafNode(BinaryTree * intree,BinaryTreeNode * todelete);
BinaryTreeNode * inordernext(BinaryTreeNode * ofnode);
BinaryTreeNode * inorderprevious(BinaryTreeNode * ofnode);
BinaryTreeNode * leftmost(BinaryTree * oftree);
BinaryTreeNode * rightmost(BinaryTree * oftree);

BinaryTree * BT_create(BTCompare comparisonfunction)
{
	BinaryTree * toreturn=(BinaryTree *)alloc(sizeof(BinaryTree));
	toreturn->compare=comparisonfunction;
	toreturn->itemcount=0;
	toreturn->root=0;

#ifdef SYNCHRONIZED
	toreturn->btsync=alloc(sizeof(CRITICAL_SECTION));
	InitializeCriticalSection(toreturn->btsync);
#endif

	return toreturn;
}

BinaryTreeNode * createnode(BinaryTreeNode * parent,void * key, void * data)
{
	BinaryTreeNode * toreturn=(BinaryTreeNode *)alloc(sizeof(BinaryTreeNode));
	toreturn->key=key;
	toreturn->data=data;
	toreturn->height=1;
	toreturn->left=0;
	toreturn->parent=parent;
	toreturn->right=0;
	return toreturn;
}

BinaryTreeNode * findinsertionpoint(BinaryTree * intree,void * tofind,int * lastcomparison)
{
	BinaryTreeNode * curnode;

	(*lastcomparison)=0;
	if(!intree)
		return 0;
	if(!intree->root)
		return 0;

	curnode=intree->root;
	while(curnode)
	{
		(*lastcomparison)=intree->compare(curnode->key,tofind);
		if(((*lastcomparison)<0)&&(curnode->left))
			curnode=curnode->left;
		else if(((*lastcomparison)>0)&&(curnode->right))
			curnode=curnode->right;
		else
			return curnode;
	}
	return curnode;//should never be reached
}

int BT_insert(BinaryTree * pThis,void * key, void * data)
{
	BinaryTreeNode * newnode;
	BinaryTreeNode * insertionnode;
	int comparisonresult;

	//special cases
	if(!pThis)//invalid tree
		return 0;

#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->btsync);//sync-lock
#endif

	if(!pThis->root)//empty tree
	{
		newnode=createnode(0,key,data);
		pThis->root=newnode;
		pThis->itemcount=1;

#ifdef SYNCHRONIZED
		LeaveCriticalSection(pThis->btsync);
#endif
		return 1;
	}

	insertionnode=findinsertionpoint(pThis,key,&comparisonresult);
	if(comparisonresult<0)
		insertionnode->left=createnode(insertionnode,key,data);
	else if(comparisonresult>0)
		insertionnode->right=createnode(insertionnode,key,data);
	else//already in tree
	{
#ifdef SYNCHRONIZED
		LeaveCriticalSection(pThis->btsync);
#endif
		return 0;
	}

	balanceup(pThis,insertionnode);

	pThis->itemcount++;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->btsync);
#endif

	return 1;
}

void recalculateheight(BinaryTreeNode * curnode)
{
	if((curnode->left)&&(curnode->right))
		curnode->height=getmax(curnode->left->height,curnode->right->height)+1;
	else if(curnode->left)
		curnode->height=curnode->left->height+1;
	else if(curnode->right)
		curnode->height=curnode->right->height+1;
	else
		curnode->height=1;
	return;
}

BinaryTreeNode * rotateleft(BinaryTreeNode * torotate)
{
	BinaryTreeNode * oldparent=torotate->parent;
	BinaryTreeNode * newtop=torotate->right;
	
	torotate->right=newtop->left;
	if(newtop->left)
		newtop->left->parent=torotate;
	
	newtop->left=torotate;
	torotate->parent=newtop;

	newtop->parent=oldparent;
	if(oldparent&&(oldparent->left==torotate))
		oldparent->left=newtop;
	else if(oldparent)
		oldparent->right=newtop;

	recalculateheight(torotate);
	recalculateheight(newtop);

	return newtop;
}

BinaryTreeNode * rotateright(BinaryTreeNode * torotate)
{
	BinaryTreeNode * oldparent=torotate->parent;
	BinaryTreeNode * newtop=torotate->left;
	
	torotate->left=newtop->right;
	if(newtop->right)
		newtop->right->parent=torotate;
	
	newtop->right=torotate;
	torotate->parent=newtop;

	newtop->parent=oldparent;
	if(oldparent&&(oldparent->left==torotate))
		oldparent->left=newtop;
	else if(oldparent)
		oldparent->right=newtop;

	recalculateheight(torotate);
	recalculateheight(newtop);

	return newtop;
}

int getbalance(BinaryTreeNode * ofnode)
{
	if((ofnode->left)&&(ofnode->right))
		return ((ofnode->right->height)-(ofnode->left->height));
	else if(ofnode->left)
		return 0-(ofnode->left->height);
	else if(ofnode->right)
		return (ofnode->right->height)-0;
	else
		return 0;
}

void balanceup(BinaryTree * tree,BinaryTreeNode * fromnode)
{
	int curbalance;
	while(1)
	{
		curbalance=getbalance(fromnode);
		if(curbalance<-1)
		{
			if(getbalance(fromnode->left)>0)
				fromnode->left=rotateleft(fromnode->left);
			fromnode=rotateright(fromnode);
		}
		else if(curbalance>+1)
		{
			if(getbalance(fromnode->right)<0)
				fromnode->right=rotateright(fromnode->right);
			fromnode=rotateleft(fromnode);
		}
		recalculateheight(fromnode);
		if(!fromnode->parent)
		{
			tree->root=fromnode;
			break;
		}
		fromnode=fromnode->parent;
	}
	return;
}

void * BT_find(BinaryTree * pThis,void * tofind)
{
	int comparisonresult;
	void * toreturn;
	BinaryTreeNode * curnode;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->btsync);//sync-lock
#endif

	curnode=findinsertionpoint(pThis,tofind,&comparisonresult);
	if((!curnode)||(comparisonresult!=0))
	{
#ifdef SYNCHRONIZED
		LeaveCriticalSection(pThis->btsync);
#endif
		return 0;
	}
	else
	{
		toreturn=curnode->data;
#ifdef SYNCHRONIZED
		LeaveCriticalSection(pThis->btsync);
#endif
		return toreturn;
	}
}

void swapnodes(BinaryTreeNode * nodea,BinaryTreeNode * nodeb)
{
	void * temp=nodea->data;
	void * tempb=nodea->key;
	nodea->data=nodeb->data;
	nodea->key=nodeb->key;
	nodeb->data=temp;
	nodeb->key=tempb;
	return;
}

BinaryTreeNode * findreplacement(BinaryTreeNode * ofnode)
{
	BinaryTreeNode * toreturn;
	if(!ofnode)
		return 0;
	if(ofnode->right)//left most in the right subtree
	{
		toreturn=ofnode->right;
		while(toreturn->left)
			toreturn=toreturn->left;
		return toreturn;
	}
	else if(ofnode->left)//or rightmost in the left subtree
	{
		toreturn=ofnode->left;
		while(toreturn->right)
			toreturn=toreturn->right;
		return toreturn;
	}
	else
		return ofnode;
}

void DeleteLeafNode(BinaryTree * intree,BinaryTreeNode * todelete)
{
	BinaryTreeNode * parent;

	if((!intree)||(!todelete))
		return;

	parent=todelete->parent;
	if(!parent)
		intree->root=0;
	else
	{
		if(parent->left==todelete)
			parent->left=0;
		else if(parent->right==todelete)
			parent->right=0;
		balanceup(intree,parent);
	}
	clean(todelete);
	intree->itemcount--;
	return;
}

void * BT_remove(BinaryTree * pThis,void * toremove)
{
	void * toreturn;
	int comparisonresult;
	BinaryTreeNode * swapnode;
	BinaryTreeNode * curnode;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(pThis->btsync);
#endif

	curnode=findinsertionpoint(pThis,toremove,&comparisonresult);
	if((!curnode)||(comparisonresult!=0))
	{
#ifdef SYNCHRONIZED
		LeaveCriticalSection(pThis->btsync);
#endif
		return 0;
	}
	toreturn=curnode->data;
	swapnode=findreplacement(curnode);
	swapnodes(curnode,swapnode);
	curnode=swapnode;
	while((curnode->left)||(curnode->right))
	{
		swapnode=findreplacement(curnode);
		swapnodes(curnode,swapnode);
		curnode=swapnode;
	}
	DeleteLeafNode(pThis,curnode);

#ifdef SYNCHRONIZED
	LeaveCriticalSection(pThis->btsync);
#endif

	return toreturn;
}

void BT_deletenode(BinaryTree * pThis,BinaryTreeNode * curnode)
{
	BinaryTreeNode * swapnode;
	swapnode=findreplacement(curnode);
	swapnodes(curnode,swapnode);
	curnode=swapnode;
	while((curnode->left)||(curnode->right))
	{
		swapnode=findreplacement(curnode);
		swapnodes(curnode,swapnode);
		curnode=swapnode;
	}
	DeleteLeafNode(pThis,curnode);
	return;
}

void BT_delete(BinaryTree * todelete)
{
	while(todelete->root)
	{
		BT_deletenode(todelete,todelete->root);
	}

#ifdef SYNCHRONIZED
		DeleteCriticalSection(todelete->btsync);
		clean(todelete->btsync);
#endif
	
	clean(todelete);
	
	return;
}

BinaryTreeNode * leftmost(BinaryTree * oftree)
{
	BinaryTreeNode * toreturn;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(oftree->btsync);
#endif

	toreturn=oftree->root;
	while(toreturn&&(toreturn->left))
		toreturn=toreturn->left;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(oftree->btsync);
#endif

	return toreturn;
}

BinaryTreeNode * rightmost(BinaryTree * oftree)
{
	BinaryTreeNode * toreturn;
	
#ifdef SYNCHRONIZED
	EnterCriticalSection(oftree->btsync);
#endif

	toreturn=oftree->root;
	while(toreturn&&(toreturn->right))
		toreturn=toreturn->right;

#ifdef SYNCHRONIZED
	LeaveCriticalSection(oftree->btsync);
#endif

	return toreturn;
}

BinaryTreeNode * inordernext(BinaryTreeNode * ofnode)
{
	BinaryTreeNode * curnode;
	if(!ofnode)
		return 0;
	if(ofnode->right)
	{
		curnode=ofnode->right;
		while(curnode->left)
			curnode=curnode->left;
		return curnode;
	}
	else
	{
		curnode=ofnode;
		while(curnode->parent)
		{
			if(curnode->parent->left==curnode)
				return curnode->parent;
			else
				curnode=curnode->parent;
		}
		return 0;
	}
}

BinaryTreeNode * inorderprevious(BinaryTreeNode * ofnode)
{
	BinaryTreeNode * curnode;
	if(!ofnode)
		return 0;
	if(ofnode->left)
	{
		curnode=ofnode->left;
		while(curnode->right)
			curnode=curnode->right;
		return curnode;
	}
	else
	{
		curnode=ofnode;
		while(curnode->parent)
		{
			if(curnode->parent->right==curnode)
				return curnode->parent;
			else
				curnode=curnode->parent;
		}
		return 0;
	}
}

BinaryTreeEnum * BT_newenum(BinaryTree * oftree)
{
	BinaryTreeEnum * toreturn;

	toreturn=(BinaryTreeEnum *)alloc(sizeof(BinaryTreeEnum));
	toreturn->tree=oftree;
	toreturn->curnode=0;

#ifdef SYNCHRONIZED
	EnterCriticalSection(oftree->btsync);
#endif

	return toreturn;
}

void * BT_next(BinaryTreeEnum * btenum)
{
	BinaryTreeNode * newnode;
	if(!btenum->curnode)
		newnode=leftmost(btenum->tree);
	else
		newnode=inordernext(btenum->curnode);
	if(!newnode)
		return 0;
	else
	{
		btenum->curnode=newnode;
		return btenum->curnode->data;
	}
}

void * BT_previous(BinaryTreeEnum * btenum)
{
	btenum->curnode=inorderprevious(btenum->curnode);
	if(!btenum->curnode)
		return 0;
	else
		return btenum->curnode->data;
}

void * BT_end(BinaryTreeEnum * btenum)
{
	btenum->curnode=rightmost(btenum->tree);
	return btenum->curnode->data;
}

void * BT_reset(BinaryTreeEnum * btenum)
{
	btenum->curnode=0;
	return 0;
}

void BT_enumdelete(BinaryTreeEnum * todelete)
{
#ifdef SYNCHRONIZED
	LeaveCriticalSection(todelete->tree->btsync);
#endif

	clean(todelete);
	return;
}

void * BT_leftmost(BinaryTree * tree)
{
	BinaryTreeNode * lm=leftmost(tree);
	if(!lm)
		return 0;
	else
		return lm->data;
}

void * BT_rightmost(BinaryTree * tree)
{
	BinaryTreeNode * rm=rightmost(tree);
	if(!rm)
		return 0;
	else
		return rm->data;
}

LinkedList * BT_values(BinaryTree * tree)
{
	LinkedList * toreturn;
	BinaryTreeNode * curnode;

	toreturn=LL_create();

#ifdef SYNCHRONIZED
	EnterCriticalSection(tree->btsync);
#endif

	curnode=leftmost(tree);

	while(curnode)
	{
		LL_push(toreturn,curnode->data);
		curnode=inordernext(curnode);
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(tree->btsync);
#endif

	return toreturn;
}

LinkedList * BT_keys(BinaryTree * tree)
{
	LinkedList * toreturn;
	BinaryTreeNode * curnode;

	toreturn=LL_create();

#ifdef SYNCHRONIZED
	EnterCriticalSection(tree->btsync);
#endif

	curnode=leftmost(tree);

	while(curnode)
	{
		LL_push(toreturn,curnode->key);
		curnode=inordernext(curnode);
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(tree->btsync);
#endif

	return toreturn;
}

LinkedList * BT_pairs(BinaryTree * tree)//PAIRS NEED TO BE CLEANED UP LATER ON!
{
	LinkedList * toreturn;
	BinaryTreeNode * curnode;
	void ** curpair;

	toreturn=LL_create();

#ifdef SYNCHRONIZED
	EnterCriticalSection(tree->btsync);
#endif

	curnode=leftmost(tree);

	while(curnode)
	{
		curpair=alloc(sizeof(void *)*2);

		curpair[0]=curnode->key;
		curpair[1]=curnode->data;

		LL_push(toreturn,curpair);
		curnode=inordernext(curnode);
	}

#ifdef SYNCHRONIZED
	LeaveCriticalSection(tree->btsync);
#endif

	return toreturn;
}

unsigned int CalculateTreeArraySize(BinaryTree * oftree)
{
	unsigned int curlevel, curcount, curlevelcount;

	curlevel=1;
	curlevelcount=1;
	curcount=0;

	if(!(oftree->root))
		return 0;

	while(curlevel<=oftree->root->height)
	{
		curcount+=curlevelcount;
		
		curlevelcount*=2;
		
		curlevel++;
	}

	return curcount;
}

void ** BT_toarray(BinaryTree * tree, unsigned int * count)
{
	void ** toreturn;
	LinkedList * llstack;
	BinaryTreeNode * curnode;
	unsigned int idx=0;
	unsigned int cursize=0;

	toreturn=0;

#ifdef SYNCHRONIZED
	EnterCriticalSection(tree->btsync);
#endif

	if(tree->itemcount)
	{
		cursize=CalculateTreeArraySize(tree);

		toreturn=alloc(sizeof(void *)*2*cursize);

		//replace with memset?
		for(idx=0;idx<(2*cursize);idx++)
			toreturn[idx]=0;
		idx=0;
		
		llstack=LL_create();

		curnode=tree->root;
		while(curnode)
		{
			if(curnode->left)
			{
				LL_push(llstack,(void *)(idx*2+1));
				LL_push(llstack,(void *)(curnode->left));
			}
			if(curnode->right)
			{
				LL_push(llstack,(void *)(idx*2+2));
				LL_push(llstack,(void *)(curnode->right));
			}

			toreturn[idx*2]=curnode->key;
			toreturn[idx*2+1]=curnode->data;

			idx=(unsigned int)LL_dequeue(llstack);
			curnode=(BinaryTreeNode *)LL_dequeue(llstack);
		}


		LL_delete(llstack);
	}

#ifdef SYNCRHONIZED
	LeaveCriticalSection(tree->btsync);
#endif

	if(count)
		(*count)=cursize;

	return toreturn;
}