#ifndef UOITEM_INCLUDED
#define UOITEM_INCLUDED

/*
	UOAI - Item Related Stuff

	-- Wim Decelle
*/

#include "Features.h"
#include "Tools.h"
#include "COMBase.h"

#pragma pack(push, 1)

typedef struct _ItemOffsetsStruct
{
	//19 first offsets are callibrated from the NewItem-packet handler (0x1A packet)
	unsigned int unknown[4];//don't know (yet) , but these are valid offsets (should be 0x8, 0x10, 0x14 and 0x18) //0 and 0x4 are also valid, but those are the vtbl and a 0xFEEDBEEF members and are not callibrated
	unsigned int oX;//0x24 -> (unsigned short *)
	unsigned int oY;//0x26 -> (unsigned short *)
	unsigned int oZ;//0x28 -> (char *)
	unsigned int oType;//0x38 -> (unsigned short *)
	unsigned int oMultiType;//0x3A -> (unsigned short *)
	unsigned int oTypeIncrement;//0x3C -> (unsigned short *)
	unsigned int oStackCount;//0x3E -> (unsigend char *)
	unsigned int oColor;//0x40 -> (unsigned short * color)
	unsigned int oHighlightColor;//0x42 -> (unsigned short * highlight_color)
	unsigned int oDirection;//0x72 -> (unsigned char  * direction)
	unsigned int oID;//0x80 on < v7 clients, 0x88 on >= v7 clients -> (unsigned int * ID)
	unsigned int oMultiContents;//0xA0 or 0xA8 -> (MultiContents *)
	unsigned int oFlags;//0xA4 or 0xAC -> (unsigned char *)
	unsigned int oContents;//0xC0 or 0xC8 -> Item *
	unsigned int oItemGump;//0xC4 or 0xCC -> Gump *
	
	//other offsets are callibrated from specific member function's on the Player-Mobile's class as found in it's vTbl.
	unsigned int oContentsNext;//pointer to next item in the same container as this item
	unsigned int oContentsPrevious;//pointer to previous item in the same container as this item
	unsigned int oContainer;//pointer to container item
	unsigned int oFirstLayer;//offset of the first layer, each layer is a pointer to an item on that layer or 0
	unsigned int oNotoriety;//(dword *)
	unsigned int oNextFollower;//(Item *) [experimental: i'm not sure if these are followers, but this is a list of items|mobiles which is accessed when the player's notoriety changes]

	//from LastObjectMacroCallibrations
	unsigned int oItemID;//identical to oID
	unsigned int oItemNext;//Item * Next in main itemlist

	//from statusgumpconstructor
	unsigned int oMobileStatus;
	unsigned int oMobileStatus2;
} _ItemOffsets;

typedef void (__stdcall * pDummy)(void);

typedef struct ItemVtblStruct
{
	void (__stdcall * Destructor)(int bDestroy);
	pDummy unknowns1[9];
	int (__stdcall * IsMobile)(void);
	pDummy unknowns2[8];
	short (__stdcall * GetZ)(void);
	pDummy unknowns3[5];
	int (__stdcall * IsMulti)(void);
	void (__stdcall * MoveInContainer)(unsigned short x, unsigned short y, short z);
	void (__stdcall * Move)(unsigned short x, unsigned short y, short z);
	int (__stdcall * unknown4)(void);
	int (__stdcall * IsWorn)(void);//is the item on a mobile's layer?
	pDummy unkowns5[15];
} ItemVtbl;

typedef struct
{
	ItemVtbl * lpVtbl;
	unsigned int * ID;
	unsigned short * Type;
	Point2D * Position;
	char * z;
	unsigned short * MultiType;
	unsigned short * TypeIncrement;
	unsigned char * StackCount;
	unsigned short * Color;
	unsigned short * HighlightColor;
	unsigned char * Direction;
	unsigned int * MultiContents;
	unsigned char * Flags;
	unsigned int * Contents;
	unsigned int * Gump;
	unsigned int * ContentsNext;
	unsigned int * ContentsPrevious;
	unsigned int * Container;
	unsigned int * Layers;
	unsigned int * Notoriety;
	unsigned int * NextFollower;
	unsigned int * Status;
	unsigned int * Status2;
} ClientItem;

#pragma pack(pop)

// {2104F772-4791-4d80-B277-D026BEA08363}
DEFINE_GUID(CLSID_Item, 
0x2104f772, 0x4791, 0x4d80, 0xb2, 0x77, 0xd0, 0x26, 0xbe, 0xa0, 0x83, 0x63);

// {FD02A988-A3E2-4ba2-A4E9-DF2CF90B2748}
DEFINE_GUID(IID_IItem, 
0xfd02a988, 0xa3e2, 0x4ba2, 0xa4, 0xe9, 0xdf, 0x2c, 0xf9, 0xb, 0x27, 0x48);

// {6913EDB4-C045-4788-AB1E-D18321BB2132}
DEFINE_GUID(CLSID_Mobile, 
0x6913edb4, 0xc045, 0x4788, 0xab, 0x1e, 0xd1, 0x83, 0x21, 0xbb, 0x21, 0x32);

