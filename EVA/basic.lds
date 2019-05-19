SECTIONS
{
	. = 0x02023400;
	
	.text : {
		src/head.o
		* (.text)
	}
    		
	.data : {
		* (.data)
	}

	__cli_cmd_start = .;
	.cli_cmd : { 
		*(.cli_cmd) 
	}
	__cli_cmd_end = .;	
	
	bss_start = .; 
	.bss : {
		* (.bss)
	}
	
	bss_end  = .;	
}
