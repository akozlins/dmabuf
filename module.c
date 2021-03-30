
#include <linux/module.h>

MODULE_AUTHOR("akozlins");
MODULE_LICENSE("GPL");

#include "dmabuf_platform_device.h"
#include "dmabuf_platform_driver.h"

static struct platform_device* dmabuf_platform_device = NULL;

static
ssize_t test_show(struct device_driver* device_driver, char* page) {
    return scnprintf(page, PAGE_SIZE, "test_attribute\n");
}
static
ssize_t test_store(struct device_driver* device_driver, const char* page, size_t size) {
    return size;
}
static DRIVER_ATTR_RW(test);

static
int __init dmabuf_module_init(void) {
    int error;

    M_INFO("\n");

    error = platform_driver_register(&dmabuf_platform_driver);
    if(error) {
        M_ERR("platform_driver_register: error = %d\n", error);
        goto err_out;
    }

    driver_create_file(&dmabuf_platform_driver.driver, &driver_attr_test);

    dmabuf_platform_device = dmabuf_platform_device_register(THIS_MODULE->name);
    if(IS_ERR_OR_NULL(dmabuf_platform_device)) {
        error = PTR_ERR(dmabuf_platform_device);
        dmabuf_platform_device = NULL;
        goto err_out;
    }

    return 0;

err_out:
    return error;
}

static
void __exit dmabuf_module_exit(void) {
    M_INFO("\n");

    platform_device_unregister(dmabuf_platform_device);
    platform_driver_unregister(&dmabuf_platform_driver);
}

module_init(dmabuf_module_init);
module_exit(dmabuf_module_exit);
