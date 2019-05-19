#include "tiny4412.h"
#include "system_clock.h"
#include "uart.h"
#include "command.h"


int main(void)
{
	int i = 0;

	system_clock_init();

	uart_init();
	command_init();
	

	puts("\r\nstart...\r\n");
	
	while (1)
	{
		command_loop();
	}

	return 0;
}

