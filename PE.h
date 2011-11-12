#ifndef PE_INCLUDED
#define PE_INCLUDED

/*
	(Partial) Parsing of PE Header information. Main use is to read/write import info; either to set hooks or to write
	injectable asm code that uses imported functions (therefore i need the import-addresses from the IAT before i can write/inject the code).

	-- Wim Decelle
 */

#include "BinaryTree.h"
#include "Process.h"
#include <windows.h>

typedef struct
{
	unsigned int baseaddress;
	unsigned int fileheader_address;
	unsigned int optionalheader_address;
	IMAGE_DOS_HEADER dosheader;
	IMAGE_FILE_HEADER fileheader;
	IMAGE_OPTIONAL_HEADER32 optionalheader;
	//IMAGE_OPTIONAL_HEADER64 optionalheader64;
} PEHeaders;

typedef struct
{
	PEHeaders headers;//note this contains RVAs and the like, so not all addresses can be used easily
	LinkedList * ImportedLibraries;
	LinkedList * ExportedSymbols;
} PEInfo;

typedef struct
{
	char * name;//might be 0, though an attempt is made to read the name from the loaded libraries PE export directory if it's not in the INT
	unsigned int IATAddress;//address of the IAT address entry (<- overwrite the address at this point when hooking)
	unsigned int Address;//symbol address
	unsigned short Ordinal;//imported symbol's ordinal
} ImportedSymbol;

typedef struct
{
	char * libname;
	LinkedList * ImportedSymbols;
} ImportedLibrary;

typedef struct
{
	unsigned int Address;
	unsigned short Ordinal;
	char * Name;
	//...?
} ExportedSymbol;

PEInfo * _GetPEInfo(Process * onprocess, unsigned int baseaddress, int need_imports, int need_exports);
PEInfo * GetPEInfo(Process * ofprocess);
void DeletePEInfo(PEInfo * todelete);

//ERRORS
#define PE_ERROR_BASE 0x1000
#define PE_ERROR PE_ERROR_BASE+1

#endif