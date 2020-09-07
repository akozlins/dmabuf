/* SPDX-License-Identifier: GPL-2.0 */

static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    int error;
    int minor;
    struct dmabuf* dmabuf = NULL;
    struct chrdev_device* chrdev_device = NULL;

    M_INFO("\n");

    minor = ida_alloc_range(&chrdev_ida, 0, chrdev->count - 1, GFP_KERNEL);
    if(minor < 0) {
        error = minor;
        M_ERR("ida_alloc_range: error = %d\n", error);
        goto err_out;
    }

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        M_ERR("dma_set_mask_and_coherent(mask = DMA_BIT_MASK(64)): error = %d\n", error);
        goto err_out;
    }

    dmabuf = dmabuf_alloc(&pdev->dev, 1024 * 1024 * 1024);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        M_ERR("dmabuf_alloc(): error = %d\n", error);
        goto err_out;
    }

    chrdev_device = chrdev_device_add(chrdev, minor, &dmabuf_chrdev_fops, &pdev->dev, dmabuf);
    if(IS_ERR_OR_NULL(chrdev_device)) {
        error = PTR_ERR(chrdev_device);
        M_ERR("chrdev_device_create(): error = %d\n", error);
        goto err_out;
    }

    platform_set_drvdata(pdev, chrdev_device);

    return 0;

err_out:
    chrdev_device_del(chrdev_device);
    dmabuf_free(dmabuf);
    if(minor >= 0) ida_free(&chrdev_ida, minor);
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device* pdev) {
    struct chrdev_device* chrdev_device = platform_get_drvdata(pdev);
    struct dmabuf* dmabuf = chrdev_device != NULL ? chrdev_device->private_data : NULL;
    int minor = chrdev_device != NULL ? (int)MINOR(chrdev_device->cdev.dev) : -1;

    M_INFO("\n");

    chrdev_device_del(chrdev_device);
    dmabuf_free(dmabuf);
    if(minor >= 0) ida_free(&chrdev_ida, minor);

    platform_set_drvdata(pdev, NULL);

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
