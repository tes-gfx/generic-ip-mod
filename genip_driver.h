#ifndef __GENIP_H__
#define __GENIP_H__

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "genip_module.h"

struct genip_platform_data;

struct genip_device { // TODO replace unsigned long with u32 etc (maybe dont????); whatever, CHECK ALL DATA TYPES!!!
	unsigned long base_phys;
	unsigned long span;
	void __iomem *mmio;
	unsigned int irq_no;
	uint32_t irq_stat; /* contents of IRQ status register */
	spinlock_t irq_slck;
	wait_queue_head_t irq_waitq;
	dev_t dev_t;
	struct device *base_dev;
	const struct genip_platform_data *platform_data;
};

struct genip_global_t {
	struct cdev cdev;
	struct genip_device *device_by_minor[GENIP_MAX_DEVICES];
	struct class *class;
	int major;
	int dev_count;
};
extern struct genip_global_t *genip_global; 

#endif
