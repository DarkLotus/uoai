#include "Streams.h"
#include "Sync.h"
#include "Error.h"
#include "allocation.h"

/*
	The idea is to define a stream interface here and implement it for different kinds of streams (file, socket, processstream, ...)
	An interface just means a C struct with function pointers here.
	-- Wim Decelle

  //to be added: HTTPGetStream, SubStream, ...?
*/

//- FileStream wraps the stream interface around the <stdio.h> fopen, fread, fwrite, fseek, fclose, fgetpos and fsetpos functions
//- BufferStream just implements a stream interface to a byte buffer (wraps only memcpy)
//- ProcessStream wraps Read- and WriteProcessMemory calls + VirtualProtectEx
//- BufferedStream wraps any other stream, reading and writing only in multiples of a specified size; which might increase performance.
//		f.e. wrapping a ProcessStream in a BufferedStream with size=4096 (pagesize) will ensure memory protection change are needed less often
//		BufferedStream comes with a flush method (void BSFlush(BufferedStream * toflush)) to do the actual writing
//- ...

//vTbls:
Stream IFileStream={
					(void (*)(Stream **))										LockObject, 
					(void (*)(Stream **))										UnlockObject, 
					(unsigned int (*)(Stream **))										FSgetLength, 
					(unsigned int (*)(Stream **))										FSgetPosition, 
					(void (*)(Stream **, unsigned int))								FSsetPosition, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			FSRead, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			FSWrite, 
					(void (*)(Stream **))										FSClose
				};
Stream IBufferStream={
					(void (*)(Stream **))										LockObject, 
					(void (*)(Stream **))										UnlockObject, 
					(unsigned int (*)(Stream **))										BSgetLength, 
					(unsigned int (*)(Stream **))										BSgetPosition, 
					(void (*)(Stream **, unsigned int))								BSsetPosition, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			BSRead, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			BSWrite, 
					(void (*)(Stream **))										BSClose
				};
Stream IProcessStream={
					(void (*)(Stream **))										LockObject, 
					(void (*)(Stream **))										UnlockObject, 
					(unsigned int (*)(Stream **))										PSgetLength, 
					(unsigned int (*)(Stream **))										PSgetPosition, 
					(void (*)(Stream **, unsigned int))								PSsetPosition, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			PSRead, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			PSWrite, 
					(void (*)(Stream **))										PSClose
				};
Stream IBufferedStream={
					(void (*)(Stream **))										LockObject, 
					(void (*)(Stream **))										UnlockObject, 
					(unsigned int (*)(Stream **))										_BSgetLength, 
					(unsigned int (*)(Stream **))										_BSgetPosition, 
					(void (*)(Stream **, unsigned int))								_BSsetPosition, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			_BSRead, 
					(unsigned int (*)(Stream **, unsigned char *, unsigned int))			_BSWrite, 
					(void (*)(Stream **))										_BSClose
				};

//Implementations:
//FileStream implementation (currently doesn't handle any files longer than the maximal 32bit unsigned integer limit correctly)

unsigned int FSgetLength(FileStream * pThis)
{
	fpos_t prevpos, endpos;
	
	fgetpos(pThis->filepointer,&prevpos);
	fseek(pThis->filepointer,0,SEEK_END);
	fgetpos(pThis->filepointer,&endpos);
	fsetpos(pThis->filepointer,&prevpos);

	return (unsigned int)endpos;
}

unsigned int FSgetPosition(FileStream * pThis)
{
	fpos_t pos;//should be an integer type, typically a 64bit one
	if(fgetpos(pThis->filepointer,&pos)==0)
		return (unsigned int)pos;
	PushError(CreateError(FILESTREAM_ERROR,"Failed to obtain the current file position (fgetpos failed)!?",0,0));
	return 0;
}

void FSsetPosition(FileStream * pThis, unsigned int newposition)
{
	if(fseek(pThis->filepointer,newposition,SEEK_SET)!=0)
		PushError(CreateError(FILESTREAM_ERROR,"Failed to set the file position (fseek failed)!?",0,0));
	return;
}

unsigned int FSRead(FileStream * pThis, unsigned char * buffer, unsigned int numberofbytes)
{
	return (unsigned int)fread((void *)buffer,numberofbytes,1,pThis->filepointer)*numberofbytes;
}

