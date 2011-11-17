#include "UOCallibrations.h"

#include "allocation.h"
#include "PE.h"
#include "Error.h"
#include "asmCallibration.h"

#include <stdio.h>

asm_function * disasm_macro_chunk(Stream ** clientstream, unsigned int macronumber, UOCallibrations * callibrations)
{
	SSetPos(clientstream, callibrations->MacroSwitchTable + 4*macronumber);
	return disasm_chunk((unsigned int)SReadUInt(clientstream));
}

int unsigned_int_compare(unsigned int a, unsigned int b)
{
	if(a>b)
		return -1;
	else if(a<b)
		return +1;
	else
		return 0;
}

unsigned int FindLoop(asm_function * onfunction)
{
	BinaryTree * seen_addresses;
	unsigned int toreturn;
	x86_insn_t * curinsn;
	unsigned int curtarget;

	//assume error
	toreturn=0;

	//setup tree to store all addresses seen
	seen_addresses=BT_create((BTCompare)unsigned_int_compare);

	while(curinsn=asm_next(onfunction))
	{
		if((curinsn->type==insn_jmp)||(curinsn->type==insn_jcc))
		{
			curtarget=insn_get_target(curinsn, 0);
			if(BT_find(seen_addresses, (void *)curtarget))
			{
				toreturn=curtarget;
				break;
			}
		}

		BT_insert(seen_addresses, (void *)curinsn->addr, (void *)curinsn);
	}

	//cleanup
	BT_delete(seen_addresses);
	return toreturn;
}

unsigned int TracePushTarget(asm_function * curfunc, unsigned int registerid)//handling of mov reg, var; ...; push reg;
{
	insn_mask * mov_reg_var, * mov_reg_varb;
	unsigned int toreturn;
	BinaryTreeNode * backup;
	x86_insn_t * curinsn;
	int prevdirection;
	op_mask * curop;

	//assume error
	toreturn=0;

	//build mask for mov reg, var; <- type=0, 2ops, op0-type=reg, op1-type=offset||op1-type=expression
	//	+ op0 data = reg with id == registerid!!!
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop->op.data.reg.id=registerid;
	curop->mask.data.reg.id=(unsigned int)-1;
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop->op.data.reg.id=registerid;
	curop->mask.data.reg.id=(unsigned int)-1;
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);


	//backup asm position and direction
	backup=curfunc->insn_enum->curnode;
	prevdirection=curfunc->backwards;

	//search backwards
	curfunc->backwards=1;

	if(curinsn=find_insn(curfunc, mov_reg_var, 0, 0))
		toreturn=insn_get_target(curinsn, 1);

	//restore asm position and direction
	curfunc->backwards=prevdirection;
	curfunc->insn_enum->curnode=backup;

	//cleanup
	delete_insn_mask(mov_reg_var);

	return toreturn;
}

unsigned int GetCallParameter(asm_function * curfunc, unsigned int parnumber)//finds parameter push, if op=reg it traces back the push
{
	insn_mask * push;
	unsigned int toreturn;
	BinaryTreeNode * backup;
	x86_insn_t * curinsn;
	unsigned int i;
	int prevdirection;

	//assume error
	toreturn=0;

	//build push mask
	push=insn_by_type(0, insn_push);

	//backup asm position and direction
	backup=curfunc->insn_enum->curnode;
	prevdirection=curfunc->backwards;

	//search backwards
	curfunc->backwards=1;

	//find right push
	for(i=0;i<parnumber;i++)
	{
		if((curinsn=find_insn(curfunc, push, 0, 0))==0)
		{
			curinsn=0;
			break;
		}
	}
	if(curinsn)
	{
		if(curinsn->operands->op.type!=op_register)
			toreturn=insn_get_target(curinsn, 0);
		else
			toreturn=TracePushTarget(curfunc, curinsn->operands->op.data.reg.id);
	}

	//restore asm position and direction
	curfunc->backwards=prevdirection;
	curfunc->insn_enum->curnode=backup;

	//cleanup
	delete_insn_mask(push);

	return toreturn;
}

//need to do this more than once, so...
x86_insn_t * find_2_jccs_to_the_same_target(asm_function * onfunction)
{
	x86_insn_t * curinsn;
	insn_mask * jcc;
	unsigned int prevtarget, curtarget;
	BinaryTreeNode * backup;

	//init masks
	prevtarget=0;
	jcc=insn_by_type(0, insn_jcc);
	insn_set(jcc, insn_by_type(0, insn_jmp));

	//backup position
	backup=onfunction->insn_enum->curnode;

	//find jccs, compare target with prevtarget
	while(curinsn=find_insn(onfunction, jcc, 0, 0))
	{
		curtarget=insn_get_target(curinsn, 0);
		if(prevtarget && (curtarget==prevtarget))
			break;
		prevtarget=curtarget;
	}

	//restore position if we couldn't find anything
	if(curinsn==0)
		onfunction->insn_enum->curnode=backup;

	//cleanup
	delete_insn_mask(jcc);

	return curinsn;
}

asm_function * SwitchToPacketHandler(unsigned char packetnumber, int followcall, int chunked, Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	x86_insn_t * curinsn;
	asm_function * toreturn;
	unsigned int targetaddr;

	insn_mask * call_relative, * call_relativeb;
	op_mask * curop;

	//assume error
	toreturn=0;

	//setup instruction mask:

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	if((callibrations->PacketSwitch_indices!=0)&&(callibrations->PacketSwitch_offsets!=0))
	{
		SSetPos(clientstream, ((unsigned int)callibrations->PacketSwitch_indices)+packetnumber-0xB);
		targetaddr=(unsigned int)SReadByte(clientstream);
		SSetPos(clientstream, ((unsigned int)callibrations->PacketSwitch_offsets) + 4*targetaddr);
		targetaddr=(unsigned int)SReadUInt(clientstream);

		if(toreturn=disasm_chunk(targetaddr))
		{
			if(followcall!=0)
			{
				if(curinsn=find_insn(toreturn, call_relative, 0, 0))
				{
					targetaddr=insn_get_target(curinsn,0);
					delete_asm_function(toreturn);
					if(chunked==0)
						toreturn=disasm_function(targetaddr);
					else
						toreturn=disasm_chunk(targetaddr);
				}
				else//something went wrong, cleanup
				{
					delete_asm_function(toreturn);
					toreturn=0;
				}
			}
		}
	}

	//cleanup
	delete_insn_mask(call_relative);

	return toreturn;
}

LinkedList * GetFieldOffsets(asm_function * curfunc, int search_sub_procedures, int minimaloffset)
{
	LinkedList * toreturn;
	BinaryTree * offsets;
	BinaryTreeNode * backup;

	BinaryTree * subfuncoffsets;
	BinaryTreeEnum * btenum;
	unsigned int curdisp;

	x86_insn_t * curinsn;
	insn_mask * call_relative, * call_relativeb;
	insn_mask * mov_expression;
	op_mask * curop;

	unsigned int targetaddr;

	asm_function * curfuncb;
	
	offsets=BT_create((BTCompare)unsigned_int_compare);

	backup=curfunc->insn_enum->curnode;

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	//mov [esi+0x??], reg
	mov_expression=create_insn_mask(1);
	insn_by_type(mov_expression, insn_mov);
	curop=insn_op_mask(mov_expression, 0);
	op_by_type(curop, op_expression);
	
	//now list all mov [esi+0x??], reg instructions		
	while(curinsn=find_insn(curfunc, mov_expression, 0, 0))
	{
		//mov [esi+0xNN], ... or mov [edi+0xNN], ...
		//edi-id=8, esi-id=7
		//a lower limit for 0xNN can be specified
		if(((curinsn->operands->op.data.expression.base.id==8)||(curinsn->operands->op.data.expression.base.id==7))&&(curinsn->operands->op.data.expression.disp>=minimaloffset))
		{
			if(BT_find(offsets, (void *)curinsn->operands->op.data.expression.disp)==0)
				BT_insert(offsets, (void *)curinsn->operands->op.data.expression.disp, (void *)curinsn->operands->op.data.expression.disp);
		}
	}

	if(search_sub_procedures!=0)
	{
		//do the same thing for all subsequent calls
		curfunc->insn_enum->curnode=backup;
		while((curinsn=find_insn(curfunc, call_relative, 0, 0))&&(targetaddr=insn_get_target(curinsn, 0)))
		{
			if(curfuncb=disasm_function(targetaddr))
			{
				subfuncoffsets=BT_create((BTCompare)unsigned_int_compare);

				while(curinsn=find_insn(curfuncb, mov_expression, 0, 0))
				{
					//mov [esi+0xNN], ... or mov [edi+0xNN], ...
					//edi-id=8, esi-id=7
					//a lower limit for 0xNN can be specified
					if(((curinsn->operands->op.data.expression.base.id==8)||(curinsn->operands->op.data.expression.base.id==7))&&(curinsn->operands->op.data.expression.disp>=minimaloffset))
					{
						if(BT_find(subfuncoffsets, (void *)curinsn->operands->op.data.expression.disp)==0)
							BT_insert(subfuncoffsets, (void *)curinsn->operands->op.data.expression.disp, (void *)curinsn->operands->op.data.expression.disp);
					}
				}

				delete_asm_function(curfuncb);

				if(subfuncoffsets->itemcount > 0)
				{
					btenum = BT_newenum(subfuncoffsets);
					while(curdisp = (unsigned int)BT_next(btenum))
						BT_insert(offsets, (void *)curdisp, (void *)curdisp);
					BT_enumdelete(btenum);
				}

				BT_delete(subfuncoffsets);
			}
		}
	}

	//restore position of passed function
	curfunc->insn_enum->curnode=backup;

	//tree->list
	toreturn=BT_values(offsets);

	//cleanup
	delete_insn_mask(call_relative);
	delete_insn_mask(mov_expression);
	BT_delete(offsets);

	return toreturn;
}

asm_function * EnterNextCall(asm_function * curfunc, int swap)
{
	unsigned int curaddr;
	x86_insn_t * curinsn;
	insn_mask * call_relative, * call_relativeb;
	op_mask * curop;
	asm_function  * toreturn;

	//assume error
	toreturn=0;

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	if(curfunc)
	{
		if(curinsn=find_insn(curfunc, call_relative, 0, 0))
		{
			if(curaddr=insn_get_target(curinsn, 0))
				toreturn=disasm_function(curaddr);
		}
	}

	if(swap)
		delete_asm_function(curfunc);

	delete_insn_mask(call_relative);

	return toreturn;
}

int AOSPropertiesCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;

	x86_insn_t * curinsn;

	asm_function * curfunc, * curfuncb, * ShowItemPropGump;

	insn_mask * push_FFh;
	insn_mask * call_relative, * call_relativeb;
	insn_mask * jmp;
	//insn_mask * mov_var_reg, * mov_var_regb;
	insn_mask * lea_reg_expression_DCh;
	//insn_mask * ccall_stacksize_10h, * add_esp_10h;
	insn_mask * add_esp_10h;
	insn_mask * lea_reg_expression_ECh;
	insn_mask * push;
	op_mask * curop;

	//assume error
	toreturn=0;

	ShowItemPropGump=disasm_function(callibrations->pShowItemPropGump);

	//dependencies:
	if((callibrations->Allocator==0)||(ShowItemPropGump==0))
		return 0;

	//setup instruction masks:

	//push dword -1
	push_FFh=create_insn_mask(1);
	insn_by_type(push_FFh, insn_push);
	curop=insn_op_mask(push_FFh, 0);
	curop->op.data.byte=(unsigned char)-1;
	curop->mask.data.byte=(unsigned char)-1;

	//call relative (E8-call)
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	//jmp
	jmp=insn_by_type(0, insn_jmp);

	/*
	//mov var, reg;
	mov_var_reg=create_insn_mask(2);//var = expression
	insn_by_type(mov_var_reg, insn_mov);
	curop=insn_op_mask(mov_var_reg, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_var_reg, 1);
	op_by_type(curop, op_register);
	mov_var_regb=create_insn_mask(2);//var = offset
	insn_by_type(mov_var_regb, insn_mov);
	curop=insn_op_mask(mov_var_regb, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(mov_var_regb, 1);
	op_by_type(curop, op_register);
	insn_set(mov_var_reg, mov_var_regb);//mov var, reg = mov expression, reg || mov offset, reg;
	*/

	//lea reg, [reg+0xDC]
	lea_reg_expression_DCh=create_insn_mask(2);
	insn_by_type(lea_reg_expression_DCh, insn_mov);//lea is implemented as insn_mov with an op_pointer flag set
	curop=insn_op_mask(lea_reg_expression_DCh, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(lea_reg_expression_DCh, 1);
	op_by_type(curop, op_expression);
	curop->op.flags=op_pointer;
	curop->mask.flags=(unsigned int)-1;
	curop->op.data.expression.disp=0xDC;
	curop->mask.data.expression.disp=(unsigned int)-1;

	//ccall stacksize 0x10
	add_esp_10h=create_insn_mask(2);
	insn_by_type(add_esp_10h, insn_add);
	curop=insn_op_mask(add_esp_10h, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(add_esp_10h, 1);
	curop->op.data.byte=0x10;
	curop->mask.data.byte=0xFF;
	//ccall_stacksize_10h=insn_seq(insn_by_type(0, insn_call), add_esp_10h);

	//lea reg, [reg+0xEC]
	lea_reg_expression_ECh=create_insn_mask(2);
	insn_by_type(lea_reg_expression_ECh, insn_mov);//lea is implemented as insn_mov with an op_pointer flag set
	curop=insn_op_mask(lea_reg_expression_ECh, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(lea_reg_expression_ECh, 1);
	op_by_type(curop, op_expression);
	curop->op.flags=op_pointer;
	curop->mask.flags=(unsigned int)-1;
	curop->op.data.expression.disp=0xEC;
	curop->mask.data.expression.disp=(unsigned int)-1;

	//push
	push=insn_by_type(0, insn_push);
	
	//			-> push 0xFFFFFFFF (twice but find once is ok)	// push 0xFFFFFFFF
	if((curinsn=find_insn(ShowItemPropGump, push_FFh, 0, 0))&&(curinsn=find_insn(ShowItemPropGump, push_FFh, 0, 0)))
	{
		//			-> call Allocate
		//			-> call NewItemPropGumpConstructor	(<- don't care)
		//while((curinsn=find_insn(ShowItemPropGump, call_relative, 0, 0))&&(((unsigned int)callibrations->Allocator)!=insn_get_target(curinsn, 0)))
		//	;

		//			-> follow next jmp								// jmp
		if((curinsn)&&(curinsn=find_insn(ShowItemPropGump, jmp, 0, 0))&&(asm_jump(ShowItemPropGump, insn_get_target(curinsn, 0))))
		{
			//			-> skip one call	-< non-relative							//
			//			-> follow next call = call PropsAndName Get			//
			if((curinsn=find_insn(ShowItemPropGump, call_relative, 0, 0)))//&&(curinsn=find_insn(ShowItemPropGump, call_relative, 0, 0)))
			{
				//			-> before that there is a mov PropGumpID, id	//mov_var_reg <- ignored
				//			-> then follow PropsAndNameGet					//
				if(curfunc=disasm_function(insn_get_target(curinsn, 0)))
				{
					//				-> lea reg, [reg+0xDC]						//lea reg, [reg+0xDC]
					if(curinsn=find_insn(curfunc, lea_reg_expression_DCh, 0, 0))
					{
						//					-> previous call=GetName				//
						curfunc->backwards=1;
						curinsn=find_insn(curfunc, call_relative, 0, 0);
						curfunc->backwards=0;
						if(curinsn && (curfuncb=disasm_function(insn_get_target(curinsn, 0))))
						{
							//						-> call stringobjectinit	//
							if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
							{
								toreturn++;//1
								callibrations->InitString=(pInitString)insn_get_target(curinsn, 0);

								//						-> backwards push defaultnamestring	//push
								curfuncb->backwards=1;
								if(curinsn=find_insn(curfuncb, push, 0, 0))
								{
									toreturn++;//2
									callibrations->DefaultNameString=(unsigned short *)insn_get_target(curinsn, 0);
								}
								curfuncb->backwards=0;
							}

							//						-> call DoGetName, add esp, 0x10	//ccall_stacksize_10h
							if(curinsn=find_insn(curfuncb, add_esp_10h, 0, 0))
							{
								curfuncb->backwards=1;
								if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
								{
									toreturn++;//3
									callibrations->GetName=(pGetName)insn_get_target(curinsn, 0);
								}
							}

							delete_asm_function(curfuncb);
						}
					}
					//				-> lea reg, [reg+0xEC]						//lea reg, [reg+0xEC]
					if(curinsn=find_insn(curfunc, lea_reg_expression_ECh, 0, 0))
					{
						//					-> previous call=GetProperties				//
						curfunc->backwards=1;
						curinsn=find_insn(curfunc, call_relative, 0, 0);
						curfunc->backwards=0;
						if(curinsn && (curfuncb=disasm_function(insn_get_target(curinsn, 0))))
						{
							//						-> backwards call stringobjectinit	//
							if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
							{
								//						-> backwards push defaultpropertiesstring	//push
								curfuncb->backwards=1;
								if(curinsn=find_insn(curfuncb, push, 0, 0))
								{
									toreturn++;//4
									callibrations->DefaultPropertiesString=(unsigned short *)insn_get_target(curinsn, 0);
								}
								curfuncb->backwards=0;
							}

							//						-> call DoGetProperties, add esp, 0x10	//ccall_stacksize_10h
							if(curinsn=find_insn(curfuncb, add_esp_10h, 0, 0))
							{
								curfuncb->backwards = 1;
								if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
								{
									toreturn++;//5
									callibrations->GetProperties=(pGetProperties)insn_get_target(curinsn, 0);
								}
								curfuncb->backwards = 0;
							}							

							delete_asm_function(curfuncb);
						}
					}
					
					delete_asm_function(curfunc);
				}
			}
		}
	}

	if(ShowItemPropGump)
		delete_asm_function(ShowItemPropGump);

	//cleanup
	delete_insn_mask(push_FFh);
	delete_insn_mask(call_relative);
	delete_insn_mask(jmp);
	//delete_insn_mask(mov_var_reg);
	delete_insn_mask(lea_reg_expression_DCh);
	//delete_insn_mask(ccall_stacksize_10h);
	delete_insn_mask(add_esp_10h);
	delete_insn_mask(lea_reg_expression_ECh);
	delete_insn_mask(push);

	if(toreturn==5)
		return 1;
	else
		return 0;
}

typedef struct GumpDescStruct
{
	unsigned int pConstructor;
	unsigned int Size;
	char GumpName[256];
} GumpDesc;

int FindGumpConstructor(asm_function * onfunction, Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations, unsigned int * constructor, unsigned int * size)
{
	int toreturn;

	insn_mask * call_relative, * call_relativeb;
	insn_mask * push;
	op_mask * curop;

	x86_insn_t * curinsn;

	//assume error
	toreturn=0;

	if((size==0)||(constructor==0))
		return 0;

	(*size)=0;
	(*constructor)=0;

	if(((unsigned int)callibrations->Allocator)==0)
		return 0;

	//setup mask(s)

	//push
	push=insn_by_type(0, insn_push);

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	while((curinsn=find_insn(onfunction, call_relative, 0, 0))&&(((unsigned int)callibrations->Allocator)!=insn_get_target(curinsn, 0)))
		;

	if(curinsn)
	{
		onfunction->backwards=1;
		if(curinsn=find_insn(onfunction, push, 0, 0))
			(*size)=insn_get_target(curinsn, 0);
		onfunction->backwards=0;
		if((curinsn=find_insn(onfunction, call_relative, 0, 0))&&(curinsn=find_insn(onfunction, call_relative, 0, 0)))
		{
			(*constructor)=insn_get_target(curinsn, 0);
			
			toreturn=1;
		}
	}

	//cleanup
	delete_insn_mask(call_relative);
	delete_insn_mask(push);

	return toreturn;
}

void AddGumpConstructor(BinaryTree * gumps_by_name, int needsubgumps, int checkall, unsigned int pConstructor, unsigned int Size, Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	asm_function * constructor, * curchunk;
	insn_mask * mov_name;
	op_mask * curop;
	unsigned int curaddr;
	GumpDesc * newdesc;
	x86_insn_t * curinsn;
	int ok;
	unsigned int i;
	unsigned char curchar;
	unsigned int sub_gump_constructor;
	unsigned int sub_gump_size;

	//assume error
	ok=0;

	//inits

	newdesc=create(GumpDesc);
	newdesc->pConstructor=pConstructor;
	newdesc->Size=Size;

	//insn mask = mov [reg + 0x8], pName
	mov_name = create_insn_mask(1);
	insn_by_type(mov_name, insn_mov);
	curop=insn_op_mask(mov_name, 0);
	op_by_type(curop, op_expression);
	curop->op.data.expression.disp=0x8;
	curop->mask.data.expression.disp=0xFFFFFFFF;

	if(constructor=disasm_function(pConstructor))
	{
		//find mov [esi+0x8], pName
		if(curinsn=find_insn(constructor, mov_name, 0, 0))
		{
			if(curaddr=insn_get_target(curinsn, 1))
			{
				SSetPos(clientstream, curaddr);
				i=0;
				while((i<256)&&((curchar=SReadByte(clientstream))!='\0'))
				{
					newdesc->GumpName[i]=curchar;
					i++;
				}
				if(i<256)
				{
					newdesc->GumpName[i]='\0';
					ok=1;
					if(BT_find(gumps_by_name, newdesc->GumpName)==0)
					{
						BT_insert(gumps_by_name, newdesc->GumpName, newdesc);

						if(checkall)
						{				
							asm_entrypoint(constructor);
							while(curchunk=EnterNextCall(constructor, 0))
							{
								while(FindGumpConstructor(curchunk, clientstream, clientprocess, callibrations, &sub_gump_constructor, &sub_gump_size))
									AddGumpConstructor(gumps_by_name, 0, 0, sub_gump_constructor, sub_gump_size, clientstream, clientprocess, callibrations); 
								delete_asm_function(curchunk);
							}
						}
						
						if(needsubgumps)
						{
							asm_entrypoint(constructor);
							while(FindGumpConstructor(constructor, clientstream, clientprocess, callibrations, &sub_gump_constructor, &sub_gump_size))
							{
								if(sub_gump_constructor&&sub_gump_size)
									AddGumpConstructor(gumps_by_name, 0, 0, sub_gump_constructor, sub_gump_size, clientstream, clientprocess, callibrations);
							}
						}
					}
					else
						clean(newdesc);
				}
				//else
				//	GumpName[0]='\0';
			}
		}
		
		delete_asm_function(constructor);
	}

	//general cleanup
	delete_insn_mask(mov_name);

	//cleanup on failure
	if(ok==0)
		clean(newdesc);
	
	return;
}

int GumpCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;

	asm_function * open_gump_function, * curchunk;
	unsigned int gump_switch;
	unsigned int curgumphandler;
	unsigned int i;
	unsigned int cursize, curconstructor;
	BinaryTree * gumps_by_name;

//	BinaryTreeEnum * btenum;

	GumpDesc * curgump;

	insn_mask * temp;
	insn_mask * switch_jmp;
	insn_mask  * mov_reg_expression;
	insn_mask * mov_reg_var, * mov_reg_varb;
	insn_mask * push_820h;
	insn_mask * mov_expression_reg;
	insn_mask * add_reg_4;
	insn_mask * party_insn;
	insn_mask * mov_reg_immediate;
	insn_mask * mov_name;
	insn_mask * cmp_reg_immediate;
	op_mask * curop;

	unsigned int curregid;
	
	x86_insn_t * curinsn;

	//assume error
	toreturn=0;

	//create instruction masks:

	//cmp_reg_immediate
	cmp_reg_immediate=create_insn_mask(2);
	insn_by_type(cmp_reg_immediate, insn_cmp);
	curop=insn_op_mask(cmp_reg_immediate, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(cmp_reg_immediate, 1);
	op_by_type(curop, op_immediate);

	//mov name = mov [reg + 0x8], pName
	mov_name = create_insn_mask(1);
	insn_by_type(mov_name, insn_mov);
	curop=insn_op_mask(mov_name, 0);
	op_by_type(curop, op_expression);
	curop->op.data.expression.disp=0x8;
	curop->mask.data.expression.disp=0xFFFFFFFF;

	//add reg, 4;
	add_reg_4=create_insn_mask(2);
	insn_by_type(add_reg_4, insn_add);
	curop=insn_op_mask(add_reg_4, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(add_reg_4, 1);
	curop->op.data.byte=4;
	curop->mask.data.byte=0xFF;

	//switch jmp
	switch_jmp=create_insn_mask(1);
	insn_by_type(switch_jmp, insn_jmp);
	insn_by_opcount(switch_jmp, 1);
	curop=insn_op_mask(switch_jmp, 0);
	op_by_type(curop, op_expression);

	//mov reg, expression
	mov_reg_expression=create_insn_mask(2);
	insn_by_type(mov_reg_expression, insn_mov);
	curop=insn_op_mask(mov_reg_expression, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_expression, 1);
	op_by_type(curop, op_expression);

	//party insn
	party_insn=insn_set(mov_reg_expression, add_reg_4);

	//mov expression, reg
	mov_expression_reg=create_insn_mask(2);
	insn_by_type(mov_expression_reg, insn_mov);
	curop=insn_op_mask(mov_expression_reg, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_expression_reg, 1);
	op_by_type(curop, op_register);

	//mov reg, var
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_immediate);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_offset);
	insn_set(mov_reg_var, mov_reg_varb);

	//mov reg, immediate
	mov_reg_immediate=create_insn_mask(2);
	insn_by_type(mov_reg_immediate, insn_mov);
	curop=insn_op_mask(mov_reg_immediate, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_immediate, 1);
	op_by_type(curop, op_immediate);


	//push 820h
	push_820h=create_insn_mask(1);
	insn_by_type(push_820h, insn_push);
	curop=insn_op_mask(push_820h, 0);
	curop->op.data.dword=0x820;
	curop->mask.data.dword=0xFFFFFFFF;

	//setup trees
	gumps_by_name=BT_create((BTCompare)strcmp);

	//-> OpenGump function
	if(open_gump_function=disasm_function((unsigned int)callibrations->OpenGump))
	{
		if(curinsn=find_insn(open_gump_function, switch_jmp, 0, 0))
		{
			if(gump_switch=insn_get_target(curinsn, 0))
			{
				for(i=0;i<5;i++)
				{
					SSetPos(clientstream, gump_switch+i*4);
					if((curgumphandler=SReadUInt(clientstream))&&(curchunk=disasm_function(curgumphandler)))
					{
						while(FindGumpConstructor(curchunk, clientstream, clientprocess, callibrations, &curconstructor, &cursize))
							AddGumpConstructor(gumps_by_name, 0, 0, curconstructor, cursize, clientstream, clientprocess, callibrations); 
							//AddGumpConstructor(gumps_by_name, 1, 0, curconstructor, cursize, clientstream, clientprocess, callibrations); 
						delete_asm_function(curchunk);
					}
				}
			}
		}

#if 0
		/* extra gumps... */		
		//more gumps:
		asm_entrypoint(open_gump_function);
		while(FindGumpConstructor(open_gump_function, clientstream, clientprocess, callibrations, &curconstructor, &cursize))
			AddGumpConstructor(gumps_by_name, 1, 0, curconstructor, cursize, clientstream, clientprocess, callibrations);

		//even more gumps:
		asm_entrypoint(open_gump_function);
		while(curchunk=EnterNextCall(open_gump_function, 0))
		{
			while(FindGumpConstructor(curchunk, clientstream, clientprocess, callibrations, &curconstructor, &cursize))
				AddGumpConstructor(gumps_by_name, 1, 1, curconstructor, cursize, clientstream, clientprocess, callibrations); 
			delete_asm_function(curchunk);
		}
#endif

		//-> StatusGumpConstructor
		//		<- Party //mov expression, reg (rest is checked inline) || add reg, 4
		//			mov ecx, PartyMembers[eax*4] //mov reg, expression, expression.base=0, expression.scale=4, expression.index!=0
		//		<- oStatus	//mov expression, reg (rest is checked inline)
		//			mov [eax+oStatus], esi//mov expression, reg with expression.base!=esi|edi|esp and reg=esi|edi
		if(curgump=(GumpDesc *)BT_find(gumps_by_name, "status gump"))
		{
			if(curchunk=disasm_function(curgump->pConstructor))
			{
				while(curinsn=find_insn(curchunk, party_insn, &temp, 0))
				{
					if(temp==add_reg_4)
						break;
					else if((curinsn->operands->next->op.data.expression.base.id==0)&&(curinsn->operands->next->op.data.expression.scale==4)&&(curinsn->operands->next->op.data.expression.index.id!=0))
						break;
				}
				if(curinsn)
				{
					if(temp==add_reg_4)
					{
						curchunk->backwards=1;
						if(curinsn=find_insn(curchunk, mov_reg_immediate, 0, 0))
						{
							toreturn++;//1
							callibrations->PartyMembers=(unsigned int *)insn_get_target(curinsn, 1);
						}
						curchunk->backwards=0;
					}
					else
					{
						toreturn++;//1
						callibrations->PartyMembers=(unsigned int *)insn_get_target(curinsn, 1);
					}

					//cmp_reg_immediate
						//<- immediate = PartyMembersLength || lastpartymember
					if(curinsn=find_insn(curchunk, cmp_reg_immediate, 0, 0))
					{
						if(curinsn->operands->next->op.datatype==op_dword)
						{
							toreturn++;//2
							callibrations->PartyMembersLength=(((unsigned int)(insn_get_target(curinsn, 1)-(unsigned int)callibrations->PartyMembers))/4);
						}
						else if(curinsn->operands->next->op.datatype==op_byte)
						{
							toreturn++;//2
							callibrations->PartyMembersLength=(unsigned int)insn_get_target(curinsn, 1);
						}
					}
				}

				if(curinsn=find_insn(curchunk, mov_name, 0, 0))
				{
					curregid=curinsn->operands->op.data.expression.base.id;

					while(curinsn=find_insn(curchunk, mov_expression_reg, 0, 0))
					{
						if((curinsn->operands->op.data.expression.base.id!=0)&&(curinsn->operands->op.data.expression.base.id!=curregid)&&(curinsn->operands->op.data.expression.base.id!=5))
						{
							if(curinsn->operands->next->op.data.reg.id==curregid)
								break;//found
						}
					}
					if(curinsn)
					{
						toreturn++;//3
						callibrations->ItemOffsets.oMobileStatus2=insn_get_target(curinsn, 0);
					}
	
					while(curinsn=find_insn(curchunk, mov_expression_reg, 0, 0))
					{
						if((curinsn->operands->op.data.expression.base.id!=0)&&(curinsn->operands->op.data.expression.base.id!=curregid)&&(curinsn->operands->op.data.expression.base.id!=5))
						{
							if(curinsn->operands->next->op.data.reg.id==curregid)
								break;//found
						}
					}
					if(curinsn)
					{
						toreturn++;//4
						callibrations->ItemOffsets.oMobileStatus=insn_get_target(curinsn, 0);
					}
				}

				delete_asm_function(curchunk);
			}
		}
		
		//-> TextGumpConstructor
		//		<- Journal
		//		push 0x820						//push 0x820
		//		backwards mov reg, Journal		//mov reg, var
		if(curgump=(GumpDesc *)BT_find(gumps_by_name, "text gump"))
		{
			if(curchunk=disasm_function(curgump->pConstructor))
			{
				if(curinsn=find_insn(curchunk, push_820h, 0, 0))
				{
					curchunk->backwards=1;
					if(curinsn=find_insn(curchunk, mov_reg_var, 0, 0))
					{
						toreturn++;//5
						callibrations->Journal=(JournalEntry **)insn_get_target(curinsn, 1);
					}
				}

				delete_asm_function(curchunk);
			}
		}

		//we assume all Gump and Journal offsets to be fixed... this will probably require an update on some patch


#if 0
		//debug print all gumps
		btenum=BT_newenum(gumps_by_name);
		while(curgump=(GumpDesc *)BT_next(btenum))
			printf("%s\n", curgump->GumpName);
		BT_enumdelete(btenum);
		getchar();
#endif

		//cleanup gumps_by_name entries			
		while((gumps_by_name->root)&&(curgump=(GumpDesc *)gumps_by_name->root->data))
		{
			BT_remove(gumps_by_name, curgump->GumpName);
			clean(curgump);
		}

		delete_asm_function(open_gump_function);
	}

	//cleanup
	BT_delete(gumps_by_name);
	delete_insn_mask(switch_jmp);
	//delete_insn_mask(mov_reg_expression);
	delete_insn_mask(mov_expression_reg);
	delete_insn_mask(mov_reg_var);
	delete_insn_mask(push_820h);
	//delete_insn_mask(add_reg_4);
	delete_insn_mask(party_insn);
	delete_insn_mask(mov_reg_immediate);
	delete_insn_mask(mov_name);
	delete_insn_mask(cmp_reg_immediate);

	if(toreturn==5)
		return 1;
	else
		return 0;
}

int OtherPacketCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;

	insn_mask * mov_var_reg, * mov_var_regb;
	insn_mask * mov_var_value, * mov_var_valueb;
	insn_mask * call_relative, * call_relativeb;
	insn_mask * jmp_or_jcc;
	insn_mask * mov_var_dword_1, * mov_var_dword_1b;
	insn_mask * mov_reg_expression;
	insn_mask * rep_strstore;
	insn_mask * rep_strstore_or_call;
	insn_mask * temp;
	insn_mask * mov_reg_value;
	insn_mask * ccall_stacksize_10h, * add_esp_10h;
	insn_mask * push_7D0h;
	insn_mask * mov_reg_var, * mov_reg_varb;
	insn_mask * push_3h_push_21h_call, * push_3h, * push_21h, * _call;
	//insn_mask * ccall_stacksize_Ch, * add_esp_Ch;
	insn_mask * add_esp_Ch;
	insn_mask * ja;
	insn_mask * switch_jmp;
	insn_mask * mov_expression_reg;//expression.scale is to be checked
	//insn_mask * cmp_var_reg, * cmp_var_regb;
	insn_mask * cmp;
	insn_mask * retn;
	insn_mask * ccall_stacksize_4h, * add_esp_4h;
	op_mask * curop;

	unsigned int SubCommandsSwitch_indices, SubCommandsSwitch_offsets;
	unsigned int curaddr, LoginConfirmSwitch;

	asm_function * curfunc, * curfuncb;
	
	x86_insn_t * curinsn;

	//assume error
	toreturn=0;

	//construct instruction masks:

	retn=insn_by_type(0, insn_return);

	mov_var_reg=create_insn_mask(2);//var = expression
	insn_by_type(mov_var_reg, insn_mov);
	curop=insn_op_mask(mov_var_reg, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_var_reg, 1);
	op_by_type(curop, op_register);
	mov_var_regb=create_insn_mask(2);//var = offset
	insn_by_type(mov_var_regb, insn_mov);
	curop=insn_op_mask(mov_var_regb, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(mov_var_regb, 1);
	op_by_type(curop, op_register);
	insn_set(mov_var_reg, mov_var_regb);//mov var, reg = mov expression, reg || mov offset, reg;

	mov_var_value=create_insn_mask(2);//mov expression, immediate
	insn_by_type(mov_var_value, insn_mov);
	curop=insn_op_mask(mov_var_value, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_var_value, 1);
	op_by_type(curop, op_immediate);
	mov_var_valueb=create_insn_mask(2);//mov offset, immediate
	insn_by_type(mov_var_valueb, insn_mov);
	curop=insn_op_mask(mov_var_valueb, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(mov_var_valueb, 1);
	op_by_type(curop, op_immediate);
	insn_set(mov_var_value, mov_var_valueb);

	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	jmp_or_jcc=insn_set(insn_by_type(0, insn_jcc), insn_by_type(0, insn_jmp));

	mov_var_dword_1=create_insn_mask(2);
	insn_by_type(mov_var_dword_1, insn_mov);
	curop=insn_op_mask(mov_var_dword_1, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_var_dword_1, 1);
	curop->op.data.dword=1;
	curop->mask.data.dword=0xFFFFFFFF;
	mov_var_dword_1b=create_insn_mask(2);
	insn_by_type(mov_var_dword_1b, insn_mov);
	curop=insn_op_mask(mov_var_dword_1b, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(mov_var_dword_1b, 1);
	curop->op.data.dword=1;
	curop->mask.data.dword=0xFFFFFFFF;
	insn_set(mov_var_dword_1, mov_var_dword_1b);

	mov_reg_expression=create_insn_mask(2);
	insn_by_type(mov_reg_expression, insn_mov);
	curop=insn_op_mask(mov_reg_expression, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_expression, 1);
	op_by_type(curop, op_expression);

	rep_strstore=insn_by_type(0, insn_strstore);
	rep_strstore->insn.prefix=insn_rep_zero;
	rep_strstore->mask.prefix=0xFFFFFFFF;

	rep_strstore_or_call=insn_by_type(0, insn_strstore);
	rep_strstore_or_call->insn.prefix=insn_rep_zero;
	rep_strstore_or_call->mask.prefix=0xFFFFFFFF;
	insn_set(rep_strstore_or_call, insn_by_type(0, insn_call));

	mov_reg_value=create_insn_mask(2);
	insn_by_type(mov_reg_value, insn_mov);
	curop=insn_op_mask(mov_reg_value, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_value, 1);
	op_by_type(curop, op_immediate);

	add_esp_10h=create_insn_mask(2);
	insn_by_type(add_esp_10h, insn_add);
	curop=insn_op_mask(add_esp_10h, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(add_esp_10h, 1);
	curop->op.data.byte=0x10;
	curop->mask.data.byte=0xFF;
	ccall_stacksize_10h=insn_seq(insn_by_type(0, insn_call), add_esp_10h);

	push_7D0h=create_insn_mask(1);
	insn_by_type(push_7D0h, insn_push);
	curop=insn_op_mask(push_7D0h, 0);
	curop->op.data.dword=0x7D0;
	curop->mask.data.dword=0xFFFFFFFF;

	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_expression);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_offset);
	insn_set(mov_reg_var, mov_reg_varb);

	push_3h=create_insn_mask(1);
	insn_by_type(push_3h, insn_push);
	curop=insn_op_mask(push_3h, 0);
	curop->op.data.byte=0x3;
	curop->mask.data.byte=0xFF;
	push_21h=create_insn_mask(1);
	insn_by_type(push_21h, insn_push);
	curop=insn_op_mask(push_21h, 0);
	curop->op.data.byte=0x21;
	curop->mask.data.byte=0xFF;
	_call=insn_by_type(0, insn_call);
	push_3h_push_21h_call=insn_seq(push_3h, push_21h);
	insn_seq(push_3h_push_21h_call, _call);

	add_esp_Ch=create_insn_mask(2);
	insn_by_type(add_esp_Ch, insn_add);
	curop=insn_op_mask(add_esp_Ch, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(add_esp_Ch, 1);
	curop->op.data.byte=0xC;
	curop->mask.data.byte=0xFF;
	//ccall_stacksize_Ch=insn_seq(insn_by_type(0, insn_call), add_esp_Ch);

	ja=insn_by_type(0, insn_jcc);//DOES insn_jcc DO THE JOB HERE?

	switch_jmp=create_insn_mask(1);
	insn_by_type(switch_jmp, insn_jmp);
	insn_by_opcount(switch_jmp, 1);
	curop=insn_op_mask(switch_jmp, 0);
	op_by_type(curop, op_expression);

	mov_expression_reg=create_insn_mask(2);
	insn_by_type(mov_expression_reg, insn_mov);
	curop=insn_op_mask(mov_expression_reg, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_expression_reg, 1);
	op_by_type(curop, op_register);

	/*cmp_var_reg=create_insn_mask(2);
	insn_by_type(cmp_var_reg, insn_cmp);
	curop=insn_op_mask(cmp_var_reg, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(cmp_var_reg, 1);
	op_by_type(curop, op_register);
	cmp_var_regb=create_insn_mask(2);
	insn_by_type(cmp_var_regb, insn_cmp);
	curop=insn_op_mask(cmp_var_regb, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(cmp_var_regb, 1);
	op_by_type(curop, op_register);
	insn_set(cmp_var_reg, cmp_var_regb);*/
	cmp=insn_by_type(0, insn_cmp);

	add_esp_4h=create_insn_mask(2);
	insn_by_type(add_esp_4h, insn_add);
	curop=insn_op_mask(add_esp_4h, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(add_esp_4h, 1);
	curop->op.data.byte=0x4;
	curop->mask.data.byte=0xFF;
	ccall_stacksize_4h=insn_seq(insn_by_type(0, insn_call), add_esp_4h);

	//start callibrating:

	//0x6C -> TargetPacket
	if(curfunc=SwitchToPacketHandler(0x6C, 1, 0, clientstream, clientprocess, callibrations))
	{
			//	follow first jcc
			if((curinsn=find_insn(curfunc, jmp_or_jcc, 0, 0))&&(asm_jump(curfunc, insn_get_target(curinsn, 0))))
			{
				//		mov TargetCursorID, reg		//mov var, reg
				if(curinsn=find_insn(curfunc, mov_var_reg, 0, 0))
				{
					toreturn++;//1
					callibrations->TargetCursorID=(unsigned int *)insn_get_target(curinsn, 0);
				}
				//		mov TargetCursorType, reg
				if(curinsn=find_insn(curfunc, mov_var_reg, 0, 0))
				{
					toreturn++;//2
					callibrations->TargetCursorType=(unsigned int *)insn_get_target(curinsn, 0);
				}
				//		mov bTargetCursorShown, reg <- duplicate callibration?
				if(curinsn=find_insn(curfunc, mov_var_reg, 0, 0))
				{
					toreturn++;//3
					callibrations->bTargetCursorShown=(unsigned char *)insn_get_target(curinsn, 0);
				}
				//		mov TargetHandler, offset DefaultTargetHandler	//mov var, immediate||mov var, offset
				if(curinsn=find_insn(curfunc, mov_var_value, 0, 0))
				{
					toreturn++;//4
					callibrations->TargetCallback=(pOnTarget *)insn_get_target(curinsn, 0);
					callibrations->DefaultTargetCallback=(pOnTarget)insn_get_target(curinsn, 1);
				}
			}
		delete_asm_function(curfunc);
	}

	//0x38 -> PathfindPacket
	if(curfunc=SwitchToPacketHandler(0x38, 1, 0, clientstream, clientprocess, callibrations))
	{
		//	retn, then backwards call, follow call	//retn
		if(curinsn=find_insn(curfunc, retn, 0, 0))
		{
			curfunc->backwards=1;
			if((curinsn=find_insn(curfunc, call_relative, 0, 0))&&(curfuncb=disasm_function(insn_get_target(curinsn, 0))))
			{
				//	skip call						//call relative
				find_insn(curfuncb, call_relative, 0, 0);

				//	next call = call Bark
				if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
				{
					toreturn++;//5
					callibrations->Bark=(pBark)insn_get_target(curinsn, 0);
				}

				//	next call = call CalculatePath
				if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
				{
					toreturn++;//6
					callibrations->CalculatePath=(pCalculatePath)insn_get_target(curinsn, 0);
				}

				//	mov StartOfPath, eax
				if(curinsn=find_insn(curfuncb, mov_var_reg, 0, 0))
				{
					toreturn++;//7
					callibrations->CurrentPathNode=(PathNode **)insn_get_target(curinsn, 0);
				}

				//	follow next jcc					//jmp_or_jcc
				if((curinsn=find_insn(curfuncb, jmp_or_jcc, 0, 0))&&(curinsn=asm_jump(curfuncb, insn_get_target(curinsn, 0))))
				{
					asm_previous(curfuncb);

					//	mov bDoPathFind, 1				//mov var, 1 (dword)
					if(curinsn=find_insn(curfuncb, mov_var_dword_1, 0, 0))
					{
						toreturn++;//8
						callibrations->bWalkPath=(int *)insn_get_target(curinsn, 0);
					}
					
					//	call InvertPath
					if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
					{
						toreturn++;//9
						callibrations->InvertPath=(pInvertPath)insn_get_target(curinsn, 0);
					}

					//	mov reg, [reg+oPathPrevious]	//mov reg, expression
					while((curinsn=find_insn(curfuncb, mov_reg_expression, 0, 0))&&(curinsn->operands->next->op.data.expression.base.id==0))
						;
					if(curinsn)
					{
						toreturn++;//10
						callibrations->PathOffsets.oPathPrevious=insn_get_target(curinsn, 1);
					}
					//	skip another mov reg, [reg+oPathPrevious]
					while((curinsn=find_insn(curfuncb, mov_reg_expression, 0, 0))&&(curinsn->operands->next->op.data.expression.base.id==0))
						;
					//	mov reg, [reg+oPathX]
					while((curinsn=find_insn(curfuncb, mov_reg_expression, 0, 0))&&(curinsn->operands->next->op.data.expression.base.id==0))
						;
					if(curinsn)
					{
						toreturn++;//11
						callibrations->PathOffsets.oPathX=insn_get_target(curinsn, 1);
					}
					//	mov reg, [reg+oPathY]
					while((curinsn=find_insn(curfuncb, mov_reg_expression, 0, 0))&&(curinsn->operands->next->op.data.expression.base.id==0))
						;
					if(curinsn)
					{
						toreturn++;//12
						callibrations->PathOffsets.oPathY=insn_get_target(curinsn, 1);
					}
					//	mov reg, [reg+oPathZ]
					while((curinsn=find_insn(curfuncb, mov_reg_expression, 0, 0))&&(curinsn->operands->next->op.data.expression.base.id==0))
						;
					if(curinsn)
					{
						toreturn++;//13
						callibrations->PathOffsets.oPathZ=insn_get_target(curinsn, 1);
					}
				}

				delete_asm_function(curfuncb);
			}
		}

		delete_asm_function(curfunc);
	}

	//0x3A -> SkillPacket
	if(curfunc=SwitchToPacketHandler(0x3A, 1, 0, clientstream, clientprocess, callibrations))
	{
		//find rep strstore isntruction,
		if(curinsn=find_insn(curfunc, rep_strstore, 0, 0))
		{
			//backwards mov reg, offset skillcaps							//mov_reg_immediate //<- unsigned short *
			curfunc->backwards=1;
			if(curinsn=find_insn(curfunc, mov_reg_value, 0, 0))
			{
				toreturn++;//14
				callibrations->SkillCaps=(unsigned short *)insn_get_target(curinsn, 1);
			}

			//forward push offset SkillLocks (call, trace back par 0)	//call				//<- unsigned char *
			curfunc->backwards=0;
			if(curinsn=find_insn(curfunc, rep_strstore_or_call, &temp, 0))
			{
				if(temp==rep_strstore_or_call)
				{
					curfunc->backwards=1;
					curaddr=curinsn->addr;//backup pos
					//backwards mov reg, offset skillrealvalues										//<- unsigned short *
					if(curinsn=find_insn(curfunc, mov_reg_value, 0, 0))
					{
						toreturn++;//15
						callibrations->SkillRealValues=(unsigned short *)insn_get_target(curinsn, 1);
					}
					asm_jump(curfunc, curaddr);//restore pos
					curfunc->backwards=0;
				}
				else//call
				{
					if(callibrations->SkillLocks=(unsigned char *)GetCallParameter(curfunc, 1))
						toreturn++;//15
				}
			}

			if(curinsn=find_insn(curfunc, rep_strstore, 0, 0))
			{
				curfunc->backwards=1;
				curaddr=curinsn->addr;//backup pos
				//backwards mov reg, offset skillrealvalues										//<- unsigned short *
				if(curinsn=find_insn(curfunc, mov_reg_value, 0, 0))
				{
					toreturn++;//16
					callibrations->SkillRealValues=(unsigned short *)insn_get_target(curinsn, 1);
				}
				asm_jump(curfunc, curaddr);//restore pos
				curfunc->backwards=0;
			}

			//mov reg, offset skillmodvalues										//<- unsigned short *
			if(curinsn=find_insn(curfunc, rep_strstore, 0, 0))
			{
				curfunc->backwards=1;
				if(curinsn=find_insn(curfunc, mov_reg_value, 0, 0))
				{
					toreturn++;//17
					callibrations->SkillModifiedValues=(unsigned short *)insn_get_target(curinsn, 1);
				}
				curfunc->backwards=0;
			}
		}

		delete_asm_function(curfunc);
	}

	//0xD6 -> PropertiesPacket ('Mega Cliloc)
	if(curfunc=SwitchToPacketHandler(0xD6, 1, 0, clientstream, clientprocess, callibrations))
	{
		//find loop
		if(FindLoop(curfunc))//<- current position is the end of the loop (jmp back)
		{
			//backwards find call xxxxxx, add esp 0x10			//ccall, stacksize 0x10
			curfunc->backwards=1;
			if((curinsn=find_insn(curfunc, ccall_stacksize_10h, 0, 0))&&(curfuncb=disasm_function(insn_get_target(curinsn, 0))))
			{
				//<- we need another asm_function, so cleanup curfunc for reusage
				delete_asm_function(curfunc);
				curfunc=0;
				//find push 0x7D0							//push 0x7D0
				//follow next call
				if((curinsn=find_insn(curfuncb, push_7D0h, 0, 0))&&(curinsn=find_insn(curfuncb, call_relative, 0, 0)))
				{
					if(curfunc=disasm_function(insn_get_target(curinsn, 0)))
					{
						//find finditemcall						//ccall, stackize 0x4
						if(curinsn=find_insn(curfunc, ccall_stacksize_4h, 0, 0))
						{
							callibrations->FindItemByID=(pFindItemByID)insn_get_target(curinsn, 0);//duplicate callibration
							toreturn++;//18
						}
						//find mov edx, HoldingID				//mov_reg_var
						if(curinsn=find_insn(curfunc, mov_reg_var, 0, 0))
						{
							toreturn++;//19
							callibrations->DraggedItemID=(unsigned int *)insn_get_target(curinsn, 1);
						}
					}
				}
				
				delete_asm_function(curfuncb);
			}
		}

		if(curfunc)
			delete_asm_function(curfunc);
	}

	//0x55 -> LoginConfirmPacket -> LoginIP/Port info, textout
	if(curfunc=SwitchToPacketHandler(0x55, 1, 0, clientstream, clientprocess, callibrations))
	{
		//to start (typically there is a jmp backwards somewhere here and we are intersted in this earlier part)
		asm_lowest(curfunc);//maybe we should find the jmp and follow it?

		// find push 0x3, push 0x21, call TextOut -> TextOut	//push_3h_push_21_h_call
		if((curinsn=find_insn(curfunc, push_3h_push_21h_call, 0, 0))&&(curinsn=find_insn(curfunc, call_relative, 0, 0)))
		{
			toreturn++;//20
			callibrations->ShowMessage=(pShowMessage)insn_get_target(curinsn, 0);
		}

		// find another call textout
		if((curinsn=find_insn(curfunc, push_3h_push_21h_call, 0, 0))&&(curinsn=find_insn(curfunc, call_relative, 0, 0)))
		{		
			// skip one call
			// follow next call
			if((curinsn=find_insn(curfunc, call_relative, 0, 0))&&(curinsn=find_insn(curfunc, call_relative, 0, 0)))
			{
				if(curfuncb=disasm_function(insn_get_target(curinsn, 0)))
				{
					//skip first mov reg, offset
					find_insn(curfuncb, mov_reg_value, 0, 0);

					// mov reg, offset WindowName
					if(curinsn=find_insn(curfuncb, mov_reg_value, 0, 0))
					{
						toreturn++;//21
						callibrations->WindowName=(char *)insn_get_target(curinsn, 1);
					}
					
					// mov reg, offset ServerName
					if(curinsn=find_insn(curfuncb, mov_reg_value, 0, 0))
					{
						toreturn++;//22
						callibrations->ServerName=(char *)insn_get_target(curinsn, 1);
					}
					
					// mov reg, offset WindowName; mov ecx, 1;
					find_insn(curfuncb, mov_reg_value, 0, 0);
					find_insn(curfuncb, mov_reg_value, 0, 0);

					// mov reg, offset CharacterName
					if(curinsn=find_insn(curfuncb, mov_reg_value, 0, 0))
					{
						toreturn++;//23
						callibrations->CharacterName=(char *)insn_get_target(curinsn, 1);
					}

					// find TWICE:
					//	call xxxxx, 0xC												//ccall stacksize 0xC
					//if((curinsn=find_insn(curfuncb, ccall_stacksize_Ch, 0, 0))&&(curinsn=find_insn(curfuncb, ccall_stacksize_Ch, 0, 0)))
					//{
					if((curinsn=find_insn(curfuncb, add_esp_Ch, 0, 0))&&(curinsn=find_insn(curfuncb, add_esp_Ch, 0, 0)))
					{
						curfuncb->backwards=1;
						if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
						{
							delete_asm_function(curfunc);
							if(curfunc=disasm_chunk(insn_get_target(curinsn, 0)))
							{
								//	find switch jmp											//switch_jmp
								if((curinsn=find_insn(curfunc, switch_jmp, 0, 0))==0)
								{
									asm_entrypoint(curfunc);
									if((curinsn=find_insn(curfunc, jmp_or_jcc, 0, 0))&&(curaddr=insn_get_target(curinsn, 0)))
									{
										delete_asm_function(curfunc);
										curfunc=disasm_chunk(curaddr);
										curinsn=find_insn(curfunc, switch_jmp, 0, 0);
									}
								}
								if(curinsn && curfunc && (LoginConfirmSwitch=insn_get_target(curinsn, 0)))
								{
									//	switch to 0x21 chunk (read as FUNCTION (too many jmps))
									delete_asm_function(curfuncb);
									SSetPos(clientstream, LoginConfirmSwitch + 0x21*4);
									curaddr=(unsigned int)SReadUInt(clientstream);
									if(curfuncb=disasm_function(curaddr))
									{
										//		find mov PortTable[idx*2], reg <- scale 2			//mov_expression_reg with expression.scale=2
										while((curinsn=find_insn(curfuncb, mov_expression_reg, 0, 0))&&(curinsn->operands->op.data.expression.scale!=2))
											;

										// newer clients use 0x22 chunk
										if(!curinsn)
										{
											delete_asm_function(curfuncb);
											SSetPos(clientstream, LoginConfirmSwitch + 0x22*4);
											curaddr=(unsigned int)SReadUInt(clientstream);
											if(curfuncb=disasm_function(curaddr))
											{
												//		find mov PortTable[idx*2], reg <- scale 2			//mov_expression_reg with expression.scale=2
												while((curinsn=find_insn(curfuncb, mov_expression_reg, 0, 0))&&(curinsn->operands->op.data.expression.scale!=2))
													;
											}
										}

										if(curinsn)
										{
											toreturn++;//24
											callibrations->LoginPortTable=(unsigned short *)insn_get_target(curinsn, 0);
	
											//		backwards find mov IPTable[eax*4], reg				//mov_expression_reg with expression.scale=4
											curfuncb->backwards=1;
											while((curinsn=find_insn(curfuncb, mov_expression_reg, 0, 0))&&(curinsn->operands->op.data.expression.scale!=4))
												;
											if(curinsn)
											{
												toreturn++;//25
												callibrations->LoginIPTable=(unsigned int *)insn_get_target(curinsn, 0);
											}
	
											//		forward
											curfuncb->backwards=0;
											//		(follow jmp?)
											//		find cmp IPIndex, ebx								//cmp_var_reg
											//	or	mov reg, IPIndex; cmp reg, ebx
											if(curinsn=find_insn(curfuncb, cmp, 0, 0))
											{
												if(curinsn->operands->op.type==op_register)
												{
													curfuncb->backwards=1;
													if(curinsn=find_insn(curfuncb, mov_reg_var, 0, 0))
													{
														toreturn++;//26
														callibrations->LoginTableIndex=(unsigned int *)insn_get_target(curinsn, 1);
													}
													curfuncb->backwards=0;
												}
												else
												{
													toreturn++;//26
													callibrations->LoginTableIndex=(unsigned int *)insn_get_target(curinsn, 0);
												}
											}
										}
									}
	
									//	switch to 0x14 chunk
									delete_asm_function(curfuncb);
									SSetPos(clientstream, LoginConfirmSwitch + 0x14*4);
									curaddr=(unsigned int)SReadUInt(clientstream);
									if(curfuncb=disasm_function(curaddr))
									{
										//		find mov LoginPort, cx
										if(curinsn=find_insn(curfuncb, mov_var_reg, 0, 0))
										{
											toreturn++;//27
											callibrations->LoginPort=(unsigned short *)insn_get_target(curinsn, 0);
										}
										
										//		find mov LoginIP, eax
										while((curinsn=find_insn(curfuncb, mov_var_reg, 0, 0))&&(curinsn->operands->op.datatype!=op_dword))
											;
										if(curinsn)
										{
											toreturn++;//28
											callibrations->LoginIP=(unsigned int *)insn_get_target(curinsn, 0);
										}
									}
								}
							}
						}
					}

					if(curfuncb)
							delete_asm_function(curfuncb);
				}
			}
		}

		if(curfunc)
			delete_asm_function(curfunc);
	}

	//0xBF (CHUNK)
	if(curfunc=SwitchToPacketHandler(0xBF, 1, 1, clientstream, clientprocess, callibrations))
	{
		//find switch_jmp -> store SubCommandsSwitch_offsets
		if(curinsn=find_insn(curfunc, switch_jmp, 0, 0))
			SubCommandsSwitch_offsets=insn_get_target(curinsn, 0);
		else
			SubCommandsSwitch_offsets=0;

		//find mov reg, SubCommandsSwitch_indices[idx] backwards
		curfunc->backwards=1;
		if(curinsn=find_insn(curfunc, mov_reg_expression, 0, 0))
			SubCommandsSwitch_indices=insn_get_target(curinsn, 1);
		else
			SubCommandsSwitch_indices=0;

		if((SubCommandsSwitch_indices!=0)&&(SubCommandsSwitch_offsets!=0))
		{
			//lookup switch index 7 (subcommand 8, but switchidx = cmd-1)
			SSetPos(clientstream, SubCommandsSwitch_indices+7);
			curaddr=SReadByte(clientstream);
			curaddr=SubCommandsSwitch_offsets+curaddr*4;
			SSetPos(clientstream, curaddr);
			curaddr=SReadUInt(clientstream);
			if(curfuncb=disasm_chunk(curaddr))
			{
				delete_asm_function(curfunc);
				curfunc=0;

				//find jcc, follow jcc (chunk)
				if(curinsn=find_insn(curfuncb, jmp_or_jcc, 0, 0))
				{
					if(curfunc=disasm_chunk(insn_get_target(curinsn, 0)))
					{
						//find mov Facet, reg (MapNumber aka Facet)
						if(curinsn=find_insn(curfunc, mov_var_reg, 0, 0))
						{
							toreturn++;//29
							callibrations->MapNumber=(unsigned char *)insn_get_target(curinsn, 0);
						}
					}
				}

				delete_asm_function(curfuncb);
			}
		}

		if(curfunc)
			delete_asm_function(curfunc);
	}

	//cleanup
	delete_insn_mask(mov_var_reg);
	delete_insn_mask(mov_var_value);
	delete_insn_mask(call_relative);
	delete_insn_mask(jmp_or_jcc);
	delete_insn_mask(mov_var_dword_1);
	delete_insn_mask(mov_reg_expression);
	delete_insn_mask(rep_strstore);
	delete_insn_mask(rep_strstore_or_call);
	delete_insn_mask(mov_reg_value);
	delete_insn_mask(ccall_stacksize_10h);
	delete_insn_mask(push_7D0h);
	delete_insn_mask(mov_reg_var);
	delete_insn_mask(push_3h_push_21h_call);
//	delete_insn_mask(ccall_stacksize_Ch);
	delete_insn_mask(add_esp_Ch);
	delete_insn_mask(ja);
	delete_insn_mask(switch_jmp);
	delete_insn_mask(mov_expression_reg);
//	delete_insn_mask(cmp_var_reg);
	delete_insn_mask(cmp);
	delete_insn_mask(retn);
	delete_insn_mask(ccall_stacksize_4h);

	if(toreturn==29)
		return 1;
	else
		return 0;
}

int NewItemPacketCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	asm_function * newitempackethandler, * curfunc;
	
	insn_mask * retn;
	insn_mask * call_relative, * call_relativeb;
	op_mask * curop;

	x86_insn_t * curinsn;

	LinkedList * offsets;

	unsigned int targetaddr;
	unsigned int curoffset;

	unsigned int i;
	unsigned int * fields;

	//assume error
	toreturn=0;

	//setup masks

	//retn
	retn=insn_by_type(0, insn_return);

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	if(newitempackethandler=SwitchToPacketHandler(0x1A, 1, 0, clientstream, clientprocess, callibrations))
	{
		//find return
		if(curinsn=find_insn(newitempackethandler, retn, 0, 0))
		{
			newitempackethandler->backwards=1;
			if((curinsn=find_insn(newitempackethandler, call_relative, 0, 0))&&(targetaddr=insn_get_target(curinsn, 0)))
			{
				if(curfunc=disasm_function(targetaddr))
				{				
					offsets=GetFieldOffsets(curfunc, 1, 0x24);

					i=0;
					fields=(unsigned int *)&(callibrations->ItemOffsets);

					/* X, Y, Z, ... */
					while((i<10)&&(curoffset=(unsigned int)LL_dequeue(offsets)))
						fields[i++]=(unsigned int)curoffset;

					/* find ID field */
					while((curoffset=(unsigned int)LL_dequeue(offsets))&&(curoffset!=callibrations->ItemOffsets.oItemID))
						;
					fields[i++]=(unsigned int)curoffset;

					/* everything after ID field */
					while((i<15)&&(curoffset=(unsigned int)LL_dequeue(offsets)))
						fields[i++]=(unsigned int)curoffset;

					if(i==15)
						toreturn=1;
						
					LL_delete(offsets);

					delete_asm_function(curfunc);
				}
			}
		}
		delete_asm_function(newitempackethandler);
	}

	delete_insn_mask(retn);
	delete_insn_mask(call_relative);

	return toreturn;
}

int VtblHandlePacketCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	
	unsigned int curaddr, curtarget;
	asm_function * vtbl_handle_packet, * curchunk;

	insn_mask * mov_var_0, * mov_var_0b;
	insn_mask * mov_or_cmp_networkobject, * mov_or_cmp_networkobjectb, * mov_or_cmp_networkobjectc, * mov_or_cmp_networkobjectd;
	insn_mask * switch_jmp;
	insn_mask * mov_reg_var, * mov_reg_varb;
	insn_mask * jmp_or_jcc;
	op_mask * curop;

	x86_insn_t * curinsn;
	
//	x86_op_datatype_t curdata;

	//assume error
	toreturn=0;

	//setup masks:

	//jmp or jcc
	jmp_or_jcc=insn_by_type(0, insn_jmp);
	insn_set(jmp_or_jcc, insn_by_type(0, insn_jcc));
	
	//switch jmp (7 byte jmp with op 0 type = expression)
	switch_jmp=create_insn_mask(1);
	insn_by_type(switch_jmp, insn_jmp);
	insn_by_opcount(switch_jmp, 1);
	curop=insn_op_mask(switch_jmp, 0);
	op_by_type(curop, op_expression);

	//mov reg, var; <- type=0, 2ops, op0-type=reg, op1-type=offset||op1-type=expression
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);

	//mov var(, 0 || reg)
	mov_var_0=create_insn_mask(1);
	insn_by_type(mov_var_0, insn_mov);
	curop=insn_op_mask(mov_var_0, 0);
	op_by_type(curop, op_offset);	
	mov_var_0b=create_insn_mask(1);
	insn_by_type(mov_var_0b, insn_mov);	
	curop=insn_op_mask(mov_var_0b, 0);
	op_by_type(curop, op_expression);	
	insn_set(mov_var_0, mov_var_0b);

	//mov_or_cmp_networkobject
	//	<- 4 possible versions
	//	<- mov reg, expression
	//	<- mov reg, offset
	//	<- cmp reg, expression
	//	<- cmp reg, offset
	//	(op0=reg is ignored here)
	mov_or_cmp_networkobject=create_insn_mask(2);
	insn_by_type(mov_or_cmp_networkobject, insn_cmp);
	curop=insn_op_mask(mov_or_cmp_networkobject, 1);
	op_by_type(curop, op_expression);
	curop->op.data.expression.disp=(unsigned int)callibrations->NetworkObject;
	curop->mask.data.expression.disp=(unsigned int)-1;

	mov_or_cmp_networkobjectb=create_insn_mask(2);
	insn_by_type(mov_or_cmp_networkobjectb, insn_cmp);
	curop=insn_op_mask(mov_or_cmp_networkobjectb, 1);
	op_by_type(curop, op_offset);
	curop->op.data.offset=(unsigned int)callibrations->NetworkObject;
	curop->mask.data.offset=(unsigned int)-1;

	mov_or_cmp_networkobjectc=create_insn_mask(2);
	insn_by_type(mov_or_cmp_networkobjectc, insn_mov);
	curop=insn_op_mask(mov_or_cmp_networkobjectc, 1);
	op_by_type(curop, op_expression);
	curop->op.data.expression.disp=(unsigned int)callibrations->NetworkObject;
	curop->mask.data.expression.disp=(unsigned int)-1;

	mov_or_cmp_networkobjectd=create_insn_mask(2);
	insn_by_type(mov_or_cmp_networkobjectd, insn_mov);
	curop=insn_op_mask(mov_or_cmp_networkobjectd, 1);
	op_by_type(curop, op_offset);
	curop->op.data.offset=(unsigned int)callibrations->NetworkObject;
	curop->mask.data.offset=(unsigned int)-1;

	insn_set(mov_or_cmp_networkobject, mov_or_cmp_networkobjectb);
	insn_set(mov_or_cmp_networkobject, mov_or_cmp_networkobjectc);
	insn_set(mov_or_cmp_networkobject, mov_or_cmp_networkobjectd);
	
	if(curaddr=(unsigned int)callibrations->NetworkObjectVtbl)
	{
		SSetPos(clientstream, curaddr+8*4);
		curaddr=(unsigned int)SReadUInt(clientstream);
	}

	if(curaddr)
	{
		if(vtbl_handle_packet=disasm_chunk(curaddr))
		{
			//mov reg, var (mapinfo <- ignore))
			//mov var, 0 (<- gameserverinfo)
			if(curinsn=find_insn(vtbl_handle_packet, mov_var_0, 0, 0))
			{
				toreturn++;//1
				callibrations->pGameServerInfo=(unsigned int)insn_get_target(curinsn, 0);
			}
			//mov|cmp reg, NetworkObject
			while((curinsn=find_insn(vtbl_handle_packet, mov_or_cmp_networkobject, 0, 0))==0)//if not found, there must be a jmp out of this chunk, find it
			{
				asm_previous(vtbl_handle_packet);
				if(curinsn=find_insn(vtbl_handle_packet, jmp_or_jcc, 0, 0))
				{
					curaddr=(unsigned int)curinsn->addr;
					curtarget=insn_get_target(curinsn, 0);

					vtbl_handle_packet->backwards=1;

					while((curtarget>=vtbl_handle_packet->entrypoint_address)&&(curtarget<=curaddr))
					{
						if(curinsn=find_insn(vtbl_handle_packet, jmp_or_jcc, 0, 0))
							curtarget=insn_get_target(curinsn, 0);
						else
							break;
					}

					vtbl_handle_packet->backwards=0;

					if(curinsn&&curtarget)
					{
						if(curchunk=disasm_chunk(curtarget))
						{
							delete_asm_function(vtbl_handle_packet);
							vtbl_handle_packet=curchunk;
						}
						else
						{
							curinsn=0;
							break;
						}
					}
					else
						break;
				}
			}

			if(curinsn)
			{
				//2jccs_to_the_same_target
				if(curinsn=find_2_jccs_to_the_same_target(vtbl_handle_packet))
				{
					//follow jcc's
					if(curchunk=disasm_chunk(insn_get_target(curinsn, 0)))
					{
						//switch_jmp <- PacketSwitch_Offsets
						if(curinsn=find_insn(curchunk, switch_jmp, 0, 0))
						{
							toreturn++;//2
							callibrations->PacketSwitch_offsets=(unsigned int *)insn_get_target(curinsn, 0);
	
							//backwards
							curchunk->backwards=1;
							//mov reg, expression -> PacketSwitch_indices
							if(curinsn=find_insn(curchunk, mov_reg_var, 0, 0))
							{
								toreturn++;//3
								callibrations->PacketSwitch_indices=(unsigned char *)insn_get_target(curinsn, 1);
							}
						}

						delete_asm_function(curchunk);
					}
				}
			}

			delete_asm_function(vtbl_handle_packet);
		}
	}

	//cleanup
	delete_insn_mask(mov_var_0);
	delete_insn_mask(mov_or_cmp_networkobject);
	delete_insn_mask(switch_jmp);
	delete_insn_mask(mov_reg_var);
	delete_insn_mask(jmp_or_jcc);

	if(toreturn==3)
		return 1;
	else
		return 0;
}

int VtblReceiveAndHandlePacketsCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;

	insn_mask * call_relative, * call_relativeb;
	insn_mask * cmp_reg_FFh;
	insn_mask * jcc_or_jmp;
	insn_mask * cmp_2733h;
	insn_mask * shr_reg_Fh;
	insn_mask * mov_reg_var, * mov_reg_varb;
	op_mask * curop;

	x86_insn_t * curinsn;
	asm_function * vtbl_recv_packets, * curchunk;
	unsigned int curaddr;

	//assume error
	toreturn=0;

	//setup instruction masks:

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	//cmp reg, 0xFF
	cmp_reg_FFh=create_insn_mask(2);
	insn_by_type(cmp_reg_FFh, insn_cmp);
	curop=insn_op_mask(cmp_reg_FFh, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(cmp_reg_FFh, 1);
	curop->op.data.byte=0xFF;
	curop->mask.data.byte=0xFF;

	//jcc_or_jmp
	jcc_or_jmp=insn_by_type(0, insn_jcc);
	insn_set(jcc_or_jmp, insn_by_type(0, insn_jmp));


	//cmp_2733h
	cmp_2733h=create_insn_mask(2);
	insn_by_type(cmp_2733h, insn_cmp);
	curop=insn_op_mask(cmp_2733h, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(cmp_2733h, 1);
	curop->op.data.dword=0x2733;
	curop->mask.data.dword=0xFFFFFFFF;

	//shr_reg_Fh
	shr_reg_Fh=create_insn_mask(2);
	insn_by_type(shr_reg_Fh, insn_shr);
	curop=insn_op_mask(shr_reg_Fh, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(shr_reg_Fh, 1);
	curop->op.data.byte=0xF;
	curop->mask.data.byte=0xFF;

	//mov reg, var
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);

	if(curaddr=(unsigned int)callibrations->NetworkObjectVtbl)
	{
		SSetPos(clientstream, curaddr+5*4);
		curaddr=(unsigned int)SReadUInt(clientstream);
	}

	if(curaddr)
	{
		if(vtbl_recv_packets=disasm_function(curaddr))
		{
			//call recv
			if(curinsn=find_insn(vtbl_recv_packets, call_relative, 0, 0))
			{
				//cmp eax, 0xFF
				if(curinsn=find_insn(vtbl_recv_packets, cmp_reg_FFh, 0, 0))
				{	
					//jcc
					if(curinsn=find_insn(vtbl_recv_packets, jcc_or_jmp, 0, 0))
					{
						//backup current position
						curaddr=curinsn->addr;

						//cmp eax, 0x2733 (checks for specific GetLastError result, so should remain consistent)
						if(curinsn=find_insn(vtbl_recv_packets, cmp_2733h, 0, 0))
						{
							//jcc
							if(curinsn=find_insn(vtbl_recv_packets, jcc_or_jmp, 0, 0))
							{
								if(asm_jump(vtbl_recv_packets, insn_get_target(curinsn, 0)))//follow jcc
								{
									//call
									if(curinsn=find_insn(vtbl_recv_packets, call_relative, 0, 0))
									{
										//follow call
										if(curchunk=disasm_function(insn_get_target(curinsn, 0)))
										{
											//shr reg, 0xF
											if(curinsn=find_insn(curchunk, shr_reg_Fh, 0, 0))
											{
												//backwards
												curchunk->backwards=1;
												//mov reg, expression -> packetinfo
												if(curinsn=find_insn(curchunk, mov_reg_var, 0, 0))
												{
													toreturn++;//1
													callibrations->Packets=(PacketInfo *)insn_get_target(curinsn, 1);
												}
											}
											delete_asm_function(curchunk);
										}
									}
								}
							}
						}
						
						//jump back
						asm_jump(vtbl_recv_packets, curaddr);
						//next jcc (or jmp when already patched) gives VtblReceive_encryptionpatchpos/target
						if(curinsn=find_insn(vtbl_recv_packets, jcc_or_jmp, 0, 0))
						{
							toreturn++;//2
							callibrations->EncryptionPatchAddresses[0]=curinsn->addr;
							callibrations->EncryptionPatchTargets[0]=insn_get_target(curinsn, 0);
						}
					}
				}
			}

			delete_asm_function(vtbl_recv_packets);
		}
	}

	//cleanup
	delete_insn_mask(call_relative);
	delete_insn_mask(cmp_reg_FFh);
	delete_insn_mask(jcc_or_jmp);
	delete_insn_mask(cmp_2733h);
	delete_insn_mask(shr_reg_Fh);
	delete_insn_mask(mov_reg_var);

	if(toreturn==2)
		return 1;
	else
		return 0;
}

int VtblSendPacketCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;

	asm_function * curfunc;
	x86_insn_t * curinsn;
	unsigned int curaddr;

	//assume error
	toreturn=0;

	if(curaddr=(unsigned int)callibrations->NetworkObjectVtbl)
	{
		SSetPos(clientstream, curaddr+6*4);
		curaddr=(unsigned int)SReadUInt(clientstream);
	}

	if(curaddr)
	{
		if(curfunc=disasm_function(curaddr))
		{
			//if curfunc starts with a call, than swap to the target
			if((curinsn=asm_entrypoint(curfunc))&&(curinsn->type==insn_call))
			{
				curaddr=insn_get_target(curinsn, 0);
				delete_asm_function(curfunc);
				curfunc=disasm_function(curaddr);
			}

			if(curfunc)
			{
				//2 jcc's to the same target
				if(curinsn=find_2_jccs_to_the_same_target(curfunc))
				{
					//2nd jcc gives VtblSendPacket_encryptionpatch/pos
					toreturn=1;
					callibrations->EncryptionPatchAddresses[1]=curinsn->addr;
					callibrations->EncryptionPatchTargets[1]=insn_get_target(curinsn, 0);
				}
				
				delete_asm_function(curfunc);
			}
		}
	}

	return toreturn;
}

int SendPacketCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;

	unsigned int i;

	BinaryTreeNode * backup;
	unsigned int backupaddress;

	insn_mask * call_relative, * call_relativeb;
	insn_mask * shl_reg_1Fh;
	//insn_mask * xor_reg_var, * xor_reg_varb;
	insn_mask * xor;
	op_mask * curop;

	asm_function * curfunc, * curfuncb;
	x86_insn_t * curinsn;
	unsigned int curaddr;

	//assume error
	toreturn=0;

	//setup instruction masks:

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	//shl reg, 0x1F
	shl_reg_1Fh=create_insn_mask(2);
	insn_by_type(shl_reg_1Fh, insn_shl);
	curop=insn_op_mask(shl_reg_1Fh, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(shl_reg_1Fh, 1);
	curop->op.data.byte=0x1F;
	curop->mask.data.byte=0xFF;

	//xor reg, var
	/*xor_reg_var=create_insn_mask(2);
	insn_by_type(xor_reg_var, insn_xor);
	curop=insn_op_mask(xor_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(xor_reg_var, 1);
	op_by_type(curop, op_expression);//xor reg, expression
	xor_reg_varb=create_insn_mask(2);
	insn_by_type(xor_reg_varb, insn_xor);
	curop=insn_op_mask(xor_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(xor_reg_varb, 1);
	op_by_type(curop, op_offset);//xor reg, offset
	insn_set(xor_reg_var, xor_reg_varb);//xor reg, var = xor reg, offset||expression*/
	xor=insn_by_type(0, insn_xor);

	if(curfunc=disasm_function((unsigned int)callibrations->SendPacket))
	{
		//first 3 calls are needed to hook this sendpacket function
		for(i=0;i<3;i++)
		{
			if(curinsn=find_insn(curfunc, call_relative, 0, 0))
			{
				callibrations->SendPacketHookAddresses[i]=curinsn->addr;
				callibrations->SendPacketHookTargets[i]=insn_get_target(curinsn, 0);
			}
		}

		//skip one call
		if(curinsn=find_insn(curfunc, call_relative, 0, 0))
		{
			//find 2 jmp|jcc's with the same target, if not found search calls from here
			curfuncb=curfunc;
			while((curinsn=find_2_jccs_to_the_same_target(curfuncb))==0)
			{				
				if((curinsn=find_insn(curfunc, call_relative, 0, 0))&&(curaddr=insn_get_target(curinsn, 0)))
				{
					if(curfuncb && (curfuncb!=curfunc))
						delete_asm_function(curfuncb);
					curfuncb=disasm_function(curaddr);
				}
				else//failed to find the 2jmps!
				{
					if(curfuncb && (curfuncb!=curfunc))
						delete_asm_function(curfuncb);
					curfuncb=0;

					break;
				}
			}

			if(curinsn && curfuncb)
			{
				//-> SendPacket_encryption_patch_pos/target
				toreturn++;//1
				callibrations->EncryptionPatchAddresses[2]=curinsn->addr;
				callibrations->EncryptionPatchTargets[2]=insn_get_target(curinsn, 0);

				/* already patched? -> we didnt get the whole function as the jmp was followed! */
				if(curinsn->type = insn_jmp)
				{
					backupaddress = curinsn->addr + curinsn->size;
					delete_asm_function(curfuncb);
					curfuncb = disasm_function(backupaddress);
				}
	
				//<- find shl esi << 0x1F (either here or in the first call)
				backup=curfuncb->insn_enum->curnode;
				if((curinsn=find_insn(curfuncb, shl_reg_1Fh, 0, 0))==0)
				{
					curfuncb->insn_enum->curnode=backup;
					
					if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
					{
						curaddr=insn_get_target(curinsn, 0);
						delete_asm_function(curfuncb);
						curfuncb=disasm_function(curaddr);
						curinsn=find_insn(curfuncb, shl_reg_1Fh, 0, 0);
					}
				}

				if(curinsn)
				{
					//<- find xor reg, loginkey1
					if(curinsn=find_insn(curfuncb, xor, 0, 0))
					{
						toreturn++;//2
						if(curinsn->operands->next->op.type==op_immediate)
							callibrations->LoginCryptKey1=(unsigned int)insn_get_target(curinsn, 1);
						else
							callibrations->pLoginCryptKey1=(unsigned int *)insn_get_target(curinsn, 1);

						//<- next is xor reg, loginkey1 AGAIN
						if(curinsn=find_insn(curfuncb, xor, 0, 0))
						{
							//find xor reg, loginkey2
							if(curinsn=find_insn(curfuncb, xor, 0, 0))
							{
								toreturn++;//3
								if(curinsn->operands->next->op.type==op_immediate)
									callibrations->LoginCryptKey2=(unsigned int)insn_get_target(curinsn, 1);
								else
									callibrations->pLoginCryptKey2=(unsigned int *)insn_get_target(curinsn, 1);
							}
						}
					}
				}

				if(curfunc!=curfuncb)
					delete_asm_function(curfuncb);
			}
		}

		delete_asm_function(curfunc);
	}

	//cleanup
	delete_insn_mask(call_relative);
	delete_insn_mask(shl_reg_1Fh);
	//delete_insn_mask(xor_reg_var);
	delete_insn_mask(xor);

	if(toreturn==3)
		return 1;
	else
		return 0;
}

int QuitMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	x86_insn_t * curinsn;
	asm_function * quit_game_macro_chunk;

	insn_mask * call_relative, * call_relativeb;
	op_mask * curop;
	
	//assume error
	toreturn=0;

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	if(quit_game_macro_chunk=disasm_macro_chunk(clientstream, 19, callibrations))
	{
		if(curinsn=find_insn(quit_game_macro_chunk, call_relative, 0, 0))
		{
			toreturn++;//1
			callibrations->YesNoGumpSize=GetCallParameter(quit_game_macro_chunk, 1);
		}
		if(curinsn=find_insn(quit_game_macro_chunk, call_relative, 0, 0))
		{
			toreturn++;//2
			callibrations->YesNoGumpConstructor=(pYesNoGumpConstructor)insn_get_target(curinsn, 0);
		}

		delete_asm_function(quit_game_macro_chunk);
	}

	//cleanup
	delete_insn_mask(call_relative);

	if(toreturn==2)
		return 1;
	else
		return 0;
}

int OpenMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	x86_insn_t * curinsn;
	asm_function * open_macro_chunk;

	insn_mask * switch_jmp;
	insn_mask * call_relative, * call_relativeb;
	op_mask * curop;

	asm_function * curchunk, * curchunkb;
	unsigned int targetaddr;

	//assume error
	toreturn=0;

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	//switch jmp (7 byte jmp with op 0 type = expression)
	switch_jmp=create_insn_mask(1);
	insn_by_type(switch_jmp, insn_jmp);
	insn_by_opcount(switch_jmp, 1);
	curop=insn_op_mask(switch_jmp, 0);
	op_by_type(curop, op_expression);

	//get open macro chunk
	if(open_macro_chunk=disasm_macro_chunk(clientstream, 7, callibrations))
	{
		//follow first call (openmacro handler function)
		if(curinsn=find_insn(open_macro_chunk, call_relative, 0, 0))
		{
			if(curchunk=disasm_chunk(insn_get_target(curinsn, 0)))
			{
				//find switch
				if(curinsn=find_insn(curchunk, switch_jmp, 0, 0))
				{
					//switch to first entry in switch table
					targetaddr=insn_get_target(curinsn, 0);//read switch table
					SSetPos(clientstream, targetaddr+0*4);//read at the start of the switchtable
					targetaddr=SReadUInt(clientstream);
					if(targetaddr && (curchunkb=disasm_chunk(targetaddr)))//disassemble the target chunk
					{
						//find call OpenGump(...)
						if(curinsn=find_insn(curchunkb, call_relative, 0, 0))
						{
							toreturn=1;
							callibrations->OpenGump=(pOpenGump)insn_get_target(curinsn, 0);
						}
						
						delete_asm_function(curchunkb);
					}
				}

				delete_asm_function(curchunk);
			}
		}
		delete_asm_function(open_macro_chunk);
	}

	//cleanup
	delete_insn_mask(call_relative);
	delete_insn_mask(switch_jmp);

	return toreturn;
}

int LastTargetMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	x86_insn_t * curinsn;
	asm_function * last_target_macro_chunk;

	insn_mask * test_or_cmp;
	insn_mask * mov_reg_var, * mov_reg_varb;
	insn_mask * dec_or_sub;
	insn_mask * call_relative, * call_relativeb;
	insn_mask * jcc;

	asm_function * curchunk, * curchunkb, * curchunkc;

	op_mask * curop;

	//assume error
	toreturn=0;

	//test or cmp
	test_or_cmp=insn_by_type(0, insn_test);
	insn_set(test_or_cmp, insn_by_type(0, insn_cmp));

	//mov reg, var; <- type=0, 2ops, op0-type=reg, op1-type=offset||op1-type=expression
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	//dec_or_sub
	dec_or_sub=insn_by_type(0, insn_dec);
	insn_set(dec_or_sub, insn_by_type(0, insn_sub));

	//jcc (jnz or jz)
	jcc=insn_by_type(0, insn_jcc);

	if(last_target_macro_chunk=disasm_macro_chunk(clientstream, 21, callibrations))
	{
		//assume error
		curchunk=0;

		//is there a dec or sub instruction in the current chunk?
		if(curinsn=find_insn(last_target_macro_chunk, dec_or_sub, 0, 0))//if so last target handling is done in the switch directly
		{
			curchunk=last_target_macro_chunk;
			asm_entrypoint(curchunk);
			curinsn=find_insn(curchunk, test_or_cmp, 0, 0);
		}
		else
		{
			//no dec or sub, then this client version has the lasttarget handling in a subprocedure, so follow the first call and swap chunks
			asm_lowest(last_target_macro_chunk);
			if(curinsn=find_insn(last_target_macro_chunk, call_relative, 0, 0))
			{
				curchunk=disasm_function(insn_get_target(curinsn, 0));
				curinsn=find_insn(curchunk, test_or_cmp, 0, 0);
			}
		}

		if(curchunk&&curinsn)
		{
			//backwards: mov reg, bTargetCursorVisible;
			curchunk->backwards=1;
			if(curinsn=find_insn(curchunk, mov_reg_var, 0, 0))
			{
				//bTargCurs
				toreturn++;//1
				callibrations->bTargetCursorVisible=(unsigned char *)insn_get_target(curinsn, 1);
			}
			
			curchunk->backwards=0;

			//find call, if not found, follow jcc and find call there
			if((curinsn=find_insn(curchunk, call_relative, 0, 0))==0)
			{
				if(curinsn=find_insn(curchunk, jcc, 0, 0))
				{
					curchunkb=disasm_chunk(insn_get_target(curinsn, 0));
					curinsn=find_insn(curchunkb, call_relative, 0, 0);
				}
			}
			else
				curchunkb=curchunk;


			if(curchunkb&&curinsn)
			{
				/*
				if(callibrations->LastTargetX=(unsigned short *)GetCallParameter(curchunkb, 4))
					toreturn++;//3
				if(callibrations->LastTargetY=(unsigned short *)GetCallParameter(curchunkb, 5))
					toreturn++;//4
				if(callibrations->LastTargetZ=(short *)GetCallParameter(curchunkb, 6))
					toreturn++;//5
				if(callibrations->LastTargetTile=(unsigned short *)GetCallParameter(curchunkb, 7))
					toreturn++;//6
				*/
				curchunkb->backwards=1;

				while((curinsn=find_insn(curchunkb, mov_reg_var, 0, 0))&&(curinsn->operands->next->op.datatype!=op_word))
					;

				if(curinsn)
				{
					callibrations->LastTargetX=(unsigned short *)insn_get_target(curinsn, 1);
					toreturn++;//2
				}
				if(curinsn=find_insn(curchunkb, mov_reg_var, 0, 0))
				{
					callibrations->LastTargetY=(unsigned short *)insn_get_target(curinsn, 1);
					toreturn++;//3
				}
				if(curinsn=find_insn(curchunkb, mov_reg_var, 0, 0))
				{
					callibrations->LastTargetZ=(short *)insn_get_target(curinsn, 1);
					toreturn++;//4
				}
				if(curinsn=find_insn(curchunkb, mov_reg_var, 0, 0))
				{
					callibrations->LastTargetTile=(unsigned short *)insn_get_target(curinsn, 1);
					toreturn++;//5
				}

				if(curinsn=find_insn(curchunkb, mov_reg_var, 0, 0))
				{
					callibrations->LastTargetKind=(unsigned int *)insn_get_target(curinsn, 1);
					toreturn++;//6
				}

				curchunkb->backwards=0;

				if(curinsn=find_insn(curchunkb, jcc, 0, 0))
				{
					if(curchunkc=disasm_chunk(insn_get_target(curinsn, 0)))
					{
						//mov reg, LastTargetID
						if(curinsn=find_insn(curchunkc, mov_reg_var, 0, 0))
						{
							toreturn++;//7
							callibrations->LastTargetID=(unsigned int *)insn_get_target(curinsn, 1);
						}

						delete_asm_function(curchunkc);
					}
				}

				/*
					//mov reg, LastTargetKind
					if(curinsn=find_insn(curchunkb, mov_reg_var, 0, 0))
					{
						toreturn++;//2
						callibrations->LastTargetKind=(unsigned int *)insn_get_target(curinsn, 1);
					}

					//jcc -> follow
					if(curinsn=find_insn(curchunkb, jcc, 0, 0))
					{
						if(curchunkc=disasm_chunk(insn_get_target(curinsn, 0)))
						{
							//mov reg, LastTargetID
							if(curinsn=find_insn(curchunkc, mov_reg_var, 0, 0))
							{
								toreturn++;//3
								callibrations->LastTargetID=(unsigned int *)insn_get_target(curinsn, 1);
							}

							delete_asm_function(curchunkc);
						}
					}
					if(curinsn=find_insn(curchunkb, call_relative, 0, 0))
					{
						//call has LastTargetX, Y, Z, Tile as parameters 4, 5, 6 and 7 respectively
						if(callibrations->LastTargetX=(unsigned short *)GetCallParameter(curchunkb, 4))
							toreturn++;//4
						if(callibrations->LastTargetY=(unsigned short *)GetCallParameter(curchunkb, 5))
							toreturn++;//5
						if(callibrations->LastTargetZ=(short *)GetCallParameter(curchunkb, 6))
							toreturn++;//6
						if(callibrations->LastTargetTile=(unsigned short *)GetCallParameter(curchunkb, 7))
							toreturn++;//7
					}
					*/

				if(curchunkb!=curchunk)
					delete_asm_function(curchunkb);
			}

			if(curchunk!=last_target_macro_chunk)
				delete_asm_function(curchunk);
		}

		delete_asm_function(last_target_macro_chunk);
	}

	//cleanup
	delete_insn_mask(test_or_cmp);
	delete_insn_mask(mov_reg_var);
	delete_insn_mask(dec_or_sub);
	delete_insn_mask(call_relative);
	delete_insn_mask(jcc);

	if(toreturn==7)
		return 1;
	else
		return 0;
}

int ToggleAlwaysRunMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	x86_insn_t * curinsn;
	asm_function * toggle_alwaysrun_macro_chunk, * fbarkgumpconstructor;

	insn_mask * push, * call_relative, * call_relativeb, * mov_var_reg, * mov_var_regb, * _call;
	op_mask * curop;

	//assume error
	toreturn=0;

	//setup masks
	
	//mov var, reg
	mov_var_reg=create_insn_mask(2);
	insn_by_type(mov_var_reg, insn_mov);
	curop=insn_op_mask(mov_var_reg, 1);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_var_reg, 0);
	op_by_type(curop, op_offset);
	mov_var_regb=create_insn_mask(2);
	insn_by_type(mov_var_regb, insn_mov);
	curop=insn_op_mask(mov_var_regb, 1);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_var_regb, 0);
	op_by_type(curop, op_expression);
	insn_set(mov_var_reg, mov_var_regb);

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	_call = insn_by_type(0, insn_call);

	//push
	push=insn_by_type(0, insn_push);

	if(toggle_alwaysrun_macro_chunk=disasm_macro_chunk(clientstream, 31, callibrations))
	{
		if(curinsn=find_insn(toggle_alwaysrun_macro_chunk, mov_var_reg, 0, 0))
		{
			//bAlwaysRun
			toreturn++;//1
			callibrations->bAlwaysRun=(int *)insn_get_target(curinsn, 0);
		}

		if(curinsn=find_insn(toggle_alwaysrun_macro_chunk, call_relative, 0, 0))
		{
			//call Allocator
			toreturn++;//2
			callibrations->Allocator=(pAllocator)insn_get_target(curinsn, 0);

			//search backwards for push BarkGumpSize
			toggle_alwaysrun_macro_chunk->backwards=1;
			if(curinsn=find_insn(toggle_alwaysrun_macro_chunk, push, 0, 0))
			{
				//push BarkGumpSize
				toreturn++;//3
				callibrations->BarkGumpSize=(unsigned int)insn_get_target(curinsn, 0);
			}

			//back to the call
			toggle_alwaysrun_macro_chunk->backwards=0;
			find_insn(toggle_alwaysrun_macro_chunk, call_relative, 0, 0);
		}

		if(curinsn=find_insn(toggle_alwaysrun_macro_chunk, call_relative, 0, 0))
		{
			//call BarkGumpConstructor
			toreturn++;//4
			callibrations->BarkGumpConstructor=(pBarkGumpConstructor)insn_get_target(curinsn, 0);

			//last call from BarkGumpConstructor is ShowGump
			if(fbarkgumpconstructor = disasm_function((unsigned int)callibrations->BarkGumpConstructor))
			{
				asm_highest(fbarkgumpconstructor);
				//asm_previous(fbarkgumpconstructor);
				fbarkgumpconstructor->backwards=1;
				
				if(curinsn = find_insn(fbarkgumpconstructor, _call, 0, 0))
				{
					toreturn++;//5
					callibrations->ShowGump = (pShowGump)insn_get_target(curinsn, 0);
				}
				delete_asm_function(fbarkgumpconstructor);
			}
		}

		delete_asm_function(toggle_alwaysrun_macro_chunk);
	}

	//cleanup
	delete_insn_mask(call_relative);
	delete_insn_mask(push);
	delete_insn_mask(mov_var_reg);
	delete_insn_mask(_call);

	if(toreturn==5)
		return 1;
	else
		return 0;
}

int LastSpellMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	x86_insn_t * curinsn;
	asm_function * last_spell_macro_chunk;

	insn_mask * test_or_cmp;
	insn_mask * mov_reg_var, * mov_reg_varb;
	op_mask * curop;

	//assume error
	toreturn=0;

	//test or cmp
	test_or_cmp=insn_by_type(0, insn_test);
	insn_set(test_or_cmp, insn_by_type(0, insn_cmp));

	//mov reg, var; <- type=0, 2ops, op0-type=reg, op1-type=offset||op1-type=expression
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);

	if(last_spell_macro_chunk=disasm_macro_chunk(clientstream, 15, callibrations))
	{
		if(curinsn=find_insn(last_spell_macro_chunk, test_or_cmp, 0, 0))
		{
			if(curinsn=find_insn(last_spell_macro_chunk, mov_reg_var, 0, 0))
			{
				callibrations->LastSpell=(unsigned int *)insn_get_target(curinsn, 1);
				toreturn=1;
			}
		}

		delete_asm_function(last_spell_macro_chunk);
	}

	//cleanup
	delete_insn_mask(test_or_cmp);
	delete_insn_mask(mov_reg_var);

	return toreturn;
}

int LastSkillMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	x86_insn_t * curinsn;
	asm_function * last_skill_macro_chunk;

	insn_mask * test_or_cmp;
	insn_mask * mov_reg_var, * mov_reg_varb;
	op_mask * curop;

	//assume error
	toreturn=0;

	//test or cmp
	test_or_cmp=insn_by_type(0, insn_test);
	insn_set(test_or_cmp, insn_by_type(0, insn_cmp));

	//mov reg, var; <- type=0, 2ops, op0-type=reg, op1-type=offset||op1-type=expression
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);

	if(last_skill_macro_chunk=disasm_macro_chunk(clientstream, 13, callibrations))
	{
		if(curinsn=find_insn(last_skill_macro_chunk, test_or_cmp, 0, 0))
		{
			if(curinsn=find_insn(last_skill_macro_chunk, mov_reg_var, 0, 0))
			{
				callibrations->LastSkill=(unsigned int *)insn_get_target(curinsn, 1);
				toreturn=1;
			}
		}

		delete_asm_function(last_skill_macro_chunk);
	}

	//cleanup
	delete_insn_mask(test_or_cmp);
	delete_insn_mask(mov_reg_var);

	return toreturn;
}

int LastObjectMacroCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	asm_function * lastobject_chunk;
	x86_insn_t * curinsn;

	insn_mask * test_or_cmp;
	insn_mask * mov_reg_var, * mov_reg_varb;
	insn_mask * mov_var_reg, * mov_var_regb;
	insn_mask * call_relative, * call_relativeb;
	insn_mask * temp;
	insn_mask * cmp;
	insn_mask * retn;
	insn_mask * ccall_stacksize_14h, * add_esp_14h;
	op_mask * curop;
	x86_op_datatype_t curdata;

	unsigned int loopaddress, findstatusgump_address;

	asm_function * curfunc, * curfuncb;

	//assume error
	toreturn=0;

	//build masks

	//ccall stacksize 0x14 = sequence: call xxxxxxxx;  add esp, 0x14;
	add_esp_14h=create_insn_mask(2);
	insn_by_type(add_esp_14h, insn_add);
	curop=insn_op_mask(add_esp_14h, 0);
	op_by_type(curop, op_register);//we could check for data.reg == esp maybe too?
	curop=insn_op_mask(add_esp_14h, 1);
	memset(&curdata, 0, sizeof(x86_op_datatype_t));
	curdata.byte=0x14;
	op_by_data(curop, curdata);
	ccall_stacksize_14h=insn_by_type(0, insn_call);
	insn_seq(ccall_stacksize_14h, add_esp_14h);

	//retn
	retn=insn_by_type(0, insn_return);
	
	//cmp
	cmp=insn_by_type(0, insn_cmp);

	//test or cmp
	test_or_cmp=insn_by_type(0, insn_test);
	insn_set(test_or_cmp, insn_by_type(0, insn_cmp));

	//mov reg, var; <- type=0, 2ops, op0-type=reg, op1-type=offset||op1-type=expression
	mov_reg_var=create_insn_mask(2);
	insn_by_type(mov_reg_var, insn_mov);
	curop=insn_op_mask(mov_reg_var, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_var, 1);
	op_by_type(curop, op_offset);
	mov_reg_varb=create_insn_mask(2);
	insn_by_type(mov_reg_varb, insn_mov);
	curop=insn_op_mask(mov_reg_varb, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_varb, 1);
	op_by_type(curop, op_expression);
	insn_set(mov_reg_var, mov_reg_varb);

	//mov var, reg; <- type=0, 2ops, op0-type=offset||op1-type=expression, op1-type=reg
	mov_var_reg=create_insn_mask(2);
	insn_by_type(mov_var_reg, insn_mov);
	curop=insn_op_mask(mov_var_reg, 1);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_var_reg, 0);
	op_by_type(curop, op_offset);
	mov_var_regb=create_insn_mask(2);
	insn_by_type(mov_var_regb, insn_mov);
	curop=insn_op_mask(mov_var_regb, 1);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_var_regb, 0);
	op_by_type(curop, op_expression);
	insn_set(mov_var_reg, mov_var_regb);

	//call (relative) = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	call_relative=create_insn_mask(1);
	insn_by_type(call_relative,insn_call);
	curop=insn_op_mask(call_relative, 0);
	op_by_type(curop, op_relative_near);
	call_relativeb=create_insn_mask(1);
	insn_by_type(call_relativeb,insn_call);
	curop=insn_op_mask(call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(call_relative, call_relativeb);

	temp=test_or_cmp;

	if(lastobject_chunk=disasm_macro_chunk(clientstream, 16, callibrations))
	{
		//find bLoggedIn
		if(curinsn=find_insn(lastobject_chunk, test_or_cmp, &temp, 0))
		{
			if(temp==test_or_cmp)//test found
			{
				//find backwards mov reg, bLoggedIn
				lastobject_chunk->backwards=1;
				if(curinsn=find_insn(lastobject_chunk, mov_reg_var, &temp, 0))
				{
					toreturn++;//1
					//seconc op = bLoggedIn
					callibrations->bLoggedIn=(int *)insn_get_target(curinsn, 1);
				}
				lastobject_chunk->backwards=0;
			}
			else//cmp found
			{
				toreturn++;//1
				//first op = bLoggedIn
				callibrations->bLoggedIn=(int *)insn_get_target(curinsn, 0);
			}
		}

		//mov eax, LastObjectId
		if(curinsn=find_insn(lastobject_chunk, mov_reg_var, &temp, 0))
		{
			toreturn++;//2
			callibrations->LastObjectID=(unsigned int *)insn_get_target(curinsn, 1);
		}

		//call FindItemByID
		if(curinsn=find_insn(lastobject_chunk, call_relative, 0, 0))
		{
			//FindItemByID
			callibrations->FindItemByID=(pFindItemByID)insn_get_target(curinsn, 0);
			if(curfunc=disasm_function((unsigned int)callibrations->FindItemByID))//callibrations within finditem_by_id
			{
				//mov eax, Player
				if(curinsn=find_insn(curfunc, mov_reg_var, &temp, 0))
				{
					toreturn++;//3
					callibrations->Player=(Item **)insn_get_target(curinsn, 1);
				}
	
				//cmp reg, expression -> oItemID
				if(curinsn=find_insn(curfunc, cmp, 0, 0))
				{
					toreturn++;//4
					callibrations->ItemOffsets.oItemID=(unsigned int)insn_get_target(curinsn, 1);
				}
	
				//mov eax, ItemList
				if(curinsn=find_insn(curfunc, mov_reg_var, &temp, 0))
				{
					toreturn++;//5
					callibrations->Items=(Item **)insn_get_target(curinsn, 1);
				}
	
				//find loop
				//mov reg, expression -> oItemNext
				if(loopaddress=FindLoop(curfunc))
				{
					asm_jump(curfunc, loopaddress);
					if(curinsn=find_insn(curfunc, mov_reg_var ,&temp,0))
					{
						toreturn++;//6
						callibrations->ItemOffsets.oItemNext=(unsigned int)insn_get_target(curinsn, 1);
					}
				}

				delete_asm_function(curfunc);
			}
		}

		//call LastObjectFunction(Item *)
		if(curinsn=find_insn(lastobject_chunk, call_relative, 0, 0))
		{
			if(curfunc=disasm_function(insn_get_target(curinsn, 0)))//callibrations within LastObjectFunction
			{
				//mov LastObjectID, reg
				while(curinsn=find_insn(curfunc, mov_var_reg, 0, 0))
				{
					if(insn_get_target(curinsn, 0)==(unsigned int)callibrations->LastObjectID)
						break;
				}
				
				//mov LastObjectType, reg
				if(curinsn=find_insn(curfunc, mov_var_reg, 0, 0))
				{
					toreturn++;//7
					callibrations->LastObjectType=(unsigned int *)insn_get_target(curinsn, 0);
				}
				
				
				//find from last return
				findstatusgump_address=0;
				if(curinsn=find_insn(curfunc, retn, 0, 0))
				{
					//<- find backwards: call FindStatusGump; add esp, 0x14;
					curfunc->backwards=1;
					if(curinsn=find_insn(curfunc, ccall_stacksize_14h, 0, 0))
						findstatusgump_address=insn_get_target(curinsn, 0);
					curfunc->backwards=0;
				}
				//if findstatusgump call was not found, then it's done in one of the subfunctions
				//	so look for it there
				if((findstatusgump_address==0)&&(curinsn=find_insn(curfunc, retn, 0, 0)))
				{
					curfunc->backwards=1;
					while((findstatusgump_address==0)&&(curinsn=find_insn(curfunc, call_relative, 0, 0))&&(curfuncb=disasm_function(insn_get_target(curinsn, 0))))
					{
						if(curinsn=find_insn(curfuncb, ccall_stacksize_14h, 0, 0))
						{
							findstatusgump_address=insn_get_target(curinsn, 0);
							delete_asm_function(curfunc);
							curfunc=curfuncb;
						}
						else
							delete_asm_function(curfuncb);
					}
					curfunc->backwards=0;
				}

				if(findstatusgump_address!=0)
				{
					if(curfuncb=disasm_function(findstatusgump_address))//find within FindStatusGump
					{
						if(curinsn=find_insn(curfuncb, cmp, 0, 0))//	-> first cmp
						{
							//	-> mov reg, expression -> oGumpItem
							curfuncb->backwards=1;
							if(curinsn=find_insn(curfuncb, mov_reg_var, &temp, 0))
							{
								toreturn++;//8
								callibrations->oGumpItem=(unsigned int)insn_get_target(curinsn, 1);
							}
							curfuncb->backwards=0;
						}
						
						delete_asm_function(curfuncb);
					}
					//backwards: mov reg, pGumpList
					curfunc->backwards=1;
					if(curinsn=find_insn(curfunc, mov_reg_var, &temp, 0))
					{
						toreturn++;//9
						callibrations->Gumps=(Gump **)insn_get_target(curinsn, 1);
					}
					curfunc->backwards=0;
					//forward : call FindStatusGump, call WriteDoubleClickPacket, call _SendPacket
					if((curinsn=find_insn(curfunc, call_relative, 0, 0))&&(curinsn=find_insn(curfunc, call_relative, 0, 0))&&(curinsn=find_insn(curfunc, call_relative, 0, 0)))
					{
						if(curfuncb=disasm_function(insn_get_target(curinsn, 0)))//follow _SendPacket
						{
							//find call SendPacket
							if(curinsn=find_insn(curfuncb, call_relative, 0, 0))
							{
								toreturn++;//10
								callibrations->SendPacket=(pSendPacket)insn_get_target(curinsn, 0);
							}
							delete_asm_function(curfuncb);
						}
					}
				}

				delete_asm_function(curfunc);
			}
		}

		delete_asm_function(lastobject_chunk);
	}

	//cleanup
	delete_insn_mask(test_or_cmp);
	delete_insn_mask(mov_reg_var);
	delete_insn_mask(call_relative);
	delete_insn_mask(mov_var_reg);
	delete_insn_mask(cmp);
	delete_insn_mask(ccall_stacksize_14h);
	delete_insn_mask(retn);

	if(toreturn==10)
		return 1;
	else
		return 0;
}

