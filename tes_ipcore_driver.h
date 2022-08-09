#ifndef __TES_IPCORE_H__
#define __TES_IPCORE_H__

#include <linux/device.h>
#include <linux/platform_device.h>

#include "tes_ipcore_module.h"

struct tes_ipcore_device { // TODO replace unsigned long with u32 etc (maybe dont????); whatever, CHECK ALL DATA TYPES!!!
	unsigned long base_phys;
	unsigned long span;
	void __iomem *mmio;
	unsigned int irq_no;
	unsigned int irq_stat;
	spinlock_t irq_slck;
	wait_queue_head_t irq_waitq;
	dev_t dev_t;
	struct device *base_dev;
	const struct tes_ipcore_platform_data *platform_data;
};

struct tes_ipcore_global_t {
	struct cdev cdev;
	struct tes_ipcore_device *device_by_minor[TES_IPCORE_MAX_DEVICES];
	struct class *class;
	int major;
	int dev_count;
};
extern struct tes_ipcore_global_t *tes_ipcore_global; 

#endif
