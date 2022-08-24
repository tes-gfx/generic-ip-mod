#include "genip_devices.h"
#include "genip_io.h"
#include "genip_driver.h"

void generic_clear_irq(struct genip_device *gdev, uint32_t irq_status)
{
	genip_write_reg(gdev->mmio, gdev->platform_data->irq_clear_reg, irq_status);
}

void d2d_clear_irq(struct genip_device *gdev, uint32_t irq_status)
{
    union {
        uint32_t value;
        struct {
            unsigned int gap : 4;
            unsigned int irq_enum : 1;
            unsigned int irq_dlist : 1;
            unsigned int irq_bus : 1;
        } bits;
    } status_reg;

    union {
        uint32_t value;
        struct {
            unsigned int enable_enum : 1;
            unsigned int enable_dlist : 1;
            unsigned int clr_enum : 1;
            unsigned int clr_dlist : 1;
            unsigned int enable_bus : 1;
            unsigned int clr_bus : 1;
        } bits;
    } clear_reg;

    status_reg.value = irq_status;
    clear_reg.bits.enable_enum = 0;
    clear_reg.bits.enable_bus = 1;
    clear_reg.bits.enable_dlist = 1;
    clear_reg.bits.clr_enum = status_reg.bits.irq_enum;
    clear_reg.bits.clr_dlist = status_reg.bits.irq_dlist;
    clear_reg.bits.clr_bus = status_reg.bits.irq_bus;

	genip_write_reg(gdev->mmio, gdev->platform_data->irq_clear_reg, clear_reg.value);
}

const struct genip_platform_data genip_cdc_pdata = {
	.fs_dev_name = "cdc",
	.irq_name = "cdc_irq",
	.version_reg = 0,
	.version_reg_mask = 0xffffff00,
	.version_reg_expected = 0x00040000,
	.irq_status_reg = 0x0e,
	.irq_clear_reg = 0x0f,
	.irq_clear_func = generic_clear_irq,};

const struct genip_platform_data genip_dhd_pdata = {
	.fs_dev_name = "dhd",
	.irq_name = "dhd_irq",
	.version_reg = 0,
	/* todo: reg mask and expected value */
	.irq_status_reg = 0x06,
	.irq_clear_reg = 0x06,
	.irq_clear_func = generic_clear_irq,};

const struct genip_platform_data genip_warp_pdata = {
	.fs_dev_name = "warp",
	.irq_name = "warp_irq",
	.version_reg = 0x0,
	.irq_status_reg = 0x11,
	.irq_clear_reg = 0x12,
	.irq_clear_func = generic_clear_irq,};

const struct genip_platform_data genip_d2d_pdata = {
	.fs_dev_name = "d2d",
	.irq_name = "d2d_irq",
	.version_reg = 0,
	/* todo: reg mask and expected value */
	.irq_status_reg = 0x00,
	.irq_clear_reg = 0x30,
	.irq_clear_func = d2d_clear_irq,}; //TODO IRQ status reg