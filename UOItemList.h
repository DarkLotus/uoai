#ifndef UOITEMLIST_INCLUDED
#define UOITEMLIST_INCLUDED

#include "COMBase.h"
#include "UOItem.h"
#include "BinaryTree.h"

typedef struct IItemList_struct IItemList;

typedef HRESULT (__stdcall * pCount)(IItemList ** pThis, int * pCount);
typedef HRESULT (__stdcall * pbyID)(IItemList ** pThis, int ID, Item ** pItem);
typedef HRESULT (__stdcall * pbyType)(IItemList ** pThis, int Type, IItemList ** pItems);
typedef HRESULT (__stdcall * pItems)(IItemList ** pThis, int index, Item ** item);
typedef HRESULT (__stdcall * pEnumerate)(IItemList ** pThis, IUnknown ** newenum);

typedef struct IItemList_struct
{
	DEFAULT_DISPATCH;
	pCount count;
	pbyID byID;
	pbyType byType;
	pItems Items;
	pEnumerate Enumerate;
} IItemList;

/* ItemList - either the main itemlist or the contents of a container item */
typedef struct
{
	COMObject header;
	unsigned int ParentID;
} ItemList;

ItemList * create_itemlist(unsigned int ParentID);

typedef struct BTItemList_struct BTItemList;

HRESULT __stdcall itemlist_Count(ItemList * pThis, int * pCount);
HRESULT __stdcall itemlist_byID(ItemList * pThis, int ID, Item ** pItem);
HRESULT __stdcall itemlist_byType(ItemList * pThis, int Type, BTItemList ** pItems);
HRESULT __stdcall itemlist_Items(ItemList * pThis, int index, Item ** item);
HRESULT __stdcall itemlist_Enumerate(ItemList * pThis, IUnknown ** newenum);

/* BTItemList - item trees */
typedef struct BTItemList_struct
{
	COMObject header;
	BinaryTree * btByID;
	BinaryTree * btByType;
} BTItemList;

BTItemList * create_bt_itemlist();

void bt_itemlist_destructor(BTItemList * pThis);

HRESULT __stdcall bt_itemlist_Count(BTItemList * pThis, int * pCount);
HRESULT __stdcall bt_itemlist_byID(BTItemList * pThis, int ID, Item ** pItem);
HRESULT __stdcall bt_itemlist_byType(BTItemList * pThis, int Type, BTItemList ** pItems);
HRESULT __stdcall bt_itemlist_Items(BTItemList * pThis, int index, Item ** item);
HRESULT __stdcall bt_itemlist_Enumerate(BTItemList * pThis, IUnknown ** newenum);

HRESULT __stdcall bt_itemlist_add(BTItemList * pThis, Item * toadd);

BTItemList * itemlist_to_bt_itemlist(ItemList * toenumerate);

#endif