
#include <linux/platform_device.h>

static
struct platform_device* dmabuf_platform_device_register(const char* name) {
    int error = 0;
    struct platform_device* pdev = NULL;

    // TODO: use platform_device_register_simple

    pdev = platform_device_alloc(name, -1);
    if(IS_ERR_OR_NULL(pdev)) {
        error = PTR_ERR(pdev);
        pdev = NULL;
        pr_err("[%s/%s] platform_device_alloc: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    error = platform_device_add(pdev);
    if(error) {
        pr_err("[%s/%s] platform_device_alloc: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_pdev_put;
    }

    return pdev;

err_pdev_put:
    platform_device_put(pdev);
err_out:
    return ERR_PTR(error);
}
