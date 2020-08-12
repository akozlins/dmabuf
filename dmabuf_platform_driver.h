
static
int dmabuf_platform_driver_probe(struct platform_device *pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
int dmabuf_platform_driver_remove(struct platform_device *pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);
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
