/*
 * keys-dt.c driver for tiny4412 on linux-4.19.27
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

struct pri_data
{
	int gpio;
	int irq;
	char name[32];
};

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
	struct pri_data *data = (struct pri_data *)dev_id;
	
	pr_info("%s enters, name : %s gpio : %d irq : %d\n", __func__, data->name,
		data->gpio, data->irq);

	return IRQ_HANDLED;
}

static int keys_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	int gpio = -1, irq = -1;
	int i;
	
	struct pri_data *data = NULL;	

	pr_info("%s called .\n", __func__);
	if(!dev->of_node){
		dev_err(dev, "no platform found .\n");
		return -EINVAL;
	}

	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		return -EINVAL;
	}

	data = devm_kmalloc(dev,sizeof(struct pri_data) * 4,GFP_KERNEL);
	if(!data){
		dev_err(dev, "no memory.\n");
		goto err;
	}

	for(i = 0; i < 4; i++){
		sprintf(data[i].name, "tiny4412,key%d", i + 1);
		/* get gpio from name */
		gpio = of_get_named_gpio(dev->of_node,data[i].name,0);
		if(gpio < 0){
			dev_err(dev, "of get gpio err. i : %d gpio : %d\n", i, gpio);
			goto err1;
		}

		data[i].gpio = gpio;

		irq = gpio_to_irq(gpio);
		if(irq < 0){
			dev_err(dev, "get irq from gpio err.\n");
			goto err1;
		}

		data[i].irq = irq;

		/* request irq */
		ret = devm_request_any_context_irq(dev,data[i].irq,keys_handler,IRQF_TRIGGER_FALLING,
			data[i].name,&data[i]);
		if(ret < 0){
			dev_err(dev, "unable request irq from irq :%d of gpio :%d", irq, gpio);
			goto err1;
		}
	}

	return 0;

err1:
	devm_kfree(dev,data);

err:
	chrdev_setdown();
	return -EINVAL;
}


static int keys_remove(struct platform_device *pdev)
{
	pr_info("%s called .\n", __func__);

	chrdev_setdown();

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


static int __init keys_dt_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tiny4412_keys_dt_drv);
	if(ret)
		printk(KERN_ERR "init demo: probe failed: %d\n", ret);

	return ret;
}

static void __exit keys_dt_exit(void)
{
	platform_driver_unregister(&tiny4412_keys_dt_drv);
}

module_init(keys_dt_init);
module_exit(keys_dt_exit);
MODULE_LICENSE("GPL");
