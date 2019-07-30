/*
 * tiny4412_ts_key.c driver for tiny4412 on linux-4.19.27
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
#include <linux/pwm.h>
#include <linux/input.h>

enum rw_status
{
    IDLE = 0,
    START,
    REQUEST,
    WAITING,
    RESPONSE,
    STOPING,
};


struct lcd_key_data {
        struct input_dev *input;
};

struct priv_data {
        struct pinctrl *pinctrl;
        struct pinctrl_state *pstate_in;
        struct pinctrl_state *pstate_out;
        int gpio;
        struct resource *res;
        struct resource *irq;
        volatile struct pwm_base *pwm;
        volatile unsigned int io_bit_count;
        volatile enum rw_status one_wire_status;
        volatile unsigned int io_data;
        int major;
        struct cdev one_wire_cdev;
        struct class *one_wire_class;
        struct clk *timer_clk;
        uint8_t req;

        /* Key sub system */
        struct lcd_key_data key;

        /* kernel timer */
        struct timer_list timer;
        unsigned char brightness;
        uint8_t bl_wr_done;
        uint8_t lcd_model;
        int last_key;
};

static struct priv_data *priv_data = NULL;

const unsigned int KEY_TABLE[] = {
        KEY_HOME,
        KEY_MENU,
        KEY_BACK,
};

#define SAMPLE_BPS 9600

#define SLOW_LOOP_FEQ 25
#define FAST_LOOP_FEQ 60

#define REQ_KEY  0x30U
#define REQ_TS   0x40U
#define REQ_INFO 0x60U

DECLARE_WAIT_QUEUE_HEAD(bl_wait);



struct pwm_base
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



