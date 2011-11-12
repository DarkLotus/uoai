#ifndef UOCALLIBRATIONS_INCLUDED
#define UOCALLIBRATIONS_INCLUDED

/*
	UOAI - Ultima Online client offset/adress callibrations, i.e. the magic of UOAI
	UOAI obtains global addresses, function pointers and struct/vtbl offsets by disassembling the client at runtime and finding points of interest.
	Disassembly is done instruction-per-instruction or function-per-function, i.e. we don't disassemble the 'whole' client only what we are interested in,
	starting from the entrypoint of the executable. See UOCallibration.c for the magic.
	The advantage of this approach is that UOAI is less sensitive to client-patches; and changes often require only small changes to the callibration code,
	which means that updates of UOAI should be easy when necessary, given the callibration code can be debugged step per step.

	-- Wim Decelle
*/

#include "libdisasm\libdisasm.h"
#include "Streams.h"
#include <windows.h>

#include "UONetwork.h"
#include "UOMacros.h"
#include "UOItem.h"
#include "UOGump.h"
#include "UOJournal.h"

//the unknowns can be set safely to 0 when constructing a custom path
//the client's calculate path returns an incomplete version of this,
//	i.e. it returns the last node of the path and all previous pointers are
//	set correctly to allow reconstruction of the complete path
//	the next pointers are not set yet, so you must loop back to the first path entry and set all next pointers on the flow
//	<- client's InvertPath actuall does that for you and returns the start of the path
//	<- InvertPath assumes the current node is in StartOfPath!
typedef struct PathNodeStruct
{
	unsigned int unknowns[4];
	unsigned int X;
	unsigned int Y;
	int Z;
	unsigned int unknown;
	struct PathNodeStruct * previous;
	struct PathNodeStruct * next;
} PathNode;

typedef int (WINAPI * pWinMain)(HINSTANCE, HINSTANCE, LPSTR pCmdLine, int nCmdShow);
typedef void (* pGeneralPurposeFunction)(int bool_argument);
typedef Item * (* pFindItemByID)(unsigned int ID);
typedef void (__stdcall * pSendPacket)(unsigned char * packet);
typedef void (__stdcall * pBarkGumpConstructor)(Item * speaker, char * text, unsigned int color, unsigned int font, unsigned unknown1, unsigned int unknown2);//THISCALL!!! + unknowns should default to 0 (client uses 0 (almost?) everywhere for the last two parameters)
typedef void * (* pAllocator)(unsigned int size);
typedef void * (* pOpenGump)(Item * item, GumpTypes gumptype, unsigned int unknown1, unsigned int unknown2, int bHasItem);
typedef void (__stdcall * pYesNoGumpCallback)(void);//gets the gump in ecx, typically on no you have to call gump->close!!!
typedef void (__stdcall * pYesNoGumpConstructor)(char * text, pYesNoGumpCallback onYes, pYesNoGumpCallback onNo, unsigned int unknown); 
typedef void (* pOnTarget)(Item * newtarget);
typedef void (__stdcall * pBark)(char * text, unsigned int color, unsigned int font, unsigned int unknown1, unsigned int unknown2);
typedef PathNode * (* pCalculatePath)(unsigned int startx, unsigned int starty, int startz, unsigned int destx, unsigned int desty, int destz, unsigned int max_node_count, int bUnknown);
//The default node limit is 0x1F4 = 500 ~= 22.36 x 22.36 ; which is where the approximate 22 tile limit on pathfinding comes from
//	that is the max_node_count limits the maximal number of nodes to be considered during pathfinding
//	in case the pathfinder would consider all tiles around you (complete NxN block) the default would limit the pathfinder
//	to 22 x 22 tiles; but in effect the pathfinder will only consider the next best tile in each step
//	so pathfinding to slightly further then this 22 tile limit will work, depending on the map layout at the current position
#define DEFAULT_MAX_NODE_COUNT 0x1F4
typedef PathNode * (* pInvertPath)();//invert path pointed to by CurrentPathNode
typedef void (* pShowMessage)(unsigned int color, unsigned int font, char * text);
typedef void (__stdcall * pInitString)(unsigned short * ustring);//ecx = 20byte=5 dword's struct, initialized to 0, -> struct[0]=short * pointer
typedef void (* pGetName)(unsigned int id, void * stringobject, int bUnknown, int bUnknown2);
typedef void (* pGetProperties)(unsigned int id, void * stringobject, int bUnknown, int bUnknown2);
typedef void (__stdcall * pShowGump)(void * parent, int bUnknown);//thiscall gump->Show(parent_gump)

