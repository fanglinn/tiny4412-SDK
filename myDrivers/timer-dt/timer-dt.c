/*
 * leds_dt driver for tiny4412 on linux-4.19.27
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
#include <linux/clk.h>
#include <linux/interrupt.h>

struct _timer {  
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
    unsigned int TCNTO4;
    unsigned int TINT_CSTAT;
};

struct priv_data 
{
	struct resource *irq;
	struct resource *res;
	struct _timer *timer_base;
	struct clk *clk;
};

struct priv_data *priv_data = NULL;



static void priv_timer_debug(int step)
{
	pr_info("---------------------%d---------------------", step);

	pr_info("TCFG0      : %08x \n", priv_data->timer_base->TCFG0);
	pr_info("TCFG1      : %08x \n", priv_data->timer_base->TCFG1);
	pr_info("TCON       : %08x \n", priv_data->timer_base->TCON);
	pr_info("TCNTB3     : %08x \n", priv_data->timer_base->TCNTB3);
	pr_info("TCMPB3     : %08x \n", priv_data->timer_base->TCMPB3);
	pr_info("TCNTO3     : %08x \n", priv_data->timer_base->TCNTO3);
	pr_info("TINT_CSTAT : %08x \n", priv_data->timer_base->TINT_CSTAT);
}


/*
* Device Tree

timer3@139D0000{
                compatible = "tiny4412,timer3";
                reg = <0x139D0000 0x48>;
                interrupts = <0 40 0>;
                clocks = <&clock CLK_PWM>;
                clock-names = "timers";
        };

*/

#define DEV_NAME		"timer3"

struct pri_dev
{
	struct cdev *cdev;
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
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


irqreturn_t timer3_irq_handler(int irq, void *dev_id)
{
	struct priv_data *priv_data = (struct priv_data *)dev_id;
	
	pr_notice("enter %s\n", __func__);

	/* Timer 3 bit. interrupt status bit. It clears by writing "1" on this */
	priv_data->timer_base->TINT_CSTAT |=(1<<8);

	//priv_data->timer_base->TCON |= (1<<17); 		/* Updates TCNTB3 */

	//priv_data->timer_base->TCON &= ~(1<<17);         /* Updates TCNTB3 */
	//priv_data->timer_base->TCON |= (1<<16);         /* start timer  */

	priv_timer_debug(3);


	return IRQ_HANDLED;
}


static int timer3_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	pr_notice("enter %s\n", __func__);

	if(!dev){
		pr_err("no platform resource.\n");
		return -EINVAL;
	}

	priv_data = devm_kmalloc(dev,sizeof(struct priv_data),GFP_KERNEL);
	if(!priv_data){
		pr_err("no memory.\n");
		return -ENOMEM;
	}

	priv_data->res = platform_get_resource(pdev,IORESOURCE_MEM,0);
	if(!priv_data->res){
		dev_err(dev, "no mem platform resource.\n");
		goto err;
	}

	/* ioremap */
	priv_data->timer_base = devm_ioremap_resource(dev,priv_data->res);
	if (IS_ERR(priv_data->timer_base)){
		dev_err(dev, "ioremap err.\n");
		goto err;
	}

	/* get clk */
	priv_data->clk = devm_clk_get(dev,"timers");
	if(IS_ERR(priv_data->clk)){
		dev_err(dev, "get clk err.\n");
		goto err1;
	}

	/* enable clk */
	ret = clk_prepare_enable(priv_data->clk);
	if(ret){
		 dev_err(dev, "failed to enable base clock\n");
		 goto err1;
	}

	printk("rate:  %lu \n" , clk_get_rate(priv_data->clk));

	priv_data->irq = platform_get_resource(pdev,IORESOURCE_IRQ,0);
	if(!priv_data->irq){
		dev_err(dev, "get irq from platform resource err.\n");
		goto err1;
	}

	/* request irq */
	ret = devm_request_irq(dev,priv_data->irq->start,timer3_irq_handler,IRQF_TIMER,"timer3",priv_data);
	if(ret < 0){
		dev_err(dev, "request irq err.\n");
		goto err2;
	}

	priv_timer_debug(0);
	
	/* 100M / 4 = 25Mhz */
	priv_data->timer_base->TCNTB3=25000000;    /* 25M */

	// init tranfer and start timer
    priv_data->timer_base->TCON |= (1<<17);         /* Updates TCNTB3 */
	priv_data->timer_base->TCON |= (1<<19);         /* Iterval mode(auto reload)  */

	priv_data->timer_base->TCON &= ~(1<<17);         /* Updates TCNTB3 */
	priv_data->timer_base->TCON |= (1<<16);         /* start timer  */

    priv_data->timer_base->TINT_CSTAT |=(1<<8) + (1<<3);   /* Enables Timer 3 interrupt */

	priv_timer_debug(1);


	ret = chrdev_setup();	
	if(ret){
		dev_err(dev, "setup error .\n");
		return -EINVAL;
	}

	return 0;
err2:
	clk_disable_unprepare(priv_data->clk);

err1:
	devm_iounmap(dev,priv_data->timer_base);
err:
	devm_kfree(dev,priv_data);
	return -EINVAL;
}

static int timer3_remove(struct platform_device *pdev)
{
	pr_notice("enter %s\n", __func__);

	chrdev_setdown();
	return 0;
}

const struct of_device_id tiny4412_timer_match_tbl[] = 
{
	{.compatible = "tiny4412,timer3"},
	{},
};


static struct platform_driver tiny4412_timer_dt_drv = 
{
	.probe = timer3_probe,
	.remove = timer3_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "timer3",
		.of_match_table = tiny4412_timer_match_tbl,
	}
};


static int __init timer_dt_init(void)
{
	int ret = 0;
	
		ret = platform_driver_register(&tiny4412_timer_dt_drv);
		if(ret)
			printk(KERN_ERR "init: probe failed: %d\n", ret);
	
		return ret;

}

static void __exit timer_dt_exit(void)
{
	platform_driver_unregister(&tiny4412_timer_dt_drv);
}


module_init(timer_dt_init);
module_exit(timer_dt_exit);
MODULE_LICENSE("GPL");
