#ifndef ONINJECTION_INCLUDED
#define ONINJECTION_INCLUDED

//	Defines the entrypoint that sets up UOAI after being injected into the client.
//
//	-- Wim Decelle

#include "UOCallibrations.h"

typedef struct OnInjectionParametersStruct
{
	int requireadminrights;
	unsigned int pid;
	unsigned int tid;
	unsigned int hwnd;
	UOCallibrations callibrations;
} OnInjectionParameters;

void OnInjection(OnInjectionParameters * parameters);

#endif