unsigned int FSWrite(FileStream * pThis, unsigned char * towrite, unsigned int length)
{
	return fwrite((void *)towrite,length,1,pThis->filepointer)*length;
}

void FSClose(FileStream * pThis)
{
	if(pThis->closed==0)
	{
		pThis->closed=1;
		fclose(pThis->filepointer);
		DeleteSyncObject((SyncObject *)pThis);
	}
	return;
}

FileStream * OpenFileStream(char * filepath, char * mode)
{
	FileStream * toreturn;
	FILE * fpointer;

	if(fpointer=fopen(filepath,mode))
	{
		toreturn=alloc(sizeof(FileStream));
		
		toreturn->closed=0;
		toreturn->fs_interface=&IFileStream;
		toreturn->filepointer=fpointer;
		InitializeSyncObject((SyncObject *)toreturn);

		return toreturn;
	}

	PushError(CreateError(FILESTREAM_ERROR,"Failed to open file in the specified mode!?",0,0));
	return 0;
}

void DeleteFileStream(FileStream * todelete)
{
	FSClose(todelete);
	clean(todelete);
	return;
}


//BufferStream Implementation:
unsigned int BSgetLength(BufferStream * pThis)
{
	return pThis->buffersize;
}

unsigned int BSgetPosition(BufferStream * pThis)
{
	return pThis->position;
}

void BSsetPosition(BufferStream * pThis, unsigned int newposition)
{
	if(newposition<=pThis->buffersize)
		pThis->position=newposition;

	return;
}

unsigned int BSRead(BufferStream * pThis, unsigned char * buffer, unsigned int numberofbytes)
{
	unsigned int bytestoread=((pThis->position+numberofbytes)<=pThis->buffersize)?numberofbytes:(pThis->buffersize-pThis->position);

	if(bytestoread)
	{
		memcpy((void *)buffer, (void *)(pThis->buffer+pThis->position), bytestoread);
		pThis->position+=bytestoread;
	}

	return bytestoread;
}

unsigned int BSWrite(BufferStream * pThis, unsigned char * towrite, unsigned int length)
{
	unsigned int bytestowrite=((pThis->position+length)<=pThis->buffersize)?length:(pThis->buffersize-pThis->position);

	if(bytestowrite)
	{
		memcpy((void *)(pThis->buffer+pThis->position),(void *)towrite,bytestowrite);
		pThis->position+=bytestowrite;
	}

	return bytestowrite;
}

void BSClose(BufferStream * pThis)
{
	if(pThis->closed==0)
	{
		pThis->closed=1;
		DeleteSyncObject((SyncObject *)pThis);
	}
	return;
}

BufferStream * CreateBufferStream(unsigned char * buffer, unsigned int size)
{
	BufferStream * toreturn;

	toreturn=alloc(sizeof(BufferStream));
	
	toreturn->bs_interface=&IBufferStream;
	toreturn->closed=0;
	toreturn->buffer=buffer;
	toreturn->buffersize=size;
	toreturn->position=0;
	InitializeSyncObject((SyncObject *)toreturn);

	return toreturn;
}

void DeleteBufferStream(BufferStream * todelete)
{
	BSClose(todelete);
	clean(todelete);
	return;
}

//BufferedStream Implementation:
unsigned int _BSgetLength(BufferedStream * pThis)
{
	return (unsigned int)(*(pThis->basestream))->getLength(pThis->basestream);
}

unsigned int _BSgetPosition(BufferedStream * pThis)
{
	return (unsigned int)pThis->bufferposition+pThis->offset;
}

void _BSsetPosition(BufferedStream * pThis, unsigned int newposition)
{
	if((newposition<pThis->bufferposition)||(newposition>=(pThis->bufferposition+pThis->buffersize)))
	{
		if(pThis->flushed==0)//anything written to this buffer?
			BSFlush(pThis);//flush the buffer

		pThis->filled=0;//buffer needs to be refilled
		
		//calculate new buffer position
		pThis->bufferposition=(newposition/pThis->buffersize)*pThis->buffersize;
		//move the basestream to where the start of this buffer should be
		(*(pThis->basestream))->setPosition(pThis->basestream,pThis->bufferposition);
	}
	
	pThis->offset=newposition-pThis->bufferposition;
}

