#include "command.h"
#include "uart.h"

char console_buffer[CONFIG_SYS_CBSIZE + 1];	/* console I/O buffer	*/
static const char erase_seq[] = "\b \b";		/* erase sequence	*/
static const char   tab_seq[] = "        ";		/* used to expand TABs	*/


static cmd_tbl_t  * CLI_cmd_start;
static cmd_tbl_t  * CLI_cmd_end ;


#define VER_MAJ		0
#define VER_MIN		1
static int do_version ()
{
	printf("version : %d.%d \r\n", VER_MAJ,VER_MIN);
	return 0;
}

REGISTER_CMD(
	ver,
	1,
	ARG_TYPE_NONE,
	do_version,
	"print monitor version"
);


static int do_readreg (unsigned int reg)
{
	unsigned int value = 0;
	value = *((volatile unsigned int *)reg);

	printf("addr :%x   value : %d \r\n", reg , value);
    return 0;
}

REGISTER_CMD(
        rd,
        2,
        ARG_TYPE_U32,
        do_readreg,
        "read reg value"
);

int do_help()
{
	cmd_tbl_t *cmdtp;
	int len = CLI_cmd_end - CLI_cmd_start;

	for (cmdtp = CLI_cmd_start; cmdtp != CLI_cmd_start + len; cmdtp++)
	{
		printf("%s\t%s \r\n",cmdtp->name, cmdtp->usage);
	}
	
	return 0;
}

REGISTER_CMD(
	help,
	1,
	ARG_TYPE_NONE,
	do_help,
	"print help information"
);


char * strcpy(char * dest,const char *src)
{
	char *tmp = dest;

	while ((*dest++ = *src++) != '\0')
		/* nothing */;
	return tmp;
}

unsigned int strlen(const char * s)
{
	const char *sc;

	for (sc = s; *sc != '\0'; ++sc)
		/* nothing */;
	return sc - s;
}

int strcmp(const char * cs,const char * ct)
{
	register signed char __res;

	while (1) {
		if ((__res = *cs - *ct++) != 0 || !*cs++)
			break;
	}

	return __res;
}




static char * delete_char (char *buffer, char *p, int *colp, int *np, int plen)
{
	char *s;

	if (*np == 0) {
		return (p);
	}

	if (*(--p) == '\t') {			/* will retype the whole line	*/
		while (*colp > plen) {
			puts (erase_seq);
			(*colp)--;
		}
		for (s=buffer; s<p; ++s) {
			if (*s == '\t') {
				puts (tab_seq+((*colp) & 07));
				*colp += 8 - ((*colp) & 07);
			} else {
				++(*colp);
				putc (*s);
			}
		}
	} else {
		puts (erase_seq);
		(*colp)--;
	}
	(*np)--;
	return (p);
}


int readline_into_buffer(const char *const prompt, char *buffer, int timeout)
{
	char *p = buffer;
	char * p_buf = p;
	int	n = 0;				/* buffer index		*/
	int	plen = 0;			/* prompt length	*/
	int	col;				/* output column cnt	*/
	char	c;

	/* print prompt */
	if (prompt) {
		plen = strlen (prompt);
		puts (prompt);
	}
	col = plen;

	for (;;)
	{
		c = getc();
		/*
		 * Special character handling
		 */
		switch (c){
		case '\r':			/* Enter		*/
		case '\n':
			*p = '\0';
			puts ("\r\n");
			return p - p_buf;

		case '\0':			/* nul			*/
			continue;

		case 0x03:			/* ^C - break		*/
			p_buf[0] = '\0';	/* discard input */
			return -1;

		case 0x15:			/* ^U - erase line	*/
			while (col > plen) {
				puts (erase_seq);
				--col;
			}
			p = p_buf;
			n = 0;
			continue;

		case 0x17:			/* ^W - erase word	*/
			p=delete_char(p_buf, p, &col, &n, plen);
			while ((n > 0) && (*p != ' ')) {
				p=delete_char(p_buf, p, &col, &n, plen);
			}
			continue;

		case 0x08:			/* ^H  - backspace	*/
		case 0x7F:			/* DEL - backspace	*/
			p=delete_char(p_buf, p, &col, &n, plen);
			continue;

		default:
			/*
			 * Must be a normal character then
			 */
			if (n < CONFIG_SYS_CBSIZE-2) {
				if (c == '\t') {	/* expand TABs */
					puts (tab_seq+(col&07));
					col += 8 - (col&07);
				} else {
					char buf[2];

					/*
					 * Echo input using puts() to force an
					 * LCD flush if we are using an LCD
					 */
					++col;
					buf[0] = c;
					buf[1] = '\0';
					puts(buf);
				}
				*p++ = c;
				++n;
			} else {			/* Buffer full		*/
				putc ('\a');
			}
		}
	}
}

int readline (const char *const prompt)
{
	/*
	 * If console_buffer isn't 0-length the user will be prompted to modify
	 * it instead of entering it from scratch as desired.
	 */
	console_buffer[0] = '\0';

	return readline_into_buffer(prompt, console_buffer, 0);
}

