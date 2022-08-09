#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kdev_t.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "tes_ipcore_driver.h"
#include "tes_ipcore_io.h"
#include "tes_ipcore_module.h"
// TODO cleanup includes

static int tes_ipcore_probe(struct platform_device *pdev);
static int tes_ipcore_remove(struct platform_device *pdev);
static void tes_ipcore_print_infos(struct tes_ipcore_device *tes_dev);

// This contains all data needed globally by the driver
struct tes_ipcore_global_t *tes_ipcore_global;

static const struct of_device_id tes_ipcore_of_ids[] = {
	{.compatible = TES_IPCORE_COMPAT_STR_CDC,
	 .data = &tes_ipcore_cdc_pdata},
	{.compatible = TES_IPCORE_COMPAT_STR_DHD,
	 .data = &tes_ipcore_dhd_pdata},
	{.compatible = TES_IPCORE_COMPAT_STR_WARP,
	 .data = &tes_ipcore_warp_pdata},
	{.compatible = TES_IPCORE_COMPAT_STR_D2D,
	 .data = &tes_ipcore_d2d_pdata},
	{}};
static struct platform_driver tes_ipcore_driver = {
	.driver = {
		.name = TES_IPCORE_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tes_ipcore_of_ids),
	},
	.probe = tes_ipcore_probe,
	.remove = tes_ipcore_remove,
};

static void tes_ipcore_print_infos(struct tes_ipcore_device *tes_dev) { 
	uint32_t hwversion;

	dev_warn(tes_dev->base_dev, "This driver is PRELIMINARY. Do NOT use in production environment!\n");
	dev_info(tes_dev->base_dev, "Base address:\t0x%08lx - 0x%08lx\n",
			 tes_dev->base_phys, tes_dev->base_phys + tes_dev->span);
	dev_info(tes_dev->base_dev, "IRQ:\t%d\n", tes_dev->irq_no);
	hwversion = tes_ipcore_read_reg(tes_dev->mmio, tes_dev->platform_data->version_reg);
	dev_info(tes_dev->base_dev, "IP core rev. 0x%08X\n", hwversion);
}
/*
 * probe function (init new hardware)
 * copy all neccessary data from device tree description to local data structure and initialize the driver part.
 * */
static int tes_ipcore_probe(struct platform_device *pdev) {
	struct resource *mem;
	int result = 0;
	int irq;
	struct tes_ipcore_device *tes_dev;
	const struct of_device_id *of_device_match;
	dev_t current_dev_t;
	uint32_t hwversion;

	// calculate the new dev_t for the device created here
	current_dev_t = MKDEV(tes_ipcore_global->major, tes_ipcore_global->dev_count);
	tes_ipcore_global->dev_count++;

	// alloc memory for device data
	tes_dev = devm_kzalloc(&pdev->dev, sizeof(struct tes_ipcore_device), GFP_KERNEL);
	if (!tes_dev) {
		dev_err(&pdev->dev,
				"Memory allocation for driver struct failed!\n");
		result = -ENOMEM;
		goto ALLOC_MEM_FAILED;
	}

	// retrieve data from device tree
	of_device_match = of_match_device(tes_ipcore_of_ids, &pdev->dev);
	tes_dev->platform_data = of_device_match->data;
	platform_set_drvdata(pdev, tes_dev);
	tes_dev->base_dev = &pdev->dev;

	// copy data from device tree to device struct
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tes_dev->mmio = devm_ioremap_resource(&pdev->dev, mem);
	dev_dbg(&pdev->dev, "Mapped IO from 0x%x to 0x%p\n", mem->start,
			tes_dev->mmio);
	if (IS_ERR(tes_dev->mmio)) {
			result = PTR_ERR(tes_dev->mmio);
			goto IOREMAP_FAILED;
	}

	tes_dev->base_phys = mem->start;
	tes_dev->span = mem->end - mem->start;

	// check IP core version
	hwversion = tes_ipcore_read_reg(tes_dev->mmio, tes_dev->platform_data->version_reg)
		& tes_dev->platform_data->version_reg_mask;
	if(hwversion != tes_dev->platform_data->version_reg_expected) {
		dev_err(&pdev->dev, "Unsupported IP core version: %08x\n", hwversion);
		result = -EINVAL;
		goto UNSUPPORTED;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
			dev_err(&pdev->dev, "Could not get platform IRQ number\n");
			return irq;
	}
	tes_dev->irq_no = irq;

	// init spinlock and irq
	spin_lock_init(&tes_dev->irq_slck);
	init_waitqueue_head(&tes_dev->irq_waitq);

	// register device and create sys-file
	tes_dev->base_dev = device_create(tes_ipcore_global->class, NULL,
		current_dev_t, tes_dev, "%lx.%s", tes_dev->base_phys, tes_dev->platform_data->fs_dev_name);
	if (!tes_dev->base_dev) {
		dev_err(tes_dev->base_dev, "can't create device: %s\n", tes_dev->platform_data->fs_dev_name);
		result = -EBUSY;
		goto DEVICE_CREATE_FAILED;
	}

	// register IRQ
	if (devm_request_irq(&pdev->dev, tes_dev->irq_no, tes_ipcore_irq_handler, 0, tes_dev->platform_data->irq_name, (void *)tes_dev)) {
		dev_err(tes_dev->base_dev, "can't register irq %d\n", tes_dev->irq_no);
		result = -EBUSY;
		goto IRQ_FAILED;
	}

	// set pointer to current device in global struct
	tes_ipcore_global->device_by_minor[MINOR(current_dev_t)] = tes_dev;

	// probe finished; print some infos
	tes_ipcore_print_infos(tes_dev);

	return 0;

IRQ_FAILED:
	device_destroy(tes_ipcore_global->class, tes_dev->base_dev->devt);
DEVICE_CREATE_FAILED:
IOREMAP_FAILED:
UNSUPPORTED:
ALLOC_MEM_FAILED:
	tes_ipcore_global->dev_count--; // this was calculated at the start of the function

	return result;
}

