#ifndef ASMCALLIBRATION_INCLUDED
#define ASMCALLIBRATION_INCLUDED

#include "Features.h"
#include "libdisasm\libdisasm.h"
#include "Streams.h"
#include "BinaryTree.h"

/*

  asmCallibration.h/.c implement a set of tools to callibrate offsets by disassembling a process instruction-per-instruction
  looking for specific instruction masks or constructions.
  
  Depends on libdisasm.dll/libdisasm.h(/libdisasm.lib) as disassembler, which were obtained 
  from "http://bastard.sourceforge.net/libdisasm.html".
  A small modification to the libdisasm.h header was made to ensure the x86_op_t::data member has a typedef'd union type.
  This new typedef is called 'x86_op_datatype_t' and was added just before the x86_op_t typedef.
	
  -- Wim Decelle
*/

/*
	1. Instruction and Operand comparison
		<- instructions come as x86_insn_t structures from libdisasm.h
		<- operands are in the op-list within that structure stored as x86_op_t structures
		<- essentially looking for specific instructions or specific operands just means looking for specific fields
		within these x86_op_t and x86_insn_t to have the expected values (type, size, datatype, ... fields)
		<- the easiest way to do this is to store a structure with the fields set to the expected values and
		an accompanying mask to indicate which fields are to be checked, byte-per-byte comparison is then possible with
		no further knowledge of the structure's semantics required.
		<- here such struct<->mask pairs are defined for x86_op_t and x86_insn_t + tool functions
		<- the instruction mask includes a list of operand masks
*/

typedef struct op_mask_struct op_mask;//forward declaration

typedef struct insn_mask_struct
{
	x86_insn_t insn;
	x86_insn_t mask;
	unsigned int opcount;
	op_mask * ops;
	struct insn_mask_struct * set_next;
	struct insn_mask_struct * seq_next;
} insn_mask;//instruction mask

//note that opcount is just used for initialization purposes and not masked in automatically
//if you need a check of the opcount, apply the insn_by_opcount function
//if the opcount specified here doesn't match the actual opcount only the first min(opcount, actual_opcount) number of operands can be checked
insn_mask * create_insn_mask(unsigned int opcount);
void delete_insn_mask(insn_mask * todelete);

void mask_in(void * pfield, unsigned int fieldsize);

#define MASK_INSN(curmask, field, data)\
	curmask->insn.field=data;\
	mask_in(&(curmask->mask.field), sizeof(curmask->mask.field))

insn_mask * insn_by_type(insn_mask * maskin, enum x86_insn_type type);
insn_mask * insn_by_size(insn_mask * maskin, unsigned int type);
insn_mask * insn_by_opcount(insn_mask * maskin, unsigned int opcount);
insn_mask * insn_set_opcount(insn_mask * maskin, unsigned int opcount);
op_mask * insn_op_mask(insn_mask * maskin, unsigned int op_number);

insn_mask * insn_set(insn_mask * maskin, insn_mask * option);
insn_mask * insn_seq(insn_mask * maskin, insn_mask * option);

typedef struct op_mask_struct
{
	x86_op_t op;
	x86_op_t mask;
	struct op_mask_struct * next;
} op_mask;//instruction mask

#define MASK_OP(curmask, field, data)\
	curmask->op.field=data;\
	mask_in(&(curmask->mask.field), sizeof(curmask->mask.field))

op_mask * create_op_mask();
void delete_op_mask(op_mask * todelete);

op_mask * op_by_type(op_mask * maskin, enum x86_op_type type);
op_mask * op_by_datatype(op_mask * maskin, enum x86_op_datatype type);
op_mask * op_by_data(op_mask * maskin, x86_op_datatype_t data);

/*
	2. Disassembly <- disassembly of chunks or functions
	3. Searching
		- find an insn_mask in a disassembled chunk or function
		- this includes finding a sequence or finding the first instruction from a set of instructions,
			by using insn_set to link insns_masks into a set of optional instructions
			and/or using insns_seq to link them into a sequence
			(note that combining both can be tricky)
	4. Other (higher level things:)
		- return an instruction's operand or address
		- find loops
		- ...
*/

typedef struct asm_function_struct
{
	unsigned int entrypoint_address;
	BinaryTree * instructions;
	BinaryTreeEnum * insn_enum;
	int backwards;
} asm_function;

//returns a binarytree storing instructions by address (address<->x86_insn_t)
//	<- difference between chunk and function is that no attempt is made to follow jmps/jnz/jz (jcc's) for chunks
//	<- note that switch jmp's are never handled correctly here
asm_function * disasm_chunk(unsigned int address);
asm_function * disasm_function(unsigned int address);

void delete_asm_function(asm_function * todelete);

//position control, for a chunk the lowest address = entrypoint address,
//	but since a function can contain chunks before the entry chunk this might not be true for chunks
x86_insn_t * asm_next(asm_function * onfunction);
x86_insn_t * asm_previous(asm_function * onfunction);
x86_insn_t * asm_lowest(asm_function * onfunction);
x86_insn_t * asm_highest(asm_function * onfunction);
x86_insn_t * asm_entrypoint(asm_function * onfunction);
x86_insn_t * asm_jump(asm_function * onfunction, unsigned int address);

//find instruction (or set or sequence of instructions) in diassembled chunk or function
//	allchunks controls whether searching is done in a linear fashion or not
//		- allchunks=false means searching is done completely linear by moving from the current address
//			to the last address in the asmfunction and returning the first match
//		- allchunks=true means searching is done treating the asmfunction as an actual function,
//			searching is done in a linear fashion, but only  for the current 'chunk'
//			all other chunks referred (jcc, jmp, ...) are queued and if they are within the current asmfunction
//			they are searched too
//			so the matching instruction found in the first encountered chunk is returned
x86_insn_t * find_insn(asm_function * disassembled, insn_mask * tofind, insn_mask ** set_out, int allchunks);

//get operand data
x86_op_datatype_t insn_op_data(x86_insn_t * curinsn, unsigned int op_number, enum x86_op_datatype * datatype);
//get the target address from a relative address operand
unsigned int insn_get_target(x86_insn_t * curinsn, unsigned int op_number);
//current address for the instruction
unsigned int insn_address(x86_insn_t * curinsn);

//find the first loop in the current function
x86_insn_t *  find_loop(asm_function * disassembled);

//to be called before and after callibration
void asm_init(Stream ** process_stream);
void asm_cleanup();

#endif