int parse_line (char *line, char *argv[])
{
	int nargs = 0;

	while (nargs < CONFIG_SYS_MAXARGS) {

		/* skip any white space */
		while (isblank(*line))
			++line;

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
			return nargs;
		}

		argv[nargs++] = line;	/* begin of argument string	*/

		/* find end of string */
		while (*line && !isblank(*line))
			++line;

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
			return nargs;
		}

		*line++ = '\0';		/* terminate current arg	 */
	}

	puts ("** Too many args (max) **\n");

	return (nargs);
}

static int find_cmd (const char *name, cmd_tbl_t **cmd)
{
	int found = 0;

	cmd_tbl_t *cmdtp;
	int len = CLI_cmd_end - CLI_cmd_start;

	for (cmdtp = CLI_cmd_start; cmdtp != CLI_cmd_start + len; cmdtp++)
	{
		if(0 == strcmp(name, cmdtp->name))
		{
			*cmd = cmdtp;
			found = 1;
			break;
		}
	}

	return found;
}

int cmd_usage(const cmd_tbl_t *cmdtp)
{
	printf("%s - %s\n\n", cmdtp->name, cmdtp->usage);
	return 1;
}

unsigned int strtoul (char *str, char **ptr, int base)
{
   unsigned long rvalue = 0;
   int neg = 0;
   int c;

   /* Validate parameters */
   if ((str != NULL) && (base >= 0) && (base <= 36))
   {
      /* Skip leading white spaces */
      while (isblank(*str))
      {
         ++str;
	}

	/* Check for notations */
       switch (str[0])
	{
		case '0':
          if (base == 0)
          {
             if ((str[1] == 'x') || (str[1] == 'X'))
				{
					base = 16;
                str += 2;
             }
             else
             {
                base = 8;
                str++;
				}
			}
			break;
    
		case '-':
			neg = 1;
          str++;
          break;

       case '+':
          str++;
			break;

		default:
			break;
	}

	if (base == 0)
		base = 10;

      /* Valid "digits" are 0..9, A..Z, a..z */
      while (isalnum(c = *str))
      {
		/* Convert char to num in 0..36 */
         if ((c -= ('a' - 10)) < 10)         /* 'a'..'z' */
         {
            if ((c += ('a' - 'A')) < 10)     /* 'A'..'Z' */
            {
               c += ('A' - '0' - 10);        /* '0'..'9' */
			}
		}

		/* check c against base */
		if (c >= base)
		{
			break;
		}

		if (neg)
		{
			rvalue = (rvalue * base) - c;
		}
		else
		{
			rvalue = (rvalue * base) + c;
         }

         ++str;
		}
	}

   /* Upon exit, 'str' points to the character at which valid info */
   /* STOPS.  No chars including and beyond 'str' are used.        */

	if (ptr != NULL)
			*ptr = str;
		
		return rvalue;
	}


int atoi (const char *str)
{
   char *s = (char *)str;
   
   return ((int)strtoul(s, NULL, 10));
}


static signed int _TOS32(const char *exp)
{
	return atoi(exp);
}

static unsigned int hexDigital2int(unsigned char ch)
{
	unsigned char val;
	if((ch >= 'A') && (ch <= 'F'))
		val = ch - 'A' + 10;
	else if((ch >= 'a') && (ch <= 'f'))
		val = ch - 'a' + 10;
	else if((ch >= '0') && (ch <= '9'))
		val = ch - '0';
	else
		val = 16;

	return val;
}

static unsigned int hex2int(const char *exp)
{
	unsigned char ch;
	unsigned int result = 0;
	unsigned int digital;

	while('\0' != (ch = *exp++))
	{
		digital = hexDigital2int(ch);

		if(digital != 16)  // fixme
		{
			result = result * 16 + digital;
		}
		else
		{
			break;
		}
	}

	return result;
}


static unsigned int _TOU32(const char *exp)
{
	char first = *exp;
	char second = *exp + 1;
	int result;

	if(('0' == first) && ('1' == second)) //??? while not 'x'/'X'
	{
		result = hex2int(exp + 2);  // skip prefix
	}
	else if('-' == first)  // s32
	{
		return atoi(exp);
	}
	else
	{
		result = (int)atoi(exp);
	}

	return result;
}

static unsigned int _TOBool(const char *exp)
{
	if(('t' == *exp) || ('T' == *exp))
		return 1;
	else
		return 0;
}



static void *conventer(char *exp, unsigned char type)
{
	switch(type)
	{
		case ARG_TYPE_S32:
			return (void *)_TOS32(exp);

		case ARG_TYPE_U32:
			return (void *)_TOU32(exp);

		case ARG_TYPE_BOOL:
			return (void *)_TOBool(exp);

		case ARG_TYPE_STR:
			return exp;

		default:
			break;
	}
    return 0;
}


