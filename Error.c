#include "Error.h"
#include "LinkedList.h"
#include "allocation.h"

/*

  As i'm mostly writing C code, not using the C++ exception handling, it's nice to have some general error-description and -handling code present.
  -- Wim Decelle

*/

LinkedList * ErrorStack=0;
LinkedList * ErrorHandlerStack=0;

void DeleteErrorStack(void * par);
void DeleteErrorHandlerStack(void * par);

void PushErrorHandler(ErrorHandler handler)
{
	if(!ErrorHandlerStack)
		ErrorHandlerStack=LL_create();

	LL_push(ErrorHandlerStack,(void *)handler);

	return;
}

void PopErrorHandler()
{
	if(ErrorHandlerStack&&(ErrorHandlerStack->itemcount))
		LL_pop(ErrorHandlerStack);

	return;
}

Error * CreateError(int id, char * description, void * data, Error * inner)
{
	Error * toreturn;

	toreturn=alloc(sizeof(Error));

	toreturn->id=id;
	toreturn->description=description;
	toreturn->data=data;
	toreturn->inner=inner;

	return toreturn;
}

void PushError(Error * topush)
{
	LinkedListEnum * llenum;
	ErrorHandler curhandler;

	if(!ErrorStack)
		ErrorStack=LL_create();

	LL_push(ErrorStack,(void *)topush);

	if(ErrorHandlerStack)
	{
		llenum=LL_newenum(ErrorHandlerStack);
		curhandler=(ErrorHandler)LL_end(llenum);
		while(curhandler)
		{
			curhandler();
			curhandler=(ErrorHandler)LL_previous(llenum);
		}
		LL_enumdelete(llenum);
	}

	return;
}

Error * PopError()
{
	if(ErrorStack&&(ErrorStack->itemcount))
		return LL_pop(ErrorStack);
	else
		return 0;
}

Error * PeekError()
{
	if(ErrorStack&&(ErrorStack->tail))
		return (Error *)ErrorStack->tail->value;
	else
		return 0;
}

int ErrorCount()
{
	if(ErrorStack)
		return ErrorStack->itemcount;
	else
		return 0;
}

void DeleteError(Error * todelete)
{
	if(todelete->inner)
		DeleteError(todelete);

	clean((void *)todelete);
	
	return;
}

void DeleteErrorHandlerStack(void * par)
{
	if(ErrorHandlerStack)
	{
		LL_delete(ErrorHandlerStack);
		ErrorHandlerStack=0;
	}

	return;
}

void DeleteErrorStack(void * par)
{
	if(ErrorStack)
	{
		while(ErrorStack->itemcount)
			DeleteError((Error *)LL_pop(ErrorStack));
		LL_delete(ErrorStack);
		ErrorStack=0;
	}

	return;
}