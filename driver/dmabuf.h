
#include <linux/dma-direct.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

struct dmabuf {
    struct device* dev;
    size_t size;
    void* cpu_addr;
    dma_addr_t dma_addr;
};

static
void dmabuf_free(struct dmabuf* dmabuf) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf == NULL) return;

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++) {
        pr_info("[%s/%s] dma_free_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dma_free_coherent(dmabuf[i].dev, dmabuf[i].size, dmabuf[i].cpu_addr, dmabuf[i].dma_addr);
        dmabuf[i].cpu_addr = NULL;
    }

    kfree(dmabuf);
    dmabuf = NULL;
}

static
struct dmabuf* dmabuf_alloc(struct device* dev, int n) {
    long error = 0;
    struct dmabuf* dmabuf = NULL;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    dmabuf = kzalloc((n + 1) * sizeof(struct dmabuf), GFP_ATOMIC);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        dmabuf = NULL;
        pr_err("[%s/%s] kzalloc: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    for(int i = 0; i < n; i++) {
        dmabuf[i].dev = dev;
        dmabuf[i].size = 1024 * PAGE_SIZE;
        pr_info("[%s/%s] dma_alloc_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dmabuf[i].cpu_addr = dma_alloc_coherent(dev, dmabuf[i].size, &dmabuf[i].dma_addr, GFP_ATOMIC); // see `pci_alloc_consistent`
        if(IS_ERR_OR_NULL(dmabuf[i].cpu_addr)) {
            error = PTR_ERR(dmabuf[i].cpu_addr);
            dmabuf[i].cpu_addr = NULL;
            pr_err("[%s/%s] dma_alloc_coherent: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
            goto err_out;
        }
        pr_info("  cpu_addr = %px\n", dmabuf[i].cpu_addr);
    }

    return dmabuf;

    err_out:
    dmabuf_free(dmabuf);
    return ERR_PTR(error);
}
