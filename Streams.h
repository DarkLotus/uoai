#ifndef STREAMS_INCLUDED
#define STREAMS_INCLUDED

#include "Features.h"
#include <stdio.h>
#include "Process.h"

/*
	The idea is to define a stream interface here and implement it for different kinds of streams (file, socket, processstream, ...)
	An interface basically means a C struct with function pointers here; where the first parameter is always a far pointer to the interface;
	so essentially we use COM-like interfaces (though there is no IUnknown, etc.)
	-- Wim Decelle
*/

//stream interface
typedef struct StreamStruct
{
	//synchronization
	void (*Lock)(struct StreamStruct ** pThis);
	void (*UnLock)(struct StreamStruct ** pThis);

	//length
	unsigned int (*getLength)(struct StreamStruct ** pThis);
	
	//position
	unsigned int (*getPosition)(struct StreamStruct ** pThis);
	void (*setPosition)(struct StreamStruct ** pThis, unsigned int newposition);
	
	//reading
	unsigned int (*Read)(struct StreamStruct ** pThis, unsigned char * buffer, unsigned int numberofbytes);
	
	//writing
	unsigned int (*Write)(struct StreamStruct ** pThis, unsigned char * towrite, unsigned int length);

	//close
	void (*Close)(struct StreamStruct ** pThis);//after this the stream becomes invalid and can be freed (clean'd)
} Stream;


//stream implementations:

//- FileStream:

typedef struct FileStreamStruct
{
	Stream * fs_interface;
	CRITICAL_SECTION fssync;
	FILE * filepointer;
	int closed;
} FileStream;

unsigned int FSgetLength(FileStream * pThis);
unsigned int FSgetPosition(FileStream * pThis);
void FSsetPosition(FileStream * pThis, unsigned int newposition);
unsigned int FSRead(FileStream * pThis, unsigned char * buffer, unsigned int numberofbytes);
unsigned int FSWrite(FileStream * pThis, unsigned char * towrite, unsigned int length);
void FSClose(FileStream * pThis);

FileStream * OpenFileStream(char * filepath, char * mode);
void DeleteFileStream(FileStream * todelete);

//- BufferStream:

typedef struct BufferStreamStruct
{
	Stream * bs_interface;
	CRITICAL_SECTION bssync;
	unsigned char * buffer;
	unsigned int buffersize;
	unsigned int position;
	int closed;
} BufferStream;

unsigned int BSgetLength(BufferStream * pThis);
unsigned int BSgetPosition(BufferStream * pThis);
void BSsetPosition(BufferStream * pThis, unsigned int newposition);
unsigned int BSRead(BufferStream * pThis, unsigned char * buffer, unsigned int numberofbytes);
unsigned int BSWrite(BufferStream * pThis, unsigned char * towrite, unsigned int length);
void BSClose(BufferStream * pThis);

BufferStream * CreateBufferStream(unsigned char * buffer, unsigned int size);
void DeleteBufferStream(BufferStream * todelete);

//- BufferedStream

typedef struct BufferedStreamStruct
{
	Stream * bs_interface;
	CRITICAL_SECTION bssync;
	unsigned char * buffer;
	unsigned int buffersize;
	unsigned int flushed;
	unsigned int filled;
	unsigned int bufferposition;//position at which this buffer belongs in the actual stream
	unsigned int offset;//offset within this buffer of the current position
	Stream ** basestream;
	int closed;
} BufferedStream;

unsigned int _BSgetLength(BufferedStream * pThis);
unsigned int _BSgetPosition(BufferedStream * pThis);
void _BSsetPosition(BufferedStream * pThis, unsigned int newposition);
unsigned int _BSRead(BufferedStream * pThis, unsigned char * buffer, unsigned int numberofbytes);
unsigned int _BSWrite(BufferedStream * pThis, unsigned char * towrite, unsigned int length);
void _BSClose(BufferedStream * pThis);

void BSFlush(BufferedStream * toflush);//resync the buffer (buffer -> stream)
void BSFill(BufferedStream * tofill);//resync the buffer (stream -> buffer)

BufferedStream * CreateBufferedStream(Stream ** tobuffer, unsigned int buffersize);
void DeleteBufferedStream(BufferedStream * todelete);

//- ProcessStream:

typedef struct ProcessStreamStruct
{
	Stream * ps_interface;
	CRITICAL_SECTION pssync;
	Process * process;
	unsigned int position;
	int closed;
} ProcessStream;

unsigned int PSgetLength(ProcessStream * pThis);
unsigned int PSgetPosition(ProcessStream * pThis);
void PSsetPosition(ProcessStream * pThis, unsigned int newposition);
unsigned int PSRead(ProcessStream * pThis, unsigned char * buffer, unsigned int numberofbytes);
unsigned int PSWrite(ProcessStream * pThis, unsigned char * towrite, unsigned int length);
void PSClose(ProcessStream * pThis);

ProcessStream * CreateProcessStream(Process * ofprocess);
void DeleteProcessStream(ProcessStream * todelete);


