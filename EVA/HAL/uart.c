#include "uart.h"


int tstc(void)
{
	return ((UTRSTAT0 & (1<<0)) == 1);
}

char getc(void)
{
	char c;
	/* 查询状态寄存器，直到有有效数据 */
	while (!(UTRSTAT0 & (1<<0)));
	
	c = URXH0; /* 读取接收寄存器的值 */
		
	return c;
}

void putc(char c)
{
    /* 查询状态寄存器，直到发送缓存为空 */
    while (!(UTRSTAT0 & (1<<2)));
    
    UTXH0 = c; /* 写入发送寄存器 */
    
    return;
}

void puts(const char *s)
{
    while (*s)
    {
        putc(*s);
        s++;
    }
}


void uart_init(void)
{
	/* 1.设置相应的GPIO用于串口功能 */
    GPA0CON = 0x22222222;
    GPA1CON = 0x222222;

    /* 2.设置UART时钟源SCLK_UART */
    /* 2.1 CLK_SRC_DMC  : bit[12]即MUX_MPLL_SEL=1, SCLKMPLLL使用MPLL的输出
     * 2.2 CLK_SRC_TOP1 : bit[12]即MUX_MPLL_USER_SEL_T=1, MUXMPLL使用SCLKMPLLL
     * 2.3 CLK_SRC_PERIL0 : bit[3:0]即UART0_SEL=6, MOUTUART0使用SCLKMPLL_USER_T
     * 所以, MOUTUART0即等于MPLL的输出, 800MHz
     */
    /*
     *   PWM_SEL = 0;
     *   UART5_SEL = 0;
     *   UART4_SEL = 6;      // 串口时钟源选 SCLKMPLL_USER_T
     *   UART3_SEL = 6;
     *   UART2_SEL = 6;
     *   UART1_SEL = 6;
     *   UART0_SEL = 6;
     */
    CLK_SRC_PERIL0 = ((0 << 24) | (0 << 20) | (6 << 16) | (6 << 12) | (6<< 8) | (6 << 4)    | (6));

    /*
     × 分频系数 = 7+1 = 8
     * 2.4 CLK_DIV_PERIL0 : bit[3:0]即UART0_RATIO=7,所以SCLK_UART0=MOUTUART0/(7+1)=100MHz
     */
    CLK_DIV_PERIL0 = ((7 << 20) | (7 << 16) | (7 << 12) | (7 << 8) | (7 << 4) | (7));

    /* 3.设置串口0相关 */
    /* 设置FIFO中断触发阈值
     * 使能FIFO
     */
    UFCON0 = 0x111;
    
    /* 设置数据格式: 8n1, 即8个数据位,没有较验位,1个停止位 */
    ULCON0 = 0x3;
    
    /* 工作于中断/查询模式
     * 另一种是DMA模式,本章不使用
     */
    UCON0  = 0x5;
    
    /* SCLK_UART0=100MHz, 波特率设置为115200
     * 寄存器的值如下计算:
     *    DIV_VAL   = 100,000,000 / (115200 * 16)   - 1 = 53.25
     *    UBRDIVn0  = 整数部分 = 53
     *    UFRACVAL0 = 小数部分 x 16 = 0.25 * 16 = 4
     */
    UBRDIV0 = 53;
    UFRACVAL0 = 4;
}

#if 0
void itoa(int n, char s[], int fmt)
{
	int i;
	int j;
	int sign;

	sign = n;	 //记录符号
	if(sign < 0)
	{
		n = -n;	//变为正数处理 
	}

	i = 0;
	do{
		s[i++] = n % fmt + '0';	//取下一个数字
	}while((n /= fmt) > 0);

	if(sign < 0 )
	{
		s[i++] = '-';
		s[i] = '\0';
	}

	for(j = i; j >= 0; j-- )
	{
		putc(s[j]);
	}
}


#define va_list char*   /* 可变参数地址 */
#define va_start(ap, x) ap=(char*)&x+sizeof(x) /* 初始化指针指向第一个可变参数 */
#define va_arg(ap, t)   (ap+=sizeof(t),*((t*)(ap-sizeof(t)))) /* 取得参数值，同时移动指针指向后续参数 */
#define va_end(ap)  ap=0 /* 结束参数处理 */

void printf(const char *fmt, ...)
{
	va_list ap;
	char *p, *sval;
	int ival;
	double dval;
	char buffer[64] = { 0 };
	
	va_start(ap, fmt);
	for (p = fmt; *p; p++) {
	if(*p != '%') {
		putc(*p);
		continue;
	}

	switch(*++p) {
		case 'x':			
		case 'd':		
			ival = va_arg(ap, int);
			itoa(ival, buffer, 10);
			char *pstr = buffer;
			while (*pstr) putc(*pstr++);
			break;
		case 'f':
			dval = va_arg(ap, double);
			printf("%f", dval);
			break;
		case 's':
			for (sval = va_arg(ap, char *); *sval; sval++)
				putc(*sval);
			break;
		default:
			putc(*p);
			break;
		}
	}
	va_end(ap);
}
#endif
