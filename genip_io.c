#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>

#include "genip_driver.h"
#include "genip_devices.h"
#include "genip_io.h"
#include "genip_module.h"

static int genip_open(struct inode *ip, struct file *fp);
static long genip_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static ssize_t genip_write(struct file *file, const char __user *user_input, size_t count, loff_t *offset);
static ssize_t genip_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);

struct file_operations genip_fops = {
	.owner = THIS_MODULE,
	.open = genip_open,
	.unlocked_ioctl = genip_ioctl,
	.read = genip_read,
	.write = genip_write,
};

uint32_t genip_read_reg(void *reg_base_virt, uint32_t reg_id) {
	// shift == *4 == conversion from register ids aka word addresses to byte addresses
	uint32_t value = ioread32((void *)((size_t)reg_base_virt + (reg_id << 2)));
	return value;
}

void genip_write_reg(void *reg_base_virt, uint32_t reg_id, uint32_t value) {
	// shift == *4 == conversion from register ids aka word addresses to byte addresses
	iowrite32(value, (void *)((size_t)reg_base_virt + (reg_id << 2)));
}

static int genip_open(struct inode *ip, struct file *fp) {
	struct genip_device *tes_device;
	int minor = iminor(ip);

	tes_device = genip_global->device_by_minor[minor];

	dev_dbg(tes_device->base_dev, "Device file opened!\n");
	fp->private_data = tes_device;

	return 0;
}

static long genip_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
	struct genip_device *gdev = fp->private_data;
	unsigned int cmd_nr;
	struct genip_reg_access ipcore_reg_access;
	struct genip_settings ipcore_settings;
	struct genip_stream_dev stream_dev[GENIP_MAX_STREAMS];
	int str_idx = 0;
	int str_count = 0;
	size_t cpy_size = 0;

	cmd_nr = _IOC_NR(cmd);
	if (_IOC_TYPE(cmd) != GENIP_IOCTL_TYPE) {
		pr_warn("WARNING: tes-ipcore ioctl called with wrong type! expected: %c; actual %c\n", GENIP_IOCTL_TYPE, _IOC_TYPE(cmd));
		pr_warn("no action will be performed!\n");
		return -EINVAL;
	}

	switch (cmd_nr) {
		// write to a register
		case GENIP_IOCTL_NR_REG_WRITE:
			if (copy_from_user(&ipcore_reg_access, (struct ipcore_reg_access __user *)arg,
					sizeof(ipcore_reg_access)))
				return -EFAULT;
			genip_write_reg(gdev->mmio, ipcore_reg_access.offset,
					ipcore_reg_access.value);
			break;

		// read from a register
		case GENIP_IOCTL_NR_REG_READ:
			if (copy_from_user(&ipcore_reg_access, (struct ipcore_reg_access __user *)arg,
					sizeof(ipcore_reg_access)))
				return -EFAULT;

			ipcore_reg_access.value = genip_read_reg(gdev->mmio, ipcore_reg_access.offset);
			if (copy_to_user((struct ipcore_reg_access __user *)arg,
					&ipcore_reg_access, sizeof(ipcore_reg_access)))
				return -EFAULT;
			break;

		// retrieve the register memory settings
		case GENIP_IOCTL_NR_SETTINGS:
			ipcore_settings.base_phys = gdev->base_phys;
			ipcore_settings.span = gdev->span;

			if (copy_to_user((void *)arg, (void *)&ipcore_settings,
								sizeof(struct genip_settings))) {
				dev_err(fp->private_data, "error while copying settings to user space\n");
				return -EFAULT;
			}
			break;

		// return connected streaming device
		case GENIP_IOCTL_NR_STREAM_DEV:
			while (str_idx < gdev->connected_stream_dev_count) {
				strncpy(stream_dev[str_idx].dev_name, gdev->stream_dev[str_idx]->dev_name, DEV_NAME_MAX_LEN);
				stream_dev[str_idx].layer = gdev->stream_dev[str_idx]->layer;
				cpy_size += sizeof(struct genip_stream_dev);
				str_idx++;
			}

			if (copy_to_user((struct genip_stream_dev *)arg, (struct genip_stream_dev *)stream_dev, cpy_size)) {
				dev_err(fp->private_data, "error while copying device name to user space\n");
				return -EFAULT;
			}
			break;

		// return the count of connected streaming device
		case GENIP_IOCTL_NR_STREAM_DEV_COUNT:
			str_count = gdev->connected_stream_dev_count;

			if (copy_to_user((int *)arg, (int *)&str_count, sizeof(int))) {
				dev_err(fp->private_data, "error while copying device name to user space\n");
				return -EFAULT;
			}
			break;

		default:
			return -EINVAL;
	}
	return 0;
}

static ssize_t genip_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	struct genip_device *dev = filp->private_data;
	unsigned long flags;
	int temp;

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
static ssize_t genip_write(struct file *file, const char __user *user_input, size_t count, loff_t *offset) {
	struct genip_device *gdev = file->private_data;
	char buf[33];
	size_t left_to_copy = count;
	size_t tocopy = 0;
	size_t actually_copied = 0;
	size_t result = 0;
	const char __user *curr_user_input = user_input;

	dev_warn(gdev->base_dev, 
			 "This driver does not have a functional interface via a file write.\n"
			 "Please use iocrtl to read/write; and a file read to wait for an IRQ!\n"
			 "This is for debug purposes only; the following data was sent:\n");
	while (left_to_copy > 0) {
		tocopy = (left_to_copy < 32) ? left_to_copy : 32;
		result = copy_from_user(buf, curr_user_input, tocopy);
		if (result > 0) { 
			dev_warn(gdev->base_dev, "Could not copy %d bytes from user!", result);
		}
		actually_copied = tocopy - result;
		buf[tocopy] = 0;
		dev_info(gdev->base_dev, "Data from user: %s", buf);
		left_to_copy -= actually_copied;
		curr_user_input += actually_copied;
	}

	return count;
}

irqreturn_t genip_irq_handler(int irq, void *dev_raw) {
	unsigned long flags;
	struct genip_device *dev = dev_raw;

	uint32_t irq_status = genip_read_reg(dev->mmio, dev->platform_data->irq_status_reg); 
	dev->platform_data->irq_clear_func(dev, irq_status);

	spin_lock_irqsave(&dev->irq_slck, flags);
	dev->irq_stat |= irq_status;
	spin_unlock_irqrestore(&dev->irq_slck, flags);

	wake_up_interruptible(&dev->irq_waitq);

	return IRQ_HANDLED;
}
