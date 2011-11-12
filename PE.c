#include "PE.h"
#include "Error.h"
#include "allocation.h"
#include "Streams.h"

/*
	(Partial) Parsing of PE Header information. Main use is to read/write import info; either to set hooks or to write
	injectable asm code that uses imported functions (therefore i need the import-addresses from the IAT before i can write/inject the code).

	-- Wim Decelle
 */

PEInfo * _GetPEInfo(Process * onprocess, unsigned int baseaddress, int need_imports, int need_exports);

void DeleteImportedSymbols(LinkedList * symbs)
{
	ImportedSymbol * curisymb;

	while(curisymb=(ImportedSymbol *)LL_pop(symbs))
	{
		if(curisymb->name)
			clean(curisymb->name);
		clean(curisymb);
	}

	return;
}

void DeleteImportedLibs(LinkedList * importedlibs)
{
	ImportedLibrary * curlib;

	while(curlib=(ImportedLibrary *)LL_pop(importedlibs))
	{
		if(curlib->ImportedSymbols)
		{
			DeleteImportedSymbols(curlib->ImportedSymbols);
			LL_delete(curlib->ImportedSymbols);
		}
		if(curlib->libname)
			clean(curlib->libname);
		clean(curlib);
	}

	return;
}

void DeleteExportedSymbols(LinkedList * symbs)
{
	ExportedSymbol * curesymb;

	while(curesymb=(ExportedSymbol *)LL_pop(symbs))
	{
		if(curesymb->Name)
			clean(curesymb->Name);
		clean(curesymb);
	}

	return;
}

typedef struct _ImportDescStruct
{
	unsigned int INT_entry;
	unsigned int IAT_entry;
	unsigned int IAT_address;
} _ImportDesc;

_ImportDesc * _CreateImportDesc(unsigned int INT_Entry)
{
	_ImportDesc * toreturn;

	toreturn=create(_ImportDesc);

	toreturn->IAT_address=0;
	toreturn->IAT_entry=0;
	toreturn->INT_entry=INT_Entry;

	return toreturn;
}

ImportedSymbol * _CreateImportedSymbol(unsigned int ordinal, unsigned int address, char * name, unsigned int IATAddress)
{
	ImportedSymbol * toreturn;

	toreturn=create(ImportedSymbol);
	
	toreturn->Address=address;
	toreturn->IATAddress=IATAddress;
	toreturn->Ordinal=ordinal;
	toreturn->name=name;

	return toreturn;
}

char * _FindExportName(PEInfo * libinfo, unsigned short ordinal)
{
	char * toreturn;
	LinkedListEnum * llenum;
	ExportedSymbol * cursymbol;

	toreturn=0;
	if(libinfo->ExportedSymbols)
	{
		llenum=LL_newenum(libinfo->ExportedSymbols);
		while(cursymbol=LL_next(llenum))
		{
			if(cursymbol->Ordinal==ordinal)
			{
				if(cursymbol->Name)
					toreturn=duplicatestring(cursymbol->Name);
				break;
			}
		}
		LL_enumdelete(llenum);
	}
	
	return toreturn;
}