//PathNode structure offsets are very unlikely to change, so it makes more sense to define a struct PathNode with fixed offsets than to callibrate the fieldoffsets.
//	However, to ensure we remember where to find those offsets in case they would change after all, the callibrations were added and tested but removed from the
//	distributed assembly. The general idea is that EXTENDED_CALLBIRATIONS are unnecessary overhead, but might become usefull some day or are at least part
//	of my knowledge of offsets in the client and therefore it makes sense to collect and test that knowledge here. The number of EXTENDED_CALLBIRATIONS is currently
//	limited, but in future more might be added; if so, then I'm probably working on some new features for which require those new offsets.
//	-- Wim Decelle
typedef struct PathNodeOffsetsStruct
{
	unsigned int oPathX;//0x10
	unsigned int oPathY;//0x14
	unsigned int oPathZ;//0x18
	unsigned int oPathPrevious;//0x24
	//unsigned int oPathNext;//0x28
} PathNodeOffsets;

typedef struct UOCallibrationsStruct
{
	//- basic callibrations
	pWinMain WinMain;//client's winmain function, root of all callibrations

	//- winmain callibrations (NetworkObject+Vtbl, PlayerVtbl)
	NetworkObjectClass ** NetworkObject;//far pointer to the client's C++ object used for all network-handling (packet-sending/receiving)
	NetworkObjectClassVtbl * NetworkObjectVtbl;//address of the networkobject's C++ class's virtual function table (list of member functions as function pointers)
	unsigned int pPlayerVtbl;
	int * bHideIntro;

	//- general purpose hook address, for an onTick event/callback
	pGeneralPurposeFunction GeneralPurposeFunction;//function used by the client to perform all kinds of tasks :: sending/receiving packets, pathfinding, following, ... something like the client's timer_on_tick event, but the period is not fixed, this function is called whenever there are no GUI messages to handle.
	unsigned int pShowItemPropGump;

	//- macro callibrations
	//		Macro(MacroList * list, unsigned int idx) function
	//		and if possible other offsets used by the client to manage the currently executing macros and key<->macro couplings
	pMacro Macro;//Macro Function used by the client to execute macros
	unsigned int MacroSwitchTable;//address of the switch table used in the Macro function of the client to switch to specific macros
	MacroList ** CurrentMacro;//current MacroList being executed by the client
	unsigned int ** CurrentMacroIndex;//current index on that macro
	MacroList ** Macros;//linked list of all Macro's configured in the client

	//- information needed to apply a multiclient-patch
	unsigned int MultiClientPatchAddresses[5];//5 locations where a jcc is to be patched into a jmp to enable multiclienting
	unsigned int MultiClientPatchTargets[5];//targets of those jcc's

	//- callibrations from last object macro
	int * bLoggedIn;
	unsigned int * LastObjectID;
	unsigned int * LastObjectType;
	pFindItemByID FindItemByID;
	Item * Player;
	Item ** Items;
	Gump ** Gumps;
	unsigned int oGumpItem;
	pSendPacket SendPacket;
	
	//- callibrations from last skill macro
	unsigned int * LastSkill;

	//- callibrations from last spell macro
	unsigned int * LastSpell;

	//- callibrations from toggle always run macro
	int * bAlwaysRun;
	pAllocator Allocator;//client allocator, using this ensures anything we pass to the client get's cleaned up from the right heap
	unsigned int BarkGumpSize;
	pBarkGumpConstructor BarkGumpConstructor;

	//- callibrations from last target macro
	unsigned char * bTargetCursorVisible;
	unsigned int * LastTargetKind;
	unsigned short * LastTargetTile;
	short * LastTargetZ;
	unsigned short * LastTargetY;
	unsigned short * LastTargetX;
	unsigned int * LastTargetID;

	//- callibrations from open macro
	pOpenGump OpenGump;//essential for all other gump size/constructor callibrations!

	//- callibrations from quit game macro
	unsigned int YesNoGumpSize;
	pYesNoGumpConstructor YesNoGumpConstructor;

	//- encryption patch info is collected here 
	//	-> callibrated from 3 different routines:
	//		NetworkObject->ReceiveAndHandlePackets
	//		NetworkObject->SendBufferedPackets
	//		SendPacket-function (<- buffers packet, later on send from winmain by the NetworkObject->SendBufferedPackets)
	unsigned int EncryptionPatchAddresses[3];
	unsigned int EncryptionPatchTargets[3];

	//- callibrations from NetworkObject->HandlePacket
	unsigned int pGameServerInfo;//has GameServerIP, Port and the current MapNumber (maybe more, don't know...)
	unsigned int * PacketSwitch_offsets;//
	unsigned char * PacketSwitch_indices;//packet-handler is at PacketSwitch_offsets[PacketSwitch_indices[packetnumber-0xB]] (packetnumber=CMD byte)

	//- callibrations from NetworkObject->ReceiveAndHandlePackets
	PacketInfo * Packets;//array of 12 byte descriptors for each packet {size, cmd, char * desc}, with size=0x8000 for dynamically sized packets 

	//- callibrations from SendPacket function
	unsigned int SendPacketHookAddresses[3];//3 calls in send packet are hooked
	unsigned int SendPacketHookTargets[3];
	unsigned int * pLoginCryptKey1;//login keys are either stored in a client location...
	unsigned int * pLoginCryptKey2;
	unsigned int LoginCryptKey1;//... or given as immediate values <- check which ones are zero
	unsigned int LoginCryptKey2;

	//- callibrations from 0x1A packet (NewItem packet)
	_ItemOffsets ItemOffsets;

	//- callibrations from 0x6C packet (Target packet)
	unsigned int * TargetCursorID;
	unsigned int * TargetCursorType;
	unsigned char * bTargetCursorShown;
	pOnTarget * TargetCallback;
	pOnTarget DefaultTargetCallback;

	//- callibrations from 0x38 packet (PathFind packet aka "Drunken Coder Packet" in, MuadDib's packetguide)
	pBark Bark;//Mobile->Say
	pCalculatePath CalculatePath;
	pInvertPath InvertPath;
	PathNode ** CurrentPathNode;
	int * bWalkPath;
	PathNodeOffsets PathOffsets;

	//- callibrations from 0x3A -> Skill packet
	unsigned short * SkillCaps;
	unsigned char * SkillLocks;
	unsigned short * SkillRealValues;
	unsigned short * SkillModifiedValues;

	//- callibrations from 0xD6 -> Properties packet (aka 'Mega Cliloc' packet)
	unsigned int * DraggedItemID;

	//- callibrations from 0x55 -> LoginConfirm packet
	pShowMessage ShowMessage;
	char * WindowName;
	char * ServerName;
	char * CharacterName;
	unsigned int * LoginIPTable;
	unsigned short * LoginPortTable;
	unsigned int * LoginTableIndex;
	unsigned int * LoginIP;
	unsigned short * LoginPort;

	//- callibrations from 0xBF -> GeneralCommand packet (<- we use only the MapChange subpacket (subcmd 8))
	unsigned char * MapNumber;

	//- callibrations from ShowItemPropertyGump
	unsigned short * DefaultNameString;
	unsigned short * DefaultPropertiesString;
	pInitString InitString;
	pGetName GetName;
	pGetProperties GetProperties;

	//- Callibrations from GumpOpener
	//-> gumpconstructors + sizes? <- we rely on generic gumps instead which can be constructed packet-based
	//-> statusgumpconstructor
	//		-< oStatusGump
	//		-< partymembers
	unsigned int * PartyMembers;//array of IDs
	unsigned int PartyMembersLength;//maximum number of entries in PartyMembers (typically 20 on newer and 10 on older clients)
	//-> TextGumpConstructor (journal)
	//		-< JournalStart, JournalOffsets
	JournalEntry ** Journal;

	// callibrated from BarkGumpConstructor
	pShowGump ShowGump;
} UOCallibrations;

int CallibrateClient(unsigned int pid, UOCallibrations * callibrations);

#define CALLIBRATION_ERROR 0x10100101


#endif