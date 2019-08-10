/*
 * tiny4412_keys_input driver for tiny4412 on linux-4.19.27
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
#include <linux/input.h>
#include <linux/timer.h>

#define MAX_KEYS_NUM	4

static int keys_code[] = 
{
	KEY_L,
	KEY_S,
	KEY_ENTER,
	KEY_BACKSPACE
};

struct input_keys_desc
{
	char name[32];
	int gpio;
	int irq;
	int code_value;
};

static 	struct input_keys_desc key[MAX_KEYS_NUM];
static struct input_dev *input = NULL;
static struct timer_list keys_timer;
struct input_keys_desc *key_desc = NULL;


#define DEV_NAME		"keys"

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

irqreturn_t keys_handler(int irq, void *dev_id)
{
	key_desc = (struct input_keys_desc *)dev_id;
	mod_timer(&keys_timer, HZ/100 + jiffies);

	return IRQ_HANDLED;
}

static void timer_handler(struct timer_list *timer)
{	
	pr_info("%s enters, name : %s gpio : %d irq : %d\n", __func__, key_desc->name,
		key_desc->gpio, key_desc->irq);
	
	input_report_key(input, key_desc->code_value, gpio_get_value(key_desc->gpio));
	
	input_sync(input);
}



static int keys_probe(struct platform_device *pdev)
{
	int ret = 0, i;
	int gpio = -1, irq = -1;
	struct device *dev = &pdev->dev;
	
	pr_info("%s called .\n", __func__);


	input = input_allocate_device();
	if(!input){
		pr_err("input alloc fail.\n");
		return -EINVAL;
	}

	input->name = "tiny4412_keys";
	input->phys = "gpio-keys";
	input->uniq = "20190808";
	input->id.bustype = BUS_HOST;
	input->id.vendor = ID_PRODUCT;
	input->id.version = ID_VERSION;

	/* type */
	set_bit(EV_SYN, input->evbit);
	set_bit(EV_REP, input->evbit);
	set_bit(EV_KEY, input->evbit);

	/* key value */
	set_bit(KEY_L, input->keybit);
	set_bit(KEY_S, input->keybit);
	set_bit(KEY_ENTER, input->keybit);
	set_bit(KEY_BACKSPACE, input->keybit);

	ret = input_register_device(input);
	if(ret){
		pr_err("input register err.\n");
		goto err;
	}

	for(i = 0; i < MAX_KEYS_NUM ; i++){
		sprintf(key[i].name, "tiny4412,key%d", i+1);

		/* get gpio from name */
		gpio = of_get_named_gpio(dev->of_node, key[i].name, 0);
		if(gpio < 0){
			dev_err(dev, "of get gpio err.\n");
			goto err1;
		}

		key[i].gpio = gpio;

		irq = gpio_to_irq(gpio);
		if(irq < 0){
			dev_err(dev, "get irq from gpio err.\n");
			goto err1;
		}

		key[i].irq = irq;

		key[i].code_value = keys_code[i];

		/* request irq */
		ret = devm_request_any_context_irq(dev, irq , keys_handler,IRQF_TRIGGER_FALLING,
			key[i].name , &key[i]);
		if(ret < 0){
			dev_err(dev, "unable request irq from irq :%d of gpio :%d", irq, gpio);
			goto err1;
		}
	}
	

	ret = chrdev_setup();
	if(ret){
		dev_err(dev, "chrdev setup err.\n");
		goto err1;
	}

	timer_setup(&keys_timer, timer_handler, 0);

	/* set platform data as remove input-dev */
	//platform_set_drvdata(pdev, data);

	return 0;
	
err1:
	input_unregister_device(input);
err:
	input_free_device(input);
	return -EINVAL;
}


static int keys_remove(struct platform_device *pdev)
{	
	int ret = 0;
	pr_info("%s called .\n", __func__);

	ret = del_timer(&keys_timer);
	if(ret){
		pr_info("active timer.\n");
	}

	chrdev_setdown();

	input_unregister_device(input);
	input_free_device(input);
	
	return 0;
}


const struct of_device_id tiny4412_keys_match_tbl[] = 
{
	{.compatible = "tiny4412,keys"},
	{},
};


static struct platform_driver tiny4412_keys_dt_drv = 
{
	.probe = keys_probe,
	.remove = keys_remove,
	.driver = 
	{
		.owner = THIS_MODULE,
		.name = "keys",
		.of_match_table = tiny4412_keys_match_tbl,
	}
};


static int __init keys_input_dt_init(void)
{
	int ret = 0;
	
	ret = platform_driver_register(&tiny4412_keys_dt_drv);
	if(ret)
		printk(KERN_ERR "%s : probe failed: %d\n", __func__,  ret);
	
	return ret;
}

static void __exit keys_input_dt_exit(void)
{
	platform_driver_unregister(&tiny4412_keys_dt_drv);
}


module_init(keys_input_dt_init);
module_exit(keys_input_dt_exit);
MODULE_LICENSE("GPL");
