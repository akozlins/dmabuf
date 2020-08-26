
#include <linux/dma-direct.h>
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

static
int dmabuf_mmap(struct dmabuf* dmabuf, struct vm_area_struct* vma) {
    int error = 0;
    size_t size = 0;
    size_t offset = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf == NULL) {
        return -ENOMEM;
    }

    pr_info("  vm_start = %lx\n", vma->vm_start);
    pr_info("  vm_end = %lx\n", vma->vm_end);
    pr_info("  vm_pgoff = %lx\n", vma->vm_pgoff);

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++) size += dmabuf[i].size;
    if(vma_pages(vma) != PAGE_ALIGN(size) >> PAGE_SHIFT) {
        return -EINVAL;
    }
    if(vma->vm_pgoff != 0) {
        return -EINVAL;
    }

    vma->vm_flags |= VM_LOCKED | VM_IO | VM_DONTEXPAND;
    // see `https://www.kernel.org/doc/html/latest/x86/pat.html#advanced-apis-for-drivers`
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); // see `pgprot_dmacoherent`

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++) {
        pr_info("[%s/%s] remap_pfn_range: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        error = remap_pfn_range(vma,
                                vma->vm_start + offset,
                                PHYS_PFN(dma_to_phys(dmabuf[i].dev, dmabuf[i].dma_addr)), // see `dma_direct_mmap`
                                dmabuf[i].size, vma->vm_page_prot
        );
        if(error) {
            pr_err("[%s/%s] remap_pfn_range: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
            break;
        }
        offset += dmabuf[i].size;
    }

    return error;
}

static
ssize_t dmabuf_read(struct dmabuf* dmabuf, char __user* user_buffer, size_t size, loff_t offset) {
    ssize_t n = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf == NULL) {
        return -ENOMEM;
    }

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++, offset -= dmabuf[i].size) {
        size_t k;

        if(offset > dmabuf[i].size) continue;

        k = dmabuf[i].size - offset;
        if(k > size) k = size;

        pr_info("[%s/%s] copy_to_user(dmabuf[%d], ..., 0x%lx)\n", THIS_MODULE->name, __FUNCTION__, i, k);
        if(copy_to_user(user_buffer, dmabuf[i].cpu_addr + offset, k)) {
            return -EFAULT;
        }
        n += k;
        user_buffer += k;
        size -= k;
        offset += k;

        if(size == 0) break;
    }

    return n;
}

static
ssize_t dmabuf_write(struct dmabuf* dmabuf, const char __user* user_buffer, size_t size, loff_t offset) {
    ssize_t n = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf == NULL) {
        return -ENOMEM;
    }

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++, offset -= dmabuf[i].size) {
        size_t k;

        if(offset > dmabuf[i].size) continue;

        k = dmabuf[i].size - offset;
        if(k > size) k = size;

        pr_info("[%s/%s] copy_from_user(dmabuf[%d], ..., 0x%lx)\n", THIS_MODULE->name, __FUNCTION__, i, k);
        if(copy_from_user(dmabuf[i].cpu_addr + offset, user_buffer, k)) {
            return -EFAULT;
        }
        n += k;
        user_buffer += k;
        size -= k;
        offset += k;

        if(size == 0) break;
    }

    return n;
}
