#include "system_clock.h"
#include "command.h"
#include "uart.h"

int get_clock_init_config(void)
{
	struct exynos4x12_clock *clk = 	(struct exynos4x12_clock *)EXYNOS4X12_CLOCK_BASE;
	
	printf("-----------------------src-------------------------\r\n");
	printf("src_cpu      : %x\r\n", clk->src_cpu);
	printf("src_leftbus  : %x\r\n", clk->src_leftbus);
	printf("src_rightbus : %x\r\n", clk->src_rightbus);
	printf("src_top0     : %x\r\n", clk->src_top0);
	printf("src_top1     : %x\r\n", clk->src_top1);
	printf("src_peril0   : %x\r\n", clk->src_peril0);
	printf("src_peril1   : %x\r\n", clk->src_peril1);
	printf("-----------------------div-------------------------\r\n");
	printf("div_cpu0     : %x\r\n", clk->div_cpu0);
	printf("div_cpu1     : %x\r\n", clk->div_cpu1);
	printf("div_top      : %x\r\n", clk->div_top);
	printf("div_peril0   : %x\r\n", clk->div_peril0);
	printf("div_peril1   : %x\r\n", clk->div_peril1);
	printf("div_peril2   : %x\r\n", clk->div_peril2);
	printf("div_peril3   : %x\r\n", clk->div_peril3);
	printf("div_peril4   : %x\r\n", clk->div_peril4);
	printf("div_peril5   : %x\r\n", clk->div_peril5);	
	printf("div_dmc0     : %x\r\n", clk->div_dmc0);
	printf("div_dmc1     : %x\r\n", clk->div_dmc1);
	printf("------------------------ctrl------------------------\r\n");
	printf("apll_con0    : %x\r\n", clk->apll_con0);
	printf("apll_con1    : %x\r\n", clk->apll_con1);
	printf("mpll_con0    : %x\r\n", clk->mpll_con0);
	printf("mpll_con1    : %x\r\n", clk->mpll_con1);

	return 0;
}

REGISTER_CMD(
        clock,
        1,
        ARG_TYPE_NONE,
        get_clock_init_config,
        "print clock information"
);


