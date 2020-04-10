all: riscvsim.out 
	
riscv_sim_framework.o: riscv_sim_framework.c riscv_sim_framework.h riscv_pipeline_registers.h
	gcc -c -Wall -DHAS_READLINE -c riscv_sim_framework.c
 
execute_one.o: execute_one.c riscv_sim_framework.h riscv_pipeline_registers.h riscv_pipeline_registers_vars.h
	gcc -c -Wall -DHAS_READLINE -c execute_one.c

riscvsim.out: riscv_sim_framework.o execute_one.o
	gcc -o riscvsim riscv_sim_framework.o execute_one.o -lreadline
