
MODULE_NAME := dmabuf

obj-m += $(MODULE_NAME).o
$(MODULE_NAME)-objs := module.o

ccflags-y := -std=gnu11 -Wall -g
KERNEL = /lib/modules/`uname -r`/build

all :
	make -C $(KERNEL) M=$(PWD) modules

clean :
	make -C $(KERNEL) M=$(PWD) clean

insmod : all rmmod
	sudo insmod ./$(MODULE_NAME).ko

rmmod :
	sudo rmmod $(MODULE_NAME) || true