unsigned int _BSRead(BufferedStream * pThis, unsigned char * buffer, unsigned int numberofbytes)
{
	unsigned int curpos;
	unsigned int toread;
	unsigned int read;
	unsigned int chunkcount;
	unsigned int restofbuffer;
	unsigned int rest;
	
	curpos=0;

	restofbuffer=pThis->buffersize-pThis->offset;
	if(numberofbytes>restofbuffer)
	{
		//read the remaining bytes in the buffer, if any
		if(restofbuffer)
		{
			toread=restofbuffer;
			if((read=_BSRead(pThis,buffer,toread))!=toread)//updates the position automatically
				return read;
			curpos+=toread;
		}

		//what do we need to read next?
		toread=numberofbytes-restofbuffer;//restofbuffer was already read
		chunkcount=toread/pThis->buffersize;//count the number of complete chunks (no need in buffering this)
		rest=toread%pThis->buffersize;//count the number of bytes in an incomplete chunk (buffer + read)

		//read complete chunks
		if(chunkcount)
		{
			//read all complete chunks directly from the basestream (no buffering required)
			toread=pThis->buffersize*chunkcount;
			if((read=(*(pThis->basestream))->Read(pThis->basestream, buffer+curpos, toread))!=toread)
			{
				_BSsetPosition(pThis,pThis->bufferposition+pThis->offset+read);//safely update the position on failure
				return curpos+read;//return the number of bytes read
			}
			curpos+=toread;

			//update position of the buffered stream
			_BSsetPosition(pThis,pThis->bufferposition+pThis->offset+toread);
		}

		//read the remaining bytes if any
		if(rest)
		{
			if((read=_BSRead(pThis,buffer+curpos,rest))!=rest)//upates position correctly
				return curpos+read;//return the total number of bytes read
			curpos+=rest;
		}

		//everything should be read now
		return curpos;//curpos should be the requested size
	}
	else
	{
		//fill the buffer if required
		if(pThis->filled==0)
			BSFill(pThis);
		
		//check filling
		if(pThis->filled<=pThis->offset)
			return 0;

		//just read and adjust the offset
		//	read at max (pThis->filled-pThis->offset) bytes, the rest was not filled correctly so probably wasn't present in the basestream
		toread=(numberofbytes>(pThis->filled-pThis->offset))?(pThis->filled-pThis->offset):numberofbytes;

		//if anything is to be read, read it
		if(toread)
		{
			memcpy(buffer,pThis->buffer+pThis->offset,toread);

			//update position
			_BSsetPosition(pThis,pThis->bufferposition+pThis->offset+toread);
		}
		
		//return what was read
		return toread;
	}
}

unsigned int _BSWrite(BufferedStream * pThis, unsigned char * buffer, unsigned int numberofbytes)
{
	unsigned int curpos;
	unsigned int towrite;
	unsigned int written;
	unsigned int chunkcount;
	unsigned int restofbuffer;
	unsigned int rest;
	
	curpos=0;

	restofbuffer=pThis->buffersize-pThis->offset;
	if(numberofbytes>restofbuffer)
	{
		//write the remaining bytes in the buffer, if any
		if(restofbuffer)
		{
			towrite=restofbuffer;
			if((written=_BSWrite(pThis,buffer,towrite))!=towrite)//updates the position automatically
				return written;
			curpos+=towrite;
		}

		//what do we need to write next?
		towrite=numberofbytes-restofbuffer;//restofbuffer was already write
		chunkcount=towrite/pThis->buffersize;//count the number of complete chunks (no need in buffering this)
		rest=towrite%pThis->buffersize;//count the number of bytes in an incomplete chunk (buffer + write)

		//write complete chunks
		if(chunkcount)
		{
			//write all complete chunks directly from the basestream (no buffering required)
			towrite=pThis->buffersize*chunkcount;
			if((written=(*(pThis->basestream))->Write(pThis->basestream, buffer+curpos, towrite))!=towrite)
			{
				_BSsetPosition(pThis,pThis->bufferposition+pThis->offset+written);//safely update the position on failure
				return curpos+written;//return the number of bytes write
			}
			curpos+=towrite;

			//update position of the buffered stream
			_BSsetPosition(pThis,pThis->bufferposition+pThis->offset+towrite);
		}

		//write the remaining bytes if any
		if(rest)
		{
			if((written=_BSWrite(pThis,buffer+curpos,rest))!=rest)//upates position correctly
				return curpos+written;//return the total number of bytes write
			curpos+=rest;
		}

		//everything should be written now
		return curpos;//curpos should be the requested size
	}
	else
	{
		//fill the buffer if required before writing
		if(pThis->filled==0)
			BSFill(pThis);

		//if anything is to be read, write it
		if(numberofbytes)
		{
			//write
			memcpy(pThis->buffer+pThis->offset, buffer, numberofbytes);			
			
			//buffer was written to, so needs to be reflushed
			pThis->flushed=0;
			
			//update position
			_BSsetPosition(pThis,pThis->bufferposition+pThis->offset+numberofbytes);
		}
		
		//return what was written
		return numberofbytes;
	}
}

