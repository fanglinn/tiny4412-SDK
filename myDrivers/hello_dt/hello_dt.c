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
#include <linux/platform_device.h>


/*
 dts:

 HelloWorld {
	compatible = "tiny4412, hello_world";
	status = "okay";
 };
 
*/

static int hello_world_probe(struct platform_device *pdev)
{
	pr_notice("------------------- %s\n", __func__);
	
	int count = 0;

	void *platdata = NULL;
	platdata = dev_get_platdata(&pdev->dev);
	pr_notice("platdata = %p\n", platdata);
	pr_notice("device_tree = %p\n", pdev->dev.of_node);
	count = of_get_child_count(pdev->dev.of_node);
	pr_notice("child_count = %d\n", count);

	return 0;
}
static int hello_world_remove(struct platform_device *pdev)
{
        pr_notice("------------------- %s\n", __func__);
        return 0;
}
static void hello_world_shutdown(struct platform_device *pdev)
{
        pr_notice("------------------- %s\n", __func__);
        return ;
}
static int hello_world_suspend(struct platform_device *pdev, pm_message_t state)
{
        pr_notice("------------------- %s\n", __func__);
        return 0;
}
static int hello_world_resume(struct platform_device *pdev)
{
        pr_notice("------------------- %s\n", __func__);
        return 0;
}



static const struct of_device_id tiny4412_hello_world_dt_match[] = {
        { .compatible = "tiny4412, hello_world" },
        {},
};


static struct platform_driver tiny4412_hello_dt_drv = 
{
	.probe          = hello_world_probe,
	.remove         = hello_world_remove,
    .shutdown   = hello_world_shutdown,
    .suspend    = hello_world_suspend,
    .resume     = hello_world_resume, 
    .driver         = {
            .name   = "hello world",
            .of_match_table = tiny4412_hello_world_dt_match,
    },
};

module_platform_driver(tiny4412_hello_dt_drv);

MODULE_LICENSE("GPL");