//Tools functions for handing any Stream:

//reading built-in types
int SReadInt(Stream ** onstream);
unsigned int SReadUInt(Stream ** onstream);
short SReadShort(Stream ** onstream);
unsigned short SReadUShort(Stream ** onstream);
char SReadChar(Stream ** onstream);
unsigned char SReadByte(Stream ** onstream);

//reading structured types
void * SReadBytes(Stream ** pThis, unsigned int numberofbytes);
unsigned int SRead(Stream ** pThis, void * buffer, unsigned int numberofbytes);

//reading strings
char * SReadString(Stream ** onstream);//0-terminated
short * SReadUString(Stream ** onstream);//00-terminated
char * SReadStringLen(Stream ** onstream, unsigned int length);
short * SReadUStringLen(Stream ** onstream, unsigned int length);

//writing built-in types
unsigned int SWriteInt(Stream ** onstream, int towrite);
unsigned int SWriteUInt(Stream ** onstream, unsigned int towrite);
unsigned int SWriteShort(Stream ** onstream, short towrite);
unsigned int SWriteUShort(Stream ** onstream, unsigned short towrite);
unsigned int SWriteChar(Stream ** onstream, char towrite);
unsigned int SWriteByte(Stream ** onstream, unsigned char towrite);

//writing structured types
unsigned int SWriteBytes(Stream ** pThis, void * towrite, unsigned int numberofbytes);

//writing strings
unsigned int SWriteString(Stream ** onstream, char * towrite);//0-terminated
unsigned int SWriteUString(Stream ** onstream, short * towrite);//00-terminated
unsigned int SWriteStringLen(Stream ** onstream, char * towrite, unsigned int length);
unsigned int SWriteUStringLen(Stream ** onstream, short * towrite, unsigned int length);


//the S* functions assume host byte order, which is annoying when reading from packet streams
//	the following N* funtions do the byte convertion for you -- S as in stream, N as in NetworkStream ;-)

//position handling dummies
unsigned int SGetPos(Stream ** onstream);
void SSetPos(Stream ** onstream, unsigned int newpos);
unsigned int NGetPos(Stream ** onstream);
void NSetPos(Stream ** onstream, unsigned int newpos);

//reading built-in types
int NReadInt(Stream ** onstream);
unsigned int NReadUInt(Stream ** onstream);
short NReadShort(Stream ** onstream);
unsigned short NReadUShort(Stream ** onstream);
char NReadChar(Stream ** onstream);
unsigned char NReadByte(Stream ** onstream);

//reading structured types
void * NReadBytes(Stream ** pThis, unsigned int numberofbytes);
unsigned int NRead(Stream ** pThis, void * buffer, unsigned int numberofbytes);

//reading strings
char * NReadString(Stream ** onstream);//0-terminated
short * NReadUString(Stream ** onstream);//00-terminated
char * NReadStringLen(Stream ** onstream, unsigned int length);
short * NReadUStringLen(Stream ** onstream, unsigned int length);

//writing built-in types
unsigned int NWriteInt(Stream ** onstream, int towrite);
unsigned int NWriteUInt(Stream ** onstream, unsigned int towrite);
unsigned int NWriteShort(Stream ** onstream, short towrite);
unsigned int NWriteUShort(Stream ** onstream, unsigned short towrite);
unsigned int NWriteChar(Stream ** onstream, char towrite);
unsigned int NWriteByte(Stream ** onstream, unsigned char towrite);

//writing structured types
unsigned int NWriteBytes(Stream ** pThis, void * towrite, unsigned int numberofbytes);

//writing strings
unsigned int NWriteString(Stream ** onstream, char * towrite);//0-terminated
unsigned int NWriteUString(Stream ** onstream, short * towrite);//00-terminated
unsigned int NWriteStringLen(Stream ** onstream, char * towrite, unsigned int length);
unsigned int NWriteUStringLen(Stream ** onstream, short * towrite, unsigned int length);

//enumeration :: enumerating a stream as if it is a list of fixed size elements can come in handy from time to time
typedef struct StreamEnumStruct
{
	Stream ** onstream;
	unsigned int blocksize;
	int bSwapByteOrder;
	unsigned int initial_position;
	unsigned int enumsize;
} StreamEnum;

StreamEnum * SNewEnum(Stream ** pThis, unsigned int blocksize, unsigned int enumsize, int bSwapByteOrder);//Enumerate in steps of Size bytes
void SEnumDelete(StreamEnum * todelete);
unsigned char * SNext(StreamEnum * onenum);
unsigned char * SPrevious(StreamEnum * onenum);
void SReset(StreamEnum * onenum);
void SEnd(StreamEnum * onenum);

//ERRORS
#define STREAM_ERROR_BASE 0x100

#define FILESTREAM_ERROR STREAM_ERROR_BASE+1
#define PROCESSSTREAM_ERROR STREAM_ERROR_BASE+2
#define BUFFERSTREAM_ERROR STREAM_ERROR_BASE+3

#endif