// {4B760F4B-FA70-48a9-BAB2-BEB5E00226E2}
DEFINE_GUID(IID_IMobile, 
0x4b760f4b, 0xfa70, 0x48a9, 0xba, 0xb2, 0xbe, 0xb5, 0xe0, 0x2, 0x26, 0xe2);

typedef struct
{
	COMObject header;
	unsigned int itemid;
} Item;

typedef struct
{
	DEFAULT_DISPATCH;
	HRESULT (__stdcall *item_get_Id)(Item * pThis, int * pID);
	HRESULT (__stdcall *item_put_Id)(Item * pThis, int ID);
	HRESULT (__stdcall *item_get_Type)(Item * pThis, int * pType);
	HRESULT (__stdcall *item_put_Type)(Item * pThis, int Type);
	HRESULT (__stdcall *item_get_TypeIncrement)(Item * pThis, int * pTypeIncrement);
	HRESULT (__stdcall *item_put_TypeIncrement)(Item * pThis, int TypeIncrement);
	HRESULT (__stdcall *item_get_Position)(Item * pThis, Point2D * pPos);
	HRESULT (__stdcall *item_put_Position)(Item * pThis, Point2D pos);
	HRESULT (__stdcall *item_get_z)(Item * pThis, int * pPos);
	HRESULT (__stdcall *item_put_z)(Item * pThis, int pos);
	HRESULT (__stdcall *item_get_StackCount)(Item * pThis, int * pCount);
	HRESULT (__stdcall *item_put_StackCount)(Item * pThis, int count);
	HRESULT (__stdcall *item_get_Color)(Item * pThis, int * pColor);
	HRESULT (__stdcall *item_put_Color)(Item * pThis, int color);
	HRESULT (__stdcall *item_get_HighlightColor)(Item * pThis, int * pHighlightColor);
	HRESULT (__stdcall *item_put_HighlightColor)(Item * pThis, int highlightcolor);
	HRESULT (__stdcall *item_get_Direction)(Item * pThis, int * pDirection);
	HRESULT (__stdcall *item_put_Direction)(Item * pThis, int direction);
	HRESULT (__stdcall *item_get_Flags)(Item * pThis, int * pFlags);
	HRESULT (__stdcall *item_put_Flags)(Item * pThis, int Flags);
	HRESULT (__stdcall *item_get_Name)(Item * pThis, BSTR * pName);
	HRESULT (__stdcall *item_put_Name)(Item * pThis, BSTR strName);
	HRESULT (__stdcall *item_get_Properties)(Item * pThis, BSTR * pProperties);
	HRESULT (__stdcall *item_put_Properties)(Item * pThis, BSTR strProperties);
	HRESULT (__stdcall *item_Click)(Item * pThis);
	HRESULT (__stdcall *item_DoubleClick)(Item * pThis);
	HRESULT (__stdcall *item_Drag)(Item * pThis);
} IItemVtbl;

HRESULT __stdcall item_get_Id(Item * pThis, int * pID);
HRESULT __stdcall item_put_Id(Item * pThis, int ID);
HRESULT __stdcall item_get_Type(Item * pThis, int * pType);
HRESULT __stdcall item_put_Type(Item * pThis, int Type);
HRESULT __stdcall item_get_TypeIncrement(Item * pThis, int * pTypeIncrement);
HRESULT __stdcall item_put_TypeIncrement(Item * pThis, int TypeIncrement);
HRESULT __stdcall item_get_Position(Item * pThis, Point2D * pPos);
HRESULT __stdcall item_put_Position(Item * pThis, Point2D pos);
HRESULT __stdcall item_get_z(Item * pThis, int * pPos);
HRESULT __stdcall item_put_z(Item * pThis, int pos);
HRESULT __stdcall item_get_StackCount(Item * pThis, int * pCount);
HRESULT __stdcall item_put_StackCount(Item * pThis, int count);
HRESULT __stdcall item_get_Color(Item * pThis, int * pColor);
HRESULT __stdcall item_put_Color(Item * pThis, int color);
HRESULT __stdcall item_get_HighlightColor(Item * pThis, int * pHighlightColor);
HRESULT __stdcall item_put_HighlightColor(Item * pThis, int highlightcolor);
HRESULT __stdcall item_get_Direction(Item * pThis, int * pDirection);
HRESULT __stdcall item_put_Direction(Item * pThis, int direction);
HRESULT __stdcall item_get_Flags(Item * pThis, int * pFlags);
HRESULT __stdcall item_put_Flags(Item * pThis, int Flags);
HRESULT __stdcall item_get_Name(Item * pThis, BSTR * pName);
HRESULT __stdcall item_put_Name(Item * pThis, BSTR strName);
HRESULT __stdcall item_get_Properties(Item * pThis, BSTR * pProperties);
HRESULT __stdcall item_put_Properties(Item * pThis, BSTR strProperties);
HRESULT __stdcall item_Click(Item * pThis);
HRESULT __stdcall item_DoubleClick(Item * pThis);
HRESULT __stdcall item_Drag(Item * pThis);

Item * GetItemByID(unsigned int ID);
Item * GetItemByOffset(unsigned int offset);

#endif