
#include "dmabuf_chrdev.h"

static
void dmabuf_platform_driver_cleanup(struct platform_device* pdev) {
    struct chrdev* chrdev = platform_get_drvdata(pdev);
    if(chrdev == NULL) return;

    for(int i = 0; i < chrdev->count; i++) {
        struct dmabuf* dmabuf = chrdev->minors[i].private_data;
        if(dmabuf == NULL) continue;
        chrdev->minors[i].private_data = NULL;
        dmabuf_free(dmabuf);
    }

    platform_set_drvdata(pdev, NULL);
    chrdev_free(chrdev);
}

static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    long error = 0;
    struct chrdev* chrdev = NULL;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        pr_err("[%s/%s] dma_set_mask_and_coherent: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    chrdev = chrdev_alloc(1, &dmabuf_chrdev_fops);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        chrdev = NULL;
        pr_err("[%s/%s] chrdev_alloc: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }
    platform_set_drvdata(pdev, chrdev);

    for(int i = 0; i < chrdev->count; i++) {
        struct dmabuf* dmabuf = dmabuf_alloc(&pdev->dev, 256); // 1 GB
        if(IS_ERR_OR_NULL(dmabuf)) {
            error = PTR_ERR(dmabuf);
            dmabuf = NULL;
            goto err_out;
        }
        chrdev->minors[i].private_data = dmabuf;
    }

    return 0;

err_out:
    dmabuf_platform_driver_cleanup(pdev);
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device* pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

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
