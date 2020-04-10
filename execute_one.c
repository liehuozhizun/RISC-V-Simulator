#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "riscv_sim_framework.h"
#include "riscv_pipeline_registers.h"
#include "riscv_pipeline_registers_vars.h"

extern struct stage_reg_d  new_d_reg;
extern struct stage_reg_x  new_x_reg;
extern struct stage_reg_m  new_m_reg;
extern struct stage_reg_w  new_w_reg;

extern struct stage_reg_d cur_d_reg;
extern struct stage_reg_x cur_x_reg;
extern struct stage_reg_m cur_m_reg;
extern struct stage_reg_w cur_w_reg;

extern bool memory_read (uint64_t address, void * value, uint64_t size_in_bytes);
extern bool memory_write (uint64_t address, uint64_t value, uint64_t size_in_bytes);
extern bool memory_status (uint64_t address, void *value);

extern void register_read (uint64_t register_a, uint64_t register_b, uint64_t * value_a, uint64_t * value_b);
extern void register_write (uint64_t register_d, uint64_t value_d);

extern void     set_pc (uint64_t pc);
extern uint64_t get_pc (void);

/*
	Define a block for i-cache
	will be used in a global struct array
*/
struct model_i_cache{
	int      valid_bit;
	uint32_t tag; // actually this tag is in length of 19 bits
	uint32_t instr1;
	uint32_t instr2;
	uint32_t instr3;
	uint32_t instr4;
};

// declare the global array for I-cache
struct model_i_cache i_cache[512];

/*
	check_i_cache mainly look for the instruction by the tag and index
	return an array containing status and instruction
*/
uint32_t* check_i_cache(struct model_i_cache i_cache[512], uint64_t pc, uint32_t result[]){
	// divide pc into parts of a block
	int tag      = (pc & 0xFFFFE000) >> 13;
	int index    = (pc & 0x1FF0) >> 4;
	int offset   = (pc & 0xF);
	//printf("> current PC is: 0x%016lx\n", pc);
	//printf("> offset is: %d\n", offset);
	
	// compare the tag in i-cache with the address's tag
	if(i_cache[index].tag == tag){
		if(i_cache[index].valid_bit == 1){
			result[0] = 1;
			if(offset == 0x0){
				result[1] = i_cache[index].instr1;
				//printf("This instruction is: 0x%016x\n", i_cache[index].instr1);
			}else if(offset == 0x4){
				result[1] = i_cache[index].instr2;
				//printf("This instruction is: 0x%016x\n", i_cache[index].instr2);
			}else if(offset == 0x8){
				result[1] = i_cache[index].instr3;
			}else if(offset == 0xc){
				result[1] = i_cache[index].instr4;
			}
			return result;
		}else{ // needs to update 
			result[0] = 0;
			result[1] = 0;
			return result;
		}
	}else{ // needs to update 
		result[0] = 0;
		result[1] = 0;
		return result;
	}
}

/*
	update_i_cache mainly updates the tag and data provided in the cache
*/
void update_i_cache(struct model_i_cache i_cache[512], uint64_t pc, uint32_t instr[]){
	// divide address into parts of a block
	int tag          = (pc & 0xFFFFE000) >> 13;
	int index        = (pc & 0x1FF0) >> 4;
	
	// update both tag, data and valid_bit in d-cache
	i_cache[index].tag       = tag;
	i_cache[index].instr1    = instr[0];
	//printf("> update_i_cache instr[0] is: 0x%016x\n", instr[0]);
	i_cache[index].instr2    = instr[1];
	//printf("> update_i_cache instr[1] is: 0x%016x\n", instr[1]);
	i_cache[index].instr3    = instr[2];
	i_cache[index].instr4    = instr[3];
	i_cache[index].valid_bit = 1;
}

/*
	Define a block for d-cache
	will be used in a global struct array
*/
struct model_d_cache{
	int      valid_bit;
	uint32_t tag; // actually this tag is in length of 19 bits
	uint64_t data;
};

// declare the global array for D-cache
struct model_d_cache d_cache[2048];

/*
	check_d_cache mainly look for the data by the tag and index
	return an array containing status and data
*/
uint64_t* check_d_cache(struct model_d_cache d_cache[2048], uint32_t address, uint64_t offset, uint64_t result[]){
	// divide address into parts of a block
	int tag          = (address & 0xFFFFC000) >> 14;
	int index        = (address & 0x3FF8) >> 3;
	
	// compare the tag in d-cache with the address's tag
	if(d_cache[index].tag == tag){
		if(d_cache[index].valid_bit == 1){
			result[0] = 1;
			result[1] = d_cache[index].data & offset;
			return result;
		}else{ // needs to update 
			result[0] = 0;
			result[1] = 0;
			return result;
		}
	}else{ // needs to update 
		result[0] = 0;
		result[1] = 0;
		return result;
	}
}

/*
	update_d_cache mainly updates the tag and data provided in the cache
*/
void update_d_cache(struct model_d_cache d_cache[2048], uint32_t address, uint64_t data){
	// divide address into parts of a block
	int tag          = (address & 0xFFFFC000) >> 14;
	int index        = (address & 0x3FF8) >> 3;
	
	// update both tag, data and valid_bit in d-cache
	d_cache[index].tag       = tag;
	d_cache[index].data      = data;
	d_cache[index].valid_bit = 1;
}

/*
	write_d_cache mainly check write hit or miss.
	1. If it's write hit, then update the value in the d-cache
	2. If it's write miss, then leave the d-cache unmodified
*/
void write_d_cache(struct model_d_cache d_cache[2048], uint32_t address, uint64_t data){
	// divide address into parts of a block
	int tag          = (address & 0xFFFFC000) >> 14;
	int index        = (address & 0x3FF8) >> 3;
	
	if(d_cache[index].tag == tag){
		if(d_cache[index].valid_bit == 1){ // write hit
			d_cache[index].data = data;
		}else{ // write miss - not valid
			return;
		}
	}else{ // write miss - no tag
		return;
	}
}

// To do the sign-extended
uint64_t converter(uint64_t i, uint64_t most_significant, uint64_t bitwiseNum){
	if(( i & most_significant ) == most_significant)
	{
		i = i | bitwiseNum;
	}
	return i;
}

