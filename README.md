Author: Denali Webber and Annika Rodriguez
Computer Architecture

Program: ARM Emulator
A C program that can execute ARM machine code by emulating the register state of an ARM CPU and emulating the execution of ARM instructions.
This emulation included:

- A representation of the register state (r0-r15, CPSR)
- A representation of memory (stack)
- Given a function pointer and zero or more arguments, the ability to emulate the execution of the function
- The ability retrieve the return value from the emulated function
- Dynamic analysis of the function execution:
		- Number of instructions executed
		- Instruction counts and percentages
		- Computation (data processing)
		- Memory
		- Branches
		- Number of branches taken
		- Number of branches not taken

How to compile: your processor must support ARMv7, using the Makefile type 'make test' to run and display the code