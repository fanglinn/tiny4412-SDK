.globl _start

_start:

	/*
	* iRAM base : 0x02020000 
	* iROM      : 5*1024            = 0x1400, 2021400
	* BL1       : 8*1024            = 0x2000, 2023400
	* BL2       : 16*1024           = 0x4000, 2027400
	* @stack    : 1024              = 0x400,  2027800
	*/
	ldr sp, =0x02027800     // set stack

	bl main

halt_loop:
	b halt_loop