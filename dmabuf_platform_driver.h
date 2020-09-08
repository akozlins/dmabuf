/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/miscdevice.h>

struct dmabuf_miscdevice {
    int id;
    char* name;
    struct dmabuf* dmabuf;
    struct miscdevice miscdevice;
};

static DEFINE_IDA(dmabuf_miscdevice_ida);

#include "dmabuf_fops.h"

static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    int error;
    struct dmabuf_miscdevice* dmabuf_miscdevice = NULL;

    M_INFO("\n");

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error != 0) {
        M_ERR("dma_set_mask_and_coherent(mask = DMA_BIT_MASK(64)): error = %d\n", error);
        goto err_out;
    }

    dmabuf_miscdevice = kzalloc(sizeof(*dmabuf_miscdevice), GFP_KERNEL);
    if(IS_ERR_OR_NULL(dmabuf_miscdevice)) {
        error = PTR_ERR(dmabuf_miscdevice);
        if(error == 0) error = -ENOMEM;
        dmabuf_miscdevice = NULL;
        M_ERR("kzalloc(): error = %d\n", error);
        goto err_out;
    }

    dmabuf_miscdevice->id = ida_alloc(&dmabuf_miscdevice_ida, GFP_KERNEL);
    if(dmabuf_miscdevice->id < 0) {
        error = dmabuf_miscdevice->id;
        M_ERR("ida_alloc: error = %d\n", error);
        goto err_out;
    }

    dmabuf_miscdevice->name = kasprintf(GFP_KERNEL, "%s%d", THIS_MODULE->name, dmabuf_miscdevice->id);
    if(IS_ERR_OR_NULL(dmabuf_miscdevice->name)) {
        error = PTR_ERR(dmabuf_miscdevice->name);
        if(error == 0) error = -ENOMEM;
        dmabuf_miscdevice->name = NULL;
        M_ERR("kasprintf(): error = %d\n", error);
        goto err_out;
    }

    dmabuf_miscdevice->dmabuf = dmabuf_alloc(&pdev->dev, 1024 * 1024 * 1024);
    if(IS_ERR_OR_NULL(dmabuf_miscdevice->dmabuf)) {
        error = PTR_ERR(dmabuf_miscdevice->dmabuf);
        M_ERR("dmabuf_alloc(): error = %d\n", error);
        goto err_out;
    }

    dmabuf_miscdevice->miscdevice.minor = MISC_DYNAMIC_MINOR;
    dmabuf_miscdevice->miscdevice.name = dmabuf_miscdevice->name;
    dmabuf_miscdevice->miscdevice.fops = &dmabuf_fops;
    dmabuf_miscdevice->miscdevice.parent = &pdev->dev;

    error = misc_register(&dmabuf_miscdevice->miscdevice);
    if(error != 0) {
        M_ERR("misc_register(): error = %d\n", error);
        goto err_out;
    }

    platform_set_drvdata(pdev, dmabuf_miscdevice);

    return 0;

err_out:
    if(dmabuf_miscdevice != NULL) {
        dmabuf_free(dmabuf_miscdevice->dmabuf);
        if(dmabuf_miscdevice->name != NULL) kfree(dmabuf_miscdevice->name);
        if(dmabuf_miscdevice->id >= 0) ida_free(&dmabuf_miscdevice_ida, dmabuf_miscdevice->id);
        kfree(dmabuf_miscdevice);
    }
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device* pdev) {
    struct dmabuf_miscdevice* dmabuf_miscdevice = platform_get_drvdata(pdev);
    platform_set_drvdata(pdev, NULL);

    M_INFO("\n");

    if(dmabuf_miscdevice != NULL) {
        misc_deregister(&dmabuf_miscdevice->miscdevice);

        dmabuf_free(dmabuf_miscdevice->dmabuf);
        if(dmabuf_miscdevice->name != NULL) kfree(dmabuf_miscdevice->name);
        if(dmabuf_miscdevice->id >= 0) ida_free(&dmabuf_miscdevice_ida, dmabuf_miscdevice->id);
        kfree(dmabuf_miscdevice);
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