ImportedLibrary * ParseImportDesc(Stream ** pestream, Process * onprocess, IMAGE_IMPORT_DESCRIPTOR * desc, unsigned int baseaddress)
{
	ImportedLibrary * toreturn;
	LinkedListEnum * llenum;
	LinkedList * imports;

	unsigned int curval;
	_ImportDesc * curimport;
	ImportedSymbol * cursymbol;
	int err_backup;

	char * name;

	unsigned int libbaseaddress;
	PEInfo * libinfo;

	toreturn=create(ImportedLibrary);

	//read library name
	if(desc->Name)
	{
		SSetPos(pestream,baseaddress+desc->Name);
		toreturn->libname=SReadString(pestream);
	}
	else
		toreturn->libname=0;

	//read imports
	//	read INT (Import Name Table)
	
	imports=LL_create();
	
	SSetPos(pestream, baseaddress+desc->OriginalFirstThunk);
	curval=SReadUInt(pestream);
	while(curval!=0)//end is marked by a 0-terminator
	{
		LL_push(imports,(void *)_CreateImportDesc(curval));
		curval=SReadUInt(pestream);
	}

	//	read IAT, there is an entry for each INT entry
	SSetPos(pestream, baseaddress+desc->FirstThunk);
	llenum=LL_newenum(imports);
	while(curimport=(_ImportDesc *)LL_next(llenum))
	{
		curimport->IAT_address=SGetPos(pestream);
		curimport->IAT_entry=SReadUInt(pestream);
	}

	// now build the imports
	toreturn->ImportedSymbols=LL_create();
	LL_reset(llenum);
	while(curimport=(_ImportDesc *)LL_next(llenum))
	{
		if(curimport->INT_entry&0x80000000)//import by ordinal (no name)
			LL_push(toreturn->ImportedSymbols, (void *)_CreateImportedSymbol(curimport->INT_entry-0x80000000,curimport->IAT_entry,0,curimport->IAT_address));
		else//import by name
		{
			SSetPos(pestream, baseaddress+curimport->INT_entry);
			curval=SReadUShort(pestream);//read ordinal hint
			name=SReadString(pestream);//read name
			LL_push(toreturn->ImportedSymbols, (void *)_CreateImportedSymbol(curval, curimport->IAT_entry, name, curimport->IAT_address));
		}
	}

	//clean up
	LL_enumdelete(llenum);
	while(curimport=(_ImportDesc *)LL_pop(imports))
		clean(curimport);
	LL_delete(imports);

	//try to fix missing names
	err_backup=ErrorCount();
	if((toreturn->libname)&&(libbaseaddress=GetModuleBaseAddress(onprocess, toreturn->libname))&&(libinfo=_GetPEInfo(onprocess, libbaseaddress,0,1)))
	{

		llenum=LL_newenum(toreturn->ImportedSymbols);
		while(cursymbol=(ImportedSymbol *)LL_next(llenum))
		{
			if(cursymbol->name==0)
			{
				cursymbol->name=_FindExportName(libinfo, cursymbol->Ordinal);
			}
		}
		LL_enumdelete(llenum);
		DeletePEInfo(libinfo);
	}
	while(err_backup<ErrorCount())//delete all errors from the previous part, it is very well possible that the PEHeaders of the imported library are not accessible (f.e. the library was not actually loaded yet, etc.)
		DeleteError(PopError());

	return toreturn;
}

