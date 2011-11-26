#ifndef PACKETS_INCLUDED
#define PACKETS_INCLUDED

//typedef the packet structs here!

#include "LinkedList.h"

typedef enum
{
		OneHanded	 = 0x01,
		TwoHanded	 = 0x02,
		Shoes		 = 0x03,
		Pants		 = 0x04,
		Shirt		 = 0x05,
		Helm		 = 0x06,
		Gloves		 = 0x07,
		Ring		 = 0x08,
		Talisman	 = 0x09,
		Neck		 = 0x0A,
		Hair		 = 0x0B,
		Waist		 = 0x0C,
		InnerTorso	 = 0x0D,
		Bracelet	 = 0x0E,
		Unused		 = 0x0F,
		FacialHair	 = 0x10,
		MiddleTorso	 = 0x11,
		Earrings	 = 0x12,
		Arms		 = 0x13,
		Cloak		 = 0x14,
		Backpack	 = 0x15,
		OuterTorso	 = 0x16,
		OuterLegs	 = 0x17,
		InnerLegs	 = 0x18,
		Mount		 = 0x19,
		ShopBuy		 = 0x1A,
		ShopResale	 = 0x1B,
		ShopSell	 = 0x1C,
		Bank		 = 0x1D
} Layers;

typedef enum
{
	North = 0,
	NorthEast = 1,
	East = 2,
	SouthEast = 3,
	South = 4,
	SouthWest = 5,
	West = 6,
	NorthWest = 7
} Directions;

typedef enum
{
	Regular = 0x00,
	Broadcast = 0x01,
	Emote = 0x02,
	System = 0x06,
	FocusMessage = 0x07,
	Whisper = 0x08,
	Yell = 0x09,
	Spell = 0x0A,
	Guild = 0x0D,
	Alliance = 0x0E,
	Command = 0x0F,
	Encoded = 0xC0
} MessageTypes;

typedef enum
{
	Bold = 0,
	Embossed = 1,
	BoldEmbossed = 2,
	Default = 3,
	Gothic = 4,
	Italic = 5,
	Dark = 6,
	Colorfull = 7,
	Runes = 8,
	Light = 9
} Fonts;

typedef enum
{
	Poisoned = 0x04,
	Movable = 0x20,
	WarMode = 0x40,
	Hidden = 0x80
} Flags;

typedef enum
{
		Alchemy = 0,
		Anatomy = 1,
		AnimalLore = 2,
		ItemID = 3,
		ArmsLore = 4,
		Parry = 5,
		Begging = 6,
		Blacksmith = 7,
		Fletching = 8,
		Peacemaking = 9,
		Camping = 10,
		Carpentry = 11,
		Cartography = 12,
		Cooking = 13,
		DetectHidden = 14,
		Discordance = 15,
		EvalInt = 16,
		Healing = 17,
		Fishing = 18,
		Forensics = 19,
		Herding = 20,
		Hiding = 21,
		Provocation = 22,
		Inscribe = 23,
		Lockpicking = 24,
		Magery = 25,
		MagicResist = 26,
		Tactics = 27,
		Snooping = 28,
		Musicianship = 29,
		Poisoning = 30,
		Archery = 31,
		SpiritSpeak = 32,
		Stealing = 33,
		Tailoring = 34,
		AnimalTaming = 35,
		TasteID = 36,
		Tinkering = 37,
		Tracking = 38,
		Veterinary = 39,
		Swords = 40,
		Macing = 41,
		Fencing = 42,
		Wrestling = 43,
		Lumberjacking = 44,
		Mining = 45,
		Meditation = 46,
		Stealth = 47,
		RemoveTrap = 48,
		Necromancy = 49,
		Focus = 50,
		Chivalry = 51,
		Bushido = 52,
		Ninjitsu = 53,
		Spellweaving = 54
} Skills;

typedef enum
{
	Up = 0,
	Down = 1,
	Locked = 2
} SkillLocks;

