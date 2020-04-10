#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "riscv_sim_pipeline_framework.h"
#include "riscv_pipeline_registers.h"

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
extern bool memory_status (uint64_t address, uint64_t *value);

extern void register_read (uint64_t register_a, uint64_t register_b, uint64_t * value_a, uint64_t * value_b);
extern void register_write (uint64_t register_d, uint64_t value_d);

extern void     set_pc (uint64_t pc);
extern uint64_t get_pc (void);

// To do the sign-extended
uint64_t converter(uint64_t i, uint64_t most_significant, uint64_t bitwiseNum)
{
	if(( i & most_significant ) == most_significant)
	{
		i = i | bitwiseNum;
	}
	return i;
}

void stage_fetch (struct stage_reg_d *new_d_reg){
	//printf(">>>>> FETCH STAGE <<<<<\n");
	
	if(cur_x_reg.stall){
		return;
	}
	
	new_d_reg->ptr = &cur_d_reg;
	
	uint64_t pc = get_pc();
	uint32_t inst;
	memory_read(pc, &inst, 4);
	new_d_reg->instruction = inst;
	new_d_reg->new_pc = (pc + 4);
	set_pc(pc+4);
	//printf("> PC: 0x%016lx\n",pc);
	//printf("> New_PC: 0x%016lx\n", new_d_reg->new_pc);
	//printf("> Instruction is: 0x%08x\n", inst);
}

