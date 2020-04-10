/*
 * Your code may not modify anything pointed to by these pointers.  However, any
 * stage may use the contents of these strucures.
 */

#pragma once
#include "riscv_pipeline_registers.h"

extern const struct stage_reg_d *   current_stage_d_register;
extern const struct stage_reg_x *   current_stage_x_register;
extern const struct stage_reg_m *   current_stage_m_register;
extern const struct stage_reg_w *   current_stage_w_register;
