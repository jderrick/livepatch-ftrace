ifneq ($(KERNELRELEASE),)
nvme-trace-y += nvme_trace.o
nvme-trace-y += override.o
ccflags-y +=-Wfatal-errors
obj-m	+= nvme-trace.o

else
KBUILD_CFLAGS+= " -Wfatal-errors"
KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: default clean localclean install

all:	default

default:
	$(MAKE) -C $(KDIR) M=$(shell pwd) modules

clean:	localclean
	$(MAKE) -C $(KDIR) M=$(shell pwd) clean

localclean:
	rm -rvf *.mod.* *.mod *.o *.ko
	rm -vf Module.symvers modules.order

install:
	$(MAKE) -C $(KDIR) M=$(shell pwd) modules_install
endif
