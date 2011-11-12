#ifndef UOMACROS_INCLUDED
#define UOMACROS_INCLUDED

/*
	UOAI - macro related stuff

	-- Wim Decelle
*/

#include "Features.h"

//Macro Enumeration:
typedef enum
{
	NullMacro=0,
	Say,
	Emote,
	Whisper,
	Yell,
	Walk,
	ToggleWarPeace,
	Paste,
	Open,
	Close,
	Minimize,
	Maximize,
	OpenDoor,
	UseSkill,
	LastSkill,
	CastSpell,
	LastSpell,
	LastObject,
	Bow,
	Salute,
	QuitGame,
	AllNames,
	LastTarget,
	TargetSelf,
	ToggleArmDisarm,
	WaitForTarget,
	TargetNext,
	AttackLast,
	Delay,
	CircleTrans,
	Command,
	CloseGumps,
	ToggleAlwaysRun,
	SaveDesktop,
	KillGumpOpen,
	PrimaryAbility,
	SecondaryAbility,
	EquipLastWeapon,
	SetUpdateRange,
	ModifyUpdateRange,
	IncreaseUpdateRange,
	DecreaseUpdateRange,
	MaxUpdateRange,
	MinUpdateRange,
	DefaultUpdateRange,
	UpdateRangeInfo,
	EnableRangeColor,
	DisableRangeColor,
	ToggleRangeColor,
	InvokeVirtue,
	SelectNext,
	SelectPrevious,
	SelectNearest,
	AttackSelectedTarget,
	UseSelectedTarget,
	CurrrentTarget,
	ToggleTargetSystemOnOff,
	ToggleBuffIconWindow,
	BandageSelf,
	BandageTarget
} Macros;

#pragma pack(push, 1)

//structures used by the UOML Client to describe macros:
typedef struct MacroStruct
{
	Macros MacroNumber;
	unsigned int IntegerParameter;
	unsigned short * UStringParameter;
	unsigned int Unknown[2];//initialize to 0
} Macro;

typedef struct MacroListStruct
{
	unsigned int VirtualKeyCode;//virtual key code of the key associated with this macrolist
	unsigned int ScanCode;//high word of the scancode corresponding to the above virtual key code
	unsigned int Unknown[3];//initialize to 0
	unsigned int MacroCount;//number of macros in the list (<10)
	Macro Macros[10];//max 10 macros to be executed in sequence
	struct MacroListStruct * Next;//next macrolist in the table of all macros associated with a key
} MacroList;

#pragma pack(pop)

//prototype for macro function used by the client
typedef void (*pMacro)(MacroList * macrolist, unsigned int index);//executes macrolist.Macros[index]

//Tool functions to be added here

#endif