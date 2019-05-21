#ifndef _COMMAND_H
#define _COMMAND_H

#define ARG_TYPE_NONE		9       /* idle */
#define ARG_TYPE_S32		0
#define ARG_TYPE_U32		1
#define ARG_TYPE_BOOL		2
#define ARG_TYPE_STR		3


#define CONFIG_SYS_PROMPT		"flinn@tiny4412 # "
#define CONFIG_SYS_CBSIZE		256	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE		384	/* Print Buffer Size */
#define CONFIG_SYS_MAXARGS		16	/* max number of command args */


enum command_ret_t {
	CMD_RET_SUCCESS,	/* 0 = Success */
	CMD_RET_FAILURE,	/* 1 = Failure */
	CMD_RET_USAGE = -1,	/* Failure, please report 'usage' error */
};

struct cmd_tbl_s {
	char		*name;		/* Command Name			*/
	int		maxargs;	/* maximum number of arguments	*/
	int		type;
	int		(*cmd)();
	char		*usage;		/* Usage message	(short)	*/
};

typedef struct cmd_tbl_s	cmd_tbl_t;

#define REGISTER_CMD(name,maxargs,type,handler,usage) \
	const cmd_tbl_t cmd_##name __attribute__ ((section (".cli_cmd"))) =  {#name, maxargs, type, handler, usage}


#ifndef NULL
#define NULL		(void *)0
#endif

#define isblank(c)	(c == ' ' || c == '\t')
#define isalnum(ch)	(((ch >= '0') && (ch <= '9')) || \
					 ((ch >= 'A') && (ch <= 'Z')) || \
					 ((ch >= 'a') && (ch <= 'z')))




void command_loop(void);
void command_init(void);


#endif /* _COMMAND_H */
