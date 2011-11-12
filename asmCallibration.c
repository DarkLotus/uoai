#include "asmCallibration.h"

#include "allocation.h"

int asm_callibrations_initialized=0;

Stream ** codestream=0;
unsigned char codebuffer[MAX_INSN_SIZE];

#define ASM_CALLIBRATIONS_INIT()\
	if(asm_callibrations_initialized==0)\
		x86_init(opt_none, NULL, NULL);\
	asm_callibrations_initialized=1

#define ASM_CALLIBRATIONS_CLEANUP()\
	if(asm_callibrations_initialized!=0)\
		x86_cleanup();\
	asm_callibrations_initialized=0

void asm_init(Stream ** processstream)
{
	ASM_CALLIBRATIONS_INIT();

	codestream=processstream;

	return;
}

void asm_cleanup()
{
	ASM_CALLIBRATIONS_CLEANUP();

	return;
}

insn_mask * create_insn_mask(unsigned int opcount)
{
	insn_mask * toreturn;
	unsigned int i;
	op_mask * newop;

	toreturn=create(insn_mask);
	memset(toreturn,0,sizeof(insn_mask));

	toreturn->opcount=opcount;
	for(i=0;i<opcount;i++)
	{
		newop=create_op_mask();
		newop->next=toreturn->ops;
		toreturn->ops=newop;
	}

	return toreturn;
}

void mask_in(void * pfield, unsigned int fieldsize)
{
	memset(pfield, 0xFF, fieldsize);
	return;
}

insn_mask * insn_by_type(insn_mask * maskin, enum x86_insn_type _type)
{
	if(maskin==0)
		maskin=create_insn_mask(0);

	maskin->insn.type=_type;
	MASK_INSN(maskin, type, _type);

	return maskin;
}

insn_mask * insn_by_size(insn_mask * maskin, unsigned int _size)
{
	if(maskin==0)
		maskin=create_insn_mask(0);

	maskin->insn.size=_size;
	MASK_INSN(maskin, size, _size);

	return maskin;
}

insn_mask * insn_by_opcount(insn_mask * maskin, unsigned int _opcount)
{
	if(maskin==0)
		maskin=create_insn_mask(0);

	maskin->insn.operand_count=_opcount;
	MASK_INSN(maskin, operand_count, _opcount);

	return maskin;
}

void _recursive_delete_opmasks(op_mask * todelete)
{
	if(todelete->next)
		_recursive_delete_opmasks(todelete->next);
	clean(todelete);
	return;
}

insn_mask * insn_set_opcount(insn_mask * maskin, unsigned int opcount)
{
	unsigned int i;
	op_mask * newmask, * lastmask;

	if(maskin==0)
		maskin=create_insn_mask(opcount);
	else if(maskin->opcount>opcount)
	{
		if(opcount>0)
		{
			lastmask=insn_op_mask(maskin, opcount-1);
			_recursive_delete_opmasks(lastmask->next);
			lastmask->next=0;
		}
		else
		{
			_recursive_delete_opmasks(maskin->ops);
			maskin->ops=0;
		}
		maskin->opcount=opcount;
	}
	else if(maskin->opcount<opcount)
	{
		if(maskin->opcount>0)
			lastmask=insn_op_mask(maskin, maskin->opcount-1);
		else
			lastmask=0;
		for(i=maskin->opcount;i<opcount;i++)
		{
			newmask=create_op_mask();
			if(lastmask)
				lastmask->next=newmask;
			else
				maskin->ops=newmask;
			lastmask=newmask;
		}
		maskin->opcount=opcount;
	}

	return maskin;
}

op_mask * insn_op_mask(insn_mask * maskin, unsigned int op_number)
{
	unsigned int i;
	op_mask * toreturn;

	if(maskin==0)
		maskin=create_insn_mask(op_number);//we need at least op_number of ops; it's bad practice to call this funtion with maskin==0 though

	toreturn=0;

	if(op_number<maskin->opcount)
	{
		toreturn=maskin->ops;
		for(i=0;i<op_number;i++)
			toreturn=toreturn->next;
	}
	
	return toreturn;
}

op_mask * create_op_mask()
{
	op_mask * toreturn;

	toreturn=create(op_mask);
	memset(toreturn,0,sizeof(op_mask));

	return toreturn;
}

op_mask * op_by_type(op_mask * maskin, enum x86_op_type _type)
{
	if(maskin==0)
		maskin=create_op_mask();

	maskin->op.type=_type;
	MASK_OP(maskin, type, _type);

	return maskin;
}

