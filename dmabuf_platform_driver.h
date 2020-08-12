
#include <linux/dma-mapping.h>

struct dmabuf {
    size_t size;
    void* cpu_addr;
    dma_addr_t dma_addr;
};
static struct dmabuf dmabuf;

static
int dmabuf_platform_driver_probe(struct platform_device *pdev) {
    int error = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        pr_err("[%s/%s] dma_set_mask_and_coherent: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    dmabuf.size = PAGE_SIZE;
    dmabuf.cpu_addr = dma_alloc_coherent(&pdev->dev, dmabuf.size, &dmabuf.dma_addr, 0);
    if(IS_ERR_OR_NULL(dmabuf.cpu_addr)) {
        error = PTR_ERR(dmabuf.cpu_addr);
        dmabuf.cpu_addr = NULL;
        pr_err("[%s/%s] dma_alloc_coherent: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    return 0;

err_out:
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device *pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf.cpu_addr) {
        dma_free_coherent(&pdev->dev, dmabuf.size, dmabuf.cpu_addr, dmabuf.dma_addr);
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
