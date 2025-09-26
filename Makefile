ifneq ($(KERNELRELEASE),)
gfn_to_pfn-y := gfn_module.o gfn_parse.o
obj-m := gfn_to_pfn.o
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif
