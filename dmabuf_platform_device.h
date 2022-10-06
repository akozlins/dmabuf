/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AKOZLINS_DMABUF_PLATFORM_DEVICE_H__
#define __AKOZLINS_DMABUF_PLATFORM_DEVICE_H__

#include "module.h"

#include <linux/platform_device.h>

static
struct platform_device* dmabuf_platform_device_register(const char* name) {
    int error;
    struct platform_device* pdev = NULL;

    // TODO: use platform_device_register_simple

    pdev = platform_device_alloc(name, -1);
    if(IS_ERR_OR_NULL(pdev)) {
        error = PTR_ERR(pdev);
        pdev = NULL;
        M_ERR("platform_device_alloc: error = %d\n", error);
        goto err_out;
    }

    error = platform_device_add(pdev);
    if(error) {
        M_ERR("platform_device_add: error = %d\n", error);
        goto err_pdev_put;
    }

    return pdev;

err_pdev_put:
    platform_device_put(pdev);
err_out:
    return ERR_PTR(error);
}

#endif // __AKOZLINS_DMABUF_PLATFORM_DEVICE_H__
