
#include <linux/module.h>



MODULE_AUTHOR("akozlins");
MODULE_LICENSE("GPL");

#define DRIVER_NAME "dmabuf"
#define DEVICE_NAME "dmabuf"



#include "dmabuf_platform_device.h"

static struct platform_device* dmabuf_platform_device = NULL;



static
int __init dmabuf_module_init(void) {
    int error = 0;

    dmabuf_platform_device = dmabuf_platform_device_create(DEVICE_NAME);
    if(IS_ERR_OR_NULL(dmabuf_platform_device)) {
        error = PTR_ERR(dmabuf_platform_device);
        dmabuf_platform_device = NULL;
        return error;
    }

    return 0;
}

static
void __exit dmabuf_module_exit(void) {
    dmabuf_platform_device_delete(dmabuf_platform_device);
}

module_init(dmabuf_module_init);
module_exit(dmabuf_module_exit);
