
MODULE_NAME := dmabuf

KDIR := /lib/modules/`uname -r`/build

all : .cache
	$(MAKE) -C $(KDIR) modules M=$(PWD)/.cache src=$(PWD) MODULE_NAME=$(MODULE_NAME)

clean : .cache
	$(MAKE) -C $(KDIR) clean M=$(PWD)/.cache src=$(PWD) MODULE_NAME=$(MODULE_NAME)

insmod : | all rmmod
	sudo insmod .cache/$(MODULE_NAME).ko
#	$(MAKE) -C $(KDIR) M=$(PWD)/.cache modules_install

rmmod :
	sudo rmmod $(MODULE_NAME) || true

.cache :
	mkdir -p .cache
	cp Kbuild .cache/
