#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "riscv_sim_framework.h"

// Pre-define the element array for parts of instruction
/*
	uint64_t imm_11_0; --0
	uint64_t imm_11_5; --1
	uint64_t imm_12_5; --2
	uint64_t imm_31_12;--3
	uint64_t UJ_Type;  --4
	uint64_t imm_4_0;  --5
	uint64_t imm_4_11; --6
	uint64_t rs2;      --7
	uint64_t rs1;      --8
	uint64_t rd;       --9
	uint64_t branchImm --10
*/

void memory(uint64_t address, uint64_t value, uint64_t size_in_bytes)
{
	bool success_write_memory = memory_write (address, value, size_in_bytes);
	if( success_write_memory == false )
	{
		printf("Failure to write back in memory, address is: %081lx", address);
	}
}

void write(uint64_t register_d, uint64_t value_d)
{
	register_write (register_d, value_d);
}

// To do the sign-extended
uint64_t converter(uint64_t i, uint64_t most_significant, uint64_t bitwiseNum)
{
	if(( i & most_significant ) == most_significant)
	{
		i = i | bitwiseNum;
	}
	return i;
}

void execution(int instrNum, uint64_t e[], uint64_t *new_pc)
{
	uint64_t p_rs1 = 0, p_rs2 = 0, p_r = 0, shiftAmount = 0, dest = 0;
	switch(instrNum)
	{
		case 1: ;// lb															// Initialize pointers
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			dest = dest & 0xFFFFFFFF;
			memory_read (dest, &p_rs1, 1); 							// Reads "dest" from memory to get value of rs1 32 bits (1 bytes)
			write (e[9], (int8_t)p_rs1);			// Write back to register rd with value of sign extended rs1
			break;

		case 2: ;// lh
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			dest = dest & 0xFFFFFFFF;
			memory_read (dest, &p_rs1, 2); 							// Reads "dest" from memory to get value of rs1 32 bits (2 bytes)
			write (e[9], (int16_t)p_rs1);								// Write back to register rd with value of sign extended rs1
			break;

		case 3: ;// lw
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			dest = dest & 0xFFFFFFFF;
			memory_read (dest, &p_rs1, 4); 							// Reads "dest" from memory to get value of rs1 32 bits (4 bytes)
			write (e[9], (int32_t)p_rs1);								// Write back to register rd with value of sign extended rs1
			break;

		case 4: ;// ld
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			memory_read (dest, &p_rs1, 8); 							// Reads "dest" from memory to get value of rs1 32 bits (8 bytes)
			write (e[9], p_rs1);										// Write back to register rd with value of sign extended rs1
			break;

		case 5: ;// lbu
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			memory_read (dest, &p_rs1, 1); 							// Reads "dest" from memory to get value of rs1 32 bits (1 bytes)
			write (e[9], (uint8_t)p_rs1);							// Write back to register rd with value of zero extended rs1
			break;

		case 6: ;// lhu
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			memory_read (dest, &p_rs1, 2); 							// Reads "dest" from memory to get value of rs1 32 bits (2 bytes)
			write (e[9], (uint16_t)p_rs1);							// Write back to register rd with value of zero extended rs1
			break;

		case 7: ;// lwu
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			dest = e[8] + e[0];													// Add offset to rs1 in "dest" to get memory address
			memory_read (dest, &p_rs1, 4); 							// Reads "dest" from memory to get value of rs1 32 bits (4 bytes)
			write (e[9], (uint32_t)p_rs1);							// Write back to register rd with value of zero extended rs1
			break;

		case 8: ;break; // fence
		case 9: ;break; // fence.i

		case 10: ;// addi
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			register_read (e[8], e[8], &p_rs1, &p_r);
			p_rs1 = p_rs1 & 0xFFFFFFFF;
			write(e[9], p_rs1 + e[0]);
			break;

		case 11: ;// slli
			register_read (e[8], e[8], &p_rs1, &p_r);
			write(e[9], p_rs1 << e[0]);
			break;

		case 12: ;// slti
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			register_read (e[8], e[8], &p_rs1, &p_r);
			if ((int64_t)p_rs1 < e[0] )
			{
				write (e[9], 0x1);
			}else{
				write (e[9], 0x0);
			}
			break;

		case 13: ;// sltiu
			register_read (e[8], e[8], &p_rs1, &p_r);
			if (p_rs1 < e[0] )
			{
				write (e[9], 0x1);
			}else{
				write (e[9], 0x0);
			}
			break;

		case 14: ;// xori
			register_read (e[8], e[8], &p_rs1, &p_r);
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			write(e[9], (int64_t)((p_rs1) ^ (e[0])));
			break;

		case 15: ;// srli
			register_read (e[8], e[8], &p_rs1, &p_r);
			write(e[9], p_rs1 >> e[0]);
			break;

		case 16: ;// srai
			register_read (e[8], e[8], &p_rs1, &p_r);
			// Shift the rs1 by the amount of rs2 and write back to rd
			p_rs1 = p_rs1 & 0xFFFFFFFF;
			e[0] = e[0] & 0x1F;
			for (int i = 1; i <= e[0]; i++)
			{
				int n = p_rs1 % 2;
				if ( n == 1 )
				{
					p_rs1 = p_rs1 >> 1;
					p_rs1 = p_rs1 + 0x80000000;
				}else{
					p_rs1 = p_rs1 >> 1;
				}
			}
			p_rs1 = converter(p_rs1,0x80000000,0xFFFFFFFF00000000);
			write(e[9], p_rs1);
			break;

		case 17: ;// ori
			register_read(e[8], e[8], &p_rs1, &p_r);
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 | e[0];
			p_r = p_r & 0xFFFFFFFF;
			write(e[9], p_r);
			break;

		case 18: ;// andi
			register_read(e[8], e[8], &p_rs1, &p_r);
			e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 & e[0];
			p_r = p_r & 0xFFFFFFFF;
			write(e[9], p_r);
			break;

		case 19: ;// auipc
			p_r = e[3] << 12;
			p_r = p_r & 0xFFFFFFFF;
			p_r = p_r + (*new_pc - 4);
			write(e[9], p_r);
			break;

		case 20: ;// addiw
			register_read(e[8], e[8], &p_rs1, &p_r);
			p_rs2 = p_rs1 + e[3];
			p_rs2 = p_rs2 & 0xFFFFFFFF;
			write(e[9], (int64_t)p_rs2);
			break;

		case 21: ;// slliw
			// Use bit-masking to know how many bits we need to shift
			shiftAmount = e[0] & 0x1F;
			// To know the rs1
			register_read(e[8], e[8], &p_rs1, &p_r);
			// Shift the rs1 by shiftAmount bits and write it into the rd
			p_rs1 = (p_rs1 & 0xFFFF) << shiftAmount;
			p_rs1 = converter(p_rs1,0x80000000,0xFFFFFFFF00000000);
			register_write(e[9], p_rs1);
			break;

		case 22: ;// srliw
			// Use bit-masking to know how many bits we need to shift
			shiftAmount = e[0] & 0x1F;
			// To know the rs1
			register_read(e[8], e[8], &p_rs1, &p_r);
			// Shift the rs1 by shiftAmount bits and write it into the rd
			p_rs1 = (p_rs1 & 0xFFFF) >> shiftAmount;
			p_rs1 = converter(p_rs1,0x80000000,0xFFFFFFFF00000000);
			register_write(e[9], p_rs1);
			break;

		case 23: ;// sraiw
			// Use bit-masking to know how many bits we need to shift
			shiftAmount = e[0] & 0x1F;
			uint64_t temp = e[0] & 0xFFFFFFFF;
			// To know the rs1
			register_read(e[8], e[8], &p_rs1, &p_r);
			// Shift the rs1 by shiftAmount bits and write it into the rd
			for (int i = 0; i <= (int)shiftAmount; i++)
			{
				int n = temp % 2;
				if ( n == 1 )
				{
					temp = temp >> 1;
					temp = temp + 0x80000000;
				}else{
					temp = temp >> 1;
				}
			}
			temp = converter(temp,0x80000000,0xFFFFFFFF00000000);
			register_write(e[9], temp);
			break;

		case 24: ;// sb
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = ((e[1] << 5) | e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;
			p_rs2 = p_rs2 & 0xFF;
			memory(p_r, (int64_t)p_rs2, 1);
			break;

		case 25: ;// sh
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = ((e[1] << 5) | e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;
			p_rs2 = p_rs2 & 0xFFFF;
			memory(p_r, (int64_t)p_rs2, 2);
			break;

		case 26: ;// sw
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = ((e[1] << 5) | e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;
			p_rs2 = p_rs2 & 0xFFFFFFFF;
			memory(p_r, (int64_t)p_rs2, 4);

			break;

		case 27: ;// sd
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = ((e[1] << 5) | e[5]);
			p_r = converter(p_r,0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + p_r;
			memory(p_r, p_rs2, 8);
			break;

		case 28: ;// add
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = (p_rs1 + p_rs2) & 0xFFFFFFFF;
			write(e[9], p_r);
			break;

		case 29: ;// sub
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = (p_rs1 - p_rs2) & 0xFFFFFFFF;
			write(e[9], (int64_t)p_r);
			break;

		case 30: ;// sll
			// Read rs1 and rs2
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			// Shift the rs1 by the amount of rs1 and write bac to rd
			p_rs2 = p_rs2 & 0x1F; // the lower 5 bits of register rs2
			p_rs1 = p_rs1 << p_rs2;
			write(e[9], p_rs1);
			break;

		case 31: ;// slt
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			if( p_rs1 < p_rs2 )
			{
				write(e[9], 0x1);
			}else{
				write(e[9], 0x0);
			}
			break;

		case 32: ;// sltu
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			if( p_rs1 < p_rs2 )
			{
				write(e[9], 0x1);
			}else{
				write(e[9], 0x0);
			}
			break;

		case 33: ;// xor
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = (p_rs1 ^ p_rs2) & 0xFFFFFFFF;
			write(e[9], (int64_t)p_r);
			break;

		case 34: ;// srl
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			// Shift the rs1 by the amount of rs2 and write back to rd
			p_rs2 = p_rs2 & 0x1F; // the lower 5 bits of register rs2
			p_rs1 = p_rs1 >> p_rs2;
			write(e[9], p_rs1);
			break;

		case 35: ;// sra
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			// Shift the rs1 by the amount of rs2 and write back to rd
			p_rs1 = p_rs1 & 0xFFFFFFFF;
			p_rs2 = p_rs2 & 0x1F; // the lower 5 bits of register rs2
			for (int i = 0; i <= (int)p_rs2; i++)
			{
				int n = p_rs1 % 2;
				if ( n == 1 )
				{
					p_rs1 = p_rs1 >> 1;
					p_rs1 = p_rs1 + 0x80000000;
				}else{
					p_rs1 = p_rs1 >> 1;
				}
			}
			write(e[9], p_rs1);
			break;

		case 36: ;// or
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = p_rs1 | p_rs2;
			write(e[9], p_r);
			break;

		case 37: ;// and
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = p_rs1 & p_rs2;
			write(e[9], p_r);
			break;

		case 38: ;// lui
			p_r = e[3] << 12;
			p_r = p_r & 0xFFFFF000;
			write(e[9], p_r);
			break;

		case 39: ;// addw
		    register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_rs1 = p_rs1 & 0xFFFFFFFF;
			p_rs2 = p_rs2 & 0xFFFFFFFF;
			p_r = p_rs1 + p_rs2;
			p_r = converter(p_r, 0x80000000, 0xFFFFFFFF00000000);
			write(e[9], (int64_t)p_r);
			break;

		case 40: ;// subw
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_rs1 = p_rs1 & 0xFFFFFFFF;
			p_rs2 = p_rs2 & 0xFFFFFFFF;
			p_r = p_rs1 - p_rs2;
			if( (p_r & 0x80000000) == 0 ){
				p_r = p_r & 0xFFFFFFFF;
			}
			write(e[9], (int64_t)p_r);
			break;

		case 41: ;// sllw
			register_read(e[8], e[7], &p_rs1, &p_rs2);
            p_rs2 = p_rs2 & 0x1F; // the lower 5 bits of register rs2
			p_r = p_rs1 << p_rs2;
			p_r = p_r & 0xFFFFFFFF;
			write(e[9], (int64_t)p_r);
			break;

		case 42: ;// srlw
            register_read(e[8], e[7], &p_rs1, &p_rs2);
            p_rs2 = p_rs2 & 0x1F; // the lower 5 bits of register rs2
			p_r = p_rs1 >> p_rs2;
			p_r = p_r & 0xFFFFFFFF;
			write(e[9], (int64_t)p_r);
			break;

		case 43: ;// sraw
            register_read(e[8], e[7], &p_rs1, &p_rs2);
			p_r = p_rs1 & 0xFFFFFFFF;
			p_rs2 = p_rs2 & 0x1F; // the lower 5 bits of register rs2
			for (int i = 0; i <= (int)p_rs2; i++)
			{
				int n = p_r % 2;
				if ( n == 1 )
				{
					p_r = p_r >> 1;
					p_r = p_r + 0x80000000;
				}else{
					p_r = p_r >> 1;
				}
			}
			write(e[9], (int64_t)p_r);
			break;

		case 44: ;// beq
            register_read(e[8], e[7], &p_rs1, &p_rs2);
			if(p_rs1 == p_rs2)
			{
				*new_pc = ((*new_pc - 4) + e[10]);
			}
			break;

		case 45: ;// bne
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			if(p_rs1 != p_rs2)
			{
				*new_pc = ((*new_pc - 4) + e[10]);
			}
			break;

		case 46: ;// blt
            register_read(e[8], e[7], &p_rs1, &p_rs2);
			if((int64_t)p_rs1 < (int64_t)p_rs2)
			{
				*new_pc = ((*new_pc - 4) + e[10]);
			}
			break;

		case 47: ;// bge
            register_read(e[8], e[7], &p_rs1, &p_rs2);
			if((int64_t)p_rs1 >= (int64_t)p_rs2)
			{
				*new_pc = ((*new_pc - 4) + e[10]);
			}
			break;


		case 48: ;// bltu
            register_read(e[8], e[7], &p_rs1, &p_rs2);
			if(p_rs1 < p_rs2)
			{
				*new_pc = ((*new_pc - 4) + e[10]);
			}
			break;

		case 49: ;// bgeu
			register_read(e[8], e[7], &p_rs1, &p_rs2);
			if(p_rs1 >= p_rs2)
			{
				*new_pc = ((*new_pc - 4) + e[10]);
			}
			break;

		case 50: ;// jalr
            register_read(e[8], e[7], &p_rs1, &p_rs2);
	        e[0] = converter(e[0],0x800,0xFFFFFFFFFFFFF000);
			p_r = p_rs1 + e[0];
			write(e[9], *new_pc);
			*new_pc = (p_r & 0xfffffffe);
			break;

		case 51: ;// jal
			write(e[9], *new_pc);
			register_read(e[9],e[9], &shiftAmount, &shiftAmount);
			*new_pc = ((*new_pc - 4) + e[4]);
			break;

		case 52: ; break; // ecall
		case 53: ; break; // ebreak
		case 54: ; break; // csrrw
		case 55: ; break; // csrrs
		case 56: ; break; // csrrc
		case 57: ; break; // csrrwi
		case 58: ; break; // csrrsi
		case 59: ; break; // csrrci

		default: ;
			printf("Failure to execute the instrcution, the instr number is %d", instrNum);
	}
}

int decode(uint64_t instr, uint64_t e[])
{
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
			e[7]    = (instr & 0x1F00000) >> 20;
			e[8]    = (instr & 0xF8000) >> 15;
			funct3  = (instr & 0x7000) >> 12;
			e[9]    = (instr & 0xF80) >> 7;
			break;

		// for 'I' format of instruction
		case 0x03:
		case 0x0F: // fence(.i) no need but kept for recoe[9]
		case 0x13:
		case 0x1B:
		case 0x67:
		case 0x73: // csr* no need but kept for recoe[9]
			e[0]    = (instr & 0xFFF00000) >> 20;
			e[8]    = (instr & 0xF8000) >> 15;
			funct3  = (instr & 0x7000) >> 12;
			e[9]    = (instr & 0xF80) >> 7;
			e[1]    = (instr & 0xFE000000) >> 25;
			e[7]    = (instr & 0x1F00000) >> 20;
			break;

		// for 'S' format of instruction
		case 0x23:
			e[1]    = (instr & 0xFE000000) >> 25;
			e[7]    = (instr & 0x1F00000) >> 20;
			e[8]    = (instr & 0xF8000) >> 15;
			funct3  = (instr & 0x7000) >> 12;
			e[5]    = (instr & 0xF80) >> 7;
			break;

		// for 'SB' format of instruction
		case 0x63:
			e[2]    = (instr & 0xFE000000) >> 25;
			e[7]    = (instr & 0x1F00000) >> 20;
			e[8]    = (instr & 0xF8000) >> 15;
			funct3  = (instr & 0x7000) >> 12;
			e[6]    = (instr & 0xF80) >> 7;
			e[10]   = (instr & 0x80000000) >> 20;
			e[10]  += (instr & 0x80) << 3;
			e[10]  += (instr & 0x7E000000) >> 21;
			e[10]  += (instr & 0xF00) >> 8;
			e[10]   = e[10] & 0xFFF;
			e[10]   = converter(e[10],0x800,0xFFFFFFFFFFFFF000);
			break;

		// for 'U' format of instruction
		case 0x17:
		case 0x37:
			e[3]    = (instr & 0xFFFFF000) >> 12;
			e[9]    = (instr & 0xF80) >> 7;
			break;

		// for 'UJ' format of instruction
		case 0x6F:
			e[4]  = (instr & 0x80000000) >> 12;
			e[4] += (instr & 0xFF000) >> 1;
			e[4] += (instr & 0x100000) >> 10;
			e[4] += (instr & 0x7FE00000) >> 21;
			e[4]  = e[4] & 0xFFFFF;
			e[4]  = converter(e[4],0x80000,0xFFFFFFFFFFF00000);
			e[9]  = (instr & 0xF80) >> 7;
			break;

		default:
			printf("Failure to distinguish the opcode in the stage of decode: "
			"\nThe failure opcode is 0x%016llx\n", opcode);
	}

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
						return 28; // add
					}else if(funct7 == 0x20){
						return 29; // sub
					}else if(funct7 == 0x1){
						return 60; // mul
					}
				case 0x1:
					return 30; // sll
				case 0x2:
					return 31; // slt
				case 0x3:
					return 32; // sltu
				case 0x4:
					if(funct7 == 0x0){
						return 33; // xor
					}else if(funct7 == 0x1){
						return 61; // div
					}
				case 0x5:
					if(funct7 == 0x0)
					{
						return 34; // srl
					}else{
						return 35; //sra
					}
				case 0x6:
					if(funct7 == 0x0){
						return 36; // or
					}else if(funct7 == 0x1){
						return 62; // rem
					}
				case 0x7:
					return 37; // and

			}
		case 0x3B:
			switch(funct3)
			{
				case 0x0:
					if(funct7 == 0x0)
					{
						return 39; // addw
					}else{
						printf("WE GOT THIS!!!!!!!!");
						return 40; // subw
					}
				case 0x1:
					return 41; // sllw
				case 0x5:
					if(funct7 == 0x0)
					{
						return 42; // srlw
					}else{
						return 43; // sraw
					}
			}

		// for 'I' format of instruction
		case 0x03:
			switch(funct3)
			{
				case 0x0:
					return 1; // lb
				case 0x1:
					return 2; // lh
				case 0x3:
					return 3; // lw
				case 0x4:
					return 4; // ld
				case 0x5:
					return 5; // lbu
				case 0x6:
					return 6; // lhu
				case 0x7:
					return 7; // lwu
			}
		// fence(.i) (0x0F) -> no need but kept for record
		case 0x0F:
			switch(funct3)
			{
				case 0x0:
					return 8; // fence
				case 0x1:
					return 9; // fence.i
			}
		case 0x13:
			switch(funct3)
			{
				case 0x0:
					return 10; // addi
				case 0x1:
					return 11; // slli
				case 0x2:
					return 12; // slti
				case 0x3:
					return 13; // sltiu
				case 0x4:
					return 14; // xori
				case 0x5:
					if(e[1] == 0x0)
					{
						return 15; // slri
					}else{
						return 16; // srai
					}
				case 0x6:
					return 17; // ori
				case 0x7:
					return 18; // andi
			}
		case 0x1B:
			switch(funct3)
			{
				case 0x0:
					return 20; // addiw
				case 0x1:
					return 21; // slliw
				case 0x2:
					if(e[0] == 0x0)
					{
						return 22; // srliw
					}else{
						return 23; // sraiw
					}
			}
		case 0x67:
			return 50; // jalr

		// csr* (0x73) -> no need but kept for record
		case 0x73:
			switch(funct3)
			{
				case 0x0:
					if (funct7 == 0x0)
					{
						return 52; // ecall
					}else{
						return 53; // ebreak
					}
				case 0x1:
					return 54; // csrrw
				case 0x2:
					return 55; // csrrs
				case 0x3:
					return 56; // csrrc
				case 0x5:
					return 57; // csrrwi
				case 0x6:
					return 58; // csrrsi
				case 0x7:
					return 59; // csrrci

			}

		// for 'S' format of instruction
		case 0x23:
			switch(funct3)
			{
				case 0x0:
					return 24; // sb
				case 0x1:
					return 25; // sh
				case 0x2:
					return 26; // sw
				case 0x3:
					return 27; // sd
			}

		// for 'SB' format of instruction
		case 0x63:
			switch(funct3)
			{
				case 0x0:
					return 44; // beq
				case 0x1:
					return 45; // bne
				case 0x4:
					return 46; // blt
				case 0x5:
					return 47; // bge
				case 0x6:
					return 48; // bltu
				case 0x7:
					return 49; // bgeu
			}

		// for 'U' format of instruction
		case 0x17:
			return 19; // auipc
		case 0x37:
			return 38; // lui

		// for 'UJ' format of instruction
		case 0x6F:
			return 51; // jal


		default:
			printf("Failure to distinguish the specific instruction in the stage of decode: "
			"\nThe failure opcode is 0x%016llx, and the funct3 is 0x%016llx\n", opcode, funct3);
			return 52; // default return
	}
}

uint64_t fetch(uint64_t pc, uint64_t * new_pc)
{
	// Get the instruction from the memory
	uint64_t temp_instr;
	memory_read(pc, &temp_instr, 4);
	// Compute the new pc address by “PC + 4” , written back to “*new_pc”
	*new_pc = (pc + 0x4);

	return temp_instr;
}

extern void execute_single_instruction(const uint64_t pc, uint64_t *new_pc)
{
	// Fetch Stage
	uint64_t instr = fetch(pc, new_pc);

	// Decode Stage
	uint64_t element[11]={0,0,0,0,0,0,0,0,0,0,0};
	int instrNum = decode(instr, element);

	// Execution Stage
	execution(instrNum, element, new_pc);
}
