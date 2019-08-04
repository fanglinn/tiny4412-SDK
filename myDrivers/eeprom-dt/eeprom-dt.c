/*
 * eeprom-dt.c driver for tiny4412 on linux-4.19.27
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

#include <linux/i2c.h>

#define DEV_NAME		"eeprom"

struct pri_dev
{	
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
	struct cdev *cdev;

	struct i2c_client *client;
};


static struct pri_dev *priv_dev = NULL;


/*
* Device Tree

&i2c_0{
    eeprom@50 {                  // the same as driverid_table->name
        compatible = "tiny4412,eeprom";
        reg = <0x50>;            // slave address
    };
};


*/


static ssize_t eeprom_dt_read (struct file *file, char __user *usrbuf, size_t len, loff_t *off)
{
	pr_info("%s called .\n", __func__);

	return 0;
}

static ssize_t eeprom_dt_write (struct file *file, const char __user *usrbuf, size_t len, loff_t *off)
{
	pr_info("%s called .\n", __func__);
	
	return 0;
}


static int eeprom_dt_open (struct inode *inode, struct file *file)
{
	pr_info("%s called .\n", __func__);
	
	return 0;
}

static int eeprom_dt_close (struct inode *inode, struct file *file)
{
	pr_info("%s called .\n", __func__);

	return 0;
}

const struct file_operations fops = 
{
	.owner = THIS_MODULE, 
	.open  = eeprom_dt_open,
	.release = eeprom_dt_close,
	.read = eeprom_dt_read,
	.write = eeprom_dt_write,
};

static int chrdev_setup(void)
{
	int ret = 0;
	priv_dev = kmalloc(sizeof(struct pri_dev),GFP_KERNEL);
	if(!priv_dev)
		return -ENOMEM;

	strcpy(priv_dev->name, DEV_NAME);

	ret = alloc_chrdev_region(&priv_dev->dev, 0, 1, priv_dev->name);
	priv_dev->major = MAJOR(priv_dev->dev);
	
	if(ret < 0)
	{
		kfree(priv_dev);
		return ret;
	}

	priv_dev->cdev = cdev_alloc();
	if(!priv_dev->cdev)
	{
		unregister_chrdev_region(priv_dev->dev,1);
		kfree(priv_dev);
		return -EFAULT;
	}
	cdev_init(priv_dev->cdev,&fops);
	priv_dev->cdev->owner = THIS_MODULE;
	priv_dev->cdev->ops = &fops;
	cdev_add(priv_dev->cdev,priv_dev->dev,1);

	priv_dev->class = class_create(THIS_MODULE,priv_dev->name);
	ret = PTR_ERR(priv_dev->class);
	if (IS_ERR(priv_dev->class))
	{
		cdev_del(priv_dev->cdev);
		unregister_chrdev_region(priv_dev->dev,1);
		kfree(priv_dev);
		return -EFAULT;
	}

	device_create(priv_dev->class,NULL,priv_dev->dev,NULL,priv_dev->name,priv_dev);

	return 0;
}

static void chrdev_setdown(void)
{
	device_destroy(priv_dev->class, priv_dev->dev);
	class_destroy(priv_dev->class);
	cdev_del(priv_dev->cdev);
	unregister_chrdev_region(priv_dev->dev,1);
	kfree(priv_dev);
}


static int eeprom_dt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	pr_info("%s called .\n", __func__);

	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		return -EINVAL;
	}

	priv_dev->client = client;

	return 0;
}

static int eeprom_dt_remove(struct i2c_client *client)
{
	pr_info("%s called .\n", __func__);
	chrdev_setdown();

	return 0;
}


static const struct i2c_device_id eeprom_id_table[] =
{
    { "eeprom", 0 },
    {}
};


static struct i2c_driver tiny4412_eeprom_driver =
{
    .driver = {
        .name   = "eeprom",
        .owner  = THIS_MODULE,
    },
    .probe      = eeprom_dt_probe,
    .remove     = eeprom_dt_remove,
    .id_table   = eeprom_id_table,
};


static int __init eeprom_dt_init(void)
{
	i2c_add_driver(&tiny4412_eeprom_driver);
	return 0;
}

static void __exit eeprom_dt_exit(void)
{
	i2c_del_driver(&tiny4412_eeprom_driver);
}


module_init(eeprom_dt_init);
module_exit(eeprom_dt_exit);
MODULE_LICENSE("GPL");