op_mask * op_by_datatype(op_mask * maskin, enum x86_op_datatype _datatype)
{
	if(maskin==0)
		maskin=create_op_mask();

	maskin->op.datatype=_datatype;
	MASK_OP(maskin, datatype, _datatype);

	return maskin;
}

op_mask * op_by_data(op_mask * maskin, x86_op_datatype_t _data)
{
	if(maskin==0)
		maskin=create_op_mask();

	maskin->op.data=_data;
	MASK_OP(maskin, data, _data);

	return maskin;
}

void recursive_delete_insn_mask(insn_mask * todelete, BinaryTree * deleted)
{
	if(BT_find(deleted, (void *)todelete))
		return;

	BT_insert(deleted, (void *)todelete, (void *)todelete);

	if(todelete->set_next)
		recursive_delete_insn_mask(todelete->set_next, deleted);
	if(todelete->seq_next)
		recursive_delete_insn_mask(todelete->seq_next, deleted);

	if(todelete->opcount)
		_recursive_delete_opmasks(todelete->ops);
	
	clean(todelete);

	return;
}

int _unsigned_int_compare(unsigned int a, unsigned int b)
{
	if(a<b)
		return +1;
	else if(a>b)
		return -1;
	else
		return 0;
}

void delete_insn_mask(insn_mask * todelete)
{
	BinaryTree * deleted;

	deleted=BT_create((BTCompare)_unsigned_int_compare);

	recursive_delete_insn_mask(todelete, deleted);

	BT_delete(deleted);

	return;
}

void delete_op_mask(op_mask * todelete)
{
	//_recursive_delete_opmasks(todelete);
	clean(todelete);
	return;
}

insn_mask * insn_set(insn_mask * maskin, insn_mask * option)
{
	insn_mask * cur;
	if(!maskin)
		maskin=option;
	else
	{
		cur=maskin;
		while(cur->set_next)
			cur=cur->set_next;
		cur->set_next=option;
	}

	return maskin;
}

insn_mask * insn_seq(insn_mask * maskin, insn_mask * nextseq)
{
	insn_mask * cur;

	if(!maskin)
		maskin=nextseq;
	else
	{
		cur=maskin;
		while(cur->seq_next)
			cur=cur->seq_next;
		cur->seq_next=nextseq;
	}

	return maskin;
}

int uint_compare(unsigned int * a, unsigned int * b)
{
	if(a<b)
		return +1;
	else if(a>b)
		return -1;
	else
		return 0;
}

x86_insn_t * disasm_insn_at(unsigned int address)
{
	x86_insn_t * curinsn;
	unsigned int curaddr;
	unsigned int insnsize;

	//calculate address
	curaddr=address;

	//new instruction
	curinsn=create(x86_insn_t);

	//disassemble first instruction
	SSetPos((Stream **)codestream, curaddr);
	SRead(codestream, codebuffer, MAX_INSN_SIZE);
	insnsize=x86_disasm(codebuffer, MAX_INSN_SIZE, (unsigned int)curaddr, 0, curinsn);

	if(insnsize>0)
	{
		SSetPos((Stream **)codestream, curaddr+insnsize);
		return curinsn;
	}
	else
	{
		clean(curinsn);
		return 0;
	}
}

x86_insn_t * disasm_insn_next()
{
	x86_insn_t * curinsn;
	unsigned int curaddr;
	unsigned int insnsize;

	//get address address
	curaddr=SGetPos(codestream);

	//new instruction
	curinsn=create(x86_insn_t);

	//disassemble first instruction
	SRead(codestream, codebuffer, MAX_INSN_SIZE);
	insnsize=x86_disasm(codebuffer, MAX_INSN_SIZE, (unsigned int)curaddr, 0, curinsn);

	if(insnsize>0)
	{
		SSetPos((Stream **)codestream, curaddr+insnsize);
		return curinsn;
	}
	else
	{
		clean(curinsn);
		return 0;
	}
}

asm_function * disasm_chunk(unsigned int address)
{
	//ASM_CALLIBRATIONS_INIT();

	x86_insn_t * curinsn;
	asm_function * toreturn;

	toreturn=create(asm_function);

	toreturn->backwards=0;
	toreturn->entrypoint_address=address;
	toreturn->instructions=BT_create((BTCompare)uint_compare);

	//disassemble first instruction
	curinsn=disasm_insn_at(address);
	
	while(curinsn)
	{
		//insert instruction
		BT_insert(toreturn->instructions,(void *)curinsn->addr, (void *)curinsn);

		//if instruction is a jmp(, jcc) or retn then we are done
		//if((curinsn->type==x86_insn_jmp)||(curinsn->type==x86_insn_jcc)||(curinsn->type==x86_insn_return))
		if((curinsn->type==insn_jmp)||(curinsn->type==insn_return))
			break;

		//disassemble next
		curinsn=disasm_insn_next();
	}

	toreturn->insn_enum=BT_newenum(toreturn->instructions);

	return toreturn;
}