/*  Branch Target Buffer - global variable
	BTB[Tag][Target]
		-Tag: current pc address
		-Target: target pc address
*/
uint64_t BTB[32][2];

/*  Branch Prediction
	Goals: 
		1. look over the BTB to see if there is a Tag that equals to the branch and give the target
		2. if there is no record, then add Tag in fetch stage, and add Target in execute stage
*/ 
int branchPrediction(uint64_t btb[32][2], uint64_t pc, uint32_t instr){
	
	// forwarding not taken, depending on immediate is positve or negative
	uint64_t tem = (instr & 0x80000000) >> 20;
	tem  += (instr & 0x80) << 3;
	tem  += (instr & 0x7E000000) >> 21;
	tem  += (instr & 0xF00) >> 8;
	tem   = tem & 0xFFF;
	tem   = converter(tem,0x800,0xFFFFFFFFFFFFF000);
	if(tem > 0){ // positive immediate means forwarding: no prediction, not taken
		return 33;
	}
	
	int i;
	for(i = 0; i < 32; i++){
		// if BTB reads the null values, break and store this pc into address
		if( (btb[i][0] == 0) & (btb[i][1] == 0) ){
			break;
		}
		// if BTB reads the exact tag has the same value of pc, then setpc with stored target 
		if(btb[i][0] == pc){
			set_pc(btb[i][1]);
			return i;
		}
	}
	btb[i][0] = pc; // add a new Tag
	return 33;
}

void stage_fetch (struct stage_reg_d *new_d_reg){
	//printf(">>>>> FETCH STAGE <<<<<\n");
	
	uint32_t inst;
	uint32_t full_inst[4];
	full_inst[0]=0; full_inst[1]=0; full_inst[2]=0; full_inst[3]=0; 
	uint32_t result_array[2]; // result[1] contains success status of cache, result[2] contains exact result
	
	if(cur_d_reg.i_cache_stall){
		//printf("> ????pc is: 0x%016lx\n",cur_d_reg.pc);
		if(memory_status(cur_d_reg.pc, &full_inst) || cur_d_reg.first_cache_stall){
			//printf("> READ in x1\n");
			if(cur_d_reg.first_cache_stall){
				new_d_reg->first_cache_stall = false;
				new_d_reg->i_cache_stall = false;
				//printf("> READ in x2\n");
			}else{
				update_i_cache(i_cache, cur_d_reg.pc, full_inst);
				//printf("0x%016lx\n",cur_d_reg.pc);
				new_d_reg->i_cache_stall     = true;
				new_d_reg->first_cache_stall = true;
				//printf("> READ in x3\n");
				return;
			}
		}else{
			new_d_reg->i_cache_stall = true;
			return;
		}
	}
	
	if(cur_w_reg.d_cache_stall){
		return;
	}
	
	if(cur_m_reg.wrong_prediction){
		new_d_reg->fixed = true;
		set_pc(cur_d_reg.pc);
	}else{
		new_d_reg->fixed = false;
	}
	if(cur_d_reg.fixed){
		new_d_reg->fixed = false;
	}
	
	if(cur_x_reg.stall){
		return;
	}
	
	new_d_reg->ptr = &cur_d_reg;
	uint64_t pc = get_pc();
	new_d_reg->pc = pc;
	new_d_reg->new_pc = (pc + 4);
	//printf("> stored in d_reg pc is: 0x%016lx\n", pc);
	
	// check i-cache
	uint32_t* temp_result = check_i_cache(i_cache, cur_d_reg.pc, result_array);
	//printf("0x%016x\n0x%016x\n",temp_result[0],temp_result[1]);
	if(temp_result[0] == 1){ // i-cache hit
		new_d_reg->instruction = temp_result[1];
		new_d_reg->i_cache_stall = false;
		inst = temp_result[1];
	}else{ // i-cache miss
		bool status = memory_read(cur_d_reg.pc, &full_inst, 16);
		//printf("read from memory, pc: 0x%016lx\n",cur_d_reg.pc);
		//printf("read instruction-1 is: 0x%016x\n",full_inst[0]);
		new_d_reg->i_cache_stall = true; // i-cache miss needs stalls
		if(status){ // successfully read value from the memory
			update_i_cache(i_cache, cur_d_reg.pc, full_inst);
		}else{ // failed to read value from the memory, need stalls
			return;
		}
	}
	
	// Branch Prediction
	if((inst & 0x7F) == 0x63) // OPCODE 0x63 is for branch operations
	{
		int x = branchPrediction(BTB, pc, inst);
		if(x == 33){ // 33 means there is NO branch prediction
			// keep going as normal
			new_d_reg->branch_prediction = false;
		}else{ // there is a branch prediction
			new_d_reg->new_pc = BTB[x][1]; // store the new_pc from the prediction
			new_d_reg->branch_prediction = true; // mark there exists a prediction
			return;
		}
	}

	set_pc(pc+4);
	
	//printf("> PC: 0x%016lx\n",pc);
	//printf("> New_PC: 0x%016lx\n", new_d_reg->new_pc);
	//printf("> Instruction is: 0x%08x\n", inst);
}

