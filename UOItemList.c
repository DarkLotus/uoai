#include "UOItemList.h"
#include "UOCallibrations.h"

extern UOCallibrations * callibrations;

IItemList ItemListVtbl = {
	DEFAULT_DISPATCH_VTBL,
	(pCount)itemlist_Count, 
	(pbyID)itemlist_byID, 
	(pbyType)itemlist_byType, 
	(pItems)itemlist_Items, 
	(pEnumerate)itemlist_Enumerate
};

GUID * ItemListGUIDs[3] = { (GUID *)&IID_IUnknown, (GUID *)&IID_IDispatch, (GUID *)&IID_IItemList };

COMClass ItemListClass = {
	"ItemList",							//name
	(GUID *)0,							//clsid
	2,									//default_iid;
	3,									//iids_count;
	ItemListGUIDs,						//iids
	(GUID *)NULL,						//events_iid
	sizeof(ItemList),					//size
	&ItemListVtbl,						//lpVtbl
	(Structor)NULL,						//constructor
	(Structor)NULL,						//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

ItemList * create_itemlist(unsigned int ParentID)
{
	ItemList * toreturn;
	
	if(toreturn = (ItemList *)ConstructObject(&ItemListClass))
		toreturn->ParentID = ParentID;

	return toreturn;
}

#define NEXT_ITEM(curitem, oNext) (*((unsigned int *)((curitem) + oNext)))

unsigned int itemlist_first_item(ItemList * pThis, unsigned int * oNext)
{
	ClientItem * parentitem;
	unsigned int toreturn;

	if(pThis->ParentID!=0)
	{
		(*oNext) = callibrations->ItemOffsets.oContentsPrevious;
		if(parentitem = item_get_pointers(pThis->ParentID))
			toreturn = *(parentitem->Contents);
	}
	else
	{
		(*oNext) = callibrations->ItemOffsets.oItemNext;
		toreturn = *((unsigned int *)(callibrations->Items));
	}

	return toreturn;
}

HRESULT __stdcall itemlist_Count(ItemList * pThis, int * pCount)
{
	unsigned int curitem, oNext;

	curitem = itemlist_first_item(pThis, &oNext);

	(*pCount) = 0;
	while(curitem)
	{
		(*pCount)++;
		curitem = NEXT_ITEM(curitem, oNext);
	}

	return S_OK;
}

#define ITEM_ID(curitem) (*((unsigned int *)((curitem)+callibrations->ItemOffsets.oID)))

HRESULT __stdcall itemlist_byID(ItemList * pThis, int ID, Item ** pItem)
{
	unsigned int curitem, oNext;

	curitem = itemlist_first_item(pThis, &oNext);

	(*pItem) = NULL;
	while(curitem)
	{
		if(ID == ITEM_ID(curitem))
			break;
		curitem = NEXT_ITEM(curitem, oNext);
	}

	if(curitem)
	{
		(*pItem) = (Item *)GetItemByOffset(curitem);
		return S_OK;
	}
	else
		return S_FALSE;
}

#define ITEM_TYPE(curitem) (*((unsigned int *)((curitem)+callibrations->ItemOffsets.oType)))

HRESULT __stdcall itemlist_byType(ItemList * pThis, int Type, BTItemList ** pItems)
{
	unsigned int curitem, oNext;

	curitem = itemlist_first_item(pThis, &oNext);
	
	(*pItems) = create_bt_itemlist();

	while(curitem)
	{
		if(Type == ITEM_TYPE(curitem))
			bt_itemlist_add((*pItems), GetItemByOffset(curitem));
		curitem = NEXT_ITEM(curitem, oNext);
	}

	return S_OK;
}

HRESULT __stdcall itemlist_Items(ItemList * pThis, int index, Item ** item)
{
	unsigned int curitem, oNext;
	int curidx;

	curitem = itemlist_first_item(pThis, &oNext);

	curidx = 0;
	while( (curitem!=0)
		&& (curidx < index) )
	{
		curidx++;
		curitem = NEXT_ITEM(curitem, oNext);
	}

	if(curitem)
	{
		(*item) = GetItemByOffset(curitem);
		return S_OK;
	}
	else
	{
		(*item) = NULL;
		return S_FALSE;
	}
}

HRESULT __stdcall itemlist_Enumerate(ItemList * pThis, IUnknown ** newenum)
{
	unsigned int curitem, oNext;
	VARIANT * curvar;
	LinkedList * curll;
	IItem ** curobj;

	curitem = itemlist_first_item(pThis, &oNext);
	
	printf("enumerating\n");
	curll = LL_create();
	while(curitem)
	{
		printf("curitem: %x\n", curitem);
		if(curobj = (IItem **)GetItemByOffset(curitem))
		{
			curvar = VDispObject((IDispatch *)curobj);
			(*curobj)->Release((IDispatch *)curobj);
			LL_push(curll, (void *)curvar);
		}
		curitem = NEXT_ITEM(curitem, oNext);
	}
	printf("done enumerating\n");
	
	printf("initiating llenum\n");
	(*newenum) = (IUnknown *)CreateLinkedListEnumerator(curll,
														NULL,
														0,
														2,
														(GUID *)&IID_IEnumVARIANT );

	printf("returning S_OK\n");
	return S_OK;
}

IItemList BTItemListVtbl = {
	DEFAULT_DISPATCH_VTBL,
	(pCount)bt_itemlist_Count, 
	(pbyID)bt_itemlist_byID, 
	(pbyType)bt_itemlist_byType, 
	(pItems)bt_itemlist_Items, 
	(pEnumerate)bt_itemlist_Enumerate
};

COMClass BTItemListClass = {
	"BTItemList",						//name
	(GUID *)0,							//clsid
	2,									//default_iid;
	3,									//iids_count;
	ItemListGUIDs,						//iids
	(GUID *)NULL,						//events_iid
	sizeof(BTItemList),					//size
	&BTItemListVtbl,					//lpVtbl
	(Structor)NULL,						//constructor
	(Structor)NULL,						//destructor
	0,									//typeinfo (init to 0)
	0,									//unused
	0,									//factory (init to 0)
	0,									//activeobject (init to 0)
	0									//instances (init to 0)
};

int btitemlist_compare(unsigned int a, unsigned int b)
{
	if(a>b)
		return +1;
	else if(a<b)
		return -1;
	else
		return 0;
}

BTItemList * create_bt_itemlist()
{
	BTItemList * toreturn;

	if(toreturn=(BTItemList *)ConstructObject(&BTItemListClass))
	{
		toreturn->btByID = BT_create((BTCompare)btitemlist_compare);
		toreturn->btByType = BT_create((BTCompare)btitemlist_compare);
	}

	return toreturn;
}

HRESULT __stdcall bt_itemlist_add(BTItemList * pThis, Item * toadd)
{
	int ID, type;
	LinkedList * llTyped;
	IItem * vtbl = (IItem *)(toadd->header.lpVtbl);

	if( (vtbl->get_Type(toadd, &type)==S_OK)
	  &&(vtbl->get_Type(toadd, &ID)==S_OK)
	  &&(ID!=0) &&(ID!=0xFFFFFFFF)
	  &&(type!=0) )
	{
		BT_insert(pThis->btByID, (void *)ID, toadd);
		if( (llTyped = (LinkedList *)BT_find(pThis->btByType, (void *)type)) == 0 )
		{
			llTyped = LL_create();
			BT_insert(pThis->btByType, (void *)type, (void *)llTyped);
		}
		LL_push(llTyped, (void *)toadd);
		return S_OK;
	}
	else
	{
		vtbl->Release((IDispatch *)toadd);
		return S_FALSE;
	}
}

void bt_itemlist_destructor(BTItemList * pThis)
{
	BinaryTreeEnum * btenum;
	IItem ** curitem;
	LinkedList * curll;

	/* release item references */
	btenum = BT_newenum(pThis->btByID);
	while(curitem = (IItem **)BT_next(btenum))
		(*curitem)->Release((IDispatch *)curitem);
	BT_enumdelete(btenum);

	/* cleanup linked lists within byType tree */
	btenum = BT_newenum(pThis->btByType);
	while(curll = (LinkedList *)BT_next(btenum))
		LL_delete(curll);
	BT_enumdelete(btenum);

	/* cleanup trees */
	BT_delete(pThis->btByID);
	BT_delete(pThis->btByType);

	return;
}

HRESULT __stdcall bt_itemlist_Count(BTItemList * pThis, int * pCount)
{
	(*pCount) = pThis->btByID->itemcount;
	return S_OK;
}

HRESULT __stdcall bt_itemlist_byID(BTItemList * pThis, int ID, Item ** pItem)
{
	if((*pItem) = (Item *)BT_find(pThis->btByID, (void *)ID))
	{
		IItem * Vtbl = (IItem *)((*pItem)->header.lpVtbl);
		Vtbl->AddRef((IDispatch *)(*pItem));
		return S_OK;
	}
	else
		return S_FALSE;
}

HRESULT __stdcall bt_itemlist_byType(BTItemList * pThis, int Type, BTItemList ** pItems)
{
	LinkedList * curll;
	LinkedListEnum * llenum;
	BTItemList * newlist;
	IItem ** curitem;

	newlist = create_bt_itemlist();
	(*pItems) = newlist;
	if(curll = (LinkedList *)BT_find(pThis->btByType, (void *)Type))
	{
		llenum = LL_newenum(curll);
		while(curitem = (IItem **)LL_next(llenum))
		{
			(*curitem)->AddRef((IDispatch *)curitem);
			bt_itemlist_add(newlist, (Item *)curitem);
		}
		LL_enumdelete(llenum);
	}

	return S_OK;
}

HRESULT __stdcall bt_itemlist_Items(BTItemList * pThis, int index, Item ** item)
{
	BinaryTreeEnum * btenum;
	int curidx;
	IItem ** curitem;

	if((index<0)||((unsigned int)index > pThis->btByID->itemcount))
		return S_FALSE;

	btenum = BT_newenum(pThis->btByID);
	for(curidx = 0; curidx <= index; curidx++)
		curitem = (IItem **)BT_next(btenum);
	BT_enumdelete(btenum);
	if(curitem)
	{
		(*item) = (Item *)curitem;
		return S_OK;
	}
	else
		return S_FALSE;
}

HRESULT __stdcall bt_itemlist_Enumerate(BTItemList * pThis, IUnknown ** newenum)
{/* Dump byID tree into a LinkedList of VARIANTs, then return an enumerator for that linkedlist */
	BinaryTreeEnum * btenum;
	LinkedList * newlist;
	VARIANT * newvariant;
	IItem ** curitem;

	newlist = LL_create();

	btenum = BT_newenum(pThis->btByID);
	while(curitem = (IItem **)BT_next(btenum))
	{
		newvariant = VObject((IUnknown *)curitem);
		LL_push(newlist, (void *)newvariant);
	}	
	BT_enumdelete(btenum);

	(*newenum) = (IUnknown *)CreateLinkedListEnumerator(
						newlist,
						NULL,
						0,
						2,
						(GUID *)&IID_IEnumVARIANT
						);
	return S_OK;
}

/* original in-game itemlist can change after conversion to the btitemlist (f.e. item is added
to the contents of a container)... packet events are required to keep the BTItemList syncd */
BTItemList * itemlist_to_bt_itemlist(ItemList * toenumerate)
{
	return NULL;
}
