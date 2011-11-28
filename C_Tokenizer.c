#define BUILD_C_TOKENIZER
#ifdef BUILD_C_TOKENIZER

#include "LinkedList.h"
#include <stdio.h>
#include "Tools.h"
#include "Allocation.h"
#include "BinaryTree.h"

#include "PacketDefs.h"

typedef enum
{
	DEF_ENUM,
	DEF_STRUCT
} PacketDefTypes;

typedef struct
{
	char * name;
	unsigned int value;
} EnumEntry;

typedef enum
{
	DEFAULT_OLE_TYPE,/* map actual type -> OLE type */
	BOOLEAN_OLE_TYPE,/* int -> boolean override */
	ARRAY_OLE_TYPE/* string -> array override */
} OleType;

typedef struct
{
	unsigned int value;
	char * name;
} Flag;

typedef enum
{
	ENUM_type=1,
	BYTE_type=2,
	WORD_type=4,
	DWORD_type=8,
	POINTER_type=16,
	REF_type=32,
	ARRAY_type=64,
	UNSIGNED_type=128,
	LIST_type=256,
	NULL_TERMINATED=512,
	UNKNOWN_type=1024
} TypeSpec;

typedef struct
{
	TypeSpec type_spec;
	char * type_name;//->count + list of fields
	char * field_name;
	TypeSpec read_spec;
	unsigned int size_parameter;
	unsigned int default_value;
	OleType ole_type;
	LinkedList * has_flags;
	LinkedList * needs_flags;
	char * length_field;
} StructEntry;

typedef struct
{
	PacketDefTypes Type;
	char * struct_name;
	char * type_name;
	LinkedList * Contents;
} PacketDef;

/* Struct/Enum typedefs in packets.h are parsed here into a set of typeinfo
	which allows runtime serialization/deserialization and Dispatch-wrapping of
	all packets. So basically protocol-definitions in packets.h are automatically
	parsed to generate all other packet-handling code.
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
			case '\t':
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

void next_line(LinkedList * tokens)
{
	char * toreturn;
	while( ( toreturn = (char *)LL_dequeue(tokens) )
		&& ( strcmp(toreturn, "\n") != 0 ) )
		clean(toreturn);
	if(toreturn)
		clean(toreturn);
	return;
}

char * next_non_newline(LinkedList * tokens)
{
	char * toreturn;
	while( ( toreturn = (char *)LL_dequeue(tokens) )
		&& ( strcmp(toreturn, "\n") == 0 ) )
		clean(toreturn);
	return toreturn;
}

unsigned int cur_enum_val = 0;

unsigned int parse_hex_symbol(char symb)
{
	if( (symb >= 'a')
	  &&(symb <= 'f') )
	{
		return (symb - 'a' + 10);
	}
	else if( (symb >= 'A')
		   &&(symb <= 'F') )
	{
		return (symb - 'A' + 10);
	}
	else if( (symb >= '0')
		   &&(symb <= '9') )
	{
		return (symb - '0');
	}

	return 0;
}

unsigned int parse_int_string(char * string)
{
	unsigned int i, len, val;
	len = strlen(string);
	if(len<=2)
		return atoi(string);
	else if( (string[0] == '0') && (string[1]=='x') )
	{
		/* hex string */
		val = 0;
		for(i=2; i<len; i++)
		{
			val *= 16;
			val += parse_hex_symbol(string[i]);
		}
		return val;
	}
	else
		return atoi(string);
}

