#define BUILD_C_TOKENIZER
#ifdef BUILD_C_TOKENIZER

#include "LinkedList.h"
#include <stdio.h>
#include "Tools.h"
#include "Allocation.h"
#include "BinaryTree.h"

/*
	- Minimalistic C header file 'tokenizer'
	currently for the purpose of autogenerating
	packet serializers/deserializers + dispatchonly
	wrappers from commented packet-structs in 
	packets.h.
*/

char * strndup(char * _str, unsigned int n)
{
	char * toreturn;

	toreturn = (char *)alloc(n+1);
	memset(toreturn, 0, n+1);
	memcpy(toreturn, _str, n);
	
	return toreturn;
}

LinkedList * tokenize(char * file)
{
	FILE * headerfile;
	LinkedList * lltokens;
	char buff[256];
	unsigned int curpos;

	lltokens=LL_create();

	if(headerfile = fopen(file, "r"))
	{
		curpos=0;
		while(fread(&buff[curpos], 1, 1, headerfile)>0)
		{
			switch(buff[curpos])
			{
			case ',':
			case ';':
			case '{':
			case '}':
			case '=':
			case '[':
			case ']':
			case '(':
			case ')':
			case '*':
			case '\n':
			case '\r':
				//complete token in action
				if(curpos>0)
				{
					LL_push(lltokens, 
								strndup(buff, curpos));
				}
				//add character as seperate token
				LL_push(lltokens, strndup(&buff[curpos], 1));
				curpos = 0;
				break;
			case ' ':
				//complete token in action
				//complete token in action
				if(curpos>0)
				{
					LL_push(lltokens, 
								strndup(buff, curpos));
				}
				//skip this character
				curpos = 0;
				break;
			case '/':
				//if the second one, insert comment token
				if((curpos>0)&&(buff[curpos-1]=='/'))
				{
					if(curpos>1)
					{
						//flush prepended token
						//complete token in action
						LL_push(lltokens, 
									strndup(buff, curpos-1));
					}
					//add comment token
					LL_push(lltokens,
									strndup(&buff[curpos-1], 2));
					curpos=0;
				}
				else
					curpos++;
				break;
			default:
				curpos++;
				if(curpos==256)
				{
					printf("error, too many characters in one token, flushing\n");
					curpos = 0;
				}
				break;
			}
		}
		fclose(headerfile);
	}

	return lltokens;
}

BinaryTree * typedefs = NULL;
BinaryTree * packettypedefs = NULL;

void parseenumtypedef(LinkedList * lltokens)
{
	return;
}

void newline(LinkedList * lltokens)
{
	char * curtoken;
	while( (curtoken = (char *)LL_dequeue(lltokens))
			&& (strcmp(curtoken, "\n")!=0) )
			clean(curtoken);
	return;
}

void parsestructtypedef(LinkedList * lltokens)
{
	char * curtoken;
	char * aliasname = NULL;
	char * actualname = NULL;

	// check for name
	if( (curtoken = (char *)LL_dequeue(lltokens))
		&& (strcmp(curtoken, "\n")!=0) )
	{
		aliasname = curtoken;
		newline(lltokens);
	}

	// now expect
	//	- "{" or field definition or "} actualname;"
	while(curtoken = (char *)LL_dequeue(lltokens))
	{
		if( (strcmp(curtoken, "{")==0) || (strcmp(curtoken, "\n")==0) )
			clean(curtoken);
		else if(strcmp(curtoken, "}")==0)
		{
			//parse actualname!
			clean(curtoken);
			break;/* done */
		}
		else
		{
			// parse field definition!
			
			// parse till ";"
			// - if the token is a type specifier -> update type
			// - else it is the name
			// - if it is a * -> pointer handling
			// - [ -> parse amount till ]
			
			// parse comments
			// check for "//"
			// parse till newline
			// - token 'boolean' -> skip ()
			// - skip ,; etc
			// - token Type (or type) -> parse between ( and )
			// - Size, Count, ... ?
		}
	}

	return;
}

void parsetypedef(LinkedList * lltokens)
{
	char * curtoken;

	if(curtoken = (char *)LL_dequeue(lltokens))
	{
		if(strcmp(curtoken, "struct")==0)
			parsestructtypedef(lltokens);
		else if(strcmp(curtoken, "enum")==0)
			parseenumtypedef(lltokens);
		clean(curtoken);
	}
}

void parseglobalcomment(LinkedList * lltokens)
{
	char * curtoken;

	while( (curtoken = (char *)LL_dequeue(lltokens))
		&& !(
				(strcmp(curtoken, "\n")==0)
			||	(strcmp(curtoken, "\r")==0)
			)
		 )
	{
		//handle comment tokens here
		clean(curtoken);
	}

	return;
}

void autogenerate_packet_code()
{
	LinkedList * lltokens;
	char * curtoken;
	
	if(lltokens=tokenize("packets.h"))
	{
		while(curtoken = (char *)LL_dequeue(lltokens))
		{
			if(strcmp(curtoken, "//")==0)
				parseglobalcomment(lltokens);
			if(strcmp(curtoken, "typedef")==0)
				parsetypedef(lltokens);
		}
		LL_delete(lltokens);
	}
}

#endif