void stage_decode (struct stage_reg_x *new_x_reg){
	//printf(">>>>> DECODE STAGE STARTS <<<<<\n");
	
	if(cur_x_reg.stall){
		new_x_reg->stall = false;
		return;
	}
	
	new_x_reg->ptr = &cur_x_reg;
	uint32_t instr = cur_d_reg.instruction;
	new_x_reg->instruction = instr;
	//printf(" Instruction is: 0x%08x\n", instr);
	// store the pieces of instruction based on their formats
	uint64_t opcode = instr & 0x7F;
	uint64_t funct3 = 0;
	uint64_t funct7 = 0;
	switch(opcode)
	{
		// for 'R' format of instrcution
		case 0x33:
		case 0x3B:
			funct7  = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
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
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
			funct3  = (instr & 0x7000) >> 12;
			new_x_reg->e[9]    = (instr & 0xF80) >> 7;
			new_x_reg->e[1]    = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			break;

		// for 'S' format of instruction
		case 0x23:
			new_x_reg->e[1]    = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
			funct3  = (instr & 0x7000) >> 12;
			new_x_reg->e[5]    = (instr & 0xF80) >> 7;
			break;

		// for 'SB' format of instruction
		case 0x63:
			new_x_reg->e[2]    = (instr & 0xFE000000) >> 25;
			new_x_reg->e[7]    = (instr & 0x1F00000) >> 20;
			new_x_reg->e[8]    = (instr & 0xF8000) >> 15;
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
					new_x_reg->funct = 10; // addi
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

	if(cur_m_reg.branch){
		new_m_reg->branch = false;
		return;
	}
	
	if(cur_x_reg.stall){
		return;
	}
	
	new_m_reg->ptr = &cur_m_reg;
	uint64_t p_rs1 = 0, p_rs2 = 0, p_r = 0, shiftAmount = 0, dest = 0, temp;
	
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
				//	printf("> forwardingValue is: 0x%016lx\n", p_rs1);
				//	printf("> ___FORWARDING HERE___\n");
					new_m_reg->forwardTag = false;
				}
			}else{
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
			//	printf("> ___READ from REGISTER___\n");
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read (cur_x_reg.e[8], cur_x_reg.e[8], &p_rs1, &p_r);
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
				register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
			  new_m_reg->forwardTag = true;
			}

			register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
				register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
			  new_m_reg->forwardTag = true;
			}

			register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
				register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
				register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
				p_rs2 = p_rs2 & 0xFF;
				//printf("> ___READ from REGISTER___\n");
				new_m_reg->forwardTag = true;
			}

			register_read(cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			if(p_rs1 == p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(get_pc() + p_r);
			}
			break;
			
		case 45: ;// bne
			p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			if(p_rs1 != p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(get_pc() + p_r);
			}
			break;

		case 46: ;// blt
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			if((int64_t)p_rs1 < (int64_t)p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(get_pc() + p_r);
			}
			break;

		case 47: ;// bge
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			if((int64_t)p_rs1 >= (int64_t)p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(get_pc() + p_r);
			}
			break;


		case 48: ;// bltu
            p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			if(p_rs1 < p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(get_pc() + p_r);
			}
			break;

		case 49: ;// bgeu
			p_r = converter(cur_x_reg.e[10], 0x800, 0xFFFFFFFFFFFFF000);
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
			if(p_rs1 >= p_rs2)
			{
				new_m_reg->branch = true;
				set_pc(get_pc() + p_r);
			}
			break;

		case 50: ;// jalr
            register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
	        dest = converter(cur_x_reg.e[0], 0x800, 0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + dest;
			shiftAmount = p_r & 0xfffffffe;
			new_m_reg->branch = true;
			set_pc(get_pc() + shiftAmount);
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = shiftAmount;
			
			// Set the M and W register status
			new_m_reg->writeRun = true;
			break;

		case 51: ;// jal
			p_r = converter(cur_x_reg.e[4], 0x8000, 0xFFFFFFFFFFFF0000);
			set_pc(get_pc() + p_r);
			new_m_reg->branch = true;
			
			// Pass the required value to M register
			new_m_reg->destinationRegister = cur_x_reg.e[9];
			new_m_reg->unsigned_passValue = get_pc() +4;
			
			// Set the M and W register status
			new_m_reg->writeRun = true;
			
			break;
		
		case 60: ; // mul
			//printf("> Execute mul\n");
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
			register_read (cur_x_reg.e[8], cur_x_reg.e[7], &p_rs1, &p_rs2);
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
	
	if(cur_m_reg.branch){
		return;
	}
	
	if(cur_m_reg.stall){
		new_w_reg->stall = false;
		return;
	}
	
	new_w_reg->ptr = &cur_w_reg;
	new_w_reg->instruction = cur_m_reg.instruction;	
	for(int i = 0; i<11; i++){
		new_w_reg->e[i] = cur_m_reg.e[i];
	}
	new_w_reg->run = cur_m_reg.writeRun;
	new_w_reg->destinationRegister = cur_m_reg.destinationRegister;
	new_w_reg->unsigned_passValue = cur_m_reg.unsigned_passValue;
	
	uint64_t temp;
	
	if(cur_m_reg.memoryRead){
		memory_read(cur_m_reg.destinationAddress, &temp, cur_m_reg.sizeOfByte);
		new_w_reg->unsigned_passValue = temp;
		
		new_w_reg->forwardingValue = temp;
		
		//printf("> Memory Reading\n> MemAddress is: 0x%016lx\n> value is: 0x%016lx\n",cur_m_reg.destinationAddress,temp);
		
	}else if(cur_m_reg.memoryWrite){
		memory_write(cur_m_reg.destinationAddress, cur_m_reg.unsigned_passValue, cur_m_reg.sizeOfByte);
		
		new_w_reg->forwardingValue = cur_m_reg.unsigned_passValue;
		
		//printf("> Memory Writing\n> MemAddress is: 0x%016lx\n> value is: 0x%016lx\n",cur_m_reg.destinationAddress,cur_m_reg.unsigned_passValue);
	}

}

void stage_writeback (void){
	//printf("----------------------------------------------\n");
	//printf(">>>>> WRITEBACK STAGE <<<<<\n");

	if(cur_w_reg.run){
		register_write(cur_w_reg.destinationRegister, cur_w_reg.unsigned_passValue);
	//	printf("> The Register is: %d\n", cur_w_reg.destinationRegister);
	//	printf("> The Value in Register is: %d\n", cur_w_reg.unsigned_passValue);
	}

}


void unit_tests(){

}