int parse_enum_entry(LinkedList * tokens, LinkedList * entrylist)
{
	EnumEntry * newentry;
	char * curtoken;
	
	if(curtoken = next_non_newline(tokens))
	{
		if(strcmp(curtoken, "}")==0)
		{
			clean(curtoken);
			return 0;
		}

		newentry = create(EnumEntry);
		memset(newentry, 0, sizeof(EnumEntry));

		/* name */
		newentry->name = curtoken;
		
		/* =  or ; */
		curtoken = (char *)LL_dequeue(tokens);
		if(curtoken && (strcmp(curtoken, "=")==0))
		{
			clean(curtoken);
			if( curtoken = (char *)LL_dequeue(tokens) )
			{
				newentry->value = parse_int_string(curtoken);
				LL_push(entrylist, (void *)newentry);
				clean(curtoken);
				next_line(tokens);
				return 1;
			}
			else
			{
				next_line(tokens);
				return 0;
			}
		}
		else if(curtoken && (strcmp(curtoken, ",")==0))
		{
			clean(curtoken);
			newentry->value = cur_enum_val++;
			LL_push(entrylist, (void *)newentry);
			next_line(tokens);
			return 1;
		}
		else if(curtoken && (strcmp(curtoken, "\n")==0))
		{
			clean(curtoken);
			newentry->value = cur_enum_val++;
			LL_push(entrylist, (void *)newentry);
			return 1;
		}
		else // shouldnt happen
		{
			clean(newentry->name);
			clean(newentry);
			if(curtoken)
				clean(curtoken);
			next_line(tokens);
			return 0;
		}
	}

	return 0;
}

int parse_type_spec(char * strspec, unsigned int * pTypeSpec)
{
	if(strcmp(strspec, "unsigned")==0)
		(*pTypeSpec)|=UNSIGNED_type;
	else if(strcmp(strspec, "*")==0)
		(*pTypeSpec)|=POINTER_type;
	else if(strcmp(strspec, "int")==0)
		(*pTypeSpec)|=DWORD_type;
	else if(strcmp(strspec, "short")==0)
		(*pTypeSpec)|=WORD_type;
	else if(strcmp(strspec, "char")==0)
		(*pTypeSpec)|=BYTE_type;
	else if(strcmp(strspec, "LinkedList")==0)
		(*pTypeSpec)|=LIST_type;
	else
		return 0;
	return 1;
}

int strcmp_no_case(char * a, char * b)
{
	strupper(a);
	return strcmp(a, b);
}

