
#include <linux/module.h>



MODULE_AUTHOR("akozlins");
MODULE_LICENSE("GPL");

#define DRIVER_NAME "dmabuf"
#define DEVICE_NAME "dmabuf"



static
int __init dmabuf_module_init(void) {
    return 0;
}

static
void __exit dmabuf_module_exit(void) {
}

module_init(dmabuf_module_init);
module_exit(dmabuf_module_exit);