static const unsigned char crc8_tab[] =
{
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
#define crc8(crc, v)   ((crc) = crc8_tab[(crc) ^(v)])



#define DEV_NAME		"ts_keys"

struct pri_dev
{	
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
	struct cdev *cdev;
};


static struct pri_dev *dev = NULL;


const struct file_operations fops = 
{
	.owner = THIS_MODULE,
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


static void set_pin_as_input(void)
{
	pinctrl_select_state(priv_data->pinctrl,priv_data->pstate_in);
}

static void set_pin_as_output(void)
{
	pinctrl_select_state(priv_data->pinctrl,priv_data->pstate_out);
}


static void set_pin_value(int v)
{
	if (v) {
        gpio_set_value(priv_data->gpio, 1);
    } else {
        gpio_set_value(priv_data->gpio, 0);
    }
}

static int get_pin_value(void)
{
	return gpio_get_value(priv_data->gpio);
}

static void notify_bl_data(unsigned char a, unsigned char b, unsigned char c)
{
	priv_data->bl_wr_done = 1;
	priv_data->brightness = 0;
	wake_up_interruptible(&bl_wait);
	printk(KERN_DEBUG "Write backlight done.\n");
}

static void notify_info_data(unsigned char _lcd_type,
                unsigned char ver_year, unsigned char week)
{
	if (_lcd_type != 0xFF) {
		unsigned int firmware_ver = ver_year * 100 + week;

		/* Currently only S702 has hard key */
		pr_notice("[LCD] model = %s, ver = %d\n", (_lcd_type == 24) ? "S702" : "N/A", firmware_ver);
		priv_data->lcd_model = _lcd_type;
	}	
}


static void ts_if_report_key(int key) 
{
	int changed = priv_data->last_key ^ key;
	int down;
	int i;
	
	if (!changed)
		return;
	
	for (i = 0; i < ARRAY_SIZE(KEY_TABLE); i++) {
		if (changed & (1 << i)) {
		down = !!((1<<i) & key);
		printk(KERN_DEBUG "report key = %d press = %d\n", KEY_TABLE[i], down);
		input_report_key(priv_data->key.input, KEY_TABLE[i], down);
		}
	}
	
	priv_data->last_key = key;
	input_sync(priv_data->key.input);
}


static void one_wire_session_complete(unsigned char req, unsigned int res)
{
	unsigned char crc;
	const unsigned char *p = (const unsigned char*)&res;
	crc8_init(crc);
	crc8(crc, p[3]);
	crc8(crc, p[2]);
	crc8(crc, p[1]);

	if (crc != p[0]) {
		return;
	}

	printk("req : %x.\n", req);

	switch (req) {
		case REQ_KEY:
			ts_if_report_key(p[1]);
			break;

		case REQ_TS:
			pr_notice("It does not support touchscreen for onewire!\n");
			break;

		case REQ_INFO:
			notify_info_data(p[3], p[2], p[1]);
			break;

		default:
			notify_bl_data(p[3], p[2], p[1]);
			break;
	}
}


static void stop_timer_for_1wire(void)
{
	priv_data->pwm->TCON &= ~(1<<16);  /* stop timer  */
}


static void start_one_wire_session(unsigned char req)
{
	unsigned char crc;
	unsigned int tcon;
	unsigned long prescale;
	priv_data->one_wire_status = START;
	
	set_pin_value(1);
	
	set_pin_as_output();
	
	crc8_init(crc);
	crc8(crc, req);
	priv_data->io_data = (req << 8) + crc;
	priv_data->io_data <<= 16;
	
	priv_data->io_bit_count = 1;
	set_pin_as_output();

	/* set prescaler of timer3 */
	prescale =  (priv_data->pwm->TCFG0 >> 8) & 0xFF;
	priv_data->pwm->TCNTB3 = clk_get_rate(priv_data->timer_clk) / (prescale + 1) / SAMPLE_BPS - 1;

	
	tcon = priv_data->pwm->TCON;
	
	tcon &= ~(0xF << 16);
	tcon |= (1 << 17);		   /* Updates TCNTB3 */ 
	priv_data->pwm->TCON = tcon;
	
	tcon |= (1 << 16);		   /* Starts Timer 3  */
	tcon |= (1 << 19);		   /* Iterval mode(auto-reload) */
	tcon &= ~(1 << 17);
	
	priv_data->pwm->TCON = tcon;
	priv_data->pwm->TINT_CSTAT |= 0x08; /* enable interrupt */
	//gpio_set_value(one_write_pin, 0);
	set_pin_value(0);
}


static irqreturn_t timer_for_1wire_interrupt(int irq, void *dev_id)
{
	//pr_info("%s called .\n", __func__);

	priv_data->pwm->TINT_CSTAT |= (1<<8);    /* clear the status */

	priv_data->io_bit_count--;
	switch(priv_data->one_wire_status){
		case IDLE:
			break;

		case START:
			if (priv_data->io_bit_count == 0) {
				priv_data->io_bit_count = 16;
				priv_data->one_wire_status = REQUEST;
			}
			break;

		case REQUEST:
			gpio_set_value(priv_data->gpio, priv_data->io_data & (1U << 31));
			priv_data->io_data <<= 1;
			if (priv_data->io_bit_count == 0) {
				priv_data->io_bit_count = 2;
				priv_data->one_wire_status = WAITING;
			}
			break;

		case WAITING:
			if (priv_data->io_bit_count == 0) {
				priv_data->io_bit_count = 32;
				priv_data->io_data = 0;
				priv_data->one_wire_status = RESPONSE;
			}
			if (priv_data->io_bit_count == 1) {
				pinctrl_select_state(priv_data->pinctrl, priv_data->pstate_in);
				gpio_set_value(priv_data->gpio, 1);
			}
			break;

		case RESPONSE:
			// Get a bit
			priv_data->io_data = (priv_data->io_data << 1) | gpio_get_value(priv_data->gpio);
			if (priv_data->io_bit_count == 0) {
				priv_data->io_bit_count = 2;
				priv_data->one_wire_status = STOPING;
				gpio_set_value(priv_data->gpio, 1);
				pinctrl_select_state(priv_data->pinctrl, priv_data->pstate_out);
				//rx data
				one_wire_session_complete(priv_data->req, priv_data->io_data);
			}
			break;

		case STOPING:
			if (priv_data->io_bit_count == 0) {
				priv_data->one_wire_status = IDLE;
				stop_timer_for_1wire();
			}
			break;

		default:
			stop_timer_for_1wire();
			break;
	}
	

	return IRQ_HANDLED;
}


static int ts_keys_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	pr_info("%s called .\n", __func__);

	if(!dev){
		pr_err("no platform dev.\n");
		return -EINVAL;
	}

	priv_data = devm_kmalloc(dev,sizeof(struct priv_data),GFP_KERNEL);
	if(!priv_data){
		pr_err("no memory.\n");
		return -ENOMEM;
	}

	/* get pinctrl */
	priv_data->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(priv_data->pinctrl)) {
		dev_err(dev, "get pinctrl error!\n");
		goto err;
	}


	priv_data->pstate_in = pinctrl_lookup_state(priv_data->pinctrl, "backlight_in");
	if (IS_ERR(priv_data->pstate_in)) {
		dev_err(dev, "backlight_in not found!\n");
		goto err;
	}

	priv_data->pstate_out = pinctrl_lookup_state(priv_data->pinctrl, "backlight_out");
	if (IS_ERR(priv_data->pstate_out)) {
		dev_err(dev, "backlight_out not found!\n");
		goto err;
	}


	/* get clk */
	priv_data->timer_clk = devm_clk_get(dev, "timers");
	if (IS_ERR(priv_data->timer_clk)) {
		dev_err(dev, "devm_clk_get error!\n");
		goto err;
	}

	printk("rate:  %lu \n" , clk_get_rate(priv_data->timer_clk));

	/* enable clk */
	ret = clk_prepare_enable(priv_data->timer_clk);
	if (ret) {
		dev_err(dev, "clk_prepare_enable error!\n");
		goto err;
	}

	/* get gpio */
	priv_data->gpio = of_get_named_gpio(dev->of_node, "gpios", 0);
	if (!gpio_is_valid(priv_data->gpio)) {
		dev_err(dev, "gpios not found!\n");
		goto err;
	}

	/* request gpio */
	ret = devm_gpio_request_one(dev, priv_data->gpio, GPIOF_OUT_INIT_HIGH, "ts_keys");
	if (ret) {
		dev_err(dev, "request gpio error!\n");
		goto err;
	}

	/* get resource */
	priv_data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!priv_data->res) {
		dev_err(dev, "get res error!\n");
		goto err;
	}

	/* ioremap */
	priv_data->pwm = (volatile struct pwm_base *)devm_ioremap_resource(dev, priv_data->res);
	if (IS_ERR((struct pwm_base *)priv_data->pwm)) {
		dev_err(dev, "devm_ioremap_resource error!\n");
		goto err;
	}


	/* timer3 setup */
	priv_data->pwm->TCFG0 |=  (0xf << 8);
	priv_data->pwm->TCFG1 &= ~(0xf << 12);

	/* get irq */
	priv_data->irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!priv_data->irq) {
		dev_err(dev, "get irq error!\n");
		goto err;
	}

	/* request irq */
	ret = devm_request_irq(dev, priv_data->irq->start, timer_for_1wire_interrupt , IRQF_TIMER, "backlight", NULL);
	if (ret) {
		dev_err(dev, "devm_request_irq error!\n");
		goto err;
	}

	start_one_wire_session(REQ_INFO);
	

	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		goto err;
	}

	return 0;