void delete_asm_function(asm_function * todelete)
{
	x86_insn_t * curinsn;

	//delete enumerator used to step through the instruction tree address-per-address
	BT_enumdelete(todelete->insn_enum);

	//delete all instructions from instruction tree
	while(todelete->instructions->itemcount>0)
	{
		curinsn=(x86_insn_t *)BT_remove(todelete->instructions, todelete->instructions->root->key);
		clean(curinsn);
	}
	
	//delete instruction tree
	BT_delete(todelete->instructions);

	//delete asm_function structure
	clean(todelete);
	return;
}

asm_function * disasm_function(unsigned int address)
{
	//ASM_CALLIBRATIONS_INIT();

	x86_insn_t * curinsn;
	asm_function * toreturn;
	unsigned int curaddr;
	unsigned int targetaddr;

	LinkedList * disasm_queue;//queue of addresses to disasm

	if(address==0)
		return 0;

	//setup new disassembled function
	toreturn=create(asm_function);

	toreturn->backwards=0;
	toreturn->entrypoint_address=address;
	toreturn->instructions=BT_create((BTCompare)uint_compare);

	//create queue and tree
	disasm_queue=LL_create();

	//insert start address
	LL_push(disasm_queue, (void *)address);

	while(curaddr=(unsigned int)LL_dequeue(disasm_queue))
	{
		if(BT_find(toreturn->instructions,(void *)curaddr)==0)
		{
			//disassemble first instruction
			curinsn=disasm_insn_at(curaddr);
			
			while(curinsn)
			{
				//insert instruction
				BT_insert(toreturn->instructions,(void *)curinsn->addr, (void *)curinsn);
		
				//if instruction is a jmp, jcc or retn then we are done
				if(curinsn->type==insn_jcc)
				{
					//queue target chunk's address, but don't break since the chunk continues after this instruction
					if((targetaddr=(unsigned int)insn_get_target(curinsn,0))!=0)
						LL_push(disasm_queue,(void *)targetaddr);
				}
				else if(curinsn->type==insn_jmp)
				{
					if(!((curinsn->operands->op.type==op_expression)&&(curinsn->operands->op.data.expression.scale!=0)))//skip 
					{
						//queue target chunk's address
						if((targetaddr=(unsigned int)insn_get_target(curinsn,0))!=0)
						{
							LL_push(disasm_queue,(void *)targetaddr);
						}
					}

					//and break : a non-conditional jmp to another chunk means this chunk is finished (jmp targets are treated as seperate chunks)
					break;
				}
				else if(curinsn->type==insn_return)
					break;//just break, chunk is finished
		
				if(BT_find(toreturn->instructions,(void *)((unsigned int)(curinsn->addr+curinsn->size)))!=0)
					break;//next instruction is already in the tree, so we are done with this chunk

				//disassemble next
				curinsn=disasm_insn_next();
			}
		}
	}

	toreturn->insn_enum=BT_newenum(toreturn->instructions);
	LL_delete(disasm_queue);

	return toreturn;
}

x86_insn_t * asm_next(asm_function * onfunction)
{
	return (x86_insn_t *)BT_next(onfunction->insn_enum);
}

x86_insn_t * asm_previous(asm_function * onfunction)
{
	return (x86_insn_t *)BT_previous(onfunction->insn_enum);
}

x86_insn_t * asm_lowest(asm_function * onfunction)
{
	return (x86_insn_t *)BT_reset(onfunction->insn_enum);
}

x86_insn_t * asm_highest(asm_function * onfunction)
{
	return (x86_insn_t *)BT_end(onfunction->insn_enum);
}

x86_insn_t * asm_entrypoint(asm_function * onfunction)
{
	x86_insn_t * curinsn;

	asm_lowest(onfunction);
	while((curinsn=asm_next(onfunction))&&(curinsn->addr<((unsigned int)onfunction->entrypoint_address)))
		;

	return curinsn;
}

x86_insn_t * asm_jump(asm_function * onfunction, unsigned int address)
{
	x86_insn_t * curinsn;

	asm_lowest(onfunction);
	while((curinsn=asm_next(onfunction))&&(curinsn->addr<((unsigned int)address)))
		;

	return curinsn;
}