typedef enum
{
	HumanMale,
	HumanFemale,
	ElfMale,
	ElfFemale,
	GargoyleMale,
	GargoyleFemale
} BodyTypes;

typedef enum
{
	QueryStats = 0x4,
	QuerySkills = 0x5
} MobileQueryTypes;

typedef enum
{
	NoItemsFollowing = 0,
	ItemsFollowing = 2
} BuyItemsFlags;

typedef struct packet_0_struct
{
	//UNKNOWN(4, 0xed)
	//UNKNOWN(4, 0xff)
	//UNKNOWN(1, 0x00)
	char Name[30];
	unsigned char Parameters[30];//nostring()
	BodyTypes BodyType;//type(unsigned char)
	unsigned char Str;
	unsigned char Dex;
	unsigned char Int;
	Skills Skill1;//type(unsigned char)
	unsigned char Skill1Value;
	Skills Skill2;//type(unsigned char)
	unsigned char SkilL2Value;
	Skills Skill3;//type(unsigned char)
	unsigned char Skill3Value;
	unsigned short SkinColor;
	unsigned short HairStyle;
	unsigned short HairColor;
	unsigned short FacialHairStyle;
	unsigned short FacialHairColor;
	//UNKNOWN(1, 0x00)
	unsigned char StartLocation;
	unsigned int CharacterSlot;
	unsigned int ClientIP;
	unsigned short ShirtColor;
	unsigned short PantsColor;
} CreateCharacter;

typedef struct packet_2_struct
{
	Directions Direction;//type(unsigned char)
	unsigned char SequenceNumber;
	unsigned int FastWalkKey;
} MoveRequest;

typedef struct packet_3_struct
{
	MessageTypes TextType;//type(unsigned char)
	unsigned short Color;
	Fonts Font;//type(unsigned short)
	char * Message;//size(length-8)
} SpeechRequest;

typedef struct packet_5_struct
{
	unsigned int ID;
} Attack;
 
typedef struct packet_6_struct
{
	unsigned int ID;
} DoubleClick;

typedef struct packet_7_struct
{
	unsigned int ID;
	unsigned short Count;
} Drag;

typedef struct packet_9_struct
{
	unsigned int ID;
} Click;

typedef struct packet_12_struct
{
	unsigned char Command;
	unsigned char * Parameters;//size(length-4)
} TextCommand;

typedef struct packet_13_struct
{
	unsigned int ID;
	Layers Layer;//type(unsigned char)
	unsigned int MobileID;
} Equip;

typedef struct packet_34_struct
{
	//UNKNOWN(4, 0xED)
	MobileQueryTypes Type;//type(unsigned char)
	unsigned int ID;
} MobileQuery;

typedef struct packet_3a_struct
{
	Skills SkillNumber;//type(unsigned short)
	SkillLocks Lock;//type(unsigned char)
} ChangeSkillLock;

typedef struct
{
	Layers Layer;//type(unsigned char)
	unsigned int ID;
	unsigned short Amount;
} BuyItemTransaction;

typedef struct packet_3b_struct
{
	unsigned int VendorID;
	BuyItemsFlags Flag;//type(unsigned char)
	LinkedList * ItemList;//type(BuyItemTransaction)
} BuyItems;

typedef struct packet_5d_struct
{
	//UNKNOWN(4, 0xED)
	unsigned char Name[30];
	unsigned char Parameters[30];//nostring()
	unsigned int Slot;
	unsigned int IP;
} SelectCharacter;

typedef enum
{
	TargetItem,
	TargetGround
} TargetKind;

typedef struct packet_6c_struct
{
	TargetKind Kind;//type(unsigned char)
	unsigned int CursorID;
	unsigned char Flags;
	unsigned int TargetID;
	unsigned short TargetX;
	unsigned short TargetY;
	short TargetZ;
	unsigned short TargetType;
} Target;

