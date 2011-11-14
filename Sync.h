#ifndef SYNC_INCLUDED
#define SYNC_INCLUDED

#include "Features.h"
#include <windows.h>

/*
	Any synchronization-related stuff goes here.
	-- Wim Decelle
*/

//Locking for COM-like interface objects
//	object has to store a CRITICAL_SECTION after it's vTbl though.
typedef struct SyncObjectStruct
{
	void * lpVtbl;
	CRITICAL_SECTION synclock;
} SyncObject;

void LockObject(SyncObject * tolock);
void UnlockObject(SyncObject * tounlock);
void InitializeSyncObject(SyncObject * toinit);
void DeleteSyncObject(SyncObject * todelete);

#endif