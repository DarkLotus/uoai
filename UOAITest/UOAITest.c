#include "UOAITest.h"

typedef void (*pDllRegisterServer)();

#include <windows.h>

#include <objbase.h>
#include <OCIdl.h>
#include <Oleauto.h>
#include <olectl.h>

#include <InitGuid.h>

#include <stdio.h>

DEFINE_GUID(CLSID_UOAI, 
0x8a11e500, 0x2101, 0x4a27, 0xbb, 0x16, 0x54, 0x98, 0xc2, 0x8f, 0x91, 0xda);

DEFINE_GUID(IID_IUOAI, 
0xcd10e6be, 0xfdee, 0x4d59, 0xa8, 0x12, 0x6f, 0xd0, 0x8c, 0x2, 0xe8, 0xce);

DEFINE_GUID(CLSID_ClientList, 
0xbe02c37f, 0x111e, 0x402e, 0x9d, 0x5, 0x62, 0x1c, 0x3d, 0x32, 0x87, 0x24);

DEFINE_GUID(IID_IClientList, 
0x39fb3c1b, 0xef1f, 0x4e30, 0xaf, 0x63, 0x83, 0xcb, 0x91, 0xb9, 0xff, 0xa8);

DEFINE_GUID(IID_IClientListEvents, 
0xb327cdfd, 0x2742, 0x42c7, 0xa7, 0x81, 0x50, 0x84, 0xee, 0x6, 0xdc, 0xdc);

#define _CRT_SECURE_NO_WARNINGS

void write_initial_packet_stuff()
{
	unsigned int i;
	FILE * header, * source, * table;
	header = fopen("header.txt", "w");
	source = fopen("source.txt", "w");
	table = fopen("table.txt", "w");
	fprintf(table, "PacketHandlers handlers = {\n");
	for(i=0; i<256; i++)
	{
		//print struct
		fprintf(header, "typedef struct packet_%x_struct packet_%x;\n", i, i);
		//print declarations
		fprintf(header, "packet_%x * deserialize_%x(Stream ** fromstream, unsigned short size);\n", i, i);
		fprintf(header, "unsigned short serialize_%x(Stream ** tostream, packet_%x * toserialize);\n", i, i);
		fprintf(header, "packet_%x * create_%x();\n", i, i);
		fprintf(header, "void destroy_%x(packet_%x * todestroy);\n", i, i);
		fprintf(header, "void printpacket_%x(packet_%x * toprint);\n", i, i);
		fprintf(header, "unsigned int packet_%x_id_of_field(packet_%x * onpacket, char * name);\n", i, i);
		fprintf(header, "VARIANT * packet_%x_get_field_value(packet_%x * onpacket, unsigned int id);\n", i, i);
		fprintf(header, "int packet_%x_set_field_value(packet_%x * onpacket, unsigned int id, VARIANT * fieldvalue);\n", i, i);
		fprintf(header, "\n");
		//print table entry
		fprintf(table, "\t{ // PACKET 0x%x\n", i);
		fprintf(table, "\t\t0x%x,\n",i);//cmd
		fprintf(table, "\t\t\"packet_%x\",\n", i);//strName
		fprintf(table, "\t\tfalse,\n");//bEnabled
		fprintf(table, "\t\t(pSerializePacket)serialize_%x,\n", i);//serialization
		fprintf(table, "\t\t(pDeserializePacket)deserialize_%x,\n", i);//deserialization
		fprintf(table, "\t\t(pCreatePacket)create_%x,\n", i);//intialization
		fprintf(table, "\t\t(pDestroyPacket)destroy_%x,\n", i);//cleanup
		fprintf(table, "\t\t(pPrintPacket)printpacket_%x,\n", i);//debug-print
		fprintf(table, "\t\t(pPacketIdOfField)packet_%x_id_of_field,\n", i);//IDispatch->GetIDOfNames (note that the packet intself is passed... f.e. a 0xBF packet has several sub-types, so the available fields depend on the subtype value
		fprintf(table, "\t\t(pPacketGetField)packet_%x_get_field_value,\n", i);//IDispatch->Invoke (propget)
		fprintf(table, "\t\t(pPacketSetField)packet_%x_set_field_value\n", i);//IDispatch->Invoke (propput)
		if(i!=255)
			fprintf(table, "\t},\n");
		else
			fprintf(table, "\t}\n");
		//print source definitions
		fprintf(source, "\n//PACKET 0x%x\n\n", i);
		fprintf(source, "packet_%x * deserialize_%x(Stream ** fromstream, unsigned short size) { return NULL; }\n\n", i, i);
		fprintf(source, "unsigned short serialize_%x(Stream ** tostream, packet_%x * toserialize) { return 0; }\n\n", i, i);
		fprintf(source, "packet_%x * create_%x() { return NULL; }\n\n", i, i);
		fprintf(source, "void destroy_%x(packet_%x * todestroy) { return; }\n\n", i, i);
		fprintf(source, "void printpacket_%x(packet_%x * toprint) { return; }\n\n", i, i);
		fprintf(source, "unsigned int packet_%x_id_of_field(packet_%x * onpacket, char * name) { return 0; }\n\n", i, i);
		fprintf(source, "VARIANT * packet_%x_get_field_value(packet_%x * onpacket, unsigned int id) { return NULL; }\n\n", i, i);
		fprintf(source, "int packet_%x_set_field_value(packet_%x * onpacket, unsigned int id, VARIANT * fieldvalue) { return 0; }\n\n", i, i);
	}
	fprintf(table, "};\n");
	fclose(header);
	fclose(source);
	fclose(table);
	printf("done\n");
	getchar();

	return;
}

int main()
{
	HMODULE hmod;
	pDllRegisterServer regsvr;
	pDllRegisterServer unregsvr;
	void (*testfunc)();

	//write_initial_packet_stuff();

	if(hmod=LoadLibrary("UOAI.dll"))
	{
		if(testfunc = (void (*)())GetProcAddress(hmod, "testfunc"))
		{
			testfunc();
		}

		if(unregsvr=(pDllRegisterServer)GetProcAddress(hmod, "DllUnregisterServer"))
		{
			unregsvr();
		}

		if(regsvr=(pDllRegisterServer)GetProcAddress(hmod, "DllRegisterServer"))
		{
			regsvr();
		}
	}

	return 0;
}