int parse_struct_entry(LinkedList * tokens, LinkedList * entrylist)
{
	char * curtoken;
	StructEntry * se;

	se = create(StructEntry);
	memset(se, 0, sizeof(StructEntry));
	se->needs_flags = LL_create();
	se->has_flags = LL_create();

	// typename
	while( 
			(curtoken = (char *)LL_dequeue(tokens))
		&&	(parse_type_spec(curtoken, (unsigned int *)&(se->type_spec)))
		)
		clean(curtoken);

	if(strcmp(curtoken, "//")==0)
	{
		/* UNKNOWN(count, value) handling */
		clean(curtoken);
		curtoken = (char *)LL_dequeue(tokens);
		if(strcmp(curtoken, "UNKNOWN")!=0)
		{
			printf("expected UKNOWN, got %s!\n", curtoken);
			getchar();
			clean(curtoken);
			next_line(tokens);
			return 1;
		}
		clean(curtoken);
		curtoken = (char *)LL_dequeue(tokens);
		clean(curtoken);
		curtoken = (char *)LL_dequeue(tokens);
		se->size_parameter = parse_int_string(curtoken);
		clean(curtoken);
		curtoken = (char *)LL_dequeue(tokens);
		clean(curtoken);
		curtoken = (char *)LL_dequeue(tokens);
		se->default_value = parse_int_string(curtoken);
		clean(curtoken);
		se->type_spec = UNKNOWN_type;
		se->read_spec = UNKNOWN_type;
		LL_push(entrylist, (void *)se);
		next_line(tokens);
		return 1;
	}
	else if(strcmp(curtoken, "}")==0)
	{
		LL_delete(se->has_flags);
		LL_delete(se->needs_flags);
		clean(se);
		clean(curtoken);
		return 0;
	}

	if(se->type_spec == 0)
	{
		se->type_spec = REF_type;
		se->type_name = curtoken;
		curtoken = (char *)LL_dequeue(tokens);
	}

	// fieldname
	se->field_name = curtoken;

	// next must be ';' or [count];
	while( (curtoken = (char *)LL_dequeue(tokens))
		&& (strcmp(curtoken, ";")!=0) )
	{
		if(strcmp(curtoken, "[") == 0)
		{
			se->type_spec= (TypeSpec)(se->type_spec|ARRAY_type);
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			if(strcmp(curtoken, "]")!=0)
				se->size_parameter = atoi(curtoken);
		}
		clean(curtoken);
	}
	clean(curtoken);

	// no type overrides yet
	se->read_spec = se->type_spec;
	se->ole_type = DEFAULT_OLE_TYPE;

	// parse till end of line
	while( (curtoken = (char *)LL_dequeue(tokens)) && (strcmp(curtoken, "\n")!=0) )
	{
		if(strcmp_no_case(curtoken, "NOSTRING")==0)
		{
			se->ole_type = ARRAY_OLE_TYPE;
			clean(curtoken);
		}
		else if(strcmp_no_case(curtoken, "TYPE")==0)
		{
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);//"("
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			if((se->type_spec&LIST_type)==LIST_type)
			{
				/* list element type */
				if(parse_type_spec(curtoken, (unsigned int *)&(se->type_spec))==0)
				{
					se->type_spec = (TypeSpec)(se->type_spec|REF_type);
					se->type_name = curtoken;
					se->read_spec = se->type_spec;
				}
				else
					clean(curtoken);
				//) etc should be skipped automatically
			}
			else if((se->type_spec&REF_type)==REF_type)
			{
				/* read spec for enum or struct ref type */
				while(curtoken && parse_type_spec(curtoken, (unsigned int *)&(se->read_spec)))
				{
					clean(curtoken);
					curtoken = (char *)LL_dequeue(tokens);
				}
				if(curtoken)
					clean(curtoken);
			}
			else
			{
				clean(curtoken);
				/* override -- currently not possible */
				printf("should not get here!\n");
				getchar();
			}
		}
		else if(strcmp_no_case(curtoken, "BOOLEAN")==0)
		{
			se->ole_type = BOOLEAN_OLE_TYPE;
			clean(curtoken);
		}
		else if(strcmp_no_case(curtoken, "SIZE")==0)
		{
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			se->length_field = curtoken;
		}
		else if(strcmp_no_case(curtoken, "NULLTERMINATED")==0)
		{
			se->read_spec=(TypeSpec)(se->read_spec|NULL_TERMINATED);
			clean(curtoken);
		}
		else if(strcmp_no_case(curtoken, "FLAG")==0)
		{
			Flag * newflag = create(Flag);
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			clean(curtoken);
			newflag->name = (char *)LL_dequeue(tokens);
			curtoken = (char *)LL_dequeue(tokens);
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			newflag->value=parse_int_string(curtoken);
			LL_push(se->has_flags, newflag);
			clean(curtoken);
		}
		else if(strcmp_no_case(curtoken, "REQUIREFLAG")==0)
		{
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			LL_push(se->needs_flags, (void *)curtoken);
		}
		else if(strcmp_no_case(curtoken, "COUNT")==0)
		{
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			clean(curtoken);
			curtoken = (char *)LL_dequeue(tokens);
			se->length_field = curtoken;
		}
		else
			clean(curtoken);
	}
	if(curtoken)
		clean(curtoken);

	LL_push(entrylist, (void *)se);

	return 1;
}

void parse_typedef(LinkedList * tokens, LinkedList * typedeflist, PacketDefTypes type)
{
	char * nexttoken;
	PacketDef * newdef;

	cur_enum_val = 0;

	newdef = create(PacketDef);
	memset(newdef, 0, sizeof(PacketDef));
	newdef->Contents=LL_create();
	newdef->Type = type;

	if(nexttoken = next_non_newline(tokens))
	{
		if(strcmp(nexttoken, "{")!=0)
		{
			newdef->struct_name = nexttoken;
			nexttoken = next_non_newline(tokens);
			clean(nexttoken);//{
		}
		else
			clean(nexttoken);
		next_line(tokens);

		if(type == DEF_ENUM)
		{
			while(parse_enum_entry(tokens, newdef->Contents))
				;
		}
		else if(type == DEF_STRUCT)
		{
			while(parse_struct_entry(tokens, newdef->Contents))
				;
		}

		if(nexttoken = next_non_newline(tokens))
			newdef->type_name = nexttoken;

		LL_push(typedeflist, (void *)newdef);

		next_line(tokens);
	}

	return;
}