void _BSClose(BufferedStream * pThis)
{
	if(pThis->closed==0)
	{
		pThis->closed=1;
		if(pThis->flushed==0)
			BSFlush(pThis);
		DeleteSyncObject((SyncObject *)pThis);
	}

	return;
}

void BSFlush(BufferedStream * toflush)
{
	//flush and keep track of the amount of bytes flushed
	(*(toflush->basestream))->setPosition(toflush->basestream,toflush->bufferposition);
	toflush->flushed=(*(toflush->basestream))->Write(toflush->basestream,toflush->buffer,toflush->buffersize);
	return;
}

void BSFill(BufferedStream * tofill)
{
	//fill and keep track of the amount of bytes filled
	(*(tofill->basestream))->setPosition(tofill->basestream,tofill->bufferposition);
	tofill->filled=(*(tofill->basestream))->Read(tofill->basestream,tofill->buffer,tofill->buffersize);
	return;
}

BufferedStream * CreateBufferedStream(Stream ** tobuffer, unsigned int buffersize)
{
	BufferedStream * toreturn;

	toreturn=alloc(sizeof(BufferedStream));

	toreturn->basestream=tobuffer;
	toreturn->bs_interface=&IBufferedStream;
	InitializeSyncObject((SyncObject *)toreturn);
	toreturn->buffer=alloc(buffersize);
	toreturn->bufferposition=0;
	toreturn->buffersize=buffersize;
	toreturn->closed=0;
	toreturn->filled=0;
	toreturn->flushed=1;
	toreturn->offset=0;
	
	_BSsetPosition(toreturn,(*tobuffer)->getPosition(tobuffer));//move to the streams current position	

	return toreturn;
}

void DeleteBufferedStream(BufferedStream * todelete)
{
	_BSClose(todelete);
	clean(todelete->buffer);
	clean(todelete);
	return;
}

//ProcessStream Implementation:
unsigned int PSgetLength(ProcessStream * pThis)//Length is not that meaningfull for a process stream, but the module size might be a good estimate
{
	MODULEENTRY32 * pi;
	unsigned int toreturn;

	if(pi=GetProcessInformation(pThis->process))
	{
		toreturn=pi->modBaseSize;
		clean(pi);
		return (unsigned int)toreturn;		
	}
	else
	{
		PushError(CreateError(PROCESSSTREAM_ERROR,"Failed to obtain the process's module size!?",0,0));
		return 0;
	}
}

unsigned int PSgetPosition(ProcessStream * pThis)
{
	return pThis->position;
}

void PSsetPosition(ProcessStream * pThis, unsigned int newposition)
{
	//set without checks!! care from the user is expected!
	pThis->position=newposition;
	return;
}

