/*
 * hello driver for tiny4412 on linux-4.19.27
*/
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <linux/platform_device.h>

typedef struct
{
    int gpio;
    int state;
    struct pinctrl *pctrl;
    struct pinctrl_state *sleep_pstate;
    struct pinctrl_state *active_pstate;
    char name[20];
} gpio_demo_data_t;

static u32 __iomem *base;
static u32 __iomem *cfg_reg;
static u32 __iomem *pud_reg;
static u32 __iomem *drv_reg;



static ssize_t gpio_demo_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t gpio_demo_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}

static DEVICE_ATTR(gpio_demo, 0644, gpio_demo_show,
    gpio_demo_store);

static int gpio_demo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	gpio_demo_data_t *data = NULL;
	struct resource *res = NULL;
	u32 config, pud, drv;
	int i = 0;

	int gpio = -1;
	
	printk("%s enter.\n", __func__);

	if (!dev->of_node) {
		dev_err(dev, "no platform data.\n");
	}

	data = devm_kzalloc(dev, sizeof(*data)*2, GFP_KERNEL);
	if (!data) {
		dev_err(dev, "no memory.\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	base = devm_ioremap(dev, res->start, res->end - res->start);
	printk("%s: remap: 0x%x ---> 0x%x to %p\n", __func__, res->start, res->end, base);

	if (IS_ERR(base)) {
		dev_err(dev, "remap failed.\n");
        devm_kfree(dev, data);
        return PTR_ERR(base);
    }


	cfg_reg = base;
    pud_reg = (u32 *)((u32)cfg_reg + 8);
    drv_reg = (u32 *)((u32)pud_reg + 4);

    for (i = 0; i < 2; i++) {
        gpio = of_get_named_gpio(dev->of_node,
            "tiny4412,gpio", i);    // ???????gpio?
        if (gpio < 0) {
            dev_err(dev, "Looking up %s (%d) property in node %s failed %d\n",
                "tiny4412,gpio", i, dev->of_node->full_name, gpio);
            goto err2;
        }

        data[i].gpio = gpio;
        if (gpio_is_valid(gpio)) {
            sprintf(data[i].name, "tiny4412.led%d", i);
            ret = devm_gpio_request_one(dev, gpio, GPIOF_INIT_HIGH, data[i].name);  // ??gpio??
            if (ret < 0) {
                dev_err(dev, "request gpio %d failed.\n", gpio);
                goto err2;
            }
        }

        data[i].pctrl = devm_pinctrl_get(dev);   // ??pinctrl
        if (IS_ERR(data[i].pctrl)) {
            dev_err(dev, "pinctrl get failed.\n");
            devm_gpio_free(dev, gpio);
            goto err2;
        }

        data[i].sleep_pstate = pinctrl_lookup_state(data[i].pctrl, "gpio_sleep");   // ??'gpio_sleep'?????
        if (IS_ERR(data[i].sleep_pstate)) {
            dev_err(dev, "look up sleep state failed.\n");
            devm_pinctrl_put(data[i].pctrl);
            devm_gpio_free(dev, gpio);
            goto err2;
        }

        data[i].active_pstate = pinctrl_lookup_state(data[i].pctrl, "gpio_active");   // ??'gpio_active'?????
        if (IS_ERR(data[i].active_pstate)) {
            dev_err(dev, "look up sleep state failed.\n");
            devm_pinctrl_put(data[i].pctrl);
            devm_gpio_free(dev, gpio);
            goto err2;
        }
    }

    dev_set_drvdata(dev, data);
    // ????/sys/bus/platform/drivers/gpio_demo/110002e0.gpio_demo/???????gpio_demo???
    device_create_file(dev, &dev_attr_gpio_demo);

    config = readl(cfg_reg);
    pud = readl(pud_reg);
    drv = readl(drv_reg);
    printk("%s, default state: cfg: 0x%x, pud: 0x%x, drv: 0x%x\n", __func__, config, pud, drv);

    pinctrl_select_state(data[0].pctrl, data[0].active_pstate);   // ?GPM4_0???active?????
    pinctrl_select_state(data[1].pctrl, data[1].active_pstate);   // ?GPM4_1???active?????
    data[0].state = 1;
    data[1].state = 1;

    config = readl(cfg_reg);
    pud = readl(pud_reg);
    drv = readl(drv_reg);
    printk("%s, active state: cfg: 0x%x, pud: 0x%x, drv: 0x%x\n", __func__, config, pud, drv);

    return 0;
	
err2:
	devm_iounmap(dev, base);
err1:
 	devm_kfree(dev, data);
err0:
 	return -EINVAL;

}

static int gpio_demo_remove(struct platform_device *pdev)
{
	printk("%s enter.\n", __func__);

	//device_remove_file(&pdev->dev, &dev_attr_gpio_demo);
	return 0;
}


static const struct of_device_id gpio_demo_dt_ids[] = {
	{ .compatible = "tiny4412,gpio_demo", },
	{},
};


MODULE_DEVICE_TABLE(of, gpio_demo_dt_ids);


static struct platform_driver gpio_demo_driver = {
	.driver        = {
		.name    = "gpio_demo",
		.of_match_table    = of_match_ptr(gpio_demo_dt_ids),
	},
	.probe        = gpio_demo_probe,
	.remove        = gpio_demo_remove,
};


static int __init gpio_demo_init(void)
{
	int ret;
	ret = platform_driver_register(&gpio_demo_driver);
	if (ret)
		printk(KERN_ERR "int demo: probe failed: %d\n", ret);

	return ret;
}


static void __exit gpio_demo_exit(void)
{
	platform_driver_unregister(&gpio_demo_driver);
}


module_init(gpio_demo_init);
module_exit(gpio_demo_exit);

MODULE_LICENSE("GPL");

