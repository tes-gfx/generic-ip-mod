#ifndef __GENIP_DEVICES__
#define __GENIP_DEVICES__

#include "genip_driver.h"
#include "genip_io.h"

#define GENIP_COMPAT_STR_CDC "tes,cdc-2.1"
#define GENIP_COMPAT_STR_DHD "tes,dhd-1.0"
#define GENIP_COMPAT_STR_WARP "tes,warp-1.0"
#define GENIP_COMPAT_STR_D2D "tes,d2d-1.0"
#define GENIP_COMPAT_STR_FBD "tes,fbd-1.0"
#define GENIP_COMPAT_STR_DSW "tes,dsw-1.0"

// struct to hold data specific to each IP that is compatible with this driver
struct genip_platform_data {
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
	// the id of the IRQ clear register
	const uint32_t irq_clear_reg;
	// irq clear function
	void(*irq_clear_func)(struct genip_device*, uint32_t);
	// the name of the irq for that device
	const char *irq_name;
};

extern const struct genip_platform_data genip_cdc_pdata;
extern const struct genip_platform_data genip_dhd_pdata;
extern const struct genip_platform_data genip_warp_pdata;
extern const struct genip_platform_data genip_d2d_pdata;
extern const struct genip_platform_data genip_fbd_pdata;
extern const struct genip_platform_data genip_dsw_pdata;

#endif /* __GENIP_DEVICES__ */