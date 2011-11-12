#ifndef UOJOURNAL_INCLUDED
#define UOJOURNAL_INCLUDED

/*
	UOAI -- Journal related stuff

	-- Wim Decelle
*/

#include "Features.h"

typedef struct JournalEntryStruct
{
	char * Text;//0x00/0
	unsigned int Color;//0x04/1
	unsigned int unknowns[4];//0x08/2, 0x0C/3, 0x010/4, 0x014/5
	struct JournalEntryStruct * Next;//0x18/6
	struct JournalEntryStruct * Previous;//0x1C/7	
} JournalEntry;

#endif