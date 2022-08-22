#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>

#include "tes_ipcore_driver.h"
#include "tes_ipcore_io.h"
#include "tes_ipcore_module.h"

static int tes_ipcore_open(struct inode *ip, struct file *fp);
static long tes_ipcore_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static ssize_t tes_ipcore_write(struct file *file, const char __user *user_input, size_t count, loff_t *offset);
static ssize_t tes_ipcore_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);

struct file_operations tes_ipcore_fops = {
	.owner = THIS_MODULE,
	.open = tes_ipcore_open,
	.unlocked_ioctl = tes_ipcore_ioctl,
	.read = tes_ipcore_read,
	.write = tes_ipcore_write,
};

uint32_t tes_ipcore_read_reg(void *reg_base_virt, uint32_t reg_id) {
	// shift == *4 == conversion from register ids aka word addresses to byte addresses
	uint32_t value = ioread32((void *)((size_t)reg_base_virt + (reg_id << 2)));
	return value;
}
void tes_ipcore_write_reg(void *reg_base_virt, uint32_t reg_id, uint32_t value) {
	// shift == *4 == conversion from register ids aka word addresses to byte addresses
	iowrite32(value, (void *)((size_t)reg_base_virt + (reg_id << 2)));
}

static int tes_ipcore_open(struct inode *ip, struct file *fp) {
	struct tes_ipcore_device *tes_device;
	int minor = iminor(ip);

	tes_device = tes_ipcore_global->device_by_minor[minor];

	dev_dbg(tes_device->base_dev, "Device file opened!\n");
	fp->private_data = tes_device;

	return 0;
}

static long tes_ipcore_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	struct tes_ipcore_device *tes_dev = fp->private_data;
	unsigned int cmd_nr;
	struct tes_ipcore_settings ipcore_settings;
	uint32_t *argptr = (uint32_t *)arg;
	uint32_t tmp;

	static size_t current_working_reg;

	//pr_info("IOCRTL: cmd: 0x%08x; arg: 0x%08lx", cmd, arg);

	cmd_nr = _IOC_NR(cmd);
	if (_IOC_TYPE(cmd) != TES_IPCORE_IOCTL_TYPE) {
		pr_warn("WARNING: tes-ipcore ioctl called with wrong type! expected: %c; actual %c\n", TES_IPCORE_IOCTL_TYPE, _IOC_TYPE(cmd));
		pr_warn("no action will be performed!\n");
		return -EINVAL;
	}
	if (_IOC_DIR(cmd) == _IOC_WRITE) {
		switch (cmd_nr) {
			// write to a register
		case TES_IPCORE_IOCTL_NR_REG_ACCESS:
			tes_ipcore_write_reg(tes_dev->mmio, current_working_reg, arg);
			break;

			// set the working register
		case TES_IPCORE_IOCTL_NR_WORKING_REG:
			if (arg > tes_dev->span) {
				return -EINVAL;
			} else {
				current_working_reg = arg;
			}
			break;
		default:
			return -EINVAL;
		}
		return 0;
	} else if (_IOC_DIR(cmd) == _IOC_READ) {
		switch (cmd_nr) {
			// read from a register
		case TES_IPCORE_IOCTL_NR_REG_ACCESS:
			tmp = tes_ipcore_read_reg(tes_dev->mmio, current_working_reg);
			if (!put_user(tmp, argptr))
				return -EFAULT;
			break;

			// retrieve the register memory settings
		case TES_IPCORE_IOCTL_NR_SETTINGS:
			ipcore_settings.base_phys = tes_dev->base_phys;
			ipcore_settings.span = tes_dev->span;

			if (copy_to_user((void *)arg, (void *)&ipcore_settings,
							 sizeof(struct tes_ipcore_settings))) {
				dev_err(fp->private_data, "error while copying settings to user space\n");
				return -EFAULT;
			}
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
}

static ssize_t tes_ipcore_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	struct tes_ipcore_device *dev = filp->private_data;
	unsigned long flags;
	int temp;

	pr_info("tes_ipcore_read called\n");

	wait_event_interruptible(dev->irq_waitq, dev->irq_stat);

	spin_lock_irqsave(&dev->irq_slck, flags);
	temp = dev->irq_stat;
	dev->irq_stat = 0;
	spin_unlock_irqrestore(&dev->irq_slck, flags);

	if (count == sizeof(temp)) {
		put_user(temp, buff);
		return sizeof(temp);
	}

	return 0;
}
static ssize_t tes_ipcore_write(struct file *file, const char __user *user_input, size_t count, loff_t *offset) {
	struct tes_ipcore_device *ipcore_dev = file->private_data;
	char buf[33];
	size_t left_to_copy = count;
	size_t tocopy = 0;
	size_t actually_copied = 0;
	size_t result = 0;
	const char __user *curr_user_input = user_input;

	dev_warn(ipcore_dev->base_dev, 
			 "This driver does not have a functional interface via a file write.\n"
			 "Please use iocrtl to read/write; and a file read to wait for an IRQ!\n"
			 "This is for debug purposes only; the following data was sent:\n");
	while (left_to_copy > 0) {
		tocopy = (left_to_copy < 32) ? left_to_copy : 32;
		result = copy_from_user(buf, curr_user_input, tocopy);
		if (result > 0) { 
			dev_warn(ipcore_dev->base_dev, "Could not copy %d bytes from user!", result);
		}
		actually_copied = tocopy - result;
		buf[tocopy] = 0;
		dev_info(ipcore_dev->base_dev, "Data from user: %s", buf);
		left_to_copy -= actually_copied;
		curr_user_input += actually_copied;
	}

	return count;
}

irqreturn_t tes_ipcore_irq_handler(int irq, void *dev_raw) {
	unsigned long flags;
	struct tes_ipcore_device *dev = dev_raw;

	int irq_status = tes_ipcore_read_reg(dev->mmio, dev->platform_data->irq_status_reg); 
	tes_ipcore_write_reg(dev->mmio, dev->platform_data->irq_status_reg, irq_status);		

	spin_lock_irqsave(&dev->irq_slck, flags);
	dev->irq_stat |= irq_status;
	spin_unlock_irqrestore(&dev->irq_slck, flags);

	wake_up_interruptible(&dev->irq_waitq);

	return IRQ_HANDLED;
}
