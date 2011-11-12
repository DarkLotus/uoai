#ifndef ASSEMBLER_INCLUDED
#define ASSEMBLER_INCLUDED

#include "Features.h"
#include "Streams.h"

/*
	This code does not implement an actual x86 assembler, however it implements serialization methods for some of the
	most often used instructions. Basically i add new instructions or constructs here whenever i need them.

	Essentially these methods just write the specified instruction to a processstream.
	The current position of the stream must be the actual virtual memory address for relative jmps and calls!
	So ProcessStream or BufferedStream(ProcessStream, 4096) are ok.

	-- Wim Decelle
*/

//size of the instruction is returned each time
unsigned int asmMovEaxImmediate(Stream ** pStream, unsigned int value);//mov eax, value
unsigned int asmMovEaxMemory(Stream ** pstream, unsigned int address);//mov eax, [address]
unsigned int asmMovMemoryEax(Stream ** pstream, unsigned int address);//mov [address], eax
unsigned int asmCallRelative(Stream ** pstream, unsigned int absolute_address);//relative call to the specified function address
unsigned int asmCallFunctionPointer(Stream ** pstream, unsigned int pointer_address);//call [pointer_address]
unsigned int asmCallEax(Stream ** pstream);//call [eax]
unsigned int asmPushEax(Stream ** pstream);//push eax
unsigned int asmPushImmediate(Stream ** pstream, unsigned int value);//push value
unsigned int asmBackupEsp(Stream ** pstream, unsigned int address);//mov [address], esp
unsigned int asmRestoreEsp(Stream ** pstream, unsigned int address);//mov esp, [address]
unsigned int asmDereferEax(Stream ** pstream);//mov eax, [eax]
unsigned int asmDereferEaxTable(Stream ** pstream, unsigned int index);//mov eax, [eax+index*4] i think...
unsigned int asmJmpRelative(Stream ** pstream, unsigned int absolute_address);
unsigned int asmPushAll(Stream ** pstream);//pusha (used for context switching purposes)
unsigned int asmPopAll(Stream ** pstream);//popa (used for context switching purposes)
unsigned int asmMovEcxMemory(Stream ** pstream, unsigned int address);//mov ecx, [address]
unsigned int asmMovEdxMemory(Stream ** pstream, unsigned int address);//mov edx, [address]
unsigned int asmMovEaxEdx(Stream ** pstream);//mov eax, edx
unsigned int asmJzRelativeShort(Stream ** pstream, unsigned int absolute_address);//jz relative short, one byte only is available for the jump-distance, so be carefull
unsigned int asmTestEaxEax(Stream ** pstream);//test eax, eax
unsigned int asmRtn(Stream ** pstream);//Rtn
unsigned int asmRtnStackSize(Stream ** pstream, unsigned short stacksize);//Rtn stacksize
unsigned int asmAddEdxImmediate(Stream ** pstream, unsigned char toadd);//Add edx, toadd
unsigned int asmDecEcx(Stream ** pstream);//dec ecx;
unsigned int asmJnzRelativeShort(Stream ** pstream, unsigned int absolute_address);//jnz relative short (only 1 byte for jump-distance!)

#endif