unsigned int PSRead(ProcessStream * pThis, unsigned char * buffer, unsigned int numberofbytes)
{
	//intensive work here, it's recommended to buffer a process stream page-per-page (create a BufferedStream, size=4096), 
	//	so that memory protection changes are not to be done constantly

	unsigned int positionbackup;
	unsigned int read;//old API headers use a DWORD here, but that's a bug, should be a SIZE_T type, which is just a platform-specific uint (unsigned int = 32bit or 64bit on the relative platforms)
	DWORD prevprotect, temp;
	Error * err;

	err=0;

	positionbackup=pThis->position;

	//try and change protection constants, note the pages have to be commited, so this might fail
	if(!VirtualProtectEx(pThis->process->hProcess,(void *)pThis->position,numberofbytes,PAGE_READWRITE,&prevprotect))
	{
		if(!VirtualProtectEx(pThis->process->hProcess,(void *)pThis->position,numberofbytes,prevprotect|PAGE_READWRITE,&temp))
		{
			PushError(CreateError(PROCESSSTREAM_ERROR,"Failed to change the target page's memory protection to read/write!?",0,0));
			return 0;
		}
	}

	//try to read, might fail for multiple reasons, most likely two are:
	//	- lacking the right privileges to read from the specified process -> you have to ensure sufficient privileges when opening the process handle
	//	- the target memory might be paged out of memory and therefore be unaccessible -> try again
	if(!ReadProcessMemory(pThis->process->hProcess,(void *)pThis->position,(void *)buffer,numberofbytes,&read))
	{
		read=0;
		err=CreateError(PROCESSSTREAM_ERROR, "Failed to read the target process's memory!?",0,err);
	}
	else
		pThis->position+=read;

	//change virtual protection back to the original state
	if(!VirtualProtectEx(pThis->process->hProcess,(void *)positionbackup,numberofbytes,prevprotect,&temp))
		err=CreateError(PROCESSSTREAM_ERROR, "Failed to restore the page's memory protection to it's original value!?",0,err);

	if(err)
		PushError(err);

	return read;
}

unsigned int PSWrite(ProcessStream * pThis, unsigned char * towrite, unsigned int length)
{
	//intensive work here, it's recommended to buffer a process stream page-per-page (create a BufferedStream, size=4096), 
	//	so that memory protection changes are not to be done constantly

	unsigned int positionbackup;
	unsigned int written;
	DWORD prevprotect, temp;
	Error * err;

	err=0;

	positionbackup=pThis->position;

	//try and change protection constants, note the pages have to be commited, so this might fail
	if(!VirtualProtectEx(pThis->process->hProcess,(void *)pThis->position,length,PAGE_READWRITE,&prevprotect))
	{
		if(!VirtualProtectEx(pThis->process->hProcess,(void *)pThis->position,length,prevprotect|PAGE_READWRITE,&temp))
		{
			PushError(CreateError(PROCESSSTREAM_ERROR,"Failed to change the target page's memory protection to read/write!?",0,0));
			return 0;
		}
	}

	//try to write, might fail for multiple reasons, most likely two are:
	//	- lacking the right privileges to read from the specified process -> you have to ensure sufficient privileges when opening the process handle
	//	- the target memory might be paged out of memory and therefore be unaccessible -> try again
	if(!WriteProcessMemory(pThis->process->hProcess,(void *)pThis->position,(void *)towrite,length,&written))
	{
		written=0;
		err=CreateError(PROCESSSTREAM_ERROR, "Failed to write the target process's memory!?",0,err);
	}
	else
		pThis->position+=written;

	//change virtual protection back to the original state
	if(!VirtualProtectEx(pThis->process->hProcess,(void *)positionbackup,length,prevprotect,&temp))
		err=CreateError(PROCESSSTREAM_ERROR, "Failed to restore the page's memory protection to it's original value!?",0,err);

	if(err)
		PushError(err);

	return (unsigned int)written;
}

void PSClose(ProcessStream * pThis)
{
	//process opening/closing is to be done outside of the processstream handling
	// just clean up the sync object here
	if(pThis->closed==0)
	{
		pThis->closed=1;
		DeleteSyncObject((SyncObject *)pThis);
	}
}

ProcessStream * CreateProcessStream(Process * ofprocess)
{
	//Process must have been opened earlier with the required permissions; otherwise read/write might fail
	ProcessStream * toreturn;

	toreturn=alloc(sizeof(ProcessStream));
	toreturn->position=0;
	toreturn->closed=0;
	toreturn->process=ofprocess;
	toreturn->ps_interface=&IProcessStream;
	
	InitializeSyncObject((SyncObject *)toreturn);

	return toreturn;
}

void DeleteProcessStream(ProcessStream * todelete)
{
	PSClose(todelete);
	clean(todelete);
	return;
}


//TOOLS implementation

//reading built-in types (note that signed/unsigned versions are sort of an overkill maybe, but they are here simply for my own convenience; also the char and byte versions do nothing different in S* or N* mode)

void SwapByteOrder(unsigned char * buff, unsigned int size)
{
	unsigned char backup;
	unsigned int i;

	for(i=0;i<(size/2);i++)
	{
		backup=buff[size];
		buff[size]=buff[i];
		buff[i]=backup;
	}

	return;
}