static int tes_ipcore_remove(struct platform_device *pdev) {
	struct tes_ipcore_device *tes_dev = platform_get_drvdata(pdev);

	device_destroy(tes_ipcore_global->class, tes_dev->base_dev->devt);
	return 0;
}

static int __init _tes_ipcore_init(void) {
	int result = 0;
	dev_t first_dev_t;

	// alloc mem for global struct
	tes_ipcore_global = kzalloc(sizeof(struct tes_ipcore_global_t), GFP_KERNEL);
	if (!tes_ipcore_global) {
		pr_err("Memory allocation for driver global struct failed!\n");
		result = -ENOMEM;
		goto ALLOC_GLOBAL_FAILED;
	}

	// create class
	tes_ipcore_global->class = class_create(THIS_MODULE, TES_IPCORE_DEVCLASS_NAME);
	if (!tes_ipcore_global->class) {
		pr_err("cannot create class %s\n", TES_IPCORE_DEVCLASS_NAME);
		result = -EBUSY;
		goto CLASS_FAILED;
	}

	// alloc chrdev region
	result = alloc_chrdev_region(&first_dev_t, 0, 1, TES_IPCORE_CHRDEV_NAME);
	tes_ipcore_global->major = MAJOR(first_dev_t);
	if (result < 0) {
		pr_err("cannot allocate chrdev region:  %s\n", TES_IPCORE_CHRDEV_NAME);
		goto CHRDEV_FAILED;
	}

	// init & add chrdev
	cdev_init(&tes_ipcore_global->cdev, &tes_ipcore_fops);
	tes_ipcore_global->cdev.owner = THIS_MODULE;
	result = cdev_add(&tes_ipcore_global->cdev, first_dev_t, TES_IPCORE_MAX_DEVICES);
	if (result) {
		pr_err("can't register cdev\n");
		goto CDEV_ADD_FAILED;
	}

	// register actual driver
	// this invokes a call to tes_ipcore_probe for each ip core
	result = platform_driver_register(&tes_ipcore_driver);
	if (result) {
		pr_err("failed to register platform driver for %s\n", TES_IPCORE_DRIVER_NAME);
		goto PLATFORM_REGISTER_FAILED;
	}

	pr_info("%s device driver successfully initialized!\n", TES_IPCORE_DRIVER_NAME);

	return 0;

PLATFORM_REGISTER_FAILED:
	cdev_del(&tes_ipcore_global->cdev);
CDEV_ADD_FAILED:
	unregister_chrdev_region(first_dev_t, TES_IPCORE_MAX_DEVICES);
CHRDEV_FAILED:
	class_destroy(tes_ipcore_global->class);
CLASS_FAILED:
ALLOC_GLOBAL_FAILED:
	return result;
}

static void __exit _tes_ipcore_exit(void) {
	platform_driver_unregister(&tes_ipcore_driver);
	class_destroy(tes_ipcore_global->class);
	cdev_del(&tes_ipcore_global->cdev);
	unregister_chrdev_region(MKDEV(tes_ipcore_global->major, 0), TES_IPCORE_MAX_DEVICES);
	kfree(tes_ipcore_global);
}

module_init(_tes_ipcore_init);
module_exit(_tes_ipcore_exit);
MODULE_AUTHOR("TES Electronic Solutions GmbH");
MODULE_DESCRIPTION("Kernel driver for all TES IP cores");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tes_ipcore_of_ids);
