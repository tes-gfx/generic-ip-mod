#ifndef GENIP_FOPS_H
#define GENIP_FOPS_H

#include <linux/interrupt.h>
#include <linux/types.h>

/**
 * File operations and interrupt handeling
 */

extern struct file_operations genip_fops;

irqreturn_t genip_irq_handler(int irq, void *dev_raw);
uint32_t genip_read_reg(void* reg_base_virt, uint32_t reg_id);
void genip_write_reg(void* reg_base_virt, uint32_t reg_id, uint32_t value);

#endif // GENIP_FOPS_H