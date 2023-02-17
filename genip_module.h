#ifndef GENIP_MODULE_H
#define GENIP_MODULE_H

// make the IOW macros work *properly* without including linux/x.h in userspace
// this file can be included in either kernel or userspace code
#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <linux/types.h>
#endif //__KERNEL__

/*
 * IOCTL configuration & commands
 */

// ioctl type; this doesnt really matter. Apperantly 't' is used rarely
#define GENIP_IOCTL_TYPE 't'
//#define GENIP_IOCTL_REG_PREFIX	  (0x80)
// Copy Settings from device tree to userspace
#define GENIP_IOCTL_NR_SETTINGS (0x01)
// write a register
#define GENIP_IOCTL_NR_REG_WRITE (0x02)
// read a register
#define GENIP_IOCTL_NR_REG_READ (0x03)
// get stream device struct
#define GENIP_IOCTL_NR_STREAM_DEV (0x04)
// get stream device count
#define GENIP_IOCTL_NR_STREAM_DEV_COUNT (0x05)

// argument = pointer to genip_reg_access
#define GENIP_IOCTL_W (_IOW(GENIP_IOCTL_TYPE, GENIP_IOCTL_NR_REG_WRITE, struct genip_reg_access))
// argument = pointer to genip_reg_access
#define GENIP_IOCTL_R (_IOWR(GENIP_IOCTL_TYPE, GENIP_IOCTL_NR_REG_READ, struct genip_reg_access))
// argument = pointer to a genip_settings to write the information to
#define GENIP_IOCTL_GET_SETTINGS (_IOR(GENIP_IOCTL_TYPE, GENIP_IOCTL_NR_SETTINGS, struct genip_settings))
// argument = pointer to write the structures of a connected streaming devices to
#define GENIP_IOCTL_GET_STREAM_DEV (_IOR(GENIP_IOCTL_TYPE, GENIP_IOCTL_NR_STREAM_DEV, struct genip_stream_dev *))
// argument = pointer to write the count of a connected streaming device to
#define GENIP_IOCTL_GET_STREAM_DEV_COUNT (_IOR(GENIP_IOCTL_TYPE, GENIP_IOCTL_NR_STREAM_DEV_COUNT, int))

/*
 * char device config & device tree matching
 */

#define GENIP_DRIVER_NAME "tes-ipcore" //basic driver name
#define GENIP_MAX_DEVICES 15 //number of maximum supported IPs (total)
#define GENIP_DEVCLASS_NAME "tes-ipcore-class" //device class name
#define GENIP_CHRDEV_NAME "tes-ipcores" //chrdev region name
#define GENIP_MAX_STREAMS 2 // number of max supported streaming devices (total)
#define DEV_NAME_MAX_LEN 20 // length of device name

// physical resource information
// this information is supplied by the device tree
// this struct is used to send the data to userspace
struct genip_settings {
	size_t base_phys; // start address of the registers
	size_t span;	  // size of the register area
};

/* This struct is used for reading/writing registers.
   Use u64 instead of pointer to stay compatible with 32/64 bit systems. */
struct genip_reg_access {
	__u64 offset;         /* in, register ID to read from / write to */
	__u32 value;          /* in/out, register value read or written */
};

/* This structure is used for working with connected streaming devices */
struct genip_stream_dev {
	char dev_name[DEV_NAME_MAX_LEN];
	int layer;
};

#endif // GENIP_MODULE_H
