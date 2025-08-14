obj-m += kthread_blink_nodt.o
obj-m += timer_blink_nodt.o
obj-m += hrtimer_blink_char_nodt.o
obj-m += blink_ctrl_ioctl.o


# Tell Kbuild where the kernel source/headers are.
# Default tries the running kernel; override with “make KDIR=/path ...”
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# The “modules” target asks the kernel build system to compile our obj-m list.
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
