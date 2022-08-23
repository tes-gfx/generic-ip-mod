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

#include "genip_driver.h"
#include "genip_io.h"
#include "genip_module.h"
// TODO cleanup includes

static int genip_probe(struct platform_device *pdev);
static int genip_remove(struct platform_device *pdev);
static void genip_print_infos(struct genip_device *tes_dev);

// This contains all data needed globally by the driver
struct genip_global_t *genip_global;

static const struct of_device_id genip_of_ids[] = {
	{.compatible = GENIP_COMPAT_STR_CDC,
	 .data = &genip_cdc_pdata},
	{.compatible = GENIP_COMPAT_STR_DHD,
	 .data = &genip_dhd_pdata},
	{.compatible = GENIP_COMPAT_STR_WARP,
	 .data = &genip_warp_pdata},
	{.compatible = GENIP_COMPAT_STR_D2D,
	 .data = &genip_d2d_pdata},
	{}};
static struct platform_driver genip_driver = {
	.driver = {
		.name = GENIP_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(genip_of_ids),
	},
	.probe = genip_probe,
	.remove = genip_remove,
};

static void genip_print_infos(struct genip_device *tes_dev) { 
	uint32_t hwversion;

	dev_warn(tes_dev->base_dev, "This driver is PRELIMINARY. Do NOT use in production environment!\n");
	dev_info(tes_dev->base_dev, "Base address:\t0x%08lx - 0x%08lx\n",
			 tes_dev->base_phys, tes_dev->base_phys + tes_dev->span);
	dev_info(tes_dev->base_dev, "IRQ:\t%d\n", tes_dev->irq_no);
	hwversion = genip_read_reg(tes_dev->mmio, tes_dev->platform_data->version_reg);
	dev_info(tes_dev->base_dev, "IP core rev. 0x%08X\n", hwversion);
}
/*
 * probe function (init new hardware)
 * copy all neccessary data from device tree description to local data structure and initialize the driver part.
 * */
static int genip_probe(struct platform_device *pdev) {
	struct resource *mem;
	int result = 0;
	int irq;
	struct genip_device *tes_dev;
	const struct of_device_id *of_device_match;
	dev_t current_dev_t;
	uint32_t hwversion;

	// calculate the new dev_t for the device created here
	current_dev_t = MKDEV(genip_global->major, genip_global->dev_count);
	genip_global->dev_count++;

	// alloc memory for device data
	tes_dev = devm_kzalloc(&pdev->dev, sizeof(struct genip_device), GFP_KERNEL);
	if (!tes_dev) {
		dev_err(&pdev->dev,
				"Memory allocation for driver struct failed!\n");
		result = -ENOMEM;
		goto ALLOC_MEM_FAILED;
	}

	// retrieve data from device tree
	of_device_match = of_match_device(genip_of_ids, &pdev->dev);
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
	hwversion = genip_read_reg(tes_dev->mmio, tes_dev->platform_data->version_reg)
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
	tes_dev->base_dev = device_create(genip_global->class, NULL,
		current_dev_t, tes_dev, tes_dev->platform_data->fs_dev_name);
	if (!tes_dev->base_dev) {
		dev_err(tes_dev->base_dev, "can't create device: %s\n", tes_dev->platform_data->fs_dev_name);
		result = -EBUSY;
		goto DEVICE_CREATE_FAILED;
	}

	// register IRQ
	if (devm_request_irq(&pdev->dev, tes_dev->irq_no, genip_irq_handler, 0, tes_dev->platform_data->irq_name, (void *)tes_dev)) {
		dev_err(tes_dev->base_dev, "can't register irq %d\n", tes_dev->irq_no);
		result = -EBUSY;
		goto IRQ_FAILED;
	}

	// set pointer to current device in global struct
	genip_global->device_by_minor[MINOR(current_dev_t)] = tes_dev;

	// probe finished; print some infos
	genip_print_infos(tes_dev);

	return 0;

IRQ_FAILED:
	device_destroy(genip_global->class, tes_dev->base_dev->devt);
DEVICE_CREATE_FAILED:
IOREMAP_FAILED:
UNSUPPORTED:
ALLOC_MEM_FAILED:
	genip_global->dev_count--; // this was calculated at the start of the function

	return result;
}

static int genip_remove(struct platform_device *pdev) {
	struct genip_device *tes_dev = platform_get_drvdata(pdev);

	device_destroy(genip_global->class, tes_dev->base_dev->devt);
	return 0;
}

static int __init _genip_init(void) {
	int result = 0;
	dev_t first_dev_t;

	// alloc mem for global struct
	genip_global = kzalloc(sizeof(struct genip_global_t), GFP_KERNEL);
	if (!genip_global) {
		pr_err("Memory allocation for driver global struct failed!\n");
		result = -ENOMEM;
		goto ALLOC_GLOBAL_FAILED;
	}

	// create class
	genip_global->class = class_create(THIS_MODULE, GENIP_DEVCLASS_NAME);
	if (!genip_global->class) {
		pr_err("cannot create class %s\n", GENIP_DEVCLASS_NAME);
		result = -EBUSY;
		goto CLASS_FAILED;
	}

	// alloc chrdev region
	result = alloc_chrdev_region(&first_dev_t, 0, 1, GENIP_CHRDEV_NAME);
	genip_global->major = MAJOR(first_dev_t);
	if (result < 0) {
		pr_err("cannot allocate chrdev region:  %s\n", GENIP_CHRDEV_NAME);
		goto CHRDEV_FAILED;
	}

	// init & add chrdev
	cdev_init(&genip_global->cdev, &genip_fops);
	genip_global->cdev.owner = THIS_MODULE;
	result = cdev_add(&genip_global->cdev, first_dev_t, GENIP_MAX_DEVICES);
	if (result) {
		pr_err("can't register cdev\n");
		goto CDEV_ADD_FAILED;
	}

	// register actual driver
	// this invokes a call to genip_probe for each ip core
	result = platform_driver_register(&genip_driver);
	if (result) {
		pr_err("failed to register platform driver for %s\n", GENIP_DRIVER_NAME);
		goto PLATFORM_REGISTER_FAILED;
	}

	pr_info("%s device driver successfully initialized!\n", GENIP_DRIVER_NAME);

	return 0;

PLATFORM_REGISTER_FAILED:
	cdev_del(&genip_global->cdev);
CDEV_ADD_FAILED:
	unregister_chrdev_region(first_dev_t, GENIP_MAX_DEVICES);
CHRDEV_FAILED:
	class_destroy(genip_global->class);
CLASS_FAILED:
ALLOC_GLOBAL_FAILED:
	return result;
}

static void __exit _genip_exit(void) {
	platform_driver_unregister(&genip_driver);
	class_destroy(genip_global->class);
	cdev_del(&genip_global->cdev);
	unregister_chrdev_region(MKDEV(genip_global->major, 0), GENIP_MAX_DEVICES);
	kfree(genip_global);
}

module_init(_genip_init);
module_exit(_genip_exit);
MODULE_AUTHOR("TES Electronic Solutions GmbH");
MODULE_DESCRIPTION("Kernel driver for all TES IP cores");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, genip_of_ids);
