#include "Sync.h"

/*
	Any synchronization-related stuff goes here.
	-- Wim Decelle
*/

void LockObject(SyncObject * tolock)
{
	EnterCriticalSection(&(tolock->synclock));
	return;
}

void UnlockObject(SyncObject * tounlock)
{
	LeaveCriticalSection(&(tounlock->synclock));
	return;
}

void InitializeSyncObject(SyncObject * toinit)
{
	InitializeCriticalSection(&(toinit->synclock));
	return;
}

void DeleteSyncObject(SyncObject * todelete)
{
	DeleteCriticalSection(&(todelete->synclock));
	return;
}