void stage_decode (struct stage_reg_x *new_x_reg){
	//printf(">>>>> DECODE STAGE STARTS <<<<<\n");

	if(cur_d_reg.i_cache_stall){
		new_x_reg->i_cache_stall = true;
		return;
	}else{
		new_x_reg->i_cache_stall = false;
	}
	
	if(cur_w_reg.d_cache_stall){
		return;
	}
	
	if(cur_d_reg.fixed){
		new_x_reg->fixed = true;
	}else{
		new_x_reg->fixed = false;
	}
	
	if(cur_x_reg.stall){
		new_x_reg->stall = false;
		return;
	}
	
	new_x_reg->pc = cur_d_reg.pc;
	//printf("> stored in x_reg pc is: 0x%016lx\n", cur_d_reg.pc);
	new_x_reg->ptr = &cur_x_reg;
	uint32_t instr = cur_d_reg.instruction;
	new_x_reg->instruction = instr;
	//printf(" Instruction is: 0x%08x\n", instr);
	// store the pieces of instruction based on their formats
	uint64_t opcode = instr & 0x7F;
	uint64_t funct3 = 0;
	uint64_t funct7 = 0;
	uint64_t temp_rs1 = 0, temp_rs2 = 0, result_rs1 = 0, result_rs2 = 0;;
	switch(opcode)
	{
		// for 'R' format of instrcution
		case 0x33:
		case 0x3B:
			funct7  = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
			temp_rs2           = (instr & 0x1F00000) >> 20;
			temp_rs1           = (instr & 0xF8000) >> 15;
			register_read (temp_rs1, temp_rs2, &result_rs1, &result_rs2);
			new_x_reg->rs2     = result_rs2;
			new_x_reg->rs1     = result_rs1;
			funct3  = (instr & 0x7000) >> 12;
			new_x_reg->e[9]    = (instr & 0xF80) >> 7;
			break;

		// for 'I' format of instruction
		case 0x03:
		case 0x0F: // fence(.i) no need but kept for recoe[9]
		case 0x13:
		case 0x1B:
		case 0x67:
		case 0x73: // csr* no need but kept for recoe[9]
			new_x_reg->e[0]    = (instr & 0xFFF00000) >> 20;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
			temp_rs2           = (instr & 0x1F00000) >> 20;
			temp_rs1           = (instr & 0xF8000) >> 15;
			register_read (temp_rs1, temp_rs2, &result_rs1, &result_rs2);
			new_x_reg->rs2     = result_rs2;
			new_x_reg->rs1     = result_rs1;
			funct3  = (instr & 0x7000) >> 12;
			new_x_reg->e[9]    = (instr & 0xF80) >> 7;
			new_x_reg->e[1]    = (instr & 0xFE000000) >> 25;
			break;

		// for 'S' format of instruction
		case 0x23:
			new_x_reg->e[1]    = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
			temp_rs2           = (instr & 0x1F00000) >> 20;
			temp_rs1           = (instr & 0xF8000) >> 15;
			register_read (temp_rs1, temp_rs2, &result_rs1, &result_rs2);
			new_x_reg->rs2     = result_rs2;
			new_x_reg->rs1     = result_rs1;
			funct3  = (instr & 0x7000) >> 12;
			new_x_reg->e[5]    = (instr & 0xF80) >> 7;
			break;

		// for 'SB' format of instruction
		case 0x63:
			new_x_reg->e[2]    = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
			temp_rs2           = (instr & 0x1F00000) >> 20;
			temp_rs1           = (instr & 0xF8000) >> 15;
			register_read (temp_rs1, temp_rs2, &result_rs1, &result_rs2);
			new_x_reg->rs2     = result_rs2;
			new_x_reg->rs1     = result_rs1;
			funct3  = (instr & 0x7000) >> 12;
			new_x_reg->e[6]    = (instr & 0xF80) >> 7;
			new_x_reg->e[10]   = (instr & 0x80000000) >> 20;
			new_x_reg->e[10]  += (instr & 0x80) << 3;
			new_x_reg->e[10]  += (instr & 0x7E000000) >> 21;
			new_x_reg->e[10]  += (instr & 0xF00) >> 8;
			new_x_reg->e[10]   = new_x_reg->e[10] & 0xFFF;
			new_x_reg->e[10]   = converter(new_x_reg->e[10],0x800,0xFFFFFFFFFFFFF000);
			break;

		// for 'U' format of instruction
		case 0x17:
		case 0x37:
			new_x_reg->e[3]    = (instr & 0xFFFFF000) >> 12;
			new_x_reg->e[9]    = (instr & 0xF80) >> 7;
			break;

		// for 'UJ' format of instruction
		case 0x6F:
			new_x_reg->e[4]  = (instr & 0x80000000) >> 12;
			new_x_reg->e[4] += (instr & 0xFF000) >> 1;
			new_x_reg->e[4] += (instr & 0x100000) >> 10;
			new_x_reg->e[4] += (instr & 0x7FE00000) >> 21;
			new_x_reg->e[4]  = new_x_reg->e[4] & 0xFFFFF;
			new_x_reg->e[4]  = converter(new_x_reg->e[4],0x80000,0xFFFFFFFFFFF00000);
			new_x_reg->e[9]  = (instr & 0xF80) >> 7;
			break;
	}
	//printf("> opcode: 0x%016lx\n", opcode);
	//printf("> funct3: 0x%016lx\n", funct3);
	//printf("> funct7: 0x%016lx\n", funct7);

	// Distinguish the specific function and return the function number
	switch(opcode)
	{
		// for 'R' format of instruction
		case 0x33:
			switch(funct3)
			{
				case 0x0:
					if(funct7 == 0x0)
					{
						new_x_reg->funct = 28; break; // add
						//printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
					}else if(funct7 == 0x20){
						new_x_reg->funct = 29; break; // sub
					}else if(funct7 == 0x1){
						new_x_reg->funct = 60; break; // mul
					}
				case 0x1:
					new_x_reg->funct = 30; break; // sll
				case 0x2:
					new_x_reg->funct = 31; break; // slt
				case 0x3:
					new_x_reg->funct = 32; break; // sltu
				case 0x4:
					if(funct7 == 0x0){
						new_x_reg->funct = 33; break; // xor
					}else if(funct7 == 0x1){
						new_x_reg->funct = 61; break; // div
					}
				case 0x5:
					if(funct7 == 0x0)
					{
						new_x_reg->funct = 34; break; // srl
					}else{
						new_x_reg->funct = 35; break; //sra
					}
				case 0x6:
					if(funct7 == 0x0){
						new_x_reg->funct = 36; break; // or
					}else if(funct7 == 0x1){
						new_x_reg->funct = 62; break; // rem
					}
				case 0x7:
					new_x_reg->funct = 37; break; // and

			}
			break;
		case 0x3B:
			switch(funct3)
			{
				case 0x0:
					if(funct7 == 0x0)
					{
						new_x_reg->funct = 39; break; // addw
					}else{
						new_x_reg->funct = 40; break; // subw
					}
				case 0x1:
					new_x_reg->funct = 41; break; // sllw
				case 0x5:
					if(funct7 == 0x0)
					{
						new_x_reg->funct = 42; break; // srlw
					}else{
						new_x_reg->funct = 43; break; // sraw
					}
			}
			break;

		// for 'I' format of instruction
		case 0x03:
			switch(funct3)
			{
				case 0x0:
					new_x_reg->funct = 1; 
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break; // lb
				case 0x1:
					new_x_reg->funct = 2; // lh
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break;
				case 0x3:
					new_x_reg->funct = 3; // lw
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break; 
				case 0x4:
					new_x_reg->funct = 4; // ld
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break; 
				case 0x5:
					new_x_reg->funct = 5; // lbu
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break; 
				case 0x6:
					new_x_reg->funct = 6; // lhu
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break; 
				case 0x7:
					new_x_reg->funct = 7; // lwu
					if((cur_x_reg.funct <= 27) & (cur_x_reg.funct >= 24)){
						if(cur_x_reg.e[7] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break; 
			}
			break;
		// fence(.i) (0x0F) -> no need but kept for record
		case 0x0F:
			switch(funct3)
			{
				case 0x0:
					new_x_reg->funct = 8; break; // fence
				case 0x1:
					new_x_reg->funct = 9; break; // fence.i
			}
			break;
		case 0x13:
			switch(funct3)
			{
				case 0x0:
					new_x_reg->funct = 10; break; // addi
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
				case 0x1:
					new_x_reg->funct = 11; break; // slli
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
				case 0x2:
					new_x_reg->funct = 12; break; // slti
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
				case 0x3:
					new_x_reg->funct = 13; break; // sltiu
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
				case 0x4:
					new_x_reg->funct = 14; break; // xori
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
				case 0x5:
					if(new_x_reg->e[1] == 0x0)
					{
						new_x_reg->funct = 15; break; // slri
				//		printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
					}else{
						new_x_reg->funct = 16; break; // srai
				//		printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
					}
				case 0x6:
					new_x_reg->funct = 17; break; // ori
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
				case 0x7:
					new_x_reg->funct = 18; break; // andi
				//	printf("____ the new_x_reg is: %d\n",new_x_reg->funct);break;
			}
			break;
		case 0x1B:
			switch(funct3)
			{
				case 0x0:
					new_x_reg->funct = 20; break; // addiw
				case 0x1:
					new_x_reg->funct = 21; break; // slliw
				case 0x2:
					if(new_x_reg->e[0] == 0x0)
					{
						new_x_reg->funct = 22; break; // srliw
					}else{
						new_x_reg->funct = 23; break; // sraiw
					}
			}
			break;
		case 0x67:
			new_x_reg->funct = 50; break; // jalr

		// csr* (0x73) -> no need but kept for record
		case 0x73:
			switch(funct3)
			{
				case 0x0:
					if (funct7 == 0x0)
					{
						new_x_reg->funct = 52; break; // ecall
					}else{
						new_x_reg->funct = 53; break; // ebreak
					}
				case 0x1:
					new_x_reg->funct = 54; break; // csrrw
				case 0x2:
					new_x_reg->funct = 55; break; // csrrs
				case 0x3:
					new_x_reg->funct = 56; break; // csrrc
				case 0x5:
					new_x_reg->funct = 57; break; // csrrwi
				case 0x6:
					new_x_reg->funct = 58; break; // csrrsi
				//case 0x7:
					//new_x_reg->funct = 59; break; // csrrci

			}
			break;

		// for 'S' format of instruction
		case 0x23:
			switch(funct3)
			{
				case 0x0:
					new_x_reg->funct = 24; // sb
					if((cur_x_reg.funct <= 7) & (cur_x_reg.funct >= 1)){
						if(cur_x_reg.e[9] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break;
				case 0x1:
					new_x_reg->funct = 25; // sh
					if((cur_x_reg.funct <= 7) & (cur_x_reg.funct >= 1)){
						if(cur_x_reg.e[9] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break;
				case 0x2:
					new_x_reg->funct = 26; // sw
					if((cur_x_reg.funct <= 7) & (cur_x_reg.funct >= 1)){
						if(cur_x_reg.e[9] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break;
				case 0x3:
					new_x_reg->funct = 27; // sd
					if((cur_x_reg.funct <= 7) & (cur_x_reg.funct >= 1)){
						if(cur_x_reg.e[9] == ((instr & 0xF8000) >> 15)){
							new_x_reg->stall = true;
						}
					}
					break;
			}
			break;

		// for 'SB' format of instruction
		case 0x63:
			switch(funct3)
			{
				case 0x0:
					new_x_reg->funct = 44; break; // beq
				case 0x1:
					new_x_reg->funct = 45; break; // bne
				case 0x4:
					new_x_reg->funct = 46; break; // blt
				case 0x5:
					new_x_reg->funct = 47; break; // bge
				case 0x6:
					new_x_reg->funct = 48; break; // bltu
				case 0x7:
					new_x_reg->funct = 49; break; // bgeu
			}
			break;

		// for 'U' format of instruction
		case 0x17:
			new_x_reg->funct = 19; break; // auipc
		case 0x37:
			new_x_reg->funct = 38; break; // lui

		// for 'UJ' format of instruction
		case 0x6F:
			new_x_reg->funct = 51; break; // jal
	}
}

void stage_execute (struct stage_reg_m *new_m_reg){
	//printf(">>>>> EXECUTE STAGE <<<<<\n");
	//printf("> The instruction is: 0x%08x\n", cur_x_reg.instruction);
	//printf("> The cur_x_reg.funct is: %d\n", cur_x_reg.funct);

	if(cur_x_reg.i_cache_stall){
		new_m_reg->i_cache_stall = true;
		return;
	}else{
		new_m_reg->i_cache_stall = false;
	}
	
	if(cur_w_reg.d_cache_stall){
		return;
	}
	
	if(cur_m_reg.wrong_prediction){
		if(cur_d_reg.fixed){
			new_m_reg->wrong_prediction = false;
		}else{
			new_m_reg->wrong_prediction = true;
			return;
		}
		
	}
	
	if(cur_m_reg.branch){
		new_m_reg->branch = false;
		return;
	}
	
	if(cur_x_reg.stall){
		return;
	}
	
	new_m_reg->ptr = &cur_m_reg;
	uint64_t p_rs1 = 0, p_rs2 = 0, p_r = 0, shiftAmount = 0, dest = 0, temp = 0;
	
	new_m_reg->pc = cur_x_reg.pc;
	//printf("> stored in m_reg pc is: 0x%016lx\n",cur_x_reg.pc);
	new_m_reg->instruction = cur_x_reg.instruction;
	for(int i = 0; i<11; i++){
		new_m_reg->e[i] = cur_x_reg.e[i];
	}
	
	new_m_reg->memoryRead = false;
	new_m_reg->memoryWrite = false;
	new_m_reg->writeRun = false;

	switch(cur_x_reg.funct)
	{
		case 1: ;// lb															// Initialize pointers
			//printf("> Execute lb\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
				//	printf("> forwardingValue is: 0x%016lx\n", p_rs2);
				//	printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
			//	printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 1;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 2: ;// lh															// Initialize pointers
			//printf("> Execute lh\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 2;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 3: ;// lw															// Initialize pointers
			//printf("> Execute lw\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 4;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 4: ;// ld															// Initialize pointers
			//printf("> Execute ld\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 8;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 5: ;// lbu															// Initialize pointers
			//printf("> Execute lbu\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 1;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 6: ;// lhu															// Initialize pointers
			//printf("> Execute lhu\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
				//	printf("> forwardingValue is: 0x%016lx\n", p_rs2);
				//	printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 2;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 7: ;// lwu															// Initialize pointers
			//printf("> Execute lwu\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[9] == cur_m_reg.e[8]){
					new_m_reg->unsigned_passValue = cur_m_reg.forwardingValue;
				//	printf("> forwardingValue is: 0x%016lx\n", p_rs2);
				//	printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				cur_x_reg.e[0] = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
				dest = cur_x_reg.e[8] + cur_x_reg.e[0];													// Add offset to rs1 in "dest" to get memory address
				dest = dest & 0xFFFFFFFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;

				// Pass the required value to M register
				new_m_reg->destinationAddress = dest;
				new_m_reg->sizeOfByte = 4;
				//new_m_reg->forwardingValue = p_rs1+temp;

				// Set the M and W register status
				new_m_reg->memoryRead = true;
			}

			new_m_reg->destinationRegister = cur_x_reg.e[9];

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 10: ;// addi
			//printf("> Execute addi\n");
			temp = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 + temp;
			new_m_reg->forwardingValue = p_rs1+temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 11: ;// slli
		//	printf("> Execute slli\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
			//		printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
			//	printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 << cur_x_reg.e[0];
			new_m_reg->forwardingValue = p_rs1 << cur_x_reg.e[0];

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 12: ;// slti
			//printf("> Execute slti\n");
			temp = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
			//		printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
			//	printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}

			if(p_rs1 < temp){
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x1;
				new_m_reg->forwardingValue = 0x1;
			}else{
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x0;
				new_m_reg->forwardingValue = 0x0;
			}

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 13: ;// sltiu
			//printf("> Execute sltiu\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			if(p_rs1 < cur_x_reg.e[0]){
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x1;
				new_m_reg->forwardingValue = 0x1;
			}else{
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x0;
				new_m_reg->forwardingValue = 0x0;
			}

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 14: ;// xori
			//printf("> Execute xori\n");
			temp = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 ^ temp;
			new_m_reg->forwardingValue = p_rs1^temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 15: ;// srli
			//printf("> Execute slri\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 >> temp;
			new_m_reg->forwardingValue = p_rs1 >> temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 16: ;// srai
			//printf("> Execute srai\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 >> temp;
			new_m_reg->forwardingValue = p_rs1 >> temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 17: ;// ori
			//printf("> Execute ori\n");
			temp = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 | temp;
			new_m_reg->forwardingValue = p_rs1 | temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 18: ;// andi
			//printf("> Execute andi\n");
			temp = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 & temp;
			new_m_reg->forwardingValue = p_rs1 & temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 19: ;// auipc
			break;

		case 20: ;// addiw
			//printf("> Execute addiw\n");
			temp = converter(cur_x_reg.e[0],0x800,0xFFFFFFFFFFFFF000);
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
				p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 + temp;
			new_m_reg->forwardingValue = p_rs1+temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 21: ;// slliw
			//printf("> Execute slliw\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 << cur_x_reg.e[0];
			new_m_reg->forwardingValue = p_rs1 << cur_x_reg.e[0];

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 22: ;// srliw
			//printf("> Execute slriw\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 >> temp;
			new_m_reg->forwardingValue = p_rs1 >> temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 23: ;// sraiw
			//printf("> Execute sraiw\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs1 = cur_x_reg.rs1;
				//printf("> ___READ___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 >> temp;
			new_m_reg->forwardingValue = p_rs1 >> temp;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 24: ;// sb
			//printf("> Execute sb\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_w_reg.forwardingValue;
					p_rs2 = p_rs2 &0xFF;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs2 = cur_x_reg.rs2;
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
			  new_m_reg->forwardTag = true;
			}

			p_rs1 = cur_x_reg.rs1;
			//register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			p_r = ((cur_x_reg.e[1] << 5) | cur_x_reg.e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;

			// Pass the required value to M register
			new_m_reg->destinationAddress = p_r;
			new_m_reg->sizeOfByte = 1;
			new_m_reg->unsigned_passValue = p_rs2;
			new_m_reg->forwardingValue = p_rs2;

			// Set the M and W register status
			new_m_reg->memoryWrite = true;

			break;

		case 25: ;// sh
			//printf("> Execute sh\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_w_reg.forwardingValue;
					p_rs2 = p_rs2 &0xFF;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs2 = cur_x_reg.rs2;
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
			  new_m_reg->forwardTag = true;
			}

			p_rs1 = cur_x_reg.rs1;
			//register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			p_r = ((cur_x_reg.e[1] << 5) | cur_x_reg.e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;

			// Pass the required value to M register
			new_m_reg->destinationAddress = p_r;
			new_m_reg->sizeOfByte = 2;
			new_m_reg->unsigned_passValue = p_rs2;
			new_m_reg->forwardingValue = p_rs2;

			// Set the M and W register status
			new_m_reg->memoryWrite = true;

			break;

		case 26: ;// sw
			//printf("> Execute sw\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_w_reg.forwardingValue;
					p_rs2 = p_rs2 &0xFF;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs2 = cur_x_reg.rs2;
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			p_rs1 = cur_x_reg.rs1;
			//register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			p_r = ((cur_x_reg.e[1] << 5) | cur_x_reg.e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;

			// Pass the required value to M register
			new_m_reg->destinationAddress = p_r;
			new_m_reg->sizeOfByte = 4;
			new_m_reg->unsigned_passValue = p_rs2;
			new_m_reg->forwardingValue = p_rs2;

			// Set the M and W register status
			new_m_reg->memoryWrite = true;

			break;

		case 27: ;// sd
			//printf("> Execute sd\n");
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_w_reg.forwardingValue;
					p_rs2 = p_rs2 &0xFF;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				p_rs2 = cur_x_reg.rs2;
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			p_rs1 = cur_x_reg.rs1;
			//register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			p_r = ((cur_x_reg.e[1] << 5) | cur_x_reg.e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;

			// Pass the required value to M register
			new_m_reg->destinationAddress = p_r;
			new_m_reg->sizeOfByte = 8;
			new_m_reg->unsigned_passValue = p_rs2;
			new_m_reg->forwardingValue = p_rs2;

			// Set the M and W register status
			new_m_reg->memoryWrite = true;

			break;

		case 28: ;// add
			//printf("> Execute add\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 + p_rs2;
			new_m_reg->forwardingValue = p_rs1+p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 29: ;// sub
			//printf("> Execute sub\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 - p_rs2;
			new_m_reg->forwardingValue = p_rs1-p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 30: ;// sll
			//printf("> Execute sll\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 << p_rs2;
			new_m_reg->forwardingValue = p_rs1<<p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 31: ;// slt
			//printf("> Execute slt\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

		  if(p_rs1 < p_rs2){
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x1;
				new_m_reg->forwardingValue = 0x1;
			}else{
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x0;
				new_m_reg->forwardingValue = 0x0;
			}

			// Set the M and W register status
			new_m_reg->writeRun = true;

		case 32: ;// sltu
			//printf("> Execute sltu\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			if(p_rs1 < p_rs2){
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x1;
				new_m_reg->forwardingValue = 0x1;
			}else{
				// Pass the required value to M register
				new_m_reg->destinationRegister = cur_x_reg.e[9];
				new_m_reg->unsigned_passValue = 0x0;
				new_m_reg->forwardingValue = 0x0;
			}

			// Set the M and W register status
			new_m_reg->writeRun = true;


		case 33: ;// xor
			//printf("> Execute xor\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 ^ p_rs2;
			new_m_reg->forwardingValue = p_rs1^p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 34: ;// srl
			//printf("> Execute srl\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 >> p_rs2;
			new_m_reg->forwardingValue = p_rs1>>p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 35: ;// sra

			break;

		case 36: ;// or
			//printf("> Execute or\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 | p_rs2;
			new_m_reg->forwardingValue = p_rs1|p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 37: ;// and
			//printf("> Execute and\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 & p_rs2;
			new_m_reg->forwardingValue = p_rs1&p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 38: ;// lui
			
			break;

		case 39: ;// addw
			//printf("> Execute addw\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 + p_rs2;
			new_m_reg->forwardingValue = p_rs1+p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;
			

		case 40: ;// subw
			//printf("> Execute subw\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 - p_rs2;
			new_m_reg->forwardingValue = p_rs1-p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;

			break;

		case 41: ;// sllw
			//printf("> Execute sllw\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 << p_rs2;
			new_m_reg->forwardingValue = p_rs1<<p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 42: ;// srlw
			//printf("> Execute srlw\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
			}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 >> p_rs2;
			new_m_reg->forwardingValue = p_rs1>>p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;
		
		case 44: ;// beq
            //printf("> Execute beq\n");
			p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_r << 1;
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			
			// forwarding
			if(cur_w_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_w_reg.unsigned_passValue;
			}else if(cur_w_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_w_reg.unsigned_passValue;
			}
			
			if(cur_m_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_m_reg.unsigned_passValue;
			}else if(cur_m_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_m_reg.unsigned_passValue;
			}
			
			//printf("p_rs1 = 0x%016lx, p_rs1 = 0x%016lx\n", p_rs1, p_rs2);
			if(p_rs1 == p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(cur_x_reg.pc + p_r);
				//printf("?????????????????????????");
			}
			
			//printf("> current_pc is: 0x%016lx\n", cur_x_reg.pc);
			//printf("> offset is: 0x%016lx\n", p_r);
			//printf("> new_pc is: 0x%016lx\n", (cur_x_reg.pc + p_r));
			
			// branch prediction checking
			for(int i = 0; i<32; i++){
				if(BTB[i][0] == cur_x_reg.pc){
					if(BTB[i][1] == (cur_x_reg.pc + p_r)){
						// same Target -> correct branch prediction, keep going
						break;
					}else{
						if(BTB[i][1] == 0){ //no previous record for prediction, then store new Target
							BTB[i][1] = (cur_x_reg.pc + p_r);
							break;
						}else{
							// different target -> wrong branch prediction, need update and stalls
							BTB[i][1] = (cur_x_reg.pc + p_r);
							new_m_reg->wrong_prediction = true;
						}
					}
				}
			}
			break;
			
		case 45: ;// bne
			p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_r << 1;
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			
			// forwarding
			if(cur_w_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_w_reg.unsigned_passValue;
			}else if(cur_w_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_w_reg.unsigned_passValue;
			}
			
			if(cur_m_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_m_reg.unsigned_passValue;
			}else if(cur_m_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_m_reg.unsigned_passValue;
			}
			
			//printf("p_rs1 = 0x%016lx, p_rs1 = 0x%016lx\n", p_rs1, p_rs2);
			if(p_rs1 != p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(cur_x_reg.pc + p_r);
			}
			
			//printf("> current_pc is: 0x%016lx\n", cur_x_reg.pc);
			//printf("> offset is: 0x%016lx\n", p_r);
			//printf("> new_pc is: 0x%016lx\n", (cur_x_reg.pc + p_r));
			
			// branch prediction checking
			for(int i = 0; i<32; i++){
				if(BTB[i][0] == cur_x_reg.pc){
					if(BTB[i][1] == (cur_x_reg.pc + p_r)){
						// same Target -> correct branch prediction, keep going
						break;
					}else{
						if(BTB[i][1] == 0){ //no previous record for prediction, then store new Target
							BTB[i][1] = (cur_x_reg.pc + p_r);
							break;
						}else{
							// different target -> wrong branch prediction, need update and stalls
							BTB[i][1] = (cur_x_reg.pc + p_r);
							new_m_reg->wrong_prediction = true;
						}
					}
				}
			}
			break;

		case 46: ;// blt
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_r << 1;
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			
			// forwarding
			if(cur_w_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_w_reg.unsigned_passValue;
			}else if(cur_w_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_w_reg.unsigned_passValue;
			}
			
			if(cur_m_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_m_reg.unsigned_passValue;
			}else if(cur_m_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_m_reg.unsigned_passValue;
			}
			
			//printf("p_rs1 = 0x%016lx, p_rs1 = 0x%016lx\n", p_rs1, p_rs2);
			if((int64_t)p_rs1 < (int64_t)p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(cur_x_reg.pc + p_r);
			}
			
			//printf("> current_pc is: 0x%016lx\n", cur_x_reg.pc);
			//printf("> offset is: 0x%016lx\n", p_r);
			//printf("> new_pc is: 0x%016lx\n", (cur_x_reg.pc + p_r));
				
			// branch prediction checking
			for(int i = 0; i<32; i++){
				if(BTB[i][0] == cur_x_reg.pc){
					if(BTB[i][1] == (cur_x_reg.pc + p_r)){
						// same Target -> correct branch prediction, keep going
						break;
					}else{
						if(BTB[i][1] == 0){ //no previous record for prediction, then store new Target
							BTB[i][1] = (cur_x_reg.pc + p_r);
							break;
						}else{
							// different target -> wrong branch prediction, need update and stalls
							BTB[i][1] = (cur_x_reg.pc + p_r);
							new_m_reg->wrong_prediction = true;
						}
					}
				}
			}
			break;

		case 47: ;// bge
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_r << 1;
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			
			// forwarding
			if(cur_w_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_w_reg.unsigned_passValue;
			}else if(cur_w_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_w_reg.unsigned_passValue;
			}
			
			if(cur_m_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_m_reg.unsigned_passValue;
			}else if(cur_m_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_m_reg.unsigned_passValue;
			}
			
			//printf("p_rs1 = 0x%016lx, p_rs1 = 0x%016lx\n", p_rs1, p_rs2);
			if((int64_t)p_rs1 >= (int64_t)p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(cur_x_reg.pc + p_r);
			}
			
			//printf("> current_pc is: 0x%016lx\n", cur_x_reg.pc);
			//printf("> offset is: 0x%016lx\n", p_r);
			//printf("> new_pc is: 0x%016lx\n", (cur_x_reg.pc + p_r));
			
			// branch prediction checking
			for(int i = 0; i<32; i++){
				if(BTB[i][0] == cur_x_reg.pc){
					if(BTB[i][1] == (cur_x_reg.pc + p_r)){
						// same Target -> correct branch prediction, keep going
						break;
					}else{
						if(BTB[i][1] == 0){ //no previous record for prediction, then store new Target
							BTB[i][1] = (cur_x_reg.pc + p_r);
							break;
						}else{
							// different target -> wrong branch prediction, need update and stalls
							BTB[i][1] = (cur_x_reg.pc + p_r);
							new_m_reg->wrong_prediction = true;
						}
					}
				}
			}
			break;


		case 48: ;// bltu
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_r << 1;
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			
			// forwarding
			if(cur_w_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_w_reg.unsigned_passValue;
			}else if(cur_w_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_w_reg.unsigned_passValue;
			}
			
			if(cur_m_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_m_reg.unsigned_passValue;
			}else if(cur_m_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_m_reg.unsigned_passValue;
			}
			
			//printf("p_rs1 = 0x%016lx, p_rs1 = 0x%016lx\n", p_rs1, p_rs2);
			if(p_rs1 < p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(cur_x_reg.pc + p_r);
			}
			
			//printf("> current_pc is: 0x%016lx\n", cur_x_reg.pc);
			//printf("> offset is: 0x%016lx\n", p_r);
			//printf("> new_pc is: 0x%016lx\n", (cur_x_reg.pc + p_r));
			
			// branch prediction checking
			for(int i = 0; i<32; i++){
				if(BTB[i][0] == cur_x_reg.pc){
					if(BTB[i][1] == (cur_x_reg.pc + p_r)){
						// same Target -> correct branch prediction, keep going
						break;
					}else{
						if(BTB[i][1] == 0){ //no previous record for prediction, then store new Target
							BTB[i][1] = (cur_x_reg.pc + p_r);
							break;
						}else{
							// different target -> wrong branch prediction, need update and stalls
							BTB[i][1] = (cur_x_reg.pc + p_r);
							new_m_reg->wrong_prediction = true;
						}
					}
				}
			}
			break;

		case 49: ;// bgeu
			p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_r << 1;
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			
			// forwarding
			if(cur_w_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_w_reg.unsigned_passValue;
			}else if(cur_w_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_w_reg.unsigned_passValue;
			}
			
			if(cur_m_reg.e[9] == cur_x_reg.e[7]){
				p_rs2 = cur_m_reg.unsigned_passValue;
			}else if(cur_m_reg.e[9] == cur_x_reg.e[8]){
				p_rs1 = cur_m_reg.unsigned_passValue;
			}
			
			//printf("p_rs1 = 0x%016lx, p_rs1 = 0x%016lx\n", p_rs1, p_rs2);
			if(p_rs1 >= p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(cur_x_reg.pc + p_r);
			}
			
			//printf("> current_pc is: 0x%016lx\n", cur_x_reg.pc);
			//printf("> offset is: 0x%016lx\n", p_r);
			//printf("> new_pc is: 0x%016lx\n", (cur_x_reg.pc + p_r));
			
			// branch prediction checking
			for(int i = 0; i<32; i++){
				if(BTB[i][0] == cur_x_reg.pc){
					if(BTB[i][1] == (cur_x_reg.pc + p_r)){
						// same Target -> correct branch prediction, keep going
						break;
					}else{
						if(BTB[i][1] == 0){ //no previous record for prediction, then store new Target
							BTB[i][1] = (cur_x_reg.pc + p_r);
							break;
						}else{
							// different target -> wrong branch prediction, need update and stalls
							BTB[i][1] = (cur_x_reg.pc + p_r);
							new_m_reg->wrong_prediction = true;
						}
					}
				}
			}
			break;

		case 50: ;// jalr
            p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
	        dest = converter(cur_x_reg.e[0], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + dest;
			shiftAmount = p_r & 0xfffffffe;
			new_m_reg->branch = true;
			set_pc(cur_x_reg.pc + shiftAmount);
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = shiftAmount;
			
			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 51: ;// jal
			p_r = converter(cur_x_reg.e[4], 0x8000, 0xFFFFFFFFFFFF0000);
			set_pc(cur_x_reg.pc + p_r);
			new_m_reg->branch = true;
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = get_pc() +4;
			
			// Set the M and W register status
			new_m_reg->writeRun = true;
			
			break;
		
		case 60: ; // mul
			//printf("> Execute mul\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
			}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 * p_rs2;
			new_m_reg->forwardingValue = p_rs1 * p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;
			
		case 61: ; // div
			//printf("> Execute div\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
			}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 / p_rs2;
			new_m_reg->forwardingValue = p_rs1 / p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;
			
		case 62: ; // rem
			//printf("> Execute rem\n");
			p_rs1 = cur_x_reg.rs1;
			p_rs2 = cur_x_reg.rs2;
			if(cur_m_reg.forwardTag){
				if(cur_x_reg.e[8] == cur_m_reg.e[9]){
					p_rs1 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs1);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
			}else if(cur_x_reg.e[7] == cur_m_reg.e[9]){
					p_rs2 = cur_m_reg.forwardingValue;
					//printf("> forwardingValue is: 0x%016lx\n", p_rs2);
					//printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = p_rs1 % p_rs2;
			new_m_reg->forwardingValue = p_rs1 % p_rs2;

			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;
			
	}

}

void stage_memory (struct stage_reg_w *new_w_reg){
	//printf(">>>>> MEMORY STAGE <<<<<\n");
	
	uint64_t temp;
	uint64_t result_array[2]; // result[1] contains success status of cache, result[2] contains exact result
	
	if(cur_m_reg.i_cache_stall){
		new_w_reg->i_cache_stall = true;
		return;
	}else{
		new_w_reg->i_cache_stall = false;
	}
	
	if(cur_w_reg.d_cache_stall){
		if(memory_status(cur_m_reg.destinationAddress, &temp)){
			if(cur_w_reg.first_cache_stall){
				new_w_reg->first_cache_stall = false;
				new_w_reg->d_cache_stall = false;
			}else{
				update_d_cache(d_cache, cur_m_reg.destinationAddress, temp);
				new_w_reg->d_cache_stall = true;
				new_w_reg->first_cache_stall = true;
			}
		}else{
			new_w_reg->d_cache_stall = true;
			return;
		}
	}
	
	if(cur_m_reg.wrong_prediction){
		new_w_reg->wrong_prediction = true;
		return;
	}
	
	if(cur_m_reg.branch){
		new_w_reg->branch = true;
		return;
	}else{
		new_w_reg->branch = false;
	}
	
	if(cur_m_reg.stall){
		new_w_reg->stall = false;
		return;
	}
	
	new_w_reg->ptr = &cur_w_reg;
	new_w_reg->pc = cur_m_reg.pc;
	new_w_reg->instruction = cur_m_reg.instruction;	
	for(int i = 0; i<11; i++){
		new_w_reg->e[i] = cur_m_reg.e[i];
	}
	new_w_reg->run = cur_m_reg.writeRun;
	new_w_reg->destinationRegister = cur_m_reg.destinationRegister;
	new_w_reg->unsigned_passValue = cur_m_reg.unsigned_passValue;
	
	if(cur_m_reg.memoryRead){
		uint64_t* temp_result = check_d_cache(d_cache, cur_m_reg.destinationAddress, cur_m_reg.sizeOfByte, result_array);
		if(temp_result[0] == 1){ // d-cache hit
			new_w_reg->unsigned_passValue = temp_result[1];
			new_w_reg->forwardingValue = temp_result[1];
			new_w_reg->d_cache_stall = false;
		}else{ // d-cache miss
			bool status = memory_read(cur_m_reg.destinationAddress, &temp, 8);
			new_w_reg->d_cache_stall = true; // memory read miss, needs stalls
			if(status){ // successfully read value from the memory
				update_d_cache(d_cache, cur_m_reg.destinationAddress, temp);
			}else{ // failed to read value from the memory, need stalls
			}
		}
			
		//printf("> Memory Reading\n> MemAddress is: 0x%016lx\n> value is: 0x%016lx\n",cur_m_reg.destinationAddress,temp);
	
	}else if(cur_m_reg.memoryWrite){

		memory_write(cur_m_reg.destinationAddress, cur_m_reg.unsigned_passValue, cur_m_reg.sizeOfByte);
		new_w_reg->forwardingValue = cur_m_reg.unsigned_passValue;
		
		write_d_cache(d_cache, cur_m_reg.destinationAddress, cur_m_reg.unsigned_passValue);
		
		//printf("> Memory Writing\n> MemAddress is: 0x%016lx\n> value is: 0x%016lx\n",cur_m_reg.destinationAddress,cur_m_reg.unsigned_passValue);
	}

}

void stage_writeback (void){
	//printf("----------------------------------------------\n");
	//printf(">>>>> WRITEBACK STAGE <<<<<\n");

	if(cur_w_reg.i_cache_stall){
		return;
	}
		
	if(cur_w_reg.d_cache_stall){
		return;
	}
	
	if(cur_m_reg.wrong_prediction){
		return;
	}
	
	if(cur_w_reg.branch){
		return;
	}
	
	if(cur_w_reg.run){
		register_write(cur_w_reg.destinationRegister, cur_w_reg.unsigned_passValue);
		//printf("> The Register is: %d\n", cur_w_reg.destinationRegister);
		//printf("> The Value in Register is: %d\n", cur_w_reg.unsigned_passValue);
	}
	
	//uint64_t p_rs1, p_rs2, p_r;
	//register_read (1, 2, &p_rs1, &p_rs2);
	//printf("> Register 1: 0x%016lx\n",p_rs1);
	//printf("> Register 2: 0x%016lx\n",p_rs2);
	//register_read (3, 3, &p_r, &p_r);
	//printf("> Register 3: 0x%016lx\n",p_r);

}

void unit_tests(){

}