err:
	devm_kfree(dev, priv_data);
	return -EINVAL;
}

static int ts_keys_remove(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);

	chrdev_setdown();

	return 0;
}

static void timer_handler(struct timer_list *t)
{
        if (priv_data->one_wire_status != IDLE) {
                return;
        }

        if (!priv_data->lcd_model) {
                start_one_wire_session(REQ_INFO);
        } else if (priv_data->brightness) {
                start_one_wire_session(priv_data->brightness);
        } else {
                start_one_wire_session(REQ_KEY);
        }
}

static void timer_callback(struct timer_list *t)
{
	priv_data->timer.expires = jiffies + msecs_to_jiffies(20);
	add_timer(&priv_data->timer);

	timer_handler(&priv_data->timer);
}


static void priv_timer_setup(void)
{
	timer_setup(&priv_data->timer, timer_callback, 0);

	/* Start up timer */
	timer_callback(0);
}

static int lcd_key_init(void)
{
	struct input_dev *input;
	int ret,i;

	input = input_allocate_device();
	if (!input) {
		pr_notice("input alloc error!\n");
		return -ENOMEM;
	}

	input->name = "tiny4412_lcd_key";
	input->id.bustype   = BUS_RS232;
	input->id.vendor        = 0xDEAD;
	input->id.product       = 0xBEEF;
	input->id.version       = 0x0100;

	input->evbit[0] = BIT_MASK(EV_KEY) | BIT(EV_SYN);

	for (i=0; i<ARRAY_SIZE(KEY_TABLE); ++i) {
		input_set_capability(input, EV_KEY, KEY_TABLE[i]);
	}

	ret = input_register_device(input);
	priv_data->key.input = input;

	return ret;
}



const struct of_device_id tiny4412_ts_keys_match_tbl[] = 
{
	{.compatible = "tiny4412,backlight"},
	{},
};


static struct platform_driver tiny4412_ts_keys_drv = 
{
	.probe = ts_keys_probe,
	.remove = ts_keys_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "ts_keys",
		.of_match_table = tiny4412_ts_keys_match_tbl,
	}
};


static int __init ts_keys_init(void)
{
	int ret = 0;
	
		ret = platform_driver_register(&tiny4412_ts_keys_drv);
		if(ret)
			printk(KERN_ERR "probe failed: %d\n", ret);
		else{
			ret = lcd_key_init();
			priv_timer_setup();
		}
	
		return ret;

}

static void __exit ts_keys_exit(void)
{
	del_timer_sync(&priv_data->timer);

	input_unregister_device(priv_data->key.input);
	platform_driver_unregister(&tiny4412_ts_keys_drv);
}


module_init(ts_keys_init);
module_exit(ts_keys_exit);
MODULE_LICENSE("GPL");