typedef struct packet_72_struct
{
	unsigned char bWar;//boolean()
	//UNKNOWN(3, 0)
} WarModePacket;

typedef struct packet_73_struct
{
	unsigned char seqnr;
} PingRequest;

typedef struct packet_75_struct
{
	unsigned int ID;
	char newname[30];
} Rename;

typedef struct packet_7d_struct
{
	unsigned int DialogID;
	unsigned int MenuID;
	unsigned int index;
	unsigned int Type;
	unsigned short Color;
} DialogResponse;

typedef struct packet_80_struct
{
	char username[30];
	char password[30];
	//UNKNOWN(1, 0)
} LoginRequest;

typedef struct packet_83_struct
{
	char password[30];
	unsigned int Slot;
	unsigned int IP;
} DeleteCharacter;

typedef struct packet_91_struct
{
	unsigned int hash;
	char username[30];
	char password[30];	
} GameServerLogin;

typedef struct packet_95_struct
{
	unsigned int ID;
	unsigned short Type;
	unsigned short Color;
} HueGumpResponse;

typedef struct packet_98_struct
{
	unsigned int ID;
	char * Name;
} MobileNameQuery;

typedef struct packet_9a_struct
{
	unsigned int ID;
	unsigned int PromptID;
	unsigned int IsResponse;//boolean()
	char * Text;
} ConsolePrompt;

typedef struct
{
	unsigned int ID;
	unsigned short Count;
} SellItemTransaction;

typedef struct packet_9f_struct
{
	unsigned int VendorID;
	unsigned short Count;
	LinkedList * Items;//type(SellItemTransaction)
} SellReply;

typedef struct packet_a0_struct
{
	unsigned short index;
} ServerSelection;

typedef struct
{
	unsigned int Selected;//boolean()
} SwitchEntry;

typedef struct
{
	unsigned int ID;
	unsigned int TextLength;
	unsigned short * Text;//Size(TextLength)
} TextEntry;

typedef struct packet_b1_struct
{
	unsigned int GumpID;
	unsigned int GumpType;
	unsigned int SelectedButton;
	unsigned int SwitchCount;
	LinkedList * Switches;//Type(SwitchEntry)
	unsigned int TextEntryCount;
	LinkedList * TextEntries;//Type(TextEntry)
} GenericGumpResponse;

typedef struct packet_b5_struct
{
	unsigned char Accepted;//boolean()
	unsigned short ChatName[30];
	//UNKNOWN(2, 0)
} ChatRequest;

typedef struct packet_b6_struct
{
	unsigned int ID;
	//UNKNOWN(1, 0)
	char Language[3];
} ItemHelpRequest;

typedef enum
{
	ReadProfile,
	WriteProfile
} ProfileQueryTypes;

typedef struct 
{
	ProfileQueryTypes Type;//Type(unsigned char)
	unsigned int ID;
	//UNKNOWN(2, 1)
	unsigned short ProfileLength;
	unsigned short * ProfileText;//Size(ProfileLength)
} ProfileQuery;

typedef struct
{
	unsigned int ID;
	char * Name;//NullTerminated()
	unsigned short * ProfileTitle;//NullTerminated()
	unsigned short * ProfileText;//NullTerminated()
} ProfileUpdate;

typedef struct packet_b8_struct
{
	union
	{
		ProfileQuery ClientVersion;
		ProfileUpdate ServerVersion;
	} u;
} ProfilePacket;

typedef struct packet_bd_struct
{
	char * versionstring;//Size(Length-3)
} ClientVersion;