asm_function * _clone_function(asm_function * toclone)
{
	asm_function * toreturn;

	toreturn=create(asm_function);

	toreturn->entrypoint_address=toclone->entrypoint_address;
	toreturn->instructions=toclone->instructions;
	toreturn->insn_enum=BT_newenum(toreturn->instructions);
	toreturn->insn_enum->curnode=toclone->insn_enum->curnode;

	return toreturn;
}

void _delete_asm_function_clone(asm_function * clone)
{
	BT_enumdelete(clone->insn_enum);
	clean(clone);

	return;
}

int mask_check(unsigned char * tocheck, unsigned char * maskdata, unsigned char * mask, unsigned int size)
{
	unsigned int i;

	for(i=0;i<size;i++)
	{
		if((mask[i]!=0)&&(tocheck[i]!=maskdata[i]))
			return 0;
	}

	return 1;
}

//does not handle sequences, but does handle options
int check_insn_mask(x86_insn_t * instruction, insn_mask ** mask)
{
	op_mask * curopmask;
	x86_oplist_t * curop;
	int ok;

	//check instruction
	ok=mask_check((unsigned char *)instruction, (unsigned char *)&((*mask)->insn), (unsigned char *)&((*mask)->mask), sizeof(x86_insn_t));

	//check operands if any
	if(ok&&(*mask)->opcount)
	{
		curopmask=(*mask)->ops;
		curop=instruction->operands;
		while(curopmask&&curop&&ok)
		{
			ok&=mask_check((unsigned char *)&(curop->op), (unsigned char *)&(curopmask->op), (unsigned char *)&(curopmask->mask), sizeof(x86_op_t));
			curopmask=curopmask->next;
			curop=curop->next;
		}
	}

	//if this didn't match, but there is another option, check the other option
	if((!ok)&&((*mask)->set_next!=0))
	{
		(*mask)=(*mask)->set_next;
		ok=check_insn_mask(instruction, mask);
	}

	return ok;
}

x86_insn_t * _asm_continue(asm_function * disassembled)
{
	if(disassembled->backwards==0)
		return asm_next(disassembled);
	else
		return asm_previous(disassembled);
}

x86_insn_t * _find_insn_linear(asm_function * disassembled, insn_mask ** tofind)
{
	x86_insn_t * curinsn, * nextinsn;
	int ok;
	insn_mask * curmask, * nextmask;
	BinaryTreeNode * _backup;

	//loop the chunk
	while(curinsn=_asm_continue(disassembled))
	{
		curmask=(*tofind);

		ok=check_insn_mask(curinsn, &curmask);
		
		if(ok)
		{
			_backup=disassembled->insn_enum->curnode;
			nextmask=curmask;

			while(ok&&(nextmask)&&(nextmask->seq_next)&&(nextinsn=asm_next(disassembled)))
			{
				nextmask=nextmask->seq_next;
				ok&=check_insn_mask(nextinsn,&nextmask);
			}

			disassembled->insn_enum->curnode=_backup;			
		}

		if(ok)
		{
			(*tofind)=curmask;

			return curinsn;
		}
	}

	return 0;
}

x86_insn_t * _find_insn_nonlinear(asm_function * disassembled, insn_mask ** tofind)
{
	x86_insn_t * curinsn, * nextinsn;
	int ok;
	insn_mask * curmask, * nextmask;
	unsigned int curaddr;
	BinaryTreeNode * _backup;

	//queue and tree to keep track of what is to be searched
	LinkedList * todo;
	BinaryTree * done;

	todo=LL_create();
	done=BT_create((BTCompare)uint_compare);
	
	//start at current address
	curaddr=0;
	/*if(disassembled->insn_enum->curnode)
		LL_push(todo, disassembled->insn_enum->curnode->key);
	else
		LL_push(todo, disassembled->entrypoint_address);*/

	//while there are chunks to check
	do
	{
		if((curaddr==0)||(BT_find(done, (void *)curaddr)==0))
		{
			if(curaddr!=0)
				curinsn=asm_jump(disassembled, curaddr);
			else
				curinsn=asm_next(disassembled);

			while(curinsn)
			{
				curmask=(*tofind);
				
				ok=check_insn_mask(curinsn, &curmask);
				
				if(ok&&(curmask->seq_next))
				{
					_backup=disassembled->insn_enum->curnode;
					nextmask=curmask;
	
					while(ok&&(nextmask)&&(nextmask->seq_next)&&(nextinsn=asm_next(disassembled)))
					{
						nextmask=nextmask->seq_next;
						ok&=check_insn_mask(nextinsn,&nextmask);
					}
	
					disassembled->insn_enum->curnode=_backup;
				}
		
				if(ok)
				{
					(*tofind)=curmask;
					LL_delete(todo);
					BT_delete(done);

					return curinsn;
				}
	
				if(curinsn->type==insn_return)
					break;//chunk done
				else if(curinsn->type==insn_jmp)
				{
					LL_push(todo, (void *)insn_get_target(curinsn, 0));
					break;
				}
				else if(curinsn->type==insn_jcc)
				{
					//queue jcc target
					LL_push(todo, (void *)insn_get_target(curinsn, 0));
				}
	
				curinsn=asm_next(disassembled);
			}

			//chunk done:
			BT_insert(done, (void *)curaddr, (void *)curaddr);
		}
	} while(curaddr=(unsigned int)LL_dequeue(todo));

	LL_delete(todo);
	BT_delete(done);

	return 0;
}

