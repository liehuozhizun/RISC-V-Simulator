all: riscvsim.out 
	
riscv_sim_framework.o: riscv_sim_framework.c riscv_sim_framework.h
	gcc -c -Wall -DHAS_READLINE -c riscv_sim_framework.c
 
riscv_execute_one.o: execute_single_instruction.c riscv_sim_framework.h
	gcc -c -Wall -DHAS_READLINE -c execute_single_instruction.c

riscvsim.out: riscv_sim_framework.o execute_single_instruction.o
	gcc -o riscvsim riscv_sim_framework.o execute_single_instruction.o -lreadline

