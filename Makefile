KERNEL_SRC := $(SDKTARGETSYSROOT)/usr/src/kernel
MODULE_ARTIFACT := genip.ko

SRC = $(shell pwd)

.PHONY:
all: modules
		
.PHONY:
modules:
	make -C $(KERNEL_SRC) M=$(SRC) modules

.PHONY:
modules_install:
	make -C $(KERNEL_SRC) M=$(SRC) modules_install

.PHONY:
clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c *.a *.mod
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers

.PHONY:
deploy: all
#	scp $(MODULE_ARTIFACT) root@$(BOARD_IP):/lib/modules/5.4.124-altera/extra
	scp $(MODULE_ARTIFACT) root@$(BOARD_IP):/home/root
