#ifndef TES_IPCORE_MODULE_H
#define TES_IPCORE_MODULE_H

// make the IOW macros work *properly* without including linux/x.h in userspace
// this file can be included in either kernel or userspace code
#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/ioctl.h>
#endif //__KERNEL__

/*
 * IOCTL configuration & commands
 */

// ioctl type; this doesnt really matter. Apperantly 't' is used rarely
#define TES_IPCORE_IOCTL_TYPE 't'
//#define TES_IPCORE_IOCTL_REG_PREFIX	  (0x80)
// Copy Settings from device tree to userspace
#define TES_IPCORE_IOCTL_NR_SETTINGS (0x01)
// set the current working register; a register read/write will end up there
#define TES_IPCORE_IOCTL_NR_WORKING_REG (0x02)
// one number for read and write, since that is determined by _IOW / _IOR
#define TES_IPCORE_IOCTL_NR_REG_ACCESS (0x03)

// argument = pointer to a new register id
#define TES_IPCORE_IOCTL_SET_REG (_IOW(TES_IPCORE_IOCTL_TYPE, TES_IPCORE_IOCTL_NR_WORKING_REG, uint32_t))
// argument = pointer to a register id
#define TES_IPCORE_IOCTL_W (_IOW(TES_IPCORE_IOCTL_TYPE, TES_IPCORE_IOCTL_NR_REG_ACCESS, uint32_t))
// argument = pointer to a uint32_t to save the read value
#define TES_IPCORE_IOCTL_R (_IOR(TES_IPCORE_IOCTL_TYPE, TES_IPCORE_IOCTL_NR_REG_ACCESS, uint32_t))
// argument = pointer to a tes_ipcore_settings to write the information to
#define TES_IPCORE_IOCTL_GET_SETTINGS (_IOR(TES_IPCORE_IOCTL_TYPE, TES_IPCORE_IOCTL_NR_SETTINGS, tes_ipcore_settings))

/*
 * char device config & device tree matching
 */

#define TES_IPCORE_DRIVER_NAME "tes-ipcore" //basic driver name
#define TES_IPCORE_MAX_DEVICES 15 //number of maximum supported IPs (total)
#define TES_IPCORE_DEVCLASS_NAME "tes-ipcore-class" //device class name
#define TES_IPCORE_CHRDEV_NAME "tes-ipcores" //chrdev region name

// struct to hold data specific to each IP that is compatible with this driver
struct tes_ipcore_platform_data {
	// The name of the device created in /dev/
	const char *fs_dev_name;
	// the id of the version register in hardware
	const uint32_t version_reg;
	// the expected value of that register
	const uint32_t version_reg_expected;
	// mask for the relevant bits of the version register
	const uint32_t version_reg_mask;
	// the id of the IRQ status register
	const uint32_t irq_status_reg;
	// the name of the irq for that device
	const char *irq_name;
};

#define TES_IPCORE_COMPAT_STR_CDC "tes,cdc-2.1"
const static struct tes_ipcore_platform_data tes_ipcore_cdc_pdata = {
	.fs_dev_name = "cdc",
	.irq_name = "cdc_irq",
	.version_reg = 0,
	.version_reg_mask = 0xffffff00,
	.version_reg_expected = 0x00040000,
	.irq_status_reg = 0};//TODO IRQ status reg

#define TES_IPCORE_COMPAT_STR_DHD "tes,dhd-1.0"
const static struct tes_ipcore_platform_data tes_ipcore_dhd_pdata = {
	.fs_dev_name = "dhd",
	.irq_name = "dhd_irq",
	.version_reg = 0,
	/* todo: reg mask and expected value */
	.irq_status_reg = 0}; //TODO IRQ status reg

#define TES_IPCORE_COMPAT_STR_WARP "tes,warp-1.0"
const static struct tes_ipcore_platform_data tes_ipcore_warp_pdata = {
	.fs_dev_name = "warp",
	.irq_name = "warp_irq",
	.version_reg = 0x0,
	.irq_status_reg = 0x11};

#define TES_IPCORE_COMPAT_STR_D2D "tes,d2d-1.0"
const static struct tes_ipcore_platform_data tes_ipcore_d2d_pdata = {
	.fs_dev_name = "d2d",
	.irq_name = "d2d_irq",
	.version_reg = 0,
	/* todo: reg mask and expected value */
	.irq_status_reg = 0}; //TODO IRQ status reg

// physical resource information
// this information is supplied by the device tree
// this struct is used to send the data to userspace
struct tes_ipcore_settings {
	size_t base_phys; // start address of the registers
	size_t span;	  // size of the register area
};

#endif // TES_IPCORE_MODULE_H
