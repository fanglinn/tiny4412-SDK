#include "tiny4412.h"
#include "system_clock.h"
#include "uart.h"
#include "command.h"


int main(void)
{
	/*
	* use the clock ,which is load from iROM,APLL=MPLL=400MHz
	*/
	uart_init();
	command_init();
	

	puts("\r\nstart...\r\n");
	printf("hello, world!\r\n");
	
	while (1)
	{
		command_loop();
	}

	return 0;
}

