#include "Tools.h"

#include <stdio.h>
#include <string.h>

void strupper(char * str)
{
	unsigned int len, i;

	len = strlen(str);
	for(i=0; i<len; i++)
	{
		if( (str[i]>='a') && (str[i] <= 'z') )
			str[i]+=('A'-'a');
	}
}

void sprintip(char * str, unsigned int ip)
{
	unsigned char * pIP = (unsigned char *)&ip;

	sprintf(str, "%u.%u.%u.%u", pIP[3], pIP[2], pIP[1], pIP[0]);

	return;
}