int MultiClientPatchingCallibrations(asm_function * winmain, UOCallibrations * callibrations)
{
	int toreturn;
	insn_mask * jmp_or_jcc;
	insn_mask * _insn_call_relative, * _insn_call_relativeb;
	insn_mask * call_test;
	insn_mask * push_40h;
	insn_mask * cmp_B7h;
	insn_mask * call_API_cmp_B7h;
	op_mask * curop;
	x86_op_datatype_t curdata;

	x86_insn_t * curinsn;
	asm_function * curfunc;

	//assume error
	toreturn=0;

	//build instruction masks 

	//jmp_or_jcc :: jmp, jnz or jz (we can't assume any here as the client might already have been patched)
	jmp_or_jcc=insn_by_type(0, insn_jmp);
	insn_set(jmp_or_jcc, insn_by_type(0, insn_jcc));

	//relative call = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	_insn_call_relative=create_insn_mask(1);
	insn_by_type(_insn_call_relative,insn_call);
	curop=insn_op_mask(_insn_call_relative, 0);
	op_by_type(curop, op_relative_near);
	_insn_call_relativeb=create_insn_mask(1);
	insn_by_type(_insn_call_relativeb,insn_call);
	curop=insn_op_mask(_insn_call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(_insn_call_relative, _insn_call_relativeb);

	//call xxxxxxxx; test eax, eax; is what we are looking for actually
	call_test=insn_by_type(0, insn_call);
	insn_seq(call_test, insn_by_type(0, insn_test));

	//push 0x40
	push_40h=create_insn_mask(1);//at least 1 op required
	insn_by_type(push_40h, insn_push);//type = push
	curop=insn_op_mask(push_40h, 0);//get op 0
	memset(&curdata, 0, sizeof(x86_op_datatype_t));
	curdata.byte=0x40;
	op_by_data(curop, curdata);//op data must be 0x40

	//call_API_cmp_B7h
	//	we are looking for call GetLastError(); cmp eax, 0xB7;
	//	we don't really need any checks on the call, since no other should be followed by cmp eax, 0xB7
	cmp_B7h=create_insn_mask(2);//at least 2 ops
	insn_by_type(cmp_B7h, insn_cmp);
	curop=insn_op_mask(cmp_B7h, 1);
	memset(&curdata, 0, sizeof(x86_op_datatype_t));
	curdata.byte=0xB7;
	op_by_data(curop, curdata);
	call_API_cmp_B7h=insn_seq(insn_by_type(0, insn_call), cmp_B7h);

	//from start of winmain
	asm_entrypoint(winmain);
	
	//find jcc, follow, then follow first call
	if(curinsn=find_insn(winmain, jmp_or_jcc, 0, 1))
	{
		if(curinsn=asm_jump(winmain, insn_get_target(curinsn, 0)))
		{
			asm_previous(winmain);
			if(curinsn=find_insn(winmain, _insn_call_relative, 0, 0))
			{
				if(curfunc=disasm_function(insn_get_target(curinsn, 0)))
				{
					if(curinsn=find_insn(curfunc, call_test, 0, 0))
					{
						if(curinsn=find_insn(curfunc, jmp_or_jcc, 0, 0))
						{
							toreturn++;
							callibrations->MultiClientPatchAddresses[0]=curinsn->addr;
							callibrations->MultiClientPatchTargets[0]=insn_get_target(curinsn, 0);
						}
					}
					if(curinsn=find_insn(curfunc, call_test, 0, 0))
					{
						if(curinsn=find_insn(curfunc, jmp_or_jcc, 0, 0))
						{
							toreturn++;
							callibrations->MultiClientPatchAddresses[1]=curinsn->addr;
							callibrations->MultiClientPatchTargets[1]=insn_get_target(curinsn, 0);
						}
					}
					delete_asm_function(curfunc);
				}
			}
		}
		if(curinsn=find_insn(winmain, push_40h, 0, 0))
		{
			winmain->backwards=1;
			if(curinsn=find_insn(winmain, jmp_or_jcc, 0, 0))
			{
				toreturn++;
				callibrations->MultiClientPatchAddresses[2]=curinsn->addr;
				callibrations->MultiClientPatchTargets[2]=insn_get_target(curinsn, 0);
			}
			winmain->backwards=0;

			if(curinsn=find_insn(winmain, call_API_cmp_B7h, 0, 0))
			{
				winmain->backwards=1;

				if(curinsn=find_insn(winmain, jmp_or_jcc, 0, 0))
				{
					toreturn++;
					callibrations->MultiClientPatchAddresses[3]=curinsn->addr;
					callibrations->MultiClientPatchTargets[3]=insn_get_target(curinsn, 0);
				}
				
				winmain->backwards=0;
				asm_next(winmain);
				if(curinsn=find_insn(winmain, jmp_or_jcc, 0, 0))
				{
					toreturn++;
					callibrations->MultiClientPatchAddresses[4]=curinsn->addr;
					callibrations->MultiClientPatchTargets[4]=insn_get_target(curinsn, 0);
				}
			}

		}

	}
	
	delete_insn_mask(jmp_or_jcc);
	delete_insn_mask(call_API_cmp_B7h);
	delete_insn_mask(call_test);
	delete_insn_mask(push_40h);
	delete_insn_mask(_insn_call_relative);

	if(toreturn!=5)
		return 0;
	else
		return 1;
}

int MacroCallibrations(asm_function * winmain, UOCallibrations * callibrations)
{
	int toreturn;
	insn_mask * push_0_call;
	insn_mask * call;
	insn_mask * _4_calls;
	insn_mask * _8_calls;
	insn_mask * switch_jmp;
	insn_mask * push;
	insn_mask * mov;
	insn_mask * cmp;
	op_mask * curop;
	asm_function * curfunc, * curfuncb, * curfuncc;
	x86_insn_t * curinsn;
	x86_op_datatype_t data;
	unsigned int i;

	unsigned int macrolist_reg, index_reg;

	//assume error
	toreturn=0;

	//cmp
	cmp=insn_by_type(0, insn_cmp);

	//push 0; call;
	push_0_call=create_insn_mask(1);
	insn_by_type(push_0_call, insn_push);
	//insn_by_opcount(push_0_call, 1);<- don't, esp is present as a 2nd hidden op
	curop=insn_op_mask(push_0_call, 0);
	memset(&data,0,sizeof(x86_op_datatype_t));
	data.byte=0;
	op_by_data(curop, data);
	insn_seq(push_0_call, insn_by_type(0, insn_call));

	//call;
	call=insn_by_type(0,insn_call);

	//4 calls in a row
	_4_calls=insn_by_type(0, insn_call);//call 1
	insn_seq(_4_calls, insn_by_type(0, insn_call));//call 2
	insn_seq(_4_calls, insn_by_type(0, insn_call));//call 3
	insn_seq(_4_calls, insn_by_type(0, insn_call));//call 4

	//8 calls in a row
	_8_calls=insn_by_type(0, insn_call);		   //call 1
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 2
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 3
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 4
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 5
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 6
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 7
	insn_seq(_8_calls, insn_by_type(0, insn_call));//call 8

	//switch jmp (7 byte jmp with op 0 type = expression)
	switch_jmp=create_insn_mask(1);
	insn_by_type(switch_jmp, insn_jmp);
	insn_by_opcount(switch_jmp, 1);
	curop=insn_op_mask(switch_jmp, 0);
	op_by_type(curop, op_expression);

	//push
	push=insn_by_type(0, insn_push);

	//mov
	mov=insn_by_type(0, insn_mov);

	//to end of winmain
	asm_highest(winmain);
	//search backwards
	winmain->backwards=1;
	//find push 0; call
	if(curinsn=find_insn(winmain, push_0_call, 0, 0))
	{
		curinsn=asm_next(winmain);//get the call
		if(curfunc=disasm_function(insn_get_target(curinsn, 0)))//follow call
		{
			callibrations->GeneralPurposeFunction=(pGeneralPurposeFunction)curfunc->entrypoint_address;

			/*
				TODO - review macro function callibration; any additional handler in the 'general purpose' update function could result in callibration failure here
			*/

			// older clients - 8 calls in a row, follow the 8th call
			// newer clients - 4 calls in a row, some calls with stack cleanup, then again 4 calls in a row -> follow the 3th of the second batch of 4 calls
			if( curinsn=find_insn(curfunc, _8_calls, 0, 1) )
			{
				for(i=0;i<7;i++)
					curinsn=find_insn(curfunc, call, 0, 1);
			}
			else
			{
				asm_lowest(curfunc);
				if((curinsn=find_insn(curfunc, _4_calls, 0, 1)) && (curinsn=find_insn(curfunc, _4_calls, 0, 1)))
				{
					for(i=0;i<2;i++)
						curinsn=find_insn(curfunc, call, 0, 1);
				}
			}

			//if( curinsn )
			//{				
				if(curinsn&&(curfuncb=disasm_function(insn_get_target(curinsn, 0))))
				{
					//now find the first call, this is the call to the macro-function
					if(curinsn=find_insn(curfuncb, call, 0, 0))
					{
						callibrations->Macro=(pMacro)insn_get_target(curinsn, 0);

						//get macro switch table from macro function
						if(curfuncc=disasm_chunk((unsigned int)callibrations->Macro))
						{
							if(curinsn=find_insn(curfuncc, switch_jmp, 0, 0))
							{
								//read event macro switch table
								callibrations->MacroSwitchTable=insn_get_target(curinsn, 0);

								//do callibrations from specific macros here
								toreturn++;//1
							}

							delete_asm_function(curfuncc);
						}

						//do other callibrations:
						curfuncb->backwards=1;
						//we need to trace the two parameters to the macro call to the memory locations used to
						//	reference the currently executing macrolist and index on that list
						if(curinsn=find_insn(curfuncb, push, 0, 0))
						{
							if(curinsn->operands->op.type == op_register)
								macrolist_reg=curinsn->operands->op.data.reg.id;
							else
							{
								macrolist_reg=(unsigned int)-1;
								callibrations->CurrentMacro=(MacroList **)curinsn->operands->op.data.offset;
							}
						}
						
						if(curinsn=find_insn(curfuncb, push, 0, 0))
						{
							if(curinsn->operands->op.type == op_register)
								index_reg=curinsn->operands->op.data.reg.id;
							else
							{
								index_reg=(unsigned int)-1;
								callibrations->CurrentMacroIndex=(unsigned int **)curinsn->operands->op.data.offset;
							}
						}
						while(((macrolist_reg!=(unsigned int)-1)||(index_reg!=(unsigned int)-1))&&(curinsn=find_insn(curfuncb, mov, 0, 0)))
						{
							//find mov's and check if the target is one of the specified registries
							if((curinsn->operands->op.type==op_register)&&(curinsn->operands->next->op.type!=op_register))
							{
								if(curinsn->operands->op.data.reg.id==macrolist_reg)
								{
									macrolist_reg=(unsigned int)-1;
									callibrations->CurrentMacro=(MacroList **)insn_get_target(curinsn, 1);
								}
								else if(curinsn->operands->op.data.reg.id==index_reg)
								{
									index_reg=(unsigned int)-1;
									callibrations->CurrentMacroIndex=(unsigned int **)insn_get_target(curinsn, 1);
								}
							}
						}
					}
					delete_asm_function(curfuncb);
				}
			//}

		//->	end
			asm_highest(curfunc);
		//-> backwards cmp edi, 1
			curfunc->backwards=1;
			while((curinsn=find_insn(curfunc, cmp, 0, 0))&&(curinsn->operands->next->op.data.byte!=1))
				;

		//-> backwards call
			curfunc->backwards=1;
			if(curfunc=EnterNextCall(curfunc, 1))
			{
			//-> push 0x11
				while((curinsn=find_insn(curfunc, push, 0, 0))&&(curinsn->operands->op.data.byte!=0x11))
					;

			//-> backwards call
				curfunc->backwards=1;
				if(curfunc=EnterNextCall(curfunc, 1))
				{
					asm_highest(curfunc);
					curfunc->backwards=1;//last call...
					if(curinsn=find_insn(curfunc, call, 0, 0))
					{
						toreturn++;
						callibrations->pShowItemPropGump=insn_get_target(curinsn, 0);
					}
				}
			}
			
			if(curfunc)
				delete_asm_function(curfunc);
		}
	}
	//restore directionality, should winmain be reused later on
	winmain->backwards=0;

	delete_insn_mask(push_0_call);
	delete_insn_mask(call);
	delete_insn_mask(_4_calls);
	delete_insn_mask(_8_calls);
	delete_insn_mask(switch_jmp);
	delete_insn_mask(push);
	delete_insn_mask(mov);
	delete_insn_mask(cmp);

	if(toreturn==2)
		return 1;
	else
		return 0;
}

//callibrations from specific member functions on the Player Mobile's class
int PlayerVtblCallibrations(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	int toreturn;
	
	x86_insn_t * curinsn;
	asm_function * curfunc;
	unsigned int curaddr;

	insn_mask * mov_expression;
	//insn_mask * cmp_expression_0;
	insn_mask * cmp_or_test;
	insn_mask * add_reg_immediate;
	insn_mask * mov_reg_expression;
	insn_mask * jcc_or_jmp;
	
	op_mask * curop;

	//assume error
	toreturn=0;

	//setup instruction masks:

	//mov_expression
	mov_expression=create_insn_mask(1);//don't care about second op
	insn_by_type(mov_expression, insn_mov);
	curop=insn_op_mask(mov_expression, 0);
	op_by_type(curop, op_expression);

	//jcc_or_jmp
	jcc_or_jmp=insn_by_type(0, insn_jcc);
	insn_set(jcc_or_jmp, insn_by_type(0, insn_jmp));

	//cmp_expression_0
	/*
	cmp_expression_0=create_insn_mask(2);
	insn_by_type(cmp_expression_0, insn_cmp);
	curop=insn_op_mask(cmp_expression_0, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(cmp_expression_0, 1);
	op_by_type(curop, op_immediate);
	curop->op.data.dword=0;
	curop->mask.data.dword=0xFFFFFFFF;//(<- the one we look for is a dword expression)
	*/
	cmp_or_test=insn_by_type(0, insn_cmp);
	insn_set(cmp_or_test, insn_by_type(0, insn_test));

	//add_reg_immediate
	add_reg_immediate=create_insn_mask(2);
	insn_by_type(add_reg_immediate, insn_add);
	curop=insn_op_mask(add_reg_immediate, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(add_reg_immediate, 1);
	op_by_type(curop, op_immediate);

	//mov_reg_expression
	mov_reg_expression=create_insn_mask(2);//don't care about second op
	insn_by_type(mov_reg_expression, insn_mov);
	curop=insn_op_mask(mov_reg_expression, 0);
	op_by_type(curop, op_register);
	curop=insn_op_mask(mov_reg_expression, 1);
	op_by_type(curop, op_expression);

	if(callibrations->pPlayerVtbl!=0)
	{
		//a. pPlayerVtbl[0x19] = MovItemInContainer?
		curaddr=callibrations->pPlayerVtbl + 4*0x19;
		SSetPos(clientstream, curaddr);
		curaddr=(unsigned int)SReadUInt(clientstream);
		if(curfunc=disasm_function(curaddr))
		{
			if(curinsn=find_insn(curfunc, jcc_or_jmp, 0, 0))
			{
				//-> mov [reg+oContentsNext], reg
				curfunc->backwards=1;
				if(curinsn=find_insn(curfunc, mov_expression, 0, 0))
				{
					toreturn++;//1
					callibrations->ItemOffsets.oContentsNext=insn_get_target(curinsn, 0);
				}
				//-> mov [reg+oContentsPrevous], reg
				curfunc->backwards=0;
				if(curinsn=find_insn(curfunc, mov_expression, 0, 0))
				{
					toreturn++;//2
					callibrations->ItemOffsets.oContentsPrevious=insn_get_target(curinsn, 0);
				}
			}

			delete_asm_function(curfunc);
		}

		//b. pPlayerVtbl[0x1C] = IsItemOnLayer?
		curaddr=callibrations->pPlayerVtbl + 4*0x1C;
		SSetPos(clientstream, curaddr);
		curaddr=(unsigned int)SReadUInt(clientstream);
		if(curfunc=disasm_function(curaddr))
		{
			//-> cmp [reg+oContainer], 0 || mov reg2, [reg1+oContainer]; test reg2, reg2;
			//if(curinsn=find_insn(curfunc, cmp_expression_0, 0, 0))
			if(curinsn=find_insn(curfunc, cmp_or_test, 0, 0))
			{
				if(curinsn->operands->op.type==op_expression)
				{
					toreturn++;//3
					callibrations->ItemOffsets.oContainer=insn_get_target(curinsn, 0);
				}
				else
				{
					curfunc->backwards=1;
					if(curinsn=find_insn(curfunc,mov_reg_expression, 0, 0))
					{
						toreturn++;//3
						callibrations->ItemOffsets.oContainer=insn_get_target(curinsn, 0);
					}
					curfunc->backwards=0;
				}
			}
			//-> add reg, oFirstLayeredItem
			if(curinsn=find_insn(curfunc, add_reg_immediate, 0, 0))
			{
				toreturn++;//4
				callibrations->ItemOffsets.oFirstLayer=insn_get_target(curinsn, 1);
			}

			delete_asm_function(curfunc);
		}
		
		//c. pPlayerVtbl[0x31] = SetNotoriety(Noto)
		curaddr=callibrations->pPlayerVtbl + 4*0x31;
		SSetPos(clientstream, curaddr);
		curaddr=(unsigned int)SReadUInt(clientstream);
		if(curfunc=disasm_function(curaddr))
		{
			//-> mov [reg+oNotoriety], reg
			if(curinsn=find_insn(curfunc, mov_expression, 0, 0))
			{
				toreturn++;//5
				callibrations->ItemOffsets.oNotoriety=insn_get_target(curinsn, 0);
			
				//- find loop
				if(curinsn=find_loop(curfunc))
				{
					//backwards (from end of the loop)
					curfunc->backwards=1;
					//-> mov reg, [reg+oNextFollower]
					if(curinsn=find_insn(curfunc, mov_reg_expression, 0, 0))
					{
						toreturn++;//6
						callibrations->ItemOffsets.oNextFollower=insn_get_target(curinsn, 1);
					}
				}
			}

			delete_asm_function(curfunc);
		}

		//newer clients have pPlayerVtbl[0x32] = SetNotoriety(Noto)
		if(toreturn!=6)
		{
			curaddr=callibrations->pPlayerVtbl + 4*0x32;
			SSetPos(clientstream, curaddr);
			curaddr=(unsigned int)SReadUInt(clientstream);
			if(curfunc=disasm_function(curaddr))
			{
				//-> mov [reg+oNotoriety], reg
				if(curinsn=find_insn(curfunc, mov_expression, 0, 0))
				{
					toreturn++;//5
					callibrations->ItemOffsets.oNotoriety=insn_get_target(curinsn, 0);
				
					//- find loop
					if(curinsn=find_loop(curfunc))
					{
						//backwards (from end of the loop)
						curfunc->backwards=1;
						//-> mov reg, [reg+oNextFollower]
						if(curinsn=find_insn(curfunc, mov_reg_expression, 0, 0))
						{
							toreturn++;//6
							callibrations->ItemOffsets.oNextFollower=insn_get_target(curinsn, 1);
						}
					}
				}
				
				delete_asm_function(curfunc);
			}
		}
	}

	//cleanup
	delete_insn_mask(mov_reg_expression);
	delete_insn_mask(add_reg_immediate);
	//delete_insn_mask(cmp_expression_0);
	delete_insn_mask(cmp_or_test);
	delete_insn_mask(mov_expression);
	delete_insn_mask(jcc_or_jmp);
	
	if(toreturn==6)
		return 1;
	else
		return 0;
}

//callibrations from winmain
int WinMainCallibrations(asm_function * winmain, UOCallibrations * callibrations)
{
	insn_mask * xor_mov_mov;
	insn_mask * call;
	insn_mask * vtbl_mov;
	insn_mask * mov_dword_register;
	insn_mask * mov_dword_register_b;
	insn_mask * three_calls;
	insn_mask * mov_reg_vtbl, * mov_reg_vtblb;
	insn_mask * push_1388h;
	insn_mask * cmp_var_0, * cmp_var_0b;
	op_mask * curop;
	asm_function * networkobject_constructor, * curfunc, * player_constructor;
	x86_op_datatype_t curdata;
	x86_insn_t * curinsn, * tempinsn;
	int toreturn;

	//assume error
	toreturn=0;

	//setup instruction masks:
	//cmp var, 0;
	cmp_var_0=create_insn_mask(2);
	insn_by_type(cmp_var_0, insn_cmp);
	curop=insn_op_mask(cmp_var_0, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(cmp_var_0, 1);
	curop->op.data.dword=0;
	curop->mask.data.dword=0xFFFFFFFF;
	cmp_var_0b=create_insn_mask(2);
	insn_by_type(cmp_var_0b, insn_cmp);
	curop=insn_op_mask(cmp_var_0b, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(cmp_var_0b, 1);
	curop->op.data.dword=0;
	curop->mask.data.dword=0xFFFFFFFF;
	insn_set(cmp_var_0, cmp_var_0b);

	//mov [reg], lpVtbl == mov expression, immediate||offset; with expression.disp==0
	mov_reg_vtbl=create_insn_mask(2);
	insn_by_type(mov_reg_vtbl, insn_mov);
	curop=insn_op_mask(mov_reg_vtbl, 0);
	op_by_type(curop, op_expression);
	curop->op.data.expression.disp=0;
	curop->mask.data.expression.disp=0xFFFFFFFF;//make sure the displacement is 0 (optionally we could add an extra check for reg==esi|edi as that are the only two ever used for construction it seems, but seems like that's not really necessary here)
	curop=insn_op_mask(mov_reg_vtbl, 1);
	op_by_type(curop, op_immediate);
	mov_reg_vtblb=create_insn_mask(2);
	insn_by_type(mov_reg_vtblb, insn_mov);
	curop=insn_op_mask(mov_reg_vtblb, 0);
	op_by_type(curop, op_expression);
	curop->op.data.expression.disp=0;
	curop->mask.data.expression.disp=0xFFFFFFFF;//make sure the displacement is 0 (optionally we could add an extra check for reg==esi|edi as that are the only two ever used for construction it seems, but seems like that's not really necessary here)
	curop=insn_op_mask(mov_reg_vtblb, 1);
	op_by_type(curop, op_offset);
	insn_set(mov_reg_vtbl, mov_reg_vtblb);

	//sequence of 3 calls
	three_calls=insn_by_type(0, insn_call);
	insn_seq(three_calls, insn_by_type(0, insn_call));
	insn_seq(three_calls, insn_by_type(0, insn_call));

	//sequence of an xor and 2 movs
	xor_mov_mov=insn_by_type(0, insn_xor);
	insn_seq(xor_mov_mov, insn_by_type(0, insn_mov));
	insn_seq(xor_mov_mov, insn_by_type(0, insn_mov));

	//just a call
	call=insn_by_type(0, insn_call);

	//vtbl_mov <- mov dword ptr[esi], offset NetworkObjectVtbl
	//6 byte mov, with 2 ops : op[0]=expression and op[1]=immediate
	vtbl_mov=create_insn_mask(2);
	insn_by_opcount(vtbl_mov, 2);
	insn_by_size(vtbl_mov, 6);
	curop=insn_op_mask(vtbl_mov, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(vtbl_mov, 1);
	op_by_type(curop, op_immediate);

	//mov NetworkObject, esi
	//mov with 2 ops, op[0]=expr|op[0]=offset; op[1]=register
	mov_dword_register=create_insn_mask(2);
	insn_by_type(mov_dword_register, insn_mov);
	insn_by_opcount(mov_dword_register, 2);
	curop=insn_op_mask(mov_dword_register, 0);
	op_by_type(curop, op_expression);
	curop=insn_op_mask(mov_dword_register, 1);
	op_by_type(curop, op_register);
	mov_dword_register_b=create_insn_mask(2);
	insn_by_type(mov_dword_register_b, insn_mov);
	insn_by_opcount(mov_dword_register_b, 2);
	curop=insn_op_mask(mov_dword_register_b, 0);
	op_by_type(curop, op_offset);
	curop=insn_op_mask(mov_dword_register_b, 1);
	op_by_type(curop, op_register);
	insn_set(mov_dword_register, mov_dword_register_b);

	//push_1388h
	push_1388h=create_insn_mask(1);
	insn_by_type(push_1388h, insn_push);
	curop=insn_op_mask(push_1388h, 0);
	curop->op.data.dword=0x1388;
	curop->mask.data.dword=0xFFFFFFFF;

	//start at start of winmain
	asm_entrypoint(winmain);

	//look for [xor eax, eax; mov ecx, eax; mov dwordxxxxxxxx, eax; call xxxxxxxx; call NetworkObjectConstructor]
	while(curinsn = find_insn(winmain, xor_mov_mov, 0, 1))
	{
		tempinsn = curinsn;
		curinsn = asm_next(winmain);
		curinsn = asm_next(winmain);
		if(curinsn->operands[0].op.type != op_register)
			break;
	}
	curinsn = tempinsn;

	if(curinsn)// = find_insn(winmain, xor_mov_mov, 0, 1))//=find_insn(winmain, xor_mov_mov, 0, 1))
	{
		curinsn=find_insn(winmain, call, 0, 1);
		curinsn=find_insn(winmain, call, 0, 1);
		if(curinsn)
		{
			//disasm the NetworkObject-constructor
			if(networkobject_constructor=disasm_function(insn_get_target(curinsn, 0)))
			{
				//look for mov dword ptr[esi], offset NetworkObjectVtbl
				if(curinsn=find_insn(networkobject_constructor, vtbl_mov, 0, 1))
				{
					callibrations->NetworkObjectVtbl=(NetworkObjectClassVtbl *)insn_op_data(curinsn, 1, 0).dword;
					toreturn++;//1
				}

				//look for mov pNetworkObject, esi
				if(curinsn=find_insn(networkobject_constructor, mov_dword_register, 0, 1))
				{
					curdata=insn_op_data(curinsn, 0, 0);
					if(curinsn->operands->op.type==op_expression)
						callibrations->NetworkObject=(NetworkObjectClass **)curdata.expression.disp;
					else
						callibrations->NetworkObject=(NetworkObjectClass **)curdata.offset;
					toreturn++;//2
				}

				delete_asm_function(networkobject_constructor);//no longer needed
			}
		}

		//find backwards 3 calls
		winmain->backwards=1;
		if(curinsn=find_insn(winmain, three_calls, 0, 0))
		{
			//follow second call
			winmain->backwards=0;
			curinsn=find_insn(winmain, call, 0, 0);

			if(curfunc=disasm_chunk(insn_get_target(curinsn, 0)))
			{
				// follow second call (PlayerConstructor)
				if((curinsn=find_insn(curfunc, call, 0, 0))&&(curinsn=find_insn(curfunc, call, 0, 0)))
				{
					if(player_constructor=disasm_function(insn_get_target(curinsn, 0)))
					{
						//find mov [reg], offset <- pPlayerVtbl
						if(curinsn=find_insn(player_constructor, mov_reg_vtbl, 0, 0))
						{
							toreturn++;//3
							callibrations->pPlayerVtbl=insn_get_target(curinsn, 1);
						}

						delete_asm_function(player_constructor);
					}
				}

				delete_asm_function(curfunc);
			}
		}

		//next is an unchecked callibration of bHideIntro (so if this fails bHideIntro will be 0 and we should not try to change it)
		//find push_1388h, then follow second call
		if(curinsn=find_insn(winmain, push_1388h, 0, 0))
		{
			if((curinsn=find_insn(winmain, call, 0, 0))&&(curinsn=find_insn(winmain, call, 0, 0)))
			{
				if(curfunc=disasm_chunk(insn_get_target(curinsn, 0)))
				{
					if(curinsn=find_insn(curfunc, cmp_var_0, 0, 0))
					{
						//do not affect "toreturn", this callibration is allowed to failed without indicating failure
						//	<- i'm just not sure about this 0x1388, it might be an value specifying which intro to load
						//	<- so on client's with another intro, this will obviously fail
						callibrations->bHideIntro=(int *)insn_get_target(curinsn, 0);
					}
					delete_asm_function(curfunc);
				}
			}
		}
	}

	//cleanup
	delete_insn_mask(xor_mov_mov);
	delete_insn_mask(call);
	delete_insn_mask(vtbl_mov);
	delete_insn_mask(mov_dword_register);
	delete_insn_mask(three_calls);
	delete_insn_mask(mov_reg_vtbl);
	delete_insn_mask(cmp_var_0);
	delete_insn_mask(push_1388h);

	if(toreturn==3)
		return 1;
	else
		return 0;
}

asm_function * CallibrateWinMain(Stream ** clientstream, Process * clientprocess, UOCallibrations * callibrations)
{
	//declarations
	PEInfo * peinfo;
	unsigned int entrypoint_address;
	int prev_error_count;
	asm_function * curfunc, * curfuncb;
	x86_insn_t * curinsn, * curinsnb;
	x86_op_datatype_t curdata;
	insn_mask * _insn_call_relative, * _insn_call_relativeb;
	insn_mask * _insn_ret10h;
	op_mask * curop;
	asm_function * toreturn=0;

	//initalize instruction masks
	
	//relative call = insn_call, op0-type=op_relative_near || insn_call, op0-type=op_relative_far
	_insn_call_relative=create_insn_mask(1);
	insn_by_type(_insn_call_relative,insn_call);
	curop=insn_op_mask(_insn_call_relative, 0);
	op_by_type(curop, op_relative_near);
	_insn_call_relativeb=create_insn_mask(1);
	insn_by_type(_insn_call_relativeb,insn_call);
	curop=insn_op_mask(_insn_call_relativeb, 0);
	op_by_type(curop, op_relative_far);
	insn_set(_insn_call_relative, _insn_call_relativeb);	
	
	//	retn 0x10
	_insn_ret10h=create_insn_mask(1);//insn mask with 1 operand
	_insn_ret10h=insn_by_type(_insn_ret10h, insn_return);//type = insn_return
	memset(&curdata,0,sizeof(x86_op_datatype_t));//
	curdata.byte=0x10;//
	op_by_data(insn_op_mask(_insn_ret10h, 0),curdata);//operand 0 should have an immediate operand with data 0x10


	//actual callibrations
	//- get client.exe's entrypoint
	//- from the entrypoint find winmain (function called from the entrypoint with stacksize 0x10, i.e. find call, disassemble target function and look for retn 0x10)
	prev_error_count=ErrorCount();
	if(peinfo=GetPEInfo(clientprocess))//parse PE_headers to get the entrypoint-address
	{
		entrypoint_address=peinfo->headers.baseaddress+peinfo->headers.optionalheader.AddressOfEntryPoint;//entrypoint RVA in optionalheader->address
		DeletePEInfo(peinfo);//no other use for PE headers here

		//disassemble entrypoint
		if(curfunc=disasm_function(entrypoint_address))
		{
			asm_lowest(curfunc);//to start
			while(curinsn=find_insn(curfunc, _insn_call_relative, 0, 0))
			{
				curfuncb=disasm_function(insn_get_target(curinsn, 0));
				asm_lowest(curfuncb);//to start
				if(curinsnb=find_insn(curfuncb, _insn_ret10h, 0, 0))
					break;
				delete_asm_function(curfuncb);
			}
	
			if(curinsnb)
			{
				toreturn=curfuncb;
				callibrations->WinMain=(pWinMain)curfuncb->entrypoint_address;
				//delete_asm_function(curfuncb);
			}
			else
			{
				if(ErrorCount()>prev_error_count)
					PushError(CreateError(CALLIBRATION_ERROR, "Crictical Callibration Error: Client's WinMain function not found!?",0,PopError()));
				else
					PushError(CreateError(CALLIBRATION_ERROR, "Crictical Callibration Error: Client's WinMain function not found!?",0,0));
			}
	
			delete_asm_function(curfunc);
		}
		else
		{
			if(ErrorCount()>prev_error_count)
				PushError(CreateError(CALLIBRATION_ERROR, "Failed to disasemmble the entry-point function!?",0,PopError()));
			else
				PushError(CreateError(CALLIBRATION_ERROR, "Failed to disasemmble the entry-point function!?",0,0));
		}
	}
	else
	{
		if(ErrorCount()>prev_error_count)
			PushError(CreateError(CALLIBRATION_ERROR, "Failed to obtain client executables entry-point from the PE headers, do you lack the required permissions to read the PE headers!?",0,PopError()));
		else
			PushError(CreateError(CALLIBRATION_ERROR, "Failed to obtain client executables entry-point from the PE headers, do you lack the required permissions to read the PE headers!?",0,0));
	}

	delete_insn_mask(_insn_call_relative);
	delete_insn_mask(_insn_ret10h);

	return toreturn;
}

int CallibrateClient(unsigned int pid, UOCallibrations * callibrations)
{
	Process * clientprocess;
	ProcessStream * _psstream;
	BufferedStream * _bsstream;
	Stream ** clientstream;
	int toreturn=0;
	asm_function * winmain;

	//0 out everything first
	memset(callibrations, 0, sizeof(UOCallibrations));

	//open client process and stream (can be local if pid==GetCurrentProcessId())
	clientprocess=GetProcess(pid, TYPICAL_PROCESS_PERMISSIONS);
	_psstream=CreateProcessStream(clientprocess);
	_bsstream=CreateBufferedStream((Stream **)_psstream, 4096);
	clientstream=(Stream **)_bsstream;

	//initialize disassembler
	asm_init(clientstream);

	//callibrate client
	if(winmain=CallibrateWinMain(clientstream, clientprocess, callibrations))//finds WinMain from the client's entrypoint address
	{
		//callibrations directly from WinMain NetworkObject, NetworkObjectVtbl, some ItemOffsets from the constructor of the 'Player' object
		if(toreturn=WinMainCallibrations(winmain, callibrations))
		{//callibrations depending on the NetworkObject, NetworkObjectVtbl or PlayerVtbl
			//NetworkObjectVtbl callibs
			toreturn&=VtblReceiveAndHandlePacketsCallibrations(clientstream, clientprocess, callibrations);
			toreturn&=VtblSendPacketCallibrations(clientstream, clientprocess, callibrations);
			toreturn&=VtblHandlePacketCallibrations(clientstream, clientprocess, callibrations);

			if(toreturn)
			{//callibrations depending on the packet-switch/packet-switch offsets (= callibrations from the client's packethandlers)
				//toreturn&=NewItemPacketCallibrations(clientstream, clientprocess, callibrations);
				toreturn&=OtherPacketCallibrations(clientstream, clientprocess, callibrations);
			}
			
			//PlayerVtbl callibs
			toreturn&=PlayerVtblCallibrations(clientstream, clientprocess, callibrations);
		}

		//Macro function
		if(toreturn&=MacroCallibrations(winmain, callibrations))
		{//callibrations from specific macros
			if(toreturn&=LastObjectMacroCallibrations(clientstream, clientprocess, callibrations))
			{
				toreturn&=NewItemPacketCallibrations(clientstream, clientprocess, callibrations);//moved here, needs oItemID
				toreturn&=SendPacketCallibrations(clientstream, clientprocess, callibrations);//lastobject macro gives SendPacket function, for which we still need to callibrate hook and encryption-patch addresses
			}

			toreturn&=LastSkillMacroCallibrations(clientstream, clientprocess, callibrations);
			toreturn&=LastSpellMacroCallibrations(clientstream, clientprocess, callibrations);
			toreturn&=ToggleAlwaysRunMacroCallibrations(clientstream, clientprocess, callibrations);
			toreturn&=LastTargetMacroCallibrations(clientstream, clientprocess, callibrations);
			toreturn&=QuitMacroCallibrations(clientstream, clientprocess, callibrations);

			if(toreturn&=OpenMacroCallibrations(clientstream, clientprocess, callibrations))//->OpenGump callibration is here
				toreturn&=GumpCallibrations(clientstream, clientprocess, callibrations);

			if(toreturn)
				toreturn&=AOSPropertiesCallibrations(clientstream, clientprocess, callibrations);
		}
		
		//multiclient-patch callibrations: locations where a jcc is to be patched to a jmp (and the required targets of those jumps)
		//	note that we do not return an error if this callibration fails, since the patch might already be applied
		//	<- f.e. if a tool like Razor is used to apply a multiclient patch, my multiclient patching will fail
		//	<- so failure of multiclient-patch-callibrations means either : 
		//		- a multiclient patch is already applied
		//		- or this is an unknown (badly callibrated) client <- but then typically other callibrations should fail too
		MultiClientPatchingCallibrations(winmain, callibrations);
	
		delete_asm_function(winmain);
	}

	//cleanup
	//- cleanup disassembler
	asm_cleanup();
	//- close streams
	DeleteBufferedStream(_bsstream);
	DeleteProcessStream(_psstream);
	//- close process
	CloseHandle(clientprocess->hThread);
	CloseHandle(clientprocess->hProcess);
	clean(clientprocess);
	
	return toreturn;
}