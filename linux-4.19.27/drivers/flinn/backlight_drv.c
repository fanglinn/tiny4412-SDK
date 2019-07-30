/*
 * backlight-dt.c driver for tiny4412 on linux-4.19.27
*/
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include <linux/platform_device.h>
#include <linux/slab.h>       //
#include <linux/io.h>

#include <linux/gfp.h>
#include <linux/cdev.h>

#include <linux/uaccess.h> /* copy_from_usr, */
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/wait.h>

/*
* @ device tree

backlight {
                compatible = "tiny4412,backlight";
                reg = <0x139D0000 0x1000>;
                pinctrl-names = "backlight_in", "backlight_out";
                pinctrl-0 = <&backlight_in>;
                pinctrl-1 = <&backlight_out>;
                gpios = <&gpx1 2 GPIO_ACTIVE_HIGH>;
                clock-names = "timers";
                clocks = <&clock CLK_PWM>;
                interrupts = <GIC_SPI 40 IRQ_TYPE_LEVEL_HIGH>;
                pwms = <&pwm 0 1000000000 0>;
        };

&pinctrl_1 {
	backlight_out: backlight_out {
			samsung,pins = "gpx1-2";
			samsung,pin-function = <EXYNOS_PIN_FUNC_OUTPUT>;
			samsung,pin-pud = <EXYNOS_PIN_PULL_NONE>;
			samsung,pin-drv = <EXYNOS4_PIN_DRV_LV1>;
	};

	backlight_in: backlight_in {
			samsung,pins = "gpx1-2";
			samsung,pin-function = <EXYNOS_PIN_FUNC_INPUT>;
			samsung,pin-pud = <EXYNOS_PIN_PULL_NONE>;
			samsung,pin-drv = <EXYNOS4_PIN_DRV_LV1>;
	};
	
}
*/


enum {
    IDLE,
    START,
    REQUEST,
    WAITING,
    RESPONSE,
    STOPING,
} one_wire_status = IDLE;


static volatile unsigned int io_bit_count;
static volatile unsigned int io_data;
static volatile unsigned char one_wire_request;
static DECLARE_WAIT_QUEUE_HEAD(bl_waitq);
static int bl_ready;
static unsigned char backlight_init_success;


struct timer_base
{
    unsigned int TCFG0;
    unsigned int TCFG1;
    unsigned int TCON;
    unsigned int TCNTB0;
    unsigned int TCMPB0;
    unsigned int TCNTO0;
    unsigned int TCNTB1;
    unsigned int TCMPB1;
    unsigned int TCNTO1;
    unsigned int TCNTB2;
    unsigned int TCMPB2;
    unsigned int TCNTO2;
    unsigned int TCNTB3;
    unsigned int TCMPB3;
    unsigned int TCNTO3;
    unsigned int TCNTB4;
    unsigned int TCBTO4;
    unsigned int TINT_CSTAT;
};



struct pri_data
{
	char name[32];
	int gpio;
	struct pwm_base *pwm;
	struct clk *clk;
	struct resource *res;
	struct resource *irq;
	struct timer_base *timer_base;
	struct pinctrl   *pinctrl;
	struct pinctrl_state *pstate_in;
	struct pinctrl_state *pstate_out;
};

static struct pri_data *data = NULL;

static void priv_timer_debug(int step)
{
	pr_info("---------------------%d---------------------", step);

	pr_info("TCFG0      : %08x \n", data->timer_base->TCFG0);
	pr_info("TCFG1      : %08x \n", data->timer_base->TCFG1);
	pr_info("TCON       : %08x \n", data->timer_base->TCON);
	pr_info("TCNTB3     : %08x \n", data->timer_base->TCNTB3);
	pr_info("TCMPB3     : %08x \n", data->timer_base->TCMPB3);
	pr_info("TCNTO3     : %08x \n", data->timer_base->TCNTO3);
	pr_info("TINT_CSTAT : %08x \n", data->timer_base->TINT_CSTAT);
}



#define DEV_NAME		"backlight-dt"