static int cmd_call(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int result;

	switch(argc - 1)
	{
		case 0:
			result = (cmdtp->cmd)();
			break;

		case 1:
			result = (cmdtp->cmd)(conventer(argv[1], cmdtp->type));
			break;

		case 2:
			result = (cmdtp->cmd)(conventer(argv[1], cmdtp->type), 
								  conventer(argv[2], cmdtp->type));
			break;

		case 3:
			result = (cmdtp->cmd)(conventer(argv[1], cmdtp->type), 
								  conventer(argv[2], cmdtp->type), 
								  conventer(argv[3], cmdtp->type));
			break;

		default:
			break;
	}

	if (result)
		printf("Command failed, result : %d \r\n", result);
	return result;
}


enum command_ret_t cmd_process(int flag, int argc, char * const argv[],
			       int *repeatable, unsigned long *ticks)
{
	enum command_ret_t rc = CMD_RET_SUCCESS;
	cmd_tbl_t *cmdtp;
	int found = 0;

	/* Look up command in command table */
	found = find_cmd(argv[0], &cmdtp);
	if (found == 0) {
		//puts("Unknown command  '");
		//puts(argv[0]);
		//puts("' - try 'help'\r\n");
		printf("Unknown command  '%s' - try 'help'\r\n");
		return 1;
	}

	/* found - check max args */
	if (argc > cmdtp->maxargs)
		rc = CMD_RET_USAGE;


	/* If OK so far, then do the command */
	if (!rc) {
		rc = cmd_call(cmdtp, flag, argc, argv);
	}
	if (rc == CMD_RET_USAGE)
		rc = cmd_usage(cmdtp);
	return rc;
}

static int builtin_run_command(const char *cmd, int flag) 
{
	char cmdbuf[CONFIG_SYS_CBSIZE];	/* working copy of cmd		*/
	char *token;			/* start of token in cmdbuf	*/
	char *sep;			/* end of token (separator) in cmdbuf */

	char *str = cmdbuf;
	char *argv[CONFIG_SYS_MAXARGS + 1];	/* NULL terminated	*/
	int argc, inquotes;
	int repeatable = 1;
	int rc = 0;
	//int i = 0;

	
	if (!cmd || !*cmd) {
		return -1;	/* empty command */
	}
	
	if (strlen(cmd) >= CONFIG_SYS_CBSIZE) {
		puts ("## Command too long!\r\n");
		return -1;
	}
	
	strcpy (cmdbuf, cmd);

	while (*str) {
		/*
		 * Find separator, or string end
		 * Allow simple escape of ';' by writing "\;"
		 */
		for (inquotes = 0, sep = str; *sep; sep++) {
			if ((*sep=='\'') &&
			    (*(sep-1) != '\\'))
				inquotes=!inquotes;

			if (!inquotes &&
			    (*sep == ';') &&	/* separator		*/
			    ( sep != str) &&	/* past string start	*/
			    (*(sep-1) != '\\'))	/* and NOT escaped	*/
				break;
		}

		/*
		 * Limit the token to data between separators
		 */
		token = str;
		if (*sep) {
			str = sep + 1;	/* start of command for next pass */
			*sep = '\0';
		}
		else
			str = sep;	/* no more commands for next pass */


		/* Extract arguments */
		if ((argc = parse_line (token, argv)) == 0) {
			rc = -1;	/* no command at all */
			continue;
		}
		
		//printf("argc : %d \r\n", argc);
		//for(i = 0; i < argc;i++)
		//	printf("argv : %s\r\n", argv[i]);

		if (cmd_process(flag, argc, argv, &repeatable, NULL))
			rc = -1;

	}

	return rc ? rc : repeatable;
}


/*
 * Run a command using the selected parser.
 *
 * @param cmd	Command to run
 * @param flag	Execution flags (CMD_FLAG_...)
 * @return 0 on success, or != 0 on error.
 */
int run_command(const char *cmd, int flag)
{
	if (builtin_run_command(cmd, flag) == -1)
		return 1;

	return 0;
}


void command_loop(void)
{
	static char lastcommand[CONFIG_SYS_CBSIZE] = { 0, };
	int len;
	int rc = 1;
	int flag;

	/*
	 * Main Loop for Monitor Command Processing
	 */
	for (;;) {
		len = readline (CONFIG_SYS_PROMPT);

		flag = 0;	/* assume no special flags for now */
		if (len > 0)
			strcpy (lastcommand, console_buffer);

		if (len == -1)
			puts ("<INTERRUPT>\n");
		else
			rc = run_command(lastcommand, flag);

		if (rc <= 0) {
			/* invalid command or not repeatable, forget it */
			lastcommand[0] = 0;
		}
	}
}

void command_init(void)
{
	extern cmd_tbl_t __cli_cmd_start,__cli_cmd_end;

	CLI_cmd_start = &__cli_cmd_start;
	CLI_cmd_end = &__cli_cmd_end;
}
