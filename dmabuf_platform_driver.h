
#include <linux/dma-mapping.h>
#include <linux/slab.h>

struct dmabuf {
    size_t size;
    void* cpu_addr;
    dma_addr_t dma_addr;
};
static struct dmabuf* dmabuf = NULL;
static int dmabuf_n = 16;

static
int dmabuf_platform_driver_probe(struct platform_device *pdev) {
    int error = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        pr_err("[%s/%s] dma_set_mask_and_coherent: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    dmabuf = kzalloc(dmabuf_n * sizeof(struct dmabuf), 0);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        dmabuf = NULL;
        pr_err("[%s/%s] kzalloc: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    for(int i = 0; i < dmabuf_n; i++) {
        dmabuf[i].size = 1024 * PAGE_SIZE;
        pr_info("[%s/%s] dma_alloc_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dmabuf[i].cpu_addr = dma_alloc_coherent(&pdev->dev, dmabuf[i].size, &dmabuf[i].dma_addr, 0);
        if(IS_ERR_OR_NULL(dmabuf[i].cpu_addr)) {
            error = PTR_ERR(dmabuf[i].cpu_addr);
            dmabuf[i].cpu_addr = NULL;
            pr_err("[%s/%s] dma_alloc_coherent: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        }
    }

    return 0;

err_out:
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device *pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf) {
        for(int i = 0; i < dmabuf_n; i++) {
            if(dmabuf[i].cpu_addr == NULL) continue;
            pr_info("[%s/%s] dma_free_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
            dma_free_coherent(&pdev->dev, dmabuf[i].size, dmabuf[i].cpu_addr, dmabuf[i].dma_addr);
        }
        kfree(dmabuf);
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