int SReadInt(Stream ** onstream)
{
	int toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(int));

	return toreturn;
}

int NReadInt(Stream ** onstream)
{
	int toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(int));

	SwapByteOrder((unsigned char *)&toreturn,sizeof(int));

	return toreturn;
}

unsigned int SReadUInt(Stream ** onstream)
{
	unsigned int toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(unsigned int));

	return toreturn;
}

unsigned int NReadUInt(Stream ** onstream)
{
	unsigned int toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(unsigned int));

	SwapByteOrder((unsigned char *)&toreturn,sizeof(unsigned int));

	return toreturn;
}

short SReadShort(Stream ** onstream)
{
	short toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(short));

	return toreturn;
}

short NReadShort(Stream ** onstream)
{
	short toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(short));

	SwapByteOrder((unsigned char *)&toreturn,sizeof(short));

	return toreturn;
}

unsigned short SReadUShort(Stream ** onstream)
{
	unsigned short toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(unsigned short));

	return toreturn;
}

unsigned short NReadUShort(Stream ** onstream)
{
	unsigned short toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(unsigned short));

	SwapByteOrder((unsigned char *)&toreturn,sizeof(unsigned short));

	return toreturn;
}

char SReadChar(Stream ** onstream)
{
	char toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(char));

	return toreturn;
}

char NReadChar(Stream ** onstream)
{
	char toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(char));

	return toreturn;
}

unsigned char SReadByte(Stream ** onstream)
{
	unsigned char toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(unsigned char));

	return toreturn;
}

unsigned char NReadByte(Stream ** onstream)
{
	unsigned char toreturn=0;

	(*onstream)->Read(onstream,(unsigned char *)&toreturn,sizeof(unsigned char));

	return toreturn;
}

//reading structured types
void * SReadBytes(Stream ** pThis,unsigned int numberofbytes)
{
	unsigned char * buff;

	buff=alloc(numberofbytes);

	if((*pThis)->Read(pThis,buff,numberofbytes)!=numberofbytes)
	{
		clean(buff);
		return 0;
	}
	
	return (void *)buff;
}

void * NReadBytes(Stream ** pThis, unsigned int numberofbytes)//identical, we don't swap bytes for non-built in types!
{
	unsigned char * buff;

	buff=alloc(numberofbytes);

	if((*pThis)->Read(pThis,buff,numberofbytes)!=numberofbytes)
	{
		clean(buff);
		return 0;
	}
	
	return (void *)buff;
}

unsigned int SRead(Stream ** pThis, void * buffer, unsigned int numberofbytes)
{
	return (*pThis)->Read(pThis, (unsigned char *)buffer, numberofbytes);
}

unsigned int NRead(Stream ** pThis, void * buffer, unsigned int numberofbytes)
{
	return (*pThis)->Read(pThis, (unsigned char *)buffer, numberofbytes);
}

//reading strings
char * SReadString(Stream ** onstream)//0-terminated
{
	char curchar;
	char * toreturn;
	unsigned int prevpos, len;

	//get string length (find null-termination and move back)
	prevpos=(*onstream)->getPosition(onstream);
	while(curchar=SReadChar(onstream))
		;
	len=(*onstream)->getPosition(onstream)-prevpos;
	(*onstream)->setPosition(onstream, prevpos);
	
	//allocate, read and return
	toreturn=alloc(len);
	(*onstream)->Read(onstream, toreturn, len);

	return toreturn;
}

char * NReadString(Stream ** onstream)//0-terminated
{
	char curchar;
	char * toreturn;
	unsigned int prevpos, len;

	//get string length (find null-termination and move back)
	prevpos=(*onstream)->getPosition(onstream);
	while(curchar=NReadChar(onstream))
		;
	len=(*onstream)->getPosition(onstream)-prevpos;
	(*onstream)->setPosition(onstream, prevpos);
	
	//allocate, read and return
	toreturn=alloc(len);
	(*onstream)->Read(onstream, toreturn, len);

	return toreturn;
}

short * SReadUString(Stream ** onstream)//00-terminated
{
	short curchar;
	short * toreturn;
	unsigned int prevpos, len;

	//get string length (find null-termination and move back)
	prevpos=(*onstream)->getPosition(onstream);
	while(curchar=SReadShort(onstream))
		;
	len=(*onstream)->getPosition(onstream)-prevpos;
	(*onstream)->setPosition(onstream, prevpos);
	
	//allocate, read and return
	toreturn=alloc(len);
	(*onstream)->Read(onstream, (unsigned char *)toreturn, len);

	return toreturn;
}