LinkedList * parse_typedefs(LinkedList * tokens)
{
	char * curtoken;
	LinkedList * toreturn;

	toreturn = LL_create();

	while(1)
	{
		while( (curtoken = (char *)LL_dequeue(tokens))
			&& (strcmp(curtoken, "typedef")!=0) )
			clean(curtoken);
		if(!curtoken)
			break;
		clean(curtoken);
		if(curtoken = (char *)LL_dequeue(tokens))
		{
			if(strcmp(curtoken, "struct")==0)
				parse_typedef(tokens, toreturn, DEF_STRUCT);
			else if(strcmp(curtoken, "enum")==0)
				parse_typedef(tokens, toreturn, DEF_ENUM);
			clean(curtoken);
		}
	}

	return toreturn;
}

void printtypespec(TypeSpec tspec)
{
	/*
	ENUM_type=1,
	BYTE_type=2,
	WORD_type=4,
	DWORD_type=8,
	POINTER_type=16,
	REF_type=32,
	ARRAY_type=64,
	UNSIGNED_type=128,
	LIST_type=256,
	NULL_TERMINATED=512
	*/

	if(tspec&UNSIGNED_type)
		printf("unsigned\t");
	if(tspec&BYTE_type)
		printf("byte\t");
	if(tspec&WORD_type)
		printf("short\t");
	if(tspec&DWORD_type)
		printf("int\t");
	if(tspec&LIST_type)
		printf("LinkedList");
	if(tspec&POINTER_type)
		printf("*\t");

	return;
}

void print_struct_entry(StructEntry * curstructentry)
{
	if(curstructentry->type_spec&UNKNOWN_type)
	{
		printf("\tskip %u bytes (value %x)\n", curstructentry->size_parameter, curstructentry->default_value);
		return;
	}

	printf("\t");
	printtypespec(curstructentry->type_spec);
	if(curstructentry->type_spec&LIST_type)
		printf("(of %s)\t", curstructentry->type_name);
	else if(curstructentry->type_spec&REF_type)
	{
		printf("%s\t", curstructentry->type_name);
		if(curstructentry->type_spec!=curstructentry->read_spec)
		{
			printf(" as ");
			printtypespec(curstructentry->read_spec);
		}
	}
	printf("%s", curstructentry->field_name);
	if(curstructentry->type_spec&ARRAY_type)
		printf("[%u]\t", curstructentry->size_parameter);
	if( (curstructentry->length_field) )
		printf("[%s]\t", curstructentry->length_field);
	if( curstructentry->ole_type == BOOLEAN_OLE_TYPE )
		printf("as boolean");
	if( curstructentry->ole_type == ARRAY_OLE_TYPE )
		printf("as array");

	printf("\n");
}

void autogenerate_packet_code()
{
	LinkedList * lltokens, * lltypedefs;
	PacketDef * curdef;
	EnumEntry * curenumentry;
	StructEntry * curstructentry;
	Flag * curflag;
	char * curflagname;
	
	if(lltokens=tokenize("packets.h"))
	{
		if(lltypedefs=parse_typedefs(lltokens))
		{
			/* print typedefs here */
			while(curdef = (PacketDef *)LL_dequeue(lltypedefs))
			{
				if(curdef->Type == DEF_STRUCT)
				{
					while(curstructentry = (StructEntry *)LL_dequeue(curdef->Contents))
					{
						clean(curstructentry->field_name);
						while(curflag = (Flag *)LL_dequeue(curstructentry->has_flags))
						{
							clean(curflag->name);
							clean(curflag);
						}
						LL_delete(curstructentry->has_flags);
						while(curflagname = (char *)LL_dequeue(curstructentry->needs_flags))
						{
							clean(curflagname);
						}
						LL_delete(curstructentry->needs_flags);
						if(curstructentry->length_field)
							clean(curstructentry->length_field);
						if(curstructentry->type_name)
							clean(curstructentry->type_name);
						clean(curstructentry);
					}
				}
				else if(curdef->Type == DEF_ENUM)
				{
					while(curenumentry = (EnumEntry *)LL_dequeue(curdef->Contents))
					{
						clean(curenumentry->name);
						clean(curenumentry);
					}
				}
				LL_delete(curdef->Contents);
				if(curdef->struct_name)
					clean(curdef->struct_name);
				if(curdef->type_name)
					clean(curdef->type_name);
				clean(curdef);
			}
			LL_delete(lltypedefs);
		}
		LL_delete(lltokens);
	}

	print_allocations();

	getchar();
}

#endif