typedef enum
{
	ResetFastWalkStack = 0x01,
	CloseGenericGump = 0x04,
	ScreenSize = 0x05,
	PartyCommand = 0x06,
	QuestArrow = 0x07,
	MapChange = 0x08,
	DisarmRequest = 0x09,
	StunRequest = 0x0A,
	Language = 0x0B,
	CloseStatus = 0x0C,
	Animate = 0x0E,
	MobileStatusQuery = 0x0F,
	Properties = 0x10,
	ContextMenuRequest = 0x13,
	ContextMenu = 0x14,
	ContextMenuResponse = 0x15,
	MapPatches = 0x18,
	StatInfo = 0x19,
	StatLockChange = 0x1A,
	NewSpellBookContents = 0x1B,
	CastSpell = 0x1C,
	ClearWeaponAbility = 0x21,
	Damage = 0x22,
	ToggleSpecialAbility = 0x25,
	ConfigureMovement = 0x26,
} Extensions;

typedef struct
{
	unsigned int Key;
} FastWalkKey;

typedef struct packet_bf_1_struct
{
	LinkedList * Stack;//Type(FastWalkKey)
} FastWalkStackExt;

typedef struct packet_bf_4_struct
{
	unsigned int GumpID;
	unsigned int GumpButton;
} CloseGenericGumpExt;

typedef struct packet_bf_5_struct
{
	unsigned int Width;
	unsigned int Height;
} ScreenSizeExt;

typedef enum
{
	AddMember=1,
	RemoveMember=2,
	PrivateMessage=3,
	PublicMessage=4,
	SetCanLoot=6,
	Accept=8,
	Decline=9
} PartyActions;

typedef struct packet_bf_6_1_struct
{
	unsigned int ID;
} PartyAddExt;
typedef struct packet_bf_6_2_struct
{
	unsigned int ID;
} PartyRemoveExt;
typedef struct packet_bf_6_3_struct
{
	unsigned int ID;
	unsigned short * Message;
} PartyPrivateMessageExt;
typedef struct packet_bf_6_4_struct
{
	unsigned short * Message;
} PartyPublicMessageExt;
typedef struct packet_bf_6_6_struct
{
	unsigned char CanLoot;//boolean()
} PartySetCanLootExt;
typedef struct packet_bf_6_8_struct
{
	unsigned int ID;
} PartyAcceptExt;
typedef struct packet_bf_6_9_struct
{
	unsigned int ID;
} PartyDeclineExt;
typedef struct packet_bf_6_struct
{
	PartyActions Action;
} PartyActionsExt;

typedef struct packet_bf_7_struct
{
	unsigned char RightClick;//boolean()
} QuestArrowExt;

typedef enum
{
	Felluca=0,
	Trammel=1,
	Ilshenar=2,
	Malas=3,
	Tokuno=4,
	TerMur=5
} Maps;

typedef struct packet_bf_8_struct
{
	Maps Map;//Type(unsigned char)
} MapChangeExt;

typedef struct packet_bf_b_struct
{
	char Language[4];
} LanguageExt;

typedef struct packet_bf_c_struct
{
	unsigned int ID;
} CloseStatusExt;

typedef struct packet_bf_e_struct
{
	unsigned int Action;
} AnimatePacket;

typedef struct packet_bf_f_struct
{
	//UNKNOWN(1, 0x0A)
	unsigned int ID;
} MobileStatusExt;

typedef struct packet_bf_10_struct
{
	unsigned int ID;
} QueryPropertiesExt;

typedef enum
{
	NoFlags = 0x00,
	Disabled = 0x01,
	Colored = 0x20
} ContextMenuEntryFlags;

typedef struct
{
	unsigned short index;
	unsigned short ID;
	ContextMenuEntryFlags Flags;//Type(unsigned short);Flag(COLORED, 0x20)
	unsigned short Color;//RequireFlag(COLORED)
} ContextMenuEntry;

typedef struct packet_bf_13_struct
{
	unsigned int ID;
} ContextMenuRequestExt;

typedef struct packet_bf_14_struct
{
	//UNKNOWN(2, 1)
	unsigned int ID;
	unsigned char EntryCount;
	LinkedList * Entries;//Type(ContextMenuEntry);Count(EntryCount)
} ContextMenuExt;