short * NReadUString(Stream ** onstream)//00-terminated
{
	short curchar;
	short * toreturn;
	unsigned int prevpos, len, i;

	//get string length (find null-termination and move back)
	prevpos=(*onstream)->getPosition(onstream);
	while(curchar=NReadShort(onstream))//NOTE THAT THERE IS A BYTEORDERSWAP HERE
		;
	len=(*onstream)->getPosition(onstream)-prevpos;
	(*onstream)->setPosition(onstream, prevpos);
	
	//allocate, read and return
	toreturn=alloc(len);
	//(*onstream)->Read(onstream, toreturn, len); <- this can't be used as it would not swap the short's byte order correctly
	for(i=0;i<(len/2);i++)
		toreturn[i]=NReadShort(onstream);

	return toreturn;
}

char * SReadStringLen(Stream ** onstream, unsigned int length)
{
	char * toreturn;
	
	//allocate, read and return
	toreturn=alloc(length+1);
	(*onstream)->Read(onstream, toreturn, length);
	toreturn[length]='\0';

	return toreturn;
}

char * NReadStringLen(Stream ** onstream, unsigned int length)
{
	char * toreturn;
	
	//allocate, read and return
	toreturn=alloc(length+1);
	(*onstream)->Read(onstream, toreturn, length);
	toreturn[length]='\0';

	return toreturn;
}

short * SReadUStringLen(Stream ** onstream, unsigned int length)
{
	short * toreturn;
	
	//allocate, read and return
	toreturn=alloc((length+1)*2);
	(*onstream)->Read(onstream, (unsigned char *)toreturn, length*2);
	toreturn[length]=0;

	return toreturn;
}

short * NReadUStringLen(Stream ** onstream, unsigned int length)
{
	short * toreturn;
	unsigned int i;
	
	//allocate, read and return
	toreturn=alloc((length+1)*2);
	
	for(i=0;i<length;i++)
		toreturn[i]=NReadShort(onstream);

	toreturn[length]=0;

	return toreturn;
}

//writing built-in types
unsigned int SWriteInt(Stream ** onstream, int towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(int));
}
unsigned int SWriteUInt(Stream ** onstream, unsigned int towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(unsigned int));
}
unsigned int SWriteShort(Stream ** onstream, short towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(short));
}
unsigned int SWriteUShort(Stream ** onstream, unsigned short towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(unsigned short));
}
unsigned int SWriteChar(Stream ** onstream, char towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(char));
}
unsigned int SWriteByte(Stream ** onstream, unsigned char towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(unsigned char));
}
unsigned int NWriteInt(Stream ** onstream, int towrite)
{
	SwapByteOrder((unsigned char *)&towrite,sizeof(int));
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(int));
}
unsigned int NWriteUInt(Stream ** onstream, unsigned int towrite)
{
	SwapByteOrder((unsigned char *)&towrite,sizeof(unsigned int));
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(unsigned int));
}
unsigned int NWriteShort(Stream ** onstream, short towrite)
{
	SwapByteOrder((unsigned char *)&towrite,sizeof(short));
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(short));
}
unsigned int NWriteUShort(Stream ** onstream, unsigned short towrite)
{
	SwapByteOrder((unsigned char *)&towrite,sizeof(unsigned short));
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(unsigned short));
}
unsigned int NWriteChar(Stream ** onstream, char towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(char));
}
unsigned int NWriteByte(Stream ** onstream, unsigned char towrite)
{
	return (*onstream)->Write(onstream,(unsigned char *)&towrite,sizeof(unsigned char));
}

//writing structured types
unsigned int SWriteBytes(Stream ** pThis, void * towrite, unsigned int numberofbytes)
{
	return (*pThis)->Write(pThis,(unsigned char *)towrite,numberofbytes);
}
unsigned int NWriteBytes(Stream ** pThis, void * towrite, unsigned int numberofbytes)
{
	return (*pThis)->Write(pThis,(unsigned char *)towrite,numberofbytes);
}

//dummies
unsigned int _cstrcount(char * cstr)
{
	unsigned int count=0;
	while(cstr[count++])
		;
	return count;
}

