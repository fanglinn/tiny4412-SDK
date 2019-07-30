/*
 * adc-dt.c driver for tiny4412 on linux-4.19.27
 *
 * Note(s) : cat /sys/bus/platform/drivers/adcs/126c0000.adc/adc
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

#include <linux/wait.h>
#include <linux/clk.h>


#define DEV_NAME		"adcs"

struct pri_dev
{	
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
	struct cdev *cdev;
};


static struct pri_dev *dev = NULL;

DECLARE_WAIT_QUEUE_HEAD(adc_wait);


/*
* @ Device Tree :

	adc@126C0000 {
                compatible = "tiny4412,adc";
                reg = <0x126C0000  0x20>;
                clocks = <&clock CLK_TSADC>;
                clock-names = "timers";
                interrupt-parent = <&combiner>;
                interrupts = <10 3>;
        };


*/

struct ADC_BASE
{
    unsigned int ADCCON;    //0
    unsigned int TEMP0;     //
    unsigned int ADCDLY;    //8
    unsigned int ADCDAT;    //c
    unsigned int TEMP1;     //10
    unsigned int TEMP2;     //14
    unsigned int CLRINTADC; //18
    unsigned int ADCMUX;    //1c
};

volatile static struct ADC_BASE *adc_base = NULL;

static int adc_open(struct inode *inode, struct file *file)
{
    printk("adc_open\n");
    return 0;
}

static int adc_release(struct inode *inode, struct file *file)
{
    printk("adc_exit\n");
    return 0;
}

static ssize_t adc_read(struct file *filp, char __user *buf, size_t count, loff_t *off)
{
	int data = 0, ret = 0;
	printk("adc_read\n");
	adc_base->ADCMUX = 0x00;
	adc_base->ADCCON = (1 << 16 | 1 << 14 | 99 << 6 | 1 << 0);
	wait_event_interruptible(adc_wait, ((adc_base->ADCCON >> 15) & 0x01));
	data = adc_base->ADCDAT & 0xfff;
	ret = copy_to_user(buf, &data, count);
	printk("copy_to_user %x\n", data);

	if (ret < 0)
	{
		printk("copy_to_user error\n");
		return -EFAULT;
	}

	return count;
}


const struct file_operations fops = 
{
	.owner = THIS_MODULE,
	.open               = adc_open,
    .read               = adc_read,
    .release            = adc_release,
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


static ssize_t adc_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	int data = 0, ret = 0;
	printk("adc read \n");
	adc_base->ADCMUX = 0x00;
	adc_base->ADCCON = (1 << 16 | 1 << 14 | 99 << 6 | 1 << 0);
	wait_event_interruptible(adc_wait, ((adc_base->ADCCON >> 15) & 0x01));
	data = adc_base->ADCDAT & 0xfff;
	ret = copy_to_user(buf, &data, 1);
	printk("copy_to_user %x\n", data);

	return 0;	
}

static ssize_t adc_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}


static DEVICE_ATTR(adc, 0644, adc_show,
    adc_store);



struct clk *base_clk;
int irq;
irqreturn_t adc_demo_isr(int irq, void *dev_id)
{
   pr_info("%s called .\n", __func__);
   
   wake_up(&adc_wait);

   /* clear irq */
   adc_base->CLRINTADC = 1;
   return IRQ_HANDLED;
}

static int adc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct resource *res = NULL;

	pr_info("%s called .\n", __func__);
	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(!res){
		pr_err("get platform resource err.\n");
		goto err;
	}

	/* get clk */
	base_clk = devm_clk_get(&pdev->dev, "timers");
	if (IS_ERR(base_clk)){
		dev_err(dev, "failed to get timer base clk\n");
		goto err;
	}

	/* enable clk */
	ret = clk_prepare_enable(base_clk);
	if(ret < 0){
		dev_err(dev, "failed to enable base clock\n");
		goto err;
	}

	/* ioremap */
	adc_base = devm_ioremap_resource(&pdev->dev, res);
	if(!adc_base){
		printk("devm_ioremap_resource error\n");
		goto err1;
	}

	/* get irq */
	irq = platform_get_irq(pdev, 0);
	if(irq < 0){
		dev_err(&pdev->dev, "no irq resource. \n");
		goto err1;
	}

	/* request irq */
	ret = request_irq(irq, adc_demo_isr, 0, "adc", NULL);
	if(ret < 0){
		dev_err(dev, "failed to request_irq\n");
		goto err1;
	}

	// /sys/bus/platform/drivers/leds/110002e0.led 下面生成led节点
	// 可以使用echo 1 > ./led写指令控制led， 最终调用leds_store达到控制的目的
	device_create_file(dev, &dev_attr_adc);

	
	return 0;

	
err1:
	clk_disable(base_clk);
	clk_unprepare(base_clk);
err:
	chrdev_setdown();
	return -EINVAL;
}

static int adc_remove(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);	

	device_remove_file(&pdev->dev, &dev_attr_adc);

	clk_disable(base_clk);
	clk_unprepare(base_clk);
	free_irq(irq, NULL);	

	chrdev_setdown();
	
	return 0;
}

const struct of_device_id tiny4412_adc_match_tbl[] = 
{
	{.compatible = "tiny4412,adc"},
	{},
};


static struct platform_driver tiny4412_adc_dt_drv = 
{
	.probe = adc_probe,
	.remove = adc_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "adcs",
		.of_match_table = tiny4412_adc_match_tbl,
	}
};


static int __init adc_dt_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tiny4412_adc_dt_drv);
	if(ret)
		printk(KERN_ERR "init demo: probe failed: %d\n", ret);

	return ret;
}

static void __exit adc_dt_exit(void)
{
	platform_driver_unregister(&tiny4412_adc_dt_drv);
}

module_init(adc_dt_init);
module_exit(adc_dt_exit);
MODULE_LICENSE("GPL");

