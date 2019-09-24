#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <linux/fb.h>
#include <asm/types.h>

#define         VIDCON0                 0x00
#define         VIDCON1                 0x04
#define         VIDTCON0                0x10
#define         VIDTCON1                0x14
#define         VIDTCON2                0x18
#define         WINCON0                 0x20
#define         VIDOSD0C                0x48
#define         SHADOWCON               0x34
#define         WINCHMAP2               0x3c
#define         VIDOSD0A                0x40
#define         VIDOSD0B                0x44
#define         VIDW00ADD0B0            0xA0
#define         VIDW00ADD1B0            0xD0
#define         CLK_SRC_LCD0            0x234
#define         CLK_SRC_MASK_LCD        0x334
#define         CLK_DIV_LCD             0x534
#define         CLK_GATE_IP_LCD         0x934
#define         LCDBLK_CFG              0x00
#define         LCDBLK_CFG2             0x04
#define         LCD_LENTH               1024
#define         LCD_WIDTH               600
#define         BITS_PER_PIXEL          32

static struct fb_info *tiny4412_lcd;
static volatile void __iomem *lcd_regs_base;
static volatile void __iomem *lcdblk_regs_base;
static volatile void __iomem *lcd0_configuration;//Configures power mode of LCD0.0x10020000+0x3C80
static volatile void __iomem *clk_regs_base;


static u32 pseudo_palette[16];
static struct resource  *res0, *res1, *res2, *res3;

/* from pxafb.c */
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
    chan &= 0xFFFF;
    chan >>= 16 - bf->length;
    return chan << bf->offset;
}
static int cfb_setcolreg(unsigned int regno, unsigned int red,
                               unsigned int green, unsigned int blue,
                               unsigned int transp, struct fb_info *info)
{

    unsigned int color = 0;
    uint32_t *p;
    color  = chan_to_field(red,   &info->var.red);
    color |= chan_to_field(green, &info->var.green);
    color |= chan_to_field(blue,  &info->var.blue);
    
    p = info->pseudo_palette;  
    p[regno] = color;
    return 0;
}

static struct fb_ops tiny4412_lcdfb_ops =
{
    .owner              = THIS_MODULE,
    .fb_setcolreg       = cfb_setcolreg, 
    .fb_fillrect        = cfb_fillrect,  
    .fb_copyarea        = cfb_copyarea,  
    .fb_imageblit       = cfb_imageblit, 
};
                            
