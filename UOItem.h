#ifndef UOITEM_INCLUDED
#define UOITEM_INCLUDED

/*
	UOAI - Item Related Stuff

	-- Wim Decelle
*/

#include "Features.h"

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

typedef struct ItemStruct
{
	ItemVtbl * lpVtbl;
} Item;

#pragma pack(pop)

#endif