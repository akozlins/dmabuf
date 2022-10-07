/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AKOZLINS_DMABUF_PLATFORM_DRIVER_H
#define __AKOZLINS_DMABUF_PLATFORM_DRIVER_H

#include "dmabuf.h"

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

struct dmabuf_device {
    int id;
    char* name;
    struct dmabuf* dmabuf;
    struct miscdevice miscdevice;
};

static DEFINE_IDA(dmabuf_ida);

/**
 * \code
 * dmabuf_device = container_of(file->private_data)
 * file->private_data = dmabuf_device->dmabuf
 * \endcode
 */
static
int dmabuf_fops_open(struct inode* inode, struct file* file) {
    struct dmabuf_device* dmabuf_device;
    struct dmabuf* dmabuf;

    M_INFO("\n");

    dmabuf_device = container_of(file->private_data, struct dmabuf_device, miscdevice);
    if(dmabuf_device == NULL) {
        M_ERR("dmabuf_device == NULL\n");
        return -ENODEV;
    }

    dmabuf = dmabuf_device->dmabuf;
    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -ENODEV;
    }

    file->private_data = dmabuf;

    return 0;
}

#include "dmabuf_fops.h"

static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    int error;
    struct dmabuf_device* dmabuf_device = NULL;

    M_INFO("\n");

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error != 0) {
        M_ERR("dma_set_mask_and_coherent(mask = DMA_BIT_MASK(64)): error = %d\n", error);
        goto err_out;
    }

    dmabuf_device = kzalloc(sizeof(*dmabuf_device), GFP_KERNEL);
    if(IS_ERR_OR_NULL(dmabuf_device)) {
        error = PTR_ERR(dmabuf_device);
        if(error == 0) error = -ENOMEM;
        dmabuf_device = NULL;
        M_ERR("kzalloc(): error = %d\n", error);
        goto err_out;
    }

    dmabuf_device->id = ida_alloc(&dmabuf_ida, GFP_KERNEL);
    if(dmabuf_device->id < 0) {
        error = dmabuf_device->id;
        M_ERR("ida_alloc: error = %d\n", error);
        goto err_out;
    }

    dmabuf_device->name = kasprintf(GFP_KERNEL, "%s%d", THIS_MODULE->name, dmabuf_device->id);
    if(IS_ERR_OR_NULL(dmabuf_device->name)) {
        error = PTR_ERR(dmabuf_device->name);
        if(error == 0) error = -ENOMEM;
        dmabuf_device->name = NULL;
        M_ERR("kasprintf(): error = %d\n", error);
        goto err_out;
    }

    dmabuf_device->dmabuf = dmabuf_alloc(&pdev->dev, 1024 * 1024 * 1024);
    if(IS_ERR_OR_NULL(dmabuf_device->dmabuf)) {
        error = PTR_ERR(dmabuf_device->dmabuf);
        M_ERR("dmabuf_alloc(): error = %d\n", error);
        goto err_out;
    }

    dmabuf_device->miscdevice.minor = MISC_DYNAMIC_MINOR;
    dmabuf_device->miscdevice.name = dmabuf_device->name;
    dmabuf_device->miscdevice.fops = &dmabuf_fops;
    dmabuf_device->miscdevice.parent = &pdev->dev;

    error = misc_register(&dmabuf_device->miscdevice);
    if(error != 0) {
        dmabuf_device->miscdevice.minor = MISC_DYNAMIC_MINOR; // mark not registered
        M_ERR("misc_register(): error = %d\n", error);
        goto err_out;
    }

    platform_set_drvdata(pdev, dmabuf_device);

    return 0;

err_out:
    if(dmabuf_device != NULL) {
        if(dmabuf_device->miscdevice.minor != MISC_DYNAMIC_MINOR) misc_deregister(&dmabuf_device->miscdevice);

        dmabuf_free(dmabuf_device->dmabuf);
        if(dmabuf_device->name != NULL) kfree(dmabuf_device->name);
        if(dmabuf_device->id >= 0) ida_free(&dmabuf_ida, dmabuf_device->id);
        kfree(dmabuf_device);
    }
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device* pdev) {
    struct dmabuf_device* dmabuf_device = platform_get_drvdata(pdev);
    platform_set_drvdata(pdev, NULL);

    M_INFO("\n");

    if(dmabuf_device != NULL) {
        if(dmabuf_device->miscdevice.minor != MISC_DYNAMIC_MINOR) misc_deregister(&dmabuf_device->miscdevice);

        dmabuf_free(dmabuf_device->dmabuf);
        if(dmabuf_device->name != NULL) kfree(dmabuf_device->name);
        if(dmabuf_device->id >= 0) ida_free(&dmabuf_ida, dmabuf_device->id);
        kfree(dmabuf_device);
    }

    return 0;
}

static
struct platform_driver dmabuf_platform_driver = {
    .probe  = dmabuf_platform_driver_probe,
    .remove = dmabuf_platform_driver_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name  = THIS_MODULE->name,
    },
};

#endif // __AKOZLINS_DMABUF_PLATFORM_DRIVER_H
