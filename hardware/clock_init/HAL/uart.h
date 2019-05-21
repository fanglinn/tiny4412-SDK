#ifndef _UART_H
#define _UART_H


// GPIO
#define GPA0CON             (*(volatile unsigned int *)0x11400000)
#define GPA1CON             (*(volatile unsigned int *)0x11400020)
// system clock
#define CLK_SRC_PERIL0      (*(volatile unsigned int *)0x1003C250)
#define CLK_DIV_PERIL0      (*(volatile unsigned int *)0x1003C550)

#define CLK_SRC_TOP0            (*(volatile unsigned int *)0x1003C210)
#define CLK_SRC_TOP1            (*(volatile unsigned int *)0x1003C214)

// UART
#define UFCON0              (*(volatile unsigned int *)0x13800008)
#define ULCON0              (*(volatile unsigned int *)0x13800000)
#define UCON0               (*(volatile unsigned int *)0x13800004)
#define UBRDIV0             (*(volatile unsigned int *)0x13800028)
#define UFRACVAL0           (*(volatile unsigned int *)0x1380002c)
#define UTXH0               (*(volatile unsigned int *)0x13800020)
#define URXH0				(*(volatile unsigned int *)0x13800024)

#define UTRSTAT0            (*(volatile unsigned int *)0x13800010)





int tstc(void);
char getc(void);


void putc(char c);
void puts(const char *s);

int printf(const char * format, ...);

void uart_init(void);



#endif /* _UART_H */
