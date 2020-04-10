/*
 * External routines for use in CMPE 110, Fall 2018
 *
 * (c) 2018 Ethan L. Miller
 * Redistribution not permitted without explicit permission from the copyright holder.
 *
 */

/*
 * IMPORTANT: rename this file to riscv_pipeline_registers.h
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

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
    uint64_t    pc;
    uint32_t    instruction;
    uint32_t    stalled;
	bool        stall;
	uint64_t    new_pc;
	struct      stage_reg_d  *ptr;
	bool        branch_prediction;
	bool        fixed;
	bool        i_cache_stall;
	bool        first_cache_stall;
};

struct stage_reg_x {
    uint64_t    pc;
    uint32_t    instruction;
    uint8_t     flags;
	bool        stall;
	uint64_t    new_pc;
	uint64_t    e[11];
	int         funct;
	struct      stage_reg_x  *ptr;
	bool        branch_prediction;
	bool        fixed;
	uint64_t    rs1;
	uint64_t    rs2;
	bool        i_cache_stall;
};

struct stage_reg_m {
    uint64_t    pc;
    uint32_t    instruction;
	bool        stall;
	bool        memoryRead;
	bool        memoryWrite;
	bool        writeRun;
	bool        forwardTag;
	bool        branch;
	uint64_t    forwardingValue;
	int         funct;
	uint64_t    e[11];
	uint64_t    destinationAddress;
	uint64_t    destinationRegister;
	int         sizeOfByte;
	uint64_t    unsigned_passValue;
	struct      stage_reg_m  *ptr;
	//int64_t     signed_passValue;
	bool        branch_prediction;
	bool        wrong_prediction;
	bool        i_cache_stall;
};

struct stage_reg_w {
    uint32_t    instruction;
    uint64_t    result;
	bool        stall;
	bool        run;
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
