#ifndef INJECTION_INCLUDED
#define INJECTION_INCLUDED

/*
	Process Injection Code : there are several methods to implement dll or code injection; one of them is implemented here.

	-- Wim Decelle	
*/

#include "Features.h"
#include "Process.h"

//startroutine should have void (*start_routine)(void *) prototype
//	the parameter_buffer is copied to the target process
//	the startroutine gets the pointer to the copied buffer as a parameter
int Inject(Process * onprocess, Thread * onthread, char * dlltoload, char * startroutine, void * parameter_buffer, unsigned int buffersize);

//errors
#define INJECTION_ERROR_BASE 0x12321
#define INJECTION_FAILURE  INJECTION_ERROR_BASE

#endif