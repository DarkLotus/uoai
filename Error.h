#ifndef ERROR_INCLUDED
#define ERROR_INCLUDED

/*

  As i'm mostly writing C code, not using the C++ exception handling, it's nice to have some general error-description and -handling code present.
  
  -- Wim Decelle

*/

#include "Features.h"

typedef struct ErrorStruct
{
	int id;
	char * description;
	void * data;
	struct ErrorStruct * inner;
} Error;

Error * CreateError(int id, char * description, void * data, Error * inner);
void PushError(Error * topush);
Error * PopError();
Error * PeekError();
int ErrorCount();

typedef void (*ErrorHandler)();

void PushErrorHandler(ErrorHandler handler);

void DeleteError(Error * todelete);

#endif