LinkedList * ParseImports(PEHeaders * headers, Process * onprocess, Stream ** pestream, unsigned int baseaddress)
{
	//idea:
	//- Get The Imports Directory -- IMAGE_DIRECTORY_ENTRY_IMPORT
	//- Read IMAGE_IMPORT_DESCRIPTOR entries 
	//- parse all of them until the originalfirstthunk == 0 (see ParseImportDesc)

	//implementation:

	IMAGE_IMPORT_DESCRIPTOR * libs, * curlib;

	LinkedList * toreturn;
	unsigned int directorysize;

	ImportedLibrary * newlib;

	//open the import directory (IMAGE_DIRECTORY_ENTRY_IMPORT)
	
	if((directorysize=headers->optionalheader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size)>0)
	{
		//allocate array for import descriptors
		libs=(IMAGE_IMPORT_DESCRIPTOR *)alloc(directorysize);

		//read all import descriptors in bulk
		SSetPos(pestream,baseaddress+headers->optionalheader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		if(SRead(pestream,libs,directorysize)!=directorysize)
		{
			clean(libs);
			PushError(CreateError(PE_ERROR,"Failed to read the import directory, do you lack the required permissions!?",0,0));
			return 0;
		}

		toreturn=LL_create();
		
		//loop all import descriptors and load their INT and IAT
		curlib=libs;
		while(curlib->OriginalFirstThunk!=0)
		{
			if((newlib=ParseImportDesc(pestream,onprocess,curlib,baseaddress))==0)
			{
				//failed to parse this library -> cleanup, throw error, return 0
				DeleteImportedLibs(toreturn);
				LL_delete(toreturn);
				clean(libs);
				PushError(CreateError(PE_ERROR,"Failed to parse import descriptor!?",0,PopError()));
				return 0;
			}
			LL_push(toreturn,newlib);
			curlib++;//next library
		}

		//cleanup and return
		clean(libs);
		return toreturn;
	}

	return 0;
}

ExportedSymbol * _CreateExportedSymbol(unsigned short ordinal, unsigned int address, char * name)
{
	ExportedSymbol * toreturn;

	toreturn=create(ExportedSymbol);

	toreturn->Address=address;
	toreturn->Ordinal=ordinal;
	toreturn->Name=name;

	return toreturn;
}

LinkedList * ParseExports(PEHeaders * headers, Stream ** pestream, unsigned int baseaddress)
{
	IMAGE_EXPORT_DIRECTORY exports;
	LinkedList * toreturn;

	char * curname;
	unsigned int curaddress;

	unsigned short * ordinals;
	unsigned int * addresses;

	unsigned int i;

	//a. read IMAGE_EXPORT_DIRECTORY
	if(headers->optionalheader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size>0)
	{
		SSetPos(pestream, baseaddress + headers->optionalheader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		if(SRead(pestream,&exports,sizeof(IMAGE_EXPORT_DIRECTORY))!=sizeof(IMAGE_EXPORT_DIRECTORY))
		{
			PushError(CreateError(PE_ERROR, "Failed to read export directory!?",0,0));
			return 0;
		}

		if(exports.NumberOfNames>0)
		{
			//use the addresses buffer as temporary buffer to store the RVAs to the names
			addresses=(unsigned int *)alloc(sizeof(unsigned int)*exports.NumberOfNames);
			SSetPos(pestream, baseaddress + exports.AddressOfNames);
			if(SRead(pestream,addresses, sizeof(unsigned int)*exports.NumberOfNames)!=(sizeof(unsigned int)*exports.NumberOfNames))
			{
				clean(addresses);
				PushError(CreateError(PE_ERROR, "Failed to read the symbol name RVAs from the export directory!?",0,0));
				return 0;
			}
		
			//read the ordinals
			ordinals=(unsigned short *)alloc(sizeof(unsigned short)*exports.NumberOfNames);
			SSetPos(pestream, baseaddress + exports.AddressOfNameOrdinals);
			if(SRead(pestream,ordinals, sizeof(unsigned short)*exports.NumberOfNames)!=(sizeof(unsigned short)*exports.NumberOfNames))
			{
				clean(addresses);
				clean(ordinals);
				PushError(CreateError(PE_ERROR, "Failed to read the symbol ordinals from the export directory!?",0,0));
				return 0;
			}
		
			//build exported symbol list
			toreturn=LL_create();
			for(i=0; i<exports.NumberOfNames;i++)
			{
				//read name from RVA
				SSetPos(pestream, baseaddress + (addresses[i]));
				curname=SReadString(pestream);
	
				//read function address
				SSetPos(pestream, baseaddress + exports.AddressOfFunctions + ((ordinals[i])*4));
				curaddress=SReadUInt(pestream);
	
				//add this exported symbol
				LL_push(toreturn, (void *)_CreateExportedSymbol((unsigned short)(ordinals[i]+exports.Base),curaddress,curname));
			}
	
			clean(ordinals);
			clean(addresses);
	
			return toreturn;
		}
	}

	return LL_create();//return empty list, at this point there should simply be no exports
}

int ParseHeaders(PEHeaders * headers, Stream ** pestream, unsigned int baseaddress)
{
	headers->baseaddress=baseaddress;

	//- read the IMAGE_DOS_HEADER
	SSetPos(pestream, baseaddress);//dosheader is at baseaddress
	if(SRead(pestream,(void *)&(headers->dosheader),sizeof(IMAGE_DOS_HEADER))!=sizeof(IMAGE_DOS_HEADER))
	{
		PushError(CreateError(PE_ERROR, "Failed to read the DOS HEADER, you might lack the required permissions!?",0,0));
		return 0;
	}

	//- check the dos header's signature
	if(headers->dosheader.e_magic!=IMAGE_DOS_SIGNATURE)
	{
		PushError(CreateError(PE_ERROR, "DOS HEADER was read, but the signature didn't seem right, wrong baseaddress!?",0,0));
		return 0;
	}

	headers->fileheader_address=baseaddress+(headers->dosheader.e_lfanew)+4;

	//- read the IMAGE_FILE_HEADER
	SSetPos(pestream,baseaddress+(headers->dosheader.e_lfanew)+4);//e_lfanew points (RVA) to the IMAGE_FILE_HEADER (actual it points to a (DWORD, IMAGE_FILE_HEADER, IMAGE_OPTIONAL_HEADER) structure; but as sizes can change we read them one by one)
	if(SRead(pestream,&(headers->fileheader),sizeof(IMAGE_FILE_HEADER))!=sizeof(IMAGE_FILE_HEADER))
	{
		PushError(CreateError(PE_ERROR, "Failed to read the FILE HEADER, you might lack the required permissions!?",0,0));
		return 0;
	}

	headers->optionalheader_address=SGetPos(pestream);
	//- read the IMAGE_OPTIONAL_HEADER32 (note that we assume a 32bit PE header, whether this was a correct assumption is checked next
	if(SRead(pestream,&(headers->optionalheader),sizeof(IMAGE_OPTIONAL_HEADER32))!=sizeof(IMAGE_OPTIONAL_HEADER32))
	{
		PushError(CreateError(PE_ERROR, "The OPTIONAL HEADER could not be read, you might lack the required permissions!?",0,0));
		return 0;
	}

	//- check the IMAGE_OPTIONAL_HEADER's magic constant to make sure it was read correctly
	//		we are assuming a 32bit PE module, 64bit and other PE Optional Headers are not supported by this code (yet)!
	if(headers->optionalheader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		PushError(CreateError(PE_ERROR, "The OPTIONAL HEADER's magic constant was incorrect, only 32bit processes are supported!?",0,0));
		return 0;
	}

	return 1;
}

PEInfo * _GetPEInfo(Process * onprocess, unsigned int baseaddress, int need_imports, int need_exports)
{
	PEInfo * toreturn;
	ProcessStream * pstream;
	BufferedStream * bstream;

	//allocate PEInfo structure
	toreturn=create(PEInfo);

	//linked lists are only setup when needed
	toreturn->ImportedLibraries=0;
	toreturn->ExportedSymbols=0;

	//setup ProcessStream and buffer it in sizes of 4096 (page-size; note that we assume 32bit processes here!)
	//	buffering means reading/writing is done in sizes of one page, which has the advantage of needing less
	//	VirtualProtectEx and Read/WriteProcessMemory calls, which are relatively slow
	pstream=CreateProcessStream(onprocess);
	bstream=CreateBufferedStream((Stream **)pstream,4096);
	
	//Parse PE Headers
	toreturn->headers.baseaddress=baseaddress;
	if(!ParseHeaders(&(toreturn->headers), (Stream **)bstream, baseaddress))
	{
		//can't continue without headers
		DeleteBufferedStream(bstream);
		DeleteProcessStream(pstream);
		clean(toreturn);
		PushError(CreateError(PE_ERROR,"Failed to parse PE Headers, can't continue!?",0,PopError()));
		return 0;
	}

	//if imports are needed, try to parse them
	if(need_imports && ((toreturn->ImportedLibraries=ParseImports(&(toreturn->headers),onprocess,(Stream **)bstream,baseaddress))==0))
	{
		//needed imports but failed to parse them, generate error, but still continue
		PushError(CreateError(PE_ERROR, "Failed to parse PE Import Directory!?",0,PopError()));
	}

	//if exports are needed, try to parse them
	if(need_exports && ((toreturn->ExportedSymbols=ParseExports(&(toreturn->headers),(Stream **)bstream,baseaddress))==0))
	{
		//needed exports but failed to parse them, generate error, but still continue
		PushError(CreateError(PE_ERROR, "Failed to parse PE Export Directory!?",0,PopError()));
	}

	//close the streams
	DeleteBufferedStream(bstream);
	DeleteProcessStream(pstream);

	return toreturn;
}

PEInfo * GetPEInfo(Process * ofprocess)
{
	PEInfo * toreturn;
	
	toreturn = _GetPEInfo(ofprocess, GetBaseAddress(ofprocess), 1, 1);

	return toreturn;
}

void DeletePEInfo(PEInfo * todelete)
{
	if(todelete->ImportedLibraries)
	{
		DeleteImportedLibs(todelete->ImportedLibraries);
		LL_delete(todelete->ImportedLibraries);
	}
	if(todelete->ExportedSymbols)
	{
		DeleteExportedSymbols(todelete->ExportedSymbols);
		LL_delete(todelete->ExportedSymbols);
	}
	clean(todelete);
	return;
}
