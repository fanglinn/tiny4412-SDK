/*
 * beep-pwm-dt driver for tiny4412 on linux-4.19.27
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
	
struct pri_data
{
	struct resource *res;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pstate;
	struct clk *base_clk;
};

static struct pri_data *dt_data = NULL;


#define DEV_NAME		"beep-pwm-dt"

struct pri_dev
{	
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
	struct cdev *cdev;
};


static struct pri_dev *dev = NULL;


/*
* @ Device Tree :

beep-pwm {
                compatible = "tiny4412,beep-pwm";
                reg = <0x139D0000 0x14>;
                pinctrl-names = "pwm_pin";
                pinctrl-0 = <&pwm_pin>;
                clocks = <&clock CLK_PWM>;
                clock-names = "timers";
        };



&pinctrl_0 {	
	pwm_pin: pwm_pin {
	                samsung,pins = "gpd0-0";
	                samsung,pin-function = <2>;
	                samsung,pin-pud = <0>;
	                samsung,pin-drv = <0>;
	        };
	...
};

*/


struct TIMER_BASE{
    unsigned int TCFG0;         
    unsigned int TCFG1;          
    unsigned int TCON;       
    unsigned int TCNTB0;         
    unsigned int TCMPB0;         
};

volatile static struct TIMER_BASE * timer = NULL;


static void beep_on(void)
{
    printk("beep on\n");
    timer->TCON = (timer->TCON &~(0xff))| 0x0d;
    printk("%x\n",timer->TCON);
}


static void beep_off(void)
{
    timer->TCON = timer->TCON & ~(0x01);
}




static int beep_open (struct inode *inode, struct file *file)
{
	printk(KERN_INFO "open \n");	
	
	return 0;
}

static int beep_close (struct inode *inode, struct file *file)
{
	printk(KERN_INFO "close \n");	
	
	return 0;
}

static ssize_t beep_write (struct file *file, const char __user *usrbuf, size_t len, loff_t *off)
{
	char cmd,opt;
	char rev_buf[8] = {0};
	
	printk(KERN_INFO "write \n");		

	if(copy_from_user(rev_buf,usrbuf,8))
		return -EFAULT;

	cmd = rev_buf[0];
	opt = rev_buf[1];

	printk(KERN_NOTICE "cmd : %d opt : %d \n", cmd, opt);

	if(cmd == 0)
	{
		// off
		beep_off();	
	}
	else if(cmd == 1)
	{
		// on
		beep_on();
	}
	else
	{
		// do nothing.
	}
	return 0;
}


const struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.open = beep_open,
	.release = beep_close,
	.write = beep_write,
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



static int beep_pwm_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct device *dev = &pdev->dev;
	
	pr_info("%s called .\n", __func__);

	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		return -EINVAL;
	}

	dt_data = devm_kmalloc(dev,sizeof(struct pri_data),  GFP_KERNEL);
	if(!dt_data){
		pr_err("no memory .\n");
		goto err;
	}

	dt_data->pinctrl = devm_pinctrl_get(dev);
	if(!dt_data->pinctrl){
		pr_err("get pinctrl err.\n");
		goto err1;
	}

	dt_data->pstate = pinctrl_lookup_state(dt_data->pinctrl , "pwm_pin");
	if(!dt_data->pstate){
		pr_err("get pinctrl state err.\n");
		goto err1;
	}

	/* select pinctrl state */
	pinctrl_select_state(dt_data->pinctrl , dt_data->pstate);

	dt_data->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!dt_data->res){
		pr_err("get resource err.\n");
		goto err1;
	}

	/* get clk */
	dt_data->base_clk = devm_clk_get(&pdev->dev, "timers");
	if (IS_ERR(dt_data->base_clk)) {
		pr_err("get base clk err.\n");
		goto err1;
	 }

	/* enable clk */
	ret = clk_prepare_enable(dt_data->base_clk);
	if (ret < 0) {
        dev_err(dev, "failed to enable base clock ret : %d \n", ret);
        goto err1;
    }

	/* ioremap */
	timer = devm_ioremap_resource(&pdev->dev, dt_data->res);
	if(!timer){
		pr_err("ioremap err.\n");
		goto err1;
	}

	/* timer setup */
	timer->TCFG0  = (timer->TCFG0 &~(0xff << 0)) | (0xfa << 0);
	timer->TCFG1  = (timer->TCFG1 &~(0x0f << 0)) | (0x02 << 0);
	timer->TCNTB0 = 100000;
	timer->TCMPB0 = 90000;
	timer->TCON   = (timer->TCON  &~(0x0f << 0)) | (0x06 << 0);
	printk("timer :TCFG0:%08x TCFG1:%08x TCNTB0:%08x TCMPB0:%08x TCON:%08x\n",timer->TCFG0,timer->TCFG1,
		timer->TCNTB0,timer->TCMPB0,timer->TCON);
	
	
	return 0;

err1:	
	devm_kfree(dev,dt_data);
err:
	chrdev_setdown();
	return -EINVAL;
}

static int beep_pwm_remove(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);
	
	chrdev_setdown();
	
	return 0;
}

static const struct of_device_id tiny4412_beep_match_tbl[] = 
{
	{.compatible = "tiny4412,beep-pwm"},
	{},
};


static struct platform_driver tiny4412_beep_pwm_dt_drv = 
{
	.probe = beep_pwm_probe,
	.remove = beep_pwm_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "beep-pwm-dt",
		.of_match_table = tiny4412_beep_match_tbl,
	}
};


static int __init beep_pwm_dt_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tiny4412_beep_pwm_dt_drv);
	if(ret)
		printk(KERN_ERR "platform register  failed: %d\n", ret);

	return ret;
}

static void __exit beep_pwm_dt_exit(void)
{
	platform_driver_unregister(&tiny4412_beep_pwm_dt_drv);
}

module_init(beep_pwm_dt_init);
module_exit(beep_pwm_dt_exit);
MODULE_LICENSE("GPL");
