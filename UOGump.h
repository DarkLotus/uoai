#ifndef UOGUMP_INCLUDED
#define UOGUMP_INCLUDED

/*
	UOAI - gump related stuff

	-- Wim Decelle
*/

#include "Features.h"

typedef void (__stdcall * gumpdummy)(void);

typedef struct GumpVtblStruct
{
	void (__stdcall * Destructor)(int bDestroy);
	gumpdummy Unknowns[6];	
	void (__stdcall * Focus)();
	void (__stdcall * Click)();
	//...
} GumpVtbl;

typedef struct GumpElementVtblStruct
{
	gumpdummy Unknowns[5];
	void (__stdcall * Click)();
	//...
} GumpElementVtbl;

typedef struct GumpStruct
{
	GumpVtbl * lpVtbl;
} Gump;

typedef struct GumpElementStruct
{
	GumpElementVtbl * lpVtbl;
} GumpElement;

typedef struct _GumpOffsetsStruct
{
	unsigned int oGumpName;
	unsigned int oGump;
	unsigned int oGumpWidth;
	unsigned int oGumpHeight;
	unsigned int oGumpX;
	unsigned int oGumpY;
	unsigned int oGumpItem;
	unsigned int oGumpNext;
	unsigned int oGumpPrevious;
	unsigned int oGumpFirstSubGump;
	unsigned int oGumpLastSubGump;
	unsigned int oGumpType;
	unsigned int oGumpClickCallback;
	unsigned int oGumpUnicodeText;
	unsigned int oGumpText;
	unsigned int oGenericGumpID;
	unsigned int oGenericGumpType;
	unsigned int oGenericGumpElements;
	unsigned int oGenericGumpSubElements;
	unsigned int oGenericGumpNoClose;
	unsigned int oGenericGumpNoMove;
	unsigned int oGenericGumpNoDispose;
	unsigned int oGumpElementType;
	unsigned int oGumpElementNext;
	unsigned int oGumpElementID;
	unsigned int oGumpElementSelected;
	unsigned int oGumpElementText;
	unsigned int oGumpElementTooltip;
	unsigned int oGumpElementX;
	unsigned int oGumpElementY;
	unsigned int oGumpElementWidth;
	unsigned int oGumpElementHeight;
	unsigned int oGumpElementReleasedType;
	unsigned int oGumpElementPressedType;
} _GumpOffsets;

typedef enum GumpTypeEnum
{
	Configuration=0x1388,
	Status=0x8,
	Journal=0x7,
	CombatBook=0x2B02,
	Unknown1=0x2B29,
	Overview=0x1392
} GumpTypes;

#endif