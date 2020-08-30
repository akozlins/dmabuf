
KDIR = /lib/modules/`uname -r`/build

all : .cache
	$(MAKE) -C $(KDIR) modules M=$(PWD)/.cache src=$(PWD)

clean : .cache
	$(MAKE) -C $(KDIR) clean M=$(PWD)/.cache src=$(PWD)

insmod : | all rmmod
	sudo insmod .cache/$(MODULE_NAME).ko

rmmod :
	sudo rmmod $(MODULE_NAME) || true

.cache :
	mkdir -p .cache