static int tiny4412_lcd_probe(struct platform_device *pdev)
{
    int ret;
    unsigned int temp;
    
    /* alloc fb_info */
    tiny4412_lcd = framebuffer_alloc(0, NULL);           
    if(!tiny4412_lcd) {
        return  -ENOMEM;
    }
   

    /* set fix args */
    strcpy(tiny4412_lcd->fix.id, "x710");                              //
    tiny4412_lcd->fix.smem_len = LCD_LENTH*LCD_WIDTH*BITS_PER_PIXEL/8; //
    tiny4412_lcd->fix.type     = FB_TYPE_PACKED_PIXELS;                //
    tiny4412_lcd->fix.visual   = FB_VISUAL_TRUECOLOR;                  //
    tiny4412_lcd->fix.line_length = LCD_LENTH*BITS_PER_PIXEL/8;        //

    /* set var args */
    tiny4412_lcd->var.xres           = LCD_LENTH;                      //
    tiny4412_lcd->var.yres           = LCD_WIDTH;                      //
    tiny4412_lcd->var.xres_virtual   = LCD_LENTH;                      //
    tiny4412_lcd->var.yres_virtual   = LCD_WIDTH;                      //
    tiny4412_lcd->var.xoffset        = 0;                              //
    tiny4412_lcd->var.yoffset        = 0;                              //
    tiny4412_lcd->var.bits_per_pixel = BITS_PER_PIXEL;                 //
    
    /* RGB:888 */
    tiny4412_lcd->var.red.length     = 8;
    tiny4412_lcd->var.red.offset     = 16;   //
    tiny4412_lcd->var.green.length   = 8;
    tiny4412_lcd->var.green.offset   = 8;    //
    tiny4412_lcd->var.blue.length    = 8;
    tiny4412_lcd->var.blue.offset    = 0;    //
    tiny4412_lcd->var.activate       = FB_ACTIVATE_NOW;      //


    tiny4412_lcd->fbops              = &tiny4412_lcdfb_ops;  //


    tiny4412_lcd->pseudo_palette     = pseudo_palette;       //
    tiny4412_lcd->screen_size        = LCD_LENTH * LCD_WIDTH * BITS_PER_PIXEL / 8;   //

	/* get resource */
    res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (res0 == NULL)
    {
        printk("platform_get_resource error.\n");
        return -EINVAL;
    }
    lcd_regs_base = devm_ioremap_resource(&pdev->dev, res0);
    if (lcd_regs_base == NULL)
    {
        printk("devm_ioremap_resource error.\n");
        return -EINVAL;
    }
    
    res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (res1 == NULL)
    {
        printk("platform_get_resource error.\n");
        return -EINVAL;
    }
    lcdblk_regs_base = devm_ioremap_resource(&pdev->dev, res1);
    if (lcdblk_regs_base == NULL)
    {
        printk("devm_ioremap_resource error.\n");
        return -EINVAL;
    }
    
    res2 = platform_get_resource(pdev, IORESOURCE_MEM, 2);
    if (res2 == NULL)
    {
        printk("platform_get_resource error.\n");
        return -EINVAL;
    }

	
    //bug:
    //lcd0_configuration = devm_ioremap_resource(&pdev->dev, res2);  
    lcd0_configuration = devm_ioremap(&pdev->dev, res2->start, resource_size(res2));  
    if (lcd0_configuration == NULL)
    {
        printk("devm_ioremap_resource error.\n");
        return -EINVAL;
    }
    *(unsigned long *)lcd0_configuration = 7; //Reset Value = 0x00000007
        
    res3 = platform_get_resource(pdev, IORESOURCE_MEM, 3);
    if (res3 == NULL)
    {
        printk("platform_get_resource error.\n");
        return -EINVAL;
    }
    //clk_regs_base = devm_ioremap_resource(&pdev->dev, res3);
	clk_regs_base = devm_ioremap(&pdev->dev, res3->start, resource_size(res3));  
    if (clk_regs_base == NULL)
    {
        printk("devm_ioremap_resource error.\n");
        return -EINVAL;
    }


    //Selects clock source for LCD_BLK
    //FIMD0_SEL:bit[3:0]=0110=SCLKMPLL_USER_T=800M
    temp = readl(clk_regs_base + CLK_SRC_LCD0);
    temp &= ~(0x0F<<0);
    temp |= (0x3<<1);      /* b'0110 */
    writel(temp, clk_regs_base + CLK_SRC_LCD0);

    //Clock source mask for LCD_BLK    
    //FIMD0_MASK:Mask output clock of MUXFIMD0 (1=Unmask)
    temp = readl(clk_regs_base + CLK_SRC_MASK_LCD);
    temp |= (0x01<<0);
    writel(temp, clk_regs_base + CLK_SRC_MASK_LCD);

    //Clock source mask for LCD_BLK    
    //SCLK_FIMD0 = MOUTFIMD0/(FIMD0_RATIO + 1),no div
    temp = readl(clk_regs_base + CLK_DIV_LCD);
    temp &= ~(0x0F<<0);
    writel(temp, clk_regs_base + CLK_DIV_LCD);

    //Controls IP clock gating for LCD_BLK   
    //CLK_FIMD0:Gating all clocks for FIMD0 (1=Pass)
    temp = readl(clk_regs_base + CLK_GATE_IP_LCD);
    temp |= (0x01<<0);
    writel(temp, clk_regs_base + CLK_GATE_IP_LCD);

	/*
	* chapter 12.2.1.5 : LCDBLK_CFG
	*/
    //FIMDBYPASS_LBLK0:FIMD of LBLK0 Bypass Selection (1=FIMD Bypass)
    temp = readl(lcdblk_regs_base + LCDBLK_CFG);
    temp |= (0x01<<1);                   /* FIMD Bypass */
    writel(temp, lcdblk_regs_base + LCDBLK_CFG);

    //MIE0_DISPON:MIE0_DISPON: PWM output control (1=PWM outpupt enable)
    temp = readl(lcdblk_regs_base + LCDBLK_CFG2);
    temp |= (0x01<<0);                             /*  PWM outpupt enable */
    writel(temp, lcdblk_regs_base + LCDBLK_CFG2);
    
    mdelay(1000);
	

	/*
	* The CLKVAL field in VIDCON0 register controls the rate of RGB_VCLK signal.
    * RGB_VCLK (Hz) = SCLK_FIMDx/(CLKVAL + 1), where CLKVAL ? 1 where, SCLK_FIMDx (x = 0, 1)
    *
    * Frame Rate = 1/[{(VSPW + 1) + (VBPD + 1) + (LIINEVAL + 1) + (VFPD + 1)} ? {(HSPW + 1) + 
    * (HBPD + 1) + (HFPD + 1) +(HOZVAL + 1)} ? {(CLKVAL + 1)/(Frequency of Clock source)}]
	*/
    //800/(15+1) == 50M
    temp = readl(lcd_regs_base + VIDCON0);
    temp |= (15<<6);
    writel(temp, lcd_regs_base + VIDCON0);

    /*
     * VIDTCON1:
     * [5]:IVSYNC  ===> 1 : Inverted(??)
     * [6]:IHSYNC  ===> 1 : Inverted(??)
     * [7]:IVCLK   ===> 1 : Fetches video data at VCLK rising edge (?????)
     * [10:9]:FIXVCLK  ====> 01 : VCLK running
     */
    temp = readl(lcd_regs_base + VIDCON1);
    temp |= (1 << 9) | (1 << 7) | (1 << 5) | (1 << 6);
    writel(temp, lcd_regs_base + VIDCON1);
    
    /*
     * VIDTCON0:
     * [23:16]:  VBPD+1=tvb-tvpw=23-11=12 --> VBPD=11
     * [15:8] :  VFPD+1=tvfp=22 --> VFPD=21
     * [7:0]  :  VSPW+1=tvpw=1~20(??11) --> VSPW=10
     */
    temp = readl(lcd_regs_base + VIDTCON0);
    temp |= (11 << 16) | (21 << 8) | (10 << 0);
    writel(temp, lcd_regs_base + VIDTCON0);

    /*
     * VIDTCON1:
     * [23:16]:  HBPD+1=thb-hpw=46-21=25 --> HBPD=24
     * [15:8] :  HFPD+1=thfp=210 --> HFPD=209
     * [7:0]  :  HSPW+1=hpw=1~40 --> HSPW=20
     */
    temp = readl(lcd_regs_base + VIDTCON1);
    temp |= (24 << 16) | (209 << 8)  | (20 << 0);
    writel(temp, lcd_regs_base + VIDTCON1);

    /*
     * HOZVAL = (Horizontal display size) - 1 and LINEVAL = (Vertical display size) - 1.
     * Horizontal(??) display size : 800
     * Vertical(??) display size : 480
     */
    temp = (LCD_WIDTH << 11) | ((LCD_LENTH - 1) << 0);
    writel(temp, lcd_regs_base + VIDTCON2);

    /*
     * WINCON0:
     * [15]:Specifies Word swap control bit.  1 = Enables swap
     * [5:2]: Selects Bits Per Pixel (BPP) mode for Window image : 1101 ===> 
     *                          Unpacked 25 BPP (non-palletized A:1-R:8-G:8-B:8)
     * [0]:Enables/disables video output   1 = Enables
     */
    temp = readl(lcd_regs_base + WINCON0);
    temp &= ~(0x0F << 2);
    temp |= (0X01 << 15) | (0x0D << 2) | (0x01<<0);
    writel(temp, lcd_regs_base + WINCON0);

    //Enables Channel 0.
    temp = readl(lcd_regs_base + SHADOWCON);
    writel(temp | 0x01, lcd_regs_base + SHADOWCON);
    //Selects Channel 0
    temp = readl(lcd_regs_base + WINCHMAP2);
    temp &= ~(7 << 16);
    temp |= (0x01 << 16);//CH0FISEL:Selects Channel 0's channel.001 = Window 0
    temp &= ~(7 << 0);
    temp |= (0x01 << 0);//W0FISEL:Selects Window 0's channel.001 = Channel 0
    writel(temp, lcd_regs_base + WINCHMAP2);


    //Window Size For example. Height *  Width (number of word)
    temp = (LCD_LENTH * LCD_WIDTH) >> 1;
    writel(temp, lcd_regs_base + VIDOSD0C);
    /*
     * bit0-10 : 
     * bit11-21: 
     */
    writel(0, lcd_regs_base + VIDOSD0A);
    /*
     * bit0-10 :
     * bit11-21:
     */
    writel(((LCD_LENTH-1) << 11) | (LCD_WIDTH-1), lcd_regs_base + VIDOSD0B);
    
    //Display On: ENVID and ENVID_F are set to "1".
    temp = readl(lcd_regs_base + VIDCON0);
    writel(temp | (0x01<<1) | (0x01<<0), lcd_regs_base + VIDCON0);
    

    tiny4412_lcd->screen_base = dma_alloc_writecombine(NULL, tiny4412_lcd->fix.smem_len, 
					(dma_addr_t *)&tiny4412_lcd->fix.smem_start, GFP_KERNEL);

    writel(tiny4412_lcd->fix.smem_start, lcd_regs_base + VIDW00ADD0B0);
    writel(tiny4412_lcd->fix.smem_start + tiny4412_lcd->fix.smem_len, lcd_regs_base + VIDW00ADD1B0);

    ret = register_framebuffer(tiny4412_lcd);
    return ret;
}
static int tiny4412_lcd_remove(struct platform_device *pdev)
{
    //Direct Off: ENVID and ENVID_F are set to "0" simultaneously. 
    unsigned int temp;
    temp = readl(lcd_regs_base + VIDCON0);
    temp &= ~(0x01<<1 | 0x01<<0); 
    writel(temp, lcd_regs_base + VIDCON0);

    unregister_framebuffer(tiny4412_lcd);
    dma_free_writecombine(NULL, tiny4412_lcd->fix.smem_len, tiny4412_lcd->screen_base, tiny4412_lcd->fix.smem_start);
    framebuffer_release(tiny4412_lcd);
    return 0;
}



