
#include <linux/module.h>

MODULE_AUTHOR("akozlins");
MODULE_LICENSE("GPL");

#include "dmabuf_platform_device.h"
static struct platform_device* dmabuf_platform_device = NULL;

#include "dmabuf_fops.h"
static struct chrdev* chrdev = NULL;
static DEFINE_IDA(chrdev_ida);

#include "dmabuf_platform_driver.h"

static
int __init dmabuf_module_init(void) {
    int error;

    M_INFO("\n");

    chrdev = chrdev_alloc(THIS_MODULE->name, 2);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        chrdev = NULL;
        M_ERR("chrdev_alloc: error = %d\n", error);
        goto err_out;
    }

    error = platform_driver_register(&dmabuf_platform_driver);
    if(error) {
        M_ERR("platform_driver_register: error = %d\n", error);
        goto err_out;
    }

    dmabuf_platform_device = dmabuf_platform_device_register(THIS_MODULE->name);
    if(IS_ERR_OR_NULL(dmabuf_platform_device)) {
        error = PTR_ERR(dmabuf_platform_device);
        dmabuf_platform_device = NULL;
        goto err_out;
    }

    return 0;

err_out:
    chrdev_free(chrdev);
    return error;
}

static
void __exit dmabuf_module_exit(void) {
    M_INFO("\n");

    platform_device_unregister(dmabuf_platform_device);
    platform_driver_unregister(&dmabuf_platform_driver);
    chrdev_free(chrdev);
}

module_init(dmabuf_module_init);
module_exit(dmabuf_module_exit);
