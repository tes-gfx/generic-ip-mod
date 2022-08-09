#ifndef TES_IPCORE_FOPS_H
#define TES_IPCORE_FOPS_H

#include <linux/interrupt.h>
#include <linux/types.h>

/**
 * File operations and interrupt handeling
 */

extern struct file_operations tes_ipcore_fops;

irqreturn_t tes_ipcore_irq_handler(int irq, void *dev_raw);
uint32_t tes_ipcore_read_reg(void* reg_base_virt, uint32_t reg_id);
void tes_ipcore_write_reg(void* reg_base_virt, uint32_t reg_id, uint32_t value);

#endif // TES_IPCORE_FOPS_H