/*
* Device Tree
lcd@11C00000 {
    compatible = "tiny4412,lcd";
    reg = <0x11C00000  0x20c0 0x10010210 0x08 0x10023c80 0x04 0x1003c000 0x1000>;
    pinctrl-names = "default";
    pinctrl-0 = <&lcd>;
    clocks = <&clock CLK_FIMD0 &clock CLK_ACLK160>;
    clock-names = "fimd0","aclk160";
};


&pinctrl_0 {
    lcd:lcd {
        samsung,pins = "gpf0-0", "gpf0-1", "gpf0-2", "gpf0-3", "gpf0-4",
        "gpf0-5", "gpf0-6","gpf0-7", "gpf1-0", "gpf1-1",
        "gpf1-2", "gpf1-3", "gpf1-4", "gpf1-5", "gpf1-6",
        "gpf1-7", "gpf2-0", "gpf2-1", "gpf2-2", "gpf2-3", 
        "gpf2-4", "gpf2-5", "gpf2-6","gpf2-7", "gpf3-0",
        "gpf3-1", "gpf3-2", "gpf3-3";
        samsung,pin-function = <2>;
        samsung,pin-pud = <0>;
        samsung,pin-drv = <0>;
    };
};

*/

static const struct of_device_id tiny4412_lcd_dt_ids[] =
{
    { .compatible = "tiny4412,lcd", },         /* match with the dts */ 
    {},
};

MODULE_DEVICE_TABLE(of, tiny4412_lcd_dt_ids);

static struct platform_driver tiny4412_lcd_driver =
{
    .driver        = {
        .name      = "lcd_x710",
        .of_match_table    = of_match_ptr(tiny4412_lcd_dt_ids),
    },
    .probe         = tiny4412_lcd_probe,
    .remove        = tiny4412_lcd_remove,
};


static int __init tiny4412_lcd_init(void)
{
	int ret;
	ret = platform_driver_register(&tiny4412_lcd_driver);
	
	if (ret)
	{
		printk(KERN_ERR "lcd: probe fail: %d\n", ret);
	}
	
	return ret;
}


static void __exit tiny4412_lcd_exit(void)
{
	pr_info("%s called .\n", __func__); 

	platform_driver_unregister(&tiny4412_lcd_driver);
}

module_init(tiny4412_lcd_init);
module_exit(tiny4412_lcd_exit);

MODULE_LICENSE("GPL");