struct pri_dev
{	
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
	struct cdev *cdev;
};


static struct pri_dev *dev = NULL;


static void stop_timer_for_1wire(void);
static void start_one_wire_session(unsigned char req);


static ssize_t backlight_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	return 0;	
}

static ssize_t backlight_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	char state[16];
	int value = 0;
	int ret = 0;
	
	if (sscanf(buf, "%s", state) != 1)
		return -EINVAL;

	if (sscanf(buf, "%d", &value)) {
		pr_notice("enter %s  value : %d \n", __func__, value);	
		start_one_wire_session(value + 0x80);

	    ret = wait_event_interruptible_timeout(bl_waitq, bl_ready, HZ / 10);
	    if (ret < 0) {
	        return ret;
	    }
	    if (ret == 0) {
	        return -ETIMEDOUT;
	    }
	}
		
	return count;
}

static DEVICE_ATTR(backlight, 0644, backlight_show,
    backlight_store);


static int backlight_open(struct inode * inode, struct file * file)
{
    printk("%s enter.\n", __func__);

    return 0;
}


static ssize_t backlight_write(struct file *file, const char *usrbuf, size_t count, loff_t * ppos)
{
	char cmd,opt;
	char rev_buf[8] = {0};
	int ret = 0;
		
	printk(KERN_INFO "write \n");		
	
	if(copy_from_user(rev_buf,usrbuf,8))
		return -EFAULT;

	cmd = rev_buf[0];
	opt = rev_buf[1];
	
	printk(KERN_NOTICE "cmd : %d opt : %d \n", cmd, opt);
	
    start_one_wire_session(cmd + 0x80);

    ret = wait_event_interruptible_timeout(bl_waitq, bl_ready, HZ / 10);
    if (ret < 0) {
        return ret;
    }
    if (ret == 0) {
        return -ETIMEDOUT;
    }

    return count;
}

static int backlight_release(struct inode *inode, struct file *file)
{
	printk("%s enter.\n", __func__);

    return 0;
}



const struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.open     = backlight_open,   
    .write    = backlight_write,
    .release = backlight_release,  
};

static int chrdev_setup(void)
{
	int ret = 0;
	dev = kmalloc(sizeof(struct pri_dev),GFP_KERNEL);
	if(!dev)
		return -ENOMEM;

	strcpy(dev->name, DEV_NAME);

	ret = alloc_chrdev_region(&dev->dev, 0, 1, dev->name);
	dev->major = MAJOR(dev->dev);
	
	if(ret < 0)
	{
		kfree(dev);
		return ret;
	}

	dev->cdev = cdev_alloc();
	if(!dev->cdev)
	{
		unregister_chrdev_region(dev->dev,1);
		kfree(dev);
		return -EFAULT;
	}
	cdev_init(dev->cdev,&fops);
	dev->cdev->owner = THIS_MODULE;
	dev->cdev->ops = &fops;
	cdev_add(dev->cdev,dev->dev,1);

	dev->class = class_create(THIS_MODULE,dev->name);
	ret = PTR_ERR(dev->class);
	if (IS_ERR(dev->class))
	{
		cdev_del(dev->cdev);
		unregister_chrdev_region(dev->dev,1);
		kfree(dev);
		return -EFAULT;
	}

	device_create(dev->class,NULL,dev->dev,NULL,dev->name,dev);

	return 0;
}

static void chrdev_setdown(void)
{
	device_destroy(dev->class, dev->dev);
	class_destroy(dev->class);
	cdev_del(dev->cdev);
	unregister_chrdev_region(dev->dev,1);
	kfree(dev);
}

static int init_timer_for_1wire(struct clk *clk)
{
	unsigned long pclk;

	pclk = clk_get_rate(clk);
    printk("PWM clock = %ld\n", pclk);

	return 0;
}


// CRC
//
static const unsigned char crc8_tab[] = {
0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3,
};


#define crc8_init(crc) ((crc) = 0XACU)
#define crc8(crc, v) ( (crc) = crc8_tab[(crc) ^(v)])


// once a session complete
static unsigned total_received, total_error;
static unsigned  last_res;

