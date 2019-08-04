/*
 * tiny4412_ts.c driver for tiny4412 on linux-4.19.27
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
#include <linux/input.h>
#include <linux/workqueue.h>



#define DEV_NAME		"eeprom"

struct ft5x06_ts {
	struct i2c_client *client;
	struct input_dev	*idev;
	int			use_count;
	int			bReady;
	int			irq;
	struct gpio_desc	*wakeup_gpio;
	struct gpio_desc	*reset_gpio;
	struct timer_list	release_timer;
	unsigned		down_mask;
	unsigned		max_x;
	unsigned		max_y;
	unsigned		firmware_bug_hit;
	struct regmap		*regmap;
};


struct ts_dev {
	int x;
	int y;
	int irq;
	int xres;
	int yres;
	int pressure;	
};

struct pri_dev
{	
	dev_t dev;
	int major;
	char name[32];
	struct class *class;
	struct cdev *cdev;
};


static struct pri_dev *priv_dev = NULL;

const struct file_operations fops = 
{
	.owner = THIS_MODULE, 
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


/* Return 0 if detection is successful, -ENODEV otherwise */
static int detect_ft5x06(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	char buffer;
	struct i2c_msg pkt = {
		client->addr,
		I2C_M_RD,
		sizeof(buffer),
		&buffer
	};
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;
	if (i2c_transfer(adapter, &pkt, 1) != 1)
		return -ENODEV;
	return 0;
}


static int tiny4412_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct ft5x06_ts *ts;
	struct device *dev = &client->dev;
	struct gpio_desc *gp;

	
	pr_info("%s called .\n", __func__);

	ret = chrdev_setup();
	if(ret){
		pr_err("chrdev setup err.\n");
		return -EINVAL;
	}

	ts = kzalloc(sizeof(struct ft5x06_ts), GFP_KERNEL);
	if (!ts) {
		dev_err(dev, "no memory \n");
		return -ENOMEM;
	}

	ts->client = client;
	ts->irq = client->irq ;

	ret = detect_ft5x06(client);
	if (ret) {
		dev_err(dev, "Could not detect touch screen %d.\n",ret);
		goto err;
	}

	return 0;
	
err:
	kfree(ts);
	chrdev_setdown();
	return -EINVAL;
}

static int tiny4412_ts_remove(struct i2c_client *client)
{
	pr_info("%s called .\n", __func__);
	chrdev_setdown();

	return 0;

}


static const struct i2c_device_id tiny4412_ts_id_table[] =
{
    { "tiny4412_ts", 0 },
    {}
};


static struct i2c_driver tiny4412_ts_driver =
{
    .driver = {
        .name   = "tiny4412-ts",
        .owner  = THIS_MODULE,
    },
    .probe      = tiny4412_ts_probe,
    .remove     = tiny4412_ts_remove,
    .id_table   = tiny4412_ts_id_table,
};


static int __init tiny4412_ts_init(void)
{
	i2c_add_driver(&tiny4412_ts_driver);
	return 0;
}

static void __exit tiny4412_ts_exit(void)
{
	i2c_del_driver(&tiny4412_ts_driver);
}


module_init(tiny4412_ts_init);
module_exit(tiny4412_ts_exit);
MODULE_LICENSE("GPL");
