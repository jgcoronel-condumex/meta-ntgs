# Kbuild makefile for a single-file, out-of-tree kernel module
obj-m := my_driver.o          # name of the *.o built from my_driver.c

# (Optional) If your module were split in several .c files:
# my_driver-objs := core.o dma.o irq.o

# Tell Kbuild where the kernel source/headers are.
# Default tries the running kernel; override with “make KDIR=/path ...”
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# The “modules” target asks the kernel build system to compile our obj-m list.
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