static inline void notify_bl_data(unsigned char a, unsigned char b, unsigned char c)
{
    bl_ready = 1;
    backlight_init_success = 1;
    wake_up_interruptible(&bl_waitq);
}

static void one_wire_session_complete(unsigned char req, unsigned int res)
{
    unsigned char crc;
    const unsigned char *p = (const unsigned char*)&res;
    total_received ++;

    last_res = res;

    crc8_init(crc);
    crc8(crc, p[3]);
    crc8(crc, p[2]);
    crc8(crc, p[1]);

    if (crc != p[0]) {
        // CRC dismatch
        if (total_received > 100) {
            total_error++;
        }
        return;
    }

    notify_bl_data(p[3], p[2], p[1]);

}


static void set_pin_as_input(void)
{
	pinctrl_select_state(data->pinctrl,data->pstate_in);
}

static void set_pin_as_output(void)
{
	pinctrl_select_state(data->pinctrl,data->pstate_out);
}


static void set_pin_value(int v)
{
	if (v) {
        gpio_set_value(data->gpio, 1);
    } else {
        gpio_set_value(data->gpio, 0);
    }
}

static int get_pin_value(void)
{
	return gpio_get_value(data->gpio);
}


static void stop_timer_for_1wire(void)
{
	data->timer_base->TCON &= ~(1<<16);  /* stop timer  */
}

static void start_one_wire_session(unsigned char req)
{
	unsigned char crc;
	unsigned int tcon;
	one_wire_status = START;

	set_pin_value(1);

	set_pin_as_output();

	crc8_init(crc);
	crc8(crc, req);
	io_data = (req << 8) + crc;
	io_data <<= 16;

	io_bit_count = 1;
	set_pin_as_output();
	data->timer_base->TCNTB3 = 650;
	tcon = data->timer_base->TCON;
    tcon &= ~(0xF << 16);
    tcon |= (1 << 17);         /* Updates TCNTB3 */ 
    data->timer_base->TCON = tcon;
    tcon |= (1 << 16);         /* Starts Timer 3  */
    tcon |= (1 << 19);         /* Iterval mode(auto-reload) */
    tcon &= ~(1 << 17);
    data->timer_base->TCON = tcon;
    data->timer_base->TINT_CSTAT |= 0x08; /* enable interrupt */
    //gpio_set_value(one_write_pin, 0);
    set_pin_value(0);
}


static irqreturn_t timer_for_1wire_interrupt(int irq, void *dev_id)
{
	//pr_info("%s called .\n", __func__);

	data->timer_base->TINT_CSTAT |= (1<<8);    /* clear the status */
	io_bit_count--;
    switch(one_wire_status){
		case START:
			if (io_bit_count == 0) {
	            io_bit_count = 16;
	            one_wire_status = REQUEST;
			}
			break;

		case REQUEST:
			// Send a bit
	        set_pin_value(io_data & (1U << 31));
	        io_data <<= 1;
	        if (io_bit_count == 0) {
	            io_bit_count = 2;
	            one_wire_status = WAITING;
	        }
			break;

		case WAITING:
			if (io_bit_count == 0) {
	            io_bit_count = 32;
	            one_wire_status = RESPONSE;
	        }
	        if (io_bit_count == 1) {
	            set_pin_as_input();
	            set_pin_value(1);
	        }
			break;

		case RESPONSE:
			// Get a bit
	        io_data = (io_data << 1) | get_pin_value();
	        if (io_bit_count == 0) {
	            io_bit_count = 2;
	            one_wire_status = STOPING;
	            set_pin_value(1);
	            set_pin_as_output();
	            one_wire_session_complete(one_wire_request, io_data);
	        }
			break;

		case STOPING:
			if (io_bit_count == 0) {
	            one_wire_status = IDLE;
	            stop_timer_for_1wire();
	        }
			break;

		default:
			stop_timer_for_1wire();
			break;
	}

	return IRQ_HANDLED;
}

static int backlight_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;	

	pr_info("%s called .\n", __func__);

	if(!dev){
		pr_err("no platform device.\n");
		return -EINVAL;
	}
	

	data = devm_kmalloc(dev,sizeof(struct pri_data),GFP_KERNEL);
	if(!data){
		pr_err("no memory.\n");
		return -EINVAL;
	}

	/* get resource */
	data->gpio = of_get_named_gpio(dev->of_node,"gpios",0);
	if(data->gpio < 0){
		dev_err(dev, "get gpio err.\n");
		goto err;
	}
	
	devm_gpio_request_one(dev, data->gpio,GPIOF_OUT_INIT_HIGH,"backlight-gpio");

	data->pinctrl = devm_pinctrl_get(dev);
	data->pstate_in = pinctrl_lookup_state(data->pinctrl,"backlight_in");
	data->pstate_out = pinctrl_lookup_state(data->pinctrl,"backlight_out");

	data->res = platform_get_resource(pdev,IORESOURCE_MEM,0);
	if(!data->res){
		dev_err(dev, "no mem platform resource.\n");
		goto err;
	}

	/* ioremap */
	data->timer_base = devm_ioremap_resource(dev,data->res);
	if (IS_ERR(data->timer_base)){
		dev_err(dev, "ioremap err.\n");
		goto err;
	}

	/* get clk */
	data->clk = devm_clk_get(dev,"timers");
	if(IS_ERR(data->clk)){
		dev_err(dev, "get clk err.\n");
		goto err1;
	}
	
	/* enable clk */
	ret = clk_prepare_enable(data->clk);
	if(ret){
		 dev_err(dev, "failed to enable base clock\n");
		 goto err1;
	}
	
	printk("rate:  %lu \n" , clk_get_rate(data->clk));

	data->timer_base->TCFG0  = 0xf00;
	data->timer_base->TCFG1  = 0x10004;
	ret = init_timer_for_1wire(data->clk);
	if(ret){
		pr_err("timer for 1wire init err.\n");
		goto err1;
	}

	data->irq = platform_get_resource(pdev,IORESOURCE_IRQ,0);
	if(!data->irq){
		dev_err(dev, "get irq from platform resource err.\n");
		goto err1;
	}
	
	/* request irq */
	ret = devm_request_irq(dev,data->irq->start,timer_for_1wire_interrupt,IRQF_TIMER,"timer3",data);
	if(ret < 0){
		dev_err(dev, "request irq err.\n");
		goto err1;
	}
	
	priv_timer_debug(0);

	start_one_wire_session(0xf0);

	// /sys/bus/platform/drivers/xxx ÏÂÃæÉú³Éxxx½Úµã
	// ¿ÉÒÔÊ¹ÓÃecho 1 > ./xxxÐ´Ö¸Áî¿ØÖÆxxx£¬ ×îÖÕµ÷ÓÃxxx_store´ïµ½¿ØÖÆµÄÄ¿µÄ
	device_create_file(dev, &dev_attr_backlight);
	

	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		goto err1;
	}

	return 0;

err1:
	clk_disable_unprepare(data->clk);
err:
	devm_kfree(dev,data);
	return -EINVAL;
}

static int backlight_remove(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);

	device_remove_file(&pdev->dev, &dev_attr_backlight);

	chrdev_setdown();

	return 0;
}



const struct of_device_id tiny4412_backlight_match_tbl[] = 
{
	{.compatible = "tiny4412,backlight"},
	{},
};


static struct platform_driver tiny4412_backlight_dt_drv = 
{
	.probe = backlight_probe,
	.remove = backlight_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "backlight",
		.of_match_table = tiny4412_backlight_match_tbl,
	}
};


static int __init backlight_dt_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tiny4412_backlight_dt_drv);
	if(ret)
		printk(KERN_ERR "init demo: probe failed: %d\n", ret);

	return ret;
}

static void __exit backlight_dt_exit(void)
{
	platform_driver_unregister(&tiny4412_backlight_dt_drv);
}

module_init(backlight_dt_init);
module_exit(backlight_dt_exit);
MODULE_LICENSE("GPL");