x86_insn_t * find_insn(asm_function * disassembled, insn_mask * tofind, insn_mask ** set_out, int allchunks)
{
	insn_mask ** result;
	insn_mask * dummy;

	if(set_out)
		result=set_out;
	else
		result=&dummy;

	(*result)=tofind;

	if(allchunks)
		return _find_insn_nonlinear(disassembled, result);
	else
		return _find_insn_linear(disassembled, result);
}

x86_op_datatype_t insn_op_data(x86_insn_t * curinsn, unsigned int op_number, enum x86_op_datatype * datatype)
{
	x86_oplist_t * curop;
	unsigned int i;

	curop=curinsn->operands;
	for(i=0;i<op_number;i++)
		curop=curop->next;

	if(datatype)
		(*datatype)=curop->op.datatype;

	return curop->op.data;
}

unsigned int op_data_to_unsigned_int(x86_op_datatype_t data, enum x86_op_datatype type)
{
	switch(type)
	{
	case op_byte:
		return (unsigned int)data.byte;
	case op_word:
		return (unsigned int)data.word;
	case op_dword:
		return (unsigned int)data.dword;
	case op_qword:
		return (unsigned int)data.qword;
	default:
		return 0;
	}
}

//originally intended just to get a target address from an operand, calculating it if this is a relative operand
//	however since targets stored as immediate values means returning the immediate value, it works just fine to
//	read immediate operands too
unsigned int insn_get_target(x86_insn_t * curinsn, unsigned int op_number)
{
	x86_oplist_t * curop;
	unsigned int i;

	curop=curinsn->operands;
	for(i=0;i<op_number;i++)
		curop=curop->next;

	if(curop->op.type==op_immediate)
		return op_data_to_unsigned_int(curop->op.data, curop->op.datatype);
	else if(curop->op.type==op_relative_near)
		return (unsigned int)((int)curinsn->addr+curinsn->size+curop->op.data.relative_near);
	else if(curop->op.type==op_relative_far)
		return (unsigned int)((int)curinsn->addr+curinsn->size+curop->op.data.relative_far);
	else if(curop->op.type==op_absolute)
		return (unsigned int)curop->op.data.absolute.offset.off32;
	else if(curop->op.type==op_expression)
		return (unsigned int)curop->op.data.expression.disp;
	else if(curop->op.type==op_offset)
		return (unsigned int)curop->op.data.offset;

	return 0;
}

unsigned int insn_address(x86_insn_t * curinsn)
{
	return (unsigned int)curinsn->addr;
}

x86_insn_t *  find_loop(asm_function * disassembled)
{
	x86_insn_t * curinsn;
	unsigned int startaddr, targetaddr;
	insn_mask * jmp_mask, * jcc_mask, * jump_mask, * pmask;

	jmp_mask=insn_by_type(0, insn_jmp);
	jcc_mask=insn_by_type(0, insn_jcc);
	jump_mask=insn_set(jmp_mask, jcc_mask);

	curinsn=asm_next(disassembled);
	startaddr=(unsigned int)curinsn->addr;

	while(curinsn)
	{
		pmask=jump_mask;
		if(check_insn_mask(curinsn, &pmask))
		{
			targetaddr=insn_get_target(curinsn, 0);
			if((targetaddr>=startaddr)&&(targetaddr<((unsigned int)curinsn->addr)))
			{
				delete_insn_mask(jump_mask);

				return curinsn;
			}
		}
		curinsn=asm_next(disassembled);
	}

	delete_insn_mask(jump_mask);

	return 0;
}
