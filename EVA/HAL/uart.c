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

#if 1
void putchar_hex(char c)
{
	char * hex = "0123456789ABCDEF";

	putc(hex[(c>>4) & 0x0F]);
	putc(hex[(c>>0) & 0x0F]);
}

void putint_hex(int a)
{
	putchar_hex( (a>>24) & 0xFF );
	putchar_hex( (a>>16) & 0xFF );
	putchar_hex( (a>>8) & 0xFF );
	putchar_hex( (a>>0) & 0xFF );
}

char * itoa(int a, char * buf)
{
	int num = a;
	int i = 0;
	int len = 0;

	do
	{
		buf[i++] = num % 10 + '0';
		num /= 10;
	}while (num);
	buf[i] = '\0';

	len = i;
	for (i = 0; i < len/2; i++)
	{
		char tmp;
		tmp = buf[i];
		buf[i] = buf[len-i-1];
		buf[len-i-1] = tmp;
	}
	
	return buf;
}


typedef int * va_list;
#define va_start(ap, A)		(ap = (int *)&(A) + 1)
#define va_arg(ap, T)		(*(T *)ap++)
#define va_end(ap)		((void)0)

int printf(const char * format, ...)
{
	char c;	
	va_list ap;
		
	va_start(ap, format);
	
	while ((c = *format++) != '\0')
	{
		switch (c)
		{
			case '%':
			c = *format++;
				
			switch (c)
			{
				char ch;
				char * p;
				int a;
				char buf[100];
									
				case 'c':
					ch = va_arg(ap, int);
					putc(ch);
					break;
				case 's':
					p = va_arg(ap, char *);
					puts(p);
					break;					
				case 'x':
					a = va_arg(ap, int);
					putint_hex(a);
						break;		
				case 'd':
					a = va_arg(ap, int);
					itoa(a, buf);
					puts(buf);
					break;	
					
				default:
					break;
			}				
			break;		
	
		default:
			putc(c);
			break;
		}
	}
	
	return 0;	
}
#endif
