
#include "dmabuf_chrdev.h"

static
void dmabuf_platform_driver_cleanup(struct platform_device* pdev) {
    struct chrdev* chrdev = NULL;
    struct dmabuf* dmabuf = NULL;

    chrdev = platform_get_drvdata(pdev);
    platform_set_drvdata(pdev, NULL);
    if(chrdev != NULL) {
        dmabuf = dev_get_drvdata(chrdev->device);
        dev_set_drvdata(chrdev->device, NULL);
    }

    dmabuf_free(dmabuf);
    chrdev_free(chrdev);
}

static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    long error = 0;
    struct chrdev* chrdev = NULL;
    struct dmabuf* dmabuf = NULL;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        pr_err("[%s/%s] dma_set_mask_and_coherent: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    chrdev = chrdev_alloc(&dmabuf_chrdev_fops);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        chrdev = NULL;
        pr_err("[%s/%s] chrdev_alloc: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }
    platform_set_drvdata(pdev, chrdev);

    dmabuf = dmabuf_alloc(&pdev->dev, 256); // 1 GB
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        dmabuf = NULL;
        goto err_out;
    }
    dev_set_drvdata(chrdev->device, dmabuf);

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
