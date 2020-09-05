
#include <linux/module.h>

MODULE_AUTHOR("akozlins");
MODULE_LICENSE("GPL");

#include "dmabuf_platform_device.h"
#include "dmabuf_platform_driver.h"

static struct platform_device* dmabuf_platform_device = NULL;

static
int __init dmabuf_module_init(void) {
    long error = 0;

    M_INFO("\n");

    error = platform_driver_register(&dmabuf_platform_driver);
    if(error) {
        M_ERR("platform_driver_register: error = %ld\n", error);
    }

    dmabuf_platform_device = dmabuf_platform_device_register(THIS_MODULE->name);
    if(IS_ERR_OR_NULL(dmabuf_platform_device)) {
        error = PTR_ERR(dmabuf_platform_device);
        dmabuf_platform_device = NULL;
        return error;
    }

    return 0;
}

static
void __exit dmabuf_module_exit(void) {
    M_INFO("\n");

    platform_device_unregister(dmabuf_platform_device);
    platform_driver_unregister(&dmabuf_platform_driver);
}

module_init(dmabuf_module_init);
module_exit(dmabuf_module_exit);
