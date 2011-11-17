#include "Assembler.h"

/*
	This code does not implement an actual x86 assembler, however it implements serialization methods for some of the
	most often used instructions. Basically i add new instructions or constructs here whenever i need them.
	Huge improvements to this code could(/will) be made if the instruction format used on Intel x86 compatible 
	platforms would(/will) be taken into account.
	-- Wim Decelle
*/

unsigned int asmMovEaxImmediate(Stream ** pstream, unsigned int value)//mov eax, value
{
	SWriteByte(pstream,0xB8);
	SWriteUInt(pstream,value);

	return 5;
}

unsigned int asmMovEaxMemory(Stream ** pstream, unsigned int address)//mov eax, [address]
{
	SWriteByte(pstream, 0xA1);
	SWriteUInt(pstream, address);

	return 5;
}

unsigned int asmMovMemoryEax(Stream ** pstream, unsigned int address)//mov [address], eax
{
	SWriteByte(pstream, 0xA3);
	SWriteUInt(pstream, address);

	return 5;
}

unsigned int asmCallRelative(Stream ** pstream, unsigned int absolute_address)//relative call to the specified function address
{
	int reladdress;

	reladdress=absolute_address - (SGetPos(pstream)+5);

	SWriteByte(pstream, 0xE8);
	SWriteInt(pstream, reladdress);

	return 5;
}

unsigned int asmCallFunctionPointer(Stream ** pstream, unsigned int pointer_address)//call [pointer_address]
{
	SWriteByte(pstream, 0xFF);
	SWriteByte(pstream, 0x15);
	SWriteUInt(pstream, pointer_address);

	return 6;
}

unsigned int asmCallEax(Stream ** pstream)//call [eax]
{
	SWriteByte(pstream,0xFF);
	SWriteByte(pstream, 0xD0);

	return 2;
}

unsigned int asmPushEax(Stream ** pstream)//push eax
{
	SWriteByte(pstream, 0x50);

	return 1;
}

unsigned int asmPushImmediate(Stream ** pstream, unsigned int value)//push value
{
	SWriteByte(pstream, 0x68);
	SWriteUInt(pstream, value);

	return 5;
}

unsigned int asmBackupEsp(Stream ** pstream, unsigned int address)//mov [address], esp
{
	SWriteByte(pstream, 0x89);
	SWriteByte(pstream, 0x25);
	SWriteUInt(pstream, address);

	return 6;
}

unsigned int asmRestoreEsp(Stream ** pstream, unsigned int address)//mov esp, [address]
{
	SWriteByte(pstream, 0x8B);
	SWriteByte(pstream, 0x25);
	SWriteUInt(pstream, address);

	return 6;
}

unsigned int asmDereferEax(Stream ** pstream)//mov eax, [eax]
{
	SWriteByte(pstream, 0x8B);
	SWriteByte(pstream, 0x00);

	return 2;
}

unsigned int asmDereferEaxTable(Stream ** pstream, unsigned int index)//mov eax, [eax+index*4] i think...
{
	SWriteByte(pstream, 0x8B);
	SWriteByte(pstream, 0x80);
	SWriteUInt(pstream, index);

	return 6;
}

unsigned int asmJmpRelative(Stream ** pstream, unsigned int absolute_address)
{
	int reladdress=absolute_address - (SGetPos(pstream)+5);

	SWriteByte(pstream, 0xE9);
	SWriteInt(pstream, reladdress);

	return 5;
}

unsigned int asmJmpImmediate(Stream ** pstream, unsigned int absolute_address)
{
	printf("aa: %x\n", absolute_address);
	SWriteByte(pstream, 0xEA);
	SWriteUInt(pstream, absolute_address);

	return 5;
}

unsigned int asmPushAll(Stream ** pstream)//pusha (used for context switching purposes)
{
	SWriteByte(pstream, 0x60);
	SWriteByte(pstream, 0x9C);

	return 2;
}

unsigned int asmPopAll(Stream ** pstream)//popa (used for context switching purposes)
{
	SWriteByte(pstream, 0x9D);
	SWriteByte(pstream, 0x61);

	return 2;
}

unsigned int asmMovEcxMemory(Stream ** pstream, unsigned int address)//mov ecx, [address]
{
	SWriteByte(pstream, 0x8B);
	SWriteByte(pstream, 0x0D);
	SWriteUInt(pstream, address);

	return 6;
}

unsigned int asmMovEdxMemory(Stream ** pstream, unsigned int address)//mov edx, [address]
{
	SWriteByte(pstream, 0x8B);
	SWriteByte(pstream, 0x15);
	SWriteUInt(pstream, address);

	return 6;
}

unsigned int asmMovEaxEdx(Stream ** pstream)//mov eax, edx
{
	SWriteByte(pstream, 0x8B);
	SWriteByte(pstream, 0xC2);

	return 2;
}

unsigned int asmJzRelativeShort(Stream ** pstream, unsigned int absolute_address)//jz relative short, one byte only is available for the jump-distance, so be carefull
{
	int reladdress;

	reladdress=absolute_address - (SGetPos(pstream)+2);

	SWriteByte(pstream, 0x74);
	SWriteChar(pstream, (char)reladdress);

	return 2;
}

unsigned int asmTestEaxEax(Stream ** pstream)//test eax, eax
{
	SWriteByte(pstream, 0x85);
	SWriteByte(pstream, 0xC0);

	return 2;
}

unsigned int asmRtn(Stream ** pstream)//Rtn
{
	SWriteByte(pstream, 0xC3);

	return 1;
}

unsigned int asmRtnStackSize(Stream ** pstream, unsigned short stacksize)//Rtn stacksize
{
	SWriteByte(pstream, 0xC2);
	SWriteUShort(pstream, stacksize);

	return 3;
}

unsigned int asmAddEdxImmediate(Stream ** pstream, unsigned char toadd)//Add edx, toadd
{
	SWriteByte(pstream, 0x83);
	SWriteByte(pstream, 0xC2);
	SWriteByte(pstream, toadd);

	return 3;
}

unsigned int asmDecEcx(Stream ** pstream)//dec ecx;
{
	SWriteByte(pstream, 0x49);

	return 1;
}

unsigned int asmJnzRelativeShort(Stream ** pstream, unsigned int absolute_address)//jnz relative short (only 1 byte for jump-distance!)
{
	int reladdress;

	reladdress=absolute_address - (SGetPos(pstream)+2);

	SWriteByte(pstream, 0x75);
	SWriteChar(pstream, (char)reladdress);

	return 2;
}

unsigned int asmPopEcx(Stream ** pstream)//pop ecx
{
	SWriteByte(pstream, 0x59);

	return 1;
}

unsigned int asmPopEax(Stream ** pstream)//pop eax
{
	SWriteByte(pstream, 0x58);

	return 1;
}

unsigned int asmPushEcx(Stream ** pstream)//push ecx
{
	SWriteByte(pstream, 0x51);

	return 1;
}