typedef struct packet_bf_15_struct
{
	unsigned int ID;
	unsigned short Index;
} ContextMenuResponseExt;

typedef struct
{
	unsigned int StaticBlocks;
	unsigned int LandBlocks;
} MapPatch;

typedef struct packet_bf_18_struct
{
	unsigned int MapCount;
	LinkedList * Patches;//Type(MapPatch)
} MapPatchesEx;

typedef enum
{
	BondedStatus = 0,
	StatLocks = 2
} StatInfoTypes;

typedef struct packet_bf_19_struct
{
	StatInfoTypes Type;//Type(unsigned char)
	unsigned int ID;
	unsigned char Bonded;//boolean()
	unsigned char LockBits;
} StatInfoExt;

typedef struct packet_bf_1a_struct
{
	unsigned char stat;
	unsigned char lockvalue;
} StatLockUpdateExt;

typedef struct packet_bf_1b_struct
{
	//UNKNOWN(2, 1)
	unsigned int ID;
	unsigned short Art;
	unsigned short Offset;
	unsigned char Content[8];//boolean()
} NewSpellbookEntryExt;

typedef struct packet_bf_1c_struct
{
	unsigned short hasSpellbookID;//boolean(),flag(SPELLBOOK_INCLUDED,0xFFFF)
	unsigned int SpellbookID;//RequireFlag(SPELLBOOK_INCLUDED)
	unsigned short Spell;
} CastSpellExt;

typedef struct packet_bf_22_struct
{
	unsigned char DamageType;
	unsigned int ID;
	unsigned char DamageAmount;
} DamageExt;

typedef struct packet_bf_25_struct
{
	unsigned short Ability;
	unsigned char Active;//boolean()
} ToggleAbilityExt;

typedef enum
{
	NoMove = 0,
	Mounted = 1,
	Walking = 2
} MovementSettings;

typedef struct packet_bf_26_struct
{
	MovementSettings Setting;//Type(unsigned char)
} ConfigureMovementExt;

typedef struct packet_bf_struct
{
	Extensions Type;//Type(unsigned short)
} ExtendedPacket;

typedef struct packet_c2_struct
{
	unsigned int ID;
	unsigned int PromptID;
	unsigned int isResponse;//boolean()
	char Language[4];
	unsigned short * Text;
} UnicodePrompt;

typedef struct packet_c8_struct
{
	unsigned char Range;
} UpdateRange;

typedef struct packet_c9_struct
{
	unsigned char Seq;
	unsigned int SentTime;
} RoundTripTime;

typedef struct packet_ca_struct
{
	unsigned char Seq;
	unsigned int SentTime;
} URoundTripTime;

typedef struct packet_cf_struct
{
	char Username[30];
	char Password[30];
} AccountLogin;

typedef struct packet_d1_struct
{
	unsigned char Logout;//boolean()
} LogoutRequest;

typedef struct
{
	unsigned int ID;
} PropertyQueryEntry;

typedef struct packet_d6_struct
{
	LinkedList * Entries;//Type(PropertyQueryEntry)
} QueryProperties;

typedef struct packet_ef_struct
{
	unsigned int Seed;
	unsigned int MajorVersion;
	unsigned int MinorVersion;
	unsigned int Revision;
	unsigned int Patch;
} LoginServerSeed;

//server->client
typedef struct packet_1b_struct
{
	unsigned int ID;
	//UNKNOWN(unsigned int, 0)
	unsigned short Type;
	unsigned short x;
	unsigned short y;
	short z;	
	unsigned char Direction;
	//UNKNOWN(unsigned char, 0)
	//UNKNOWN(unsigned int, 0xFFFFFFFF)
	//UNKNOWN(unsigned short, 0)
	//UNKNOWN(unsigned short, 0)
	unsigned short MapWidth;
	unsigned short MapHeight;
	//UNKNOWN(7, 0)
} LoginConfirm;

#endif