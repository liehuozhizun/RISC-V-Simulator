/*
 * External routines for use in CMPE 110, Fall 2018
 *
 * (c) 2018 Ethan L. Miller
 * Redistribution not permitted without explicit permission from the copyright holder.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

extern bool memory_read (uint64_t address, void * value, uint64_t size_in_bytes);
extern bool memory_write (uint64_t address, uint64_t value, uint64_t size_in_bytes);
extern bool memory_status (uint64_t address, void *value);

extern void register_read (uint64_t register_a, uint64_t register_b, uint64_t * value_a, uint64_t * value_b);
extern void register_write (uint64_t register_d, uint64_t value_d);

extern void     set_pc (uint64_t pc);
extern uint64_t get_pc (void);

/*
 * These pipeline stage registers are loaded at the end of the cycle with whatever
 * values were filled in by the relevant pipeline stage.
 *
 * Add fields to these stage registers for whatever values you'd like to have passed from
 * one stage to the next.  You may have as many values as you like in each stage.  However,
 * this is the ONLY information that may be passed from one stage to stages in the next
 * cycle.
 */
struct stage_reg_d {
    bool        stall;
	uint64_t    new_pc;
    uint32_t    instruction;
	struct      stage_reg_d  *ptr;
	uint64_t    pc;
	bool        branch_prediction;
	bool        fixed;
	bool        i_cache_stall;
	bool        first_cache_stall;
};
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
struct stage_reg_x {
    bool        stall;
	uint64_t    new_pc;
    uint32_t    instruction;
	uint64_t    e[11];
	int         funct;
	struct      stage_reg_x  *ptr;
	uint64_t    pc;
	bool        branch_prediction;
	bool        fixed;
	uint64_t    rs1;
	uint64_t    rs2;
	bool        i_cache_stall;
};

struct stage_reg_m {
    bool        stall;
	bool        memoryRead;
	bool        memoryWrite;
	bool        writeRun;
	bool        forwardTag;
	bool        branch;
	uint64_t    forwardingValue;
	uint32_t    instruction;
	int         funct;
	uint64_t    e[11];
	uint64_t    destinationAddress;
	uint64_t    destinationRegister;
	int         sizeOfByte;
	uint64_t    unsigned_passValue;
	struct      stage_reg_m  *ptr;
	//int64_t     signed_passValue;
	uint64_t    pc;
	bool        branch_prediction;
	bool        wrong_prediction;
	bool        i_cache_stall;
};

struct stage_reg_w {
    bool        stall;
	bool        run;
	uint32_t    instruction;
	bool        forwardingTag;
	bool        branch;
	uint64_t    forwardingValue;
	uint64_t    destinationRegister;
	uint64_t    e[11];
	uint64_t    unsigned_passValue;
	struct      stage_reg_w  *ptr;
	//int64_t     signed_passValue;
	uint64_t    pc;
	bool        branch_prediction;
	bool        wrong_prediction;
	bool        d_cache_stall;
	bool        read_again;
	bool        first_cache_stall;
	bool        i_cache_stall;
};

/*
 * These are the functions students need to implement for Assignment 2.
 * Each of your functions must fill in the fields for the stage register
 * passed by reference.  The contents of the fields that you fill in
 * will be copied to the pipeline stage registers above (current_stage_?_register)
 * at the end of a CPU cycle, and will be available for the pipeline
 * stage functions in the next cycle.
 */
extern void stage_fetch (struct stage_reg_d *new_d_reg);
extern void stage_decode (struct stage_reg_x *new_x_reg);
extern void stage_execute (struct stage_reg_m *new_m_reg);
extern void stage_memory (struct stage_reg_w *new_w_reg);
extern void stage_writeback (void);
