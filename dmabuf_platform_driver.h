/* SPDX-License-Identifier: GPL-2.0 */

#include "dmabuf_chrdev.h"

static
void dmabuf_platform_driver_cleanup(struct platform_device* pdev) {
    struct chrdev* chrdev = platform_get_drvdata(pdev);
    if(chrdev == NULL) return;

    for(int i = 0; i < chrdev->count; i++) {
        struct dmabuf* dmabuf = chrdev->devices[i].private_data;
        chrdev_device_del(chrdev, i);
        chrdev->devices[i].private_data = NULL;
        dmabuf_free(dmabuf);
    }

    platform_set_drvdata(pdev, NULL);
    chrdev_free(chrdev);
}

static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    int error;
    struct chrdev* chrdev = NULL;

    M_INFO("\n");

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        M_ERR("dma_set_mask_and_coherent(mask = DMA_BIT_MASK(64)): error = %d\n", error);
        goto err_out;
    }

    chrdev = chrdev_alloc(THIS_MODULE->name, 2, &dmabuf_chrdev_fops);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        chrdev = NULL;
        M_ERR("chrdev_alloc: error = %d\n", error);
        goto err_out;
    }
    platform_set_drvdata(pdev, chrdev);

    for(int i = 0; i < chrdev->count; i++) {
        struct dmabuf* dmabuf = dmabuf_alloc(&pdev->dev, 256 * 1024 * 1024);
        if(IS_ERR_OR_NULL(dmabuf)) {
            error = PTR_ERR(dmabuf);
            dmabuf = NULL;
            goto err_out;
        }
        chrdev->devices[i].private_data = dmabuf;

        error = chrdev_device_add(chrdev, i, &pdev->dev);
        if(error) {
            goto err_out;
        }
    }

    return 0;

err_out:
    dmabuf_platform_driver_cleanup(pdev);
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device* pdev) {
    M_INFO("\n");

    dmabuf_platform_driver_cleanup(pdev);

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