unsigned int _ustrcount(short * ustr)
{
	unsigned int count=0;
	while(ustr[count++])
		;
	return count;
}

//writing strings
unsigned int SWriteString(Stream ** onstream, char * towrite)//0-terminated
{
	return (*onstream)->Write(onstream, towrite, _cstrcount(towrite));
}

unsigned int SWriteUString(Stream ** onstream, short * towrite)//00-terminated
{
	return (*onstream)->Write(onstream, (unsigned char *)towrite, _ustrcount(towrite)*2);
}

unsigned int SWriteStringLen(Stream ** onstream, char * towrite, unsigned int length)
{
	return (*onstream)->Write(onstream, towrite, length);
}

unsigned int SWriteUStringLen(Stream ** onstream, short * towrite, unsigned int length)
{
	return (*onstream)->Write(onstream, (unsigned char *)towrite, length*2);
}

unsigned int NWriteString(Stream ** onstream, char * towrite)//0-terminated
{
	return (*onstream)->Write(onstream, towrite, _cstrcount(towrite));
}

unsigned int NWriteUString(Stream ** onstream, short * towrite)//00-terminated
{
	unsigned int size;
	unsigned int i;
	unsigned int written;

	written=0;

	size=_ustrcount(towrite);
	for(i=0;i<size;i++)
		written+=NWriteShort(onstream,towrite[i]);

	return written;
}

unsigned int NWriteStringLen(Stream ** onstream, char * towrite, unsigned int length)
{
	return (*onstream)->Write(onstream, towrite, length);
}

unsigned int NWriteUStringLen(Stream ** onstream, short * towrite, unsigned int length)
{
	unsigned int i;
	unsigned int written;

	written=0;
	
	for(i=0;i<length;i++)
		written+=NWriteShort(onstream,towrite[i]);

	return written;
}

//enumeration
StreamEnum * SNewEnum(Stream ** pThis, unsigned int blocksize, unsigned int enumsize, int bSwapByteOrder)//Enumerate in steps of Size bytes
{
	StreamEnum * newenum;

	newenum=alloc(sizeof(StreamEnum));

	newenum->bSwapByteOrder=bSwapByteOrder;
	newenum->onstream=pThis;
	newenum->blocksize=blocksize;
	newenum->enumsize=enumsize;
	newenum->initial_position=(*pThis)->getPosition(pThis);

	return newenum;
}

void SEnumDelete(StreamEnum * todelete)
{
	clean(todelete);
	return;
}

unsigned char * SNext(StreamEnum * onenum)
{
	unsigned int curpos;

	curpos=(*(onenum->onstream))->getPosition(onenum->onstream);
	if(curpos>=(onenum->initial_position+onenum->enumsize))
		return 0;
	else if(onenum->bSwapByteOrder)
		return NReadBytes(onenum->onstream,onenum->blocksize);
	else
		return SReadBytes(onenum->onstream,onenum->blocksize);
}

unsigned char * SPrevious(StreamEnum * onenum)
{
	unsigned int curpos;

	curpos=(*(onenum->onstream))->getPosition(onenum->onstream);
	if(curpos<(onenum->initial_position+onenum->blocksize))
		return 0;
	else
	{
		(*(onenum->onstream))->setPosition(onenum->onstream,curpos-onenum->blocksize);
		if(onenum->bSwapByteOrder)
			return NReadBytes(onenum->onstream,onenum->blocksize);
		else
			return SReadBytes(onenum->onstream,onenum->blocksize);
	}
}

void SReset(StreamEnum * onenum)
{
	(*(onenum->onstream))->setPosition(onenum->onstream, onenum->initial_position);

	return;
}

void SEnd(StreamEnum * onenum)
{
	(*(onenum->onstream))->setPosition(onenum->onstream,onenum->initial_position+onenum->enumsize);

	return;
}

unsigned int SGetPos(Stream ** onstream)
{
	return (*onstream)->getPosition(onstream);
}

void SSetPos(Stream ** onstream, unsigned int newpos)
{
	(*onstream)->setPosition(onstream, newpos);
	
	return;
}

unsigned int NGetPos(Stream ** onstream)
{
	return (*onstream)->getPosition(onstream);
}

void NSetPos(Stream ** onstream, unsigned int newpos)
{
	(*onstream)->setPosition(onstream, newpos);
	
	return;
}