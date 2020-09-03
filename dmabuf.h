
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

    if(IS_ERR_OR_NULL(dmabuf)) return;

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++) {
        pr_info("[%s/%s] dma_free_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dma_free_coherent(dmabuf[i].dev, dmabuf[i].size, dmabuf[i].cpu_addr, dmabuf[i].dma_addr);
        dmabuf[i].cpu_addr = NULL;
    }

    kfree(dmabuf);
}

static
struct dmabuf* dmabuf_alloc(struct device* dev, int n) {
    long error;
    struct dmabuf* dmabuf;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    dmabuf = kzalloc((n + 1) * sizeof(struct dmabuf), GFP_KERNEL);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        if(error == 0) error = -ENOMEM;
        dmabuf = NULL;
        pr_err("[%s/%s] kzalloc: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    for(int i = 0; i < n; i++) {
        dmabuf[i].dev = dev;
        dmabuf[i].size = 1024 << PAGE_SHIFT;
        pr_info("[%s/%s] dma_alloc_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dmabuf[i].cpu_addr = dma_alloc_coherent(dmabuf[i].dev, dmabuf[i].size, &dmabuf[i].dma_addr, GFP_ATOMIC); // see `pci_alloc_consistent`
        if(IS_ERR_OR_NULL(dmabuf[i].cpu_addr)) {
            error = PTR_ERR(dmabuf[i].cpu_addr);
            if(error == 0) error = -ENOMEM;
            dmabuf[i].cpu_addr = NULL;
            pr_err("[%s/%s] dma_alloc_coherent: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
            goto err_out;
        }
        pr_info("  cpu_addr = %px\n", dmabuf[i].cpu_addr);
        pr_info("  dma_addr = %llx\n", dmabuf[i].dma_addr);
    }

    return dmabuf;

err_out:
    dmabuf_free(dmabuf);
    return ERR_PTR(error);
}

static
size_t dmabuf_size(struct dmabuf* dmabuf) {
    size_t size = 0;

    if(dmabuf == NULL) {
        pr_err("[%s/%s] dmabuf == NULL\n", THIS_MODULE->name, __FUNCTION__);
        return 0;
    }

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++) {
        size += dmabuf[i].size;
    }

    return size;
}

static
loff_t dmabuf_llseek(struct dmabuf* dmabuf, struct file* file, loff_t loff, int whence) {
    size_t size;

    if(dmabuf == NULL) {
        pr_err("[%s/%s] dmabuf == NULL\n", THIS_MODULE->name, __FUNCTION__);
        return -EFAULT;
    }

    size = dmabuf_size(dmabuf);

    if(whence == SEEK_END && 0 <= -loff && -loff <= size) {
        file->f_pos = size + loff;
        return file->f_pos;
    }

    if(whence == SEEK_SET && 0 <= loff && loff <= size) {
        file->f_pos = loff;
        return file->f_pos;
    }

    return -EINVAL;
}

static
int dmabuf_mmap(struct dmabuf* dmabuf, struct vm_area_struct* vma) {
    int error = 0;
    size_t offset = 0;

    if(dmabuf == NULL) {
        pr_err("[%s/%s] dmabuf == NULL\n", THIS_MODULE->name, __FUNCTION__);
        return -EFAULT;
    }

    pr_info("  vm_start = %lx\n", vma->vm_start);
    pr_info("  vm_end = %lx\n", vma->vm_end);
    pr_info("  vm_pgoff = %lx\n", vma->vm_pgoff);

    if(vma_pages(vma) != PAGE_ALIGN(dmabuf_size(dmabuf)) >> PAGE_SHIFT) {
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
            pr_err("[%s/%s] remap_pfn_range: i = %d, error = %d\n", THIS_MODULE->name, __FUNCTION__, i, error);
            break;
        }
        offset += dmabuf[i].size;
    }

    return error;
}

static
ssize_t dmabuf_read(struct dmabuf* dmabuf, char __user* user_buffer, size_t size, loff_t offset) {
    ssize_t n = 0;

    if(dmabuf == NULL) {
        pr_err("[%s/%s] dmabuf == NULL\n", THIS_MODULE->name, __FUNCTION__);
        return -EFAULT;
    }

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++, offset -= dmabuf[i].size) {
        size_t k;

        if(offset > dmabuf[i].size) continue;

        k = dmabuf[i].size - offset;
        if(k > size) k = size;

        pr_info("[%s/%s] copy_to_user(dmabuf[%d], ..., 0x%lx)\n", THIS_MODULE->name, __FUNCTION__, i, k);
        if(copy_to_user(user_buffer, dmabuf[i].cpu_addr + offset, k)) {
            pr_err("[%s/%s] copy_to_user != 0\n", THIS_MODULE->name, __FUNCTION__);
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

    if(dmabuf == NULL) {
        pr_err("[%s/%s] dmabuf == NULL\n", THIS_MODULE->name, __FUNCTION__);
        return -EFAULT;
    }

    for(int i = 0; dmabuf[i].cpu_addr != NULL; i++, offset -= dmabuf[i].size) {
        size_t k;

        if(offset > dmabuf[i].size) continue;

        k = dmabuf[i].size - offset;
        if(k > size) k = size;

        pr_info("[%s/%s] copy_from_user(dmabuf[%d], ..., 0x%lx)\n", THIS_MODULE->name, __FUNCTION__, i, k);
        if(copy_from_user(dmabuf[i].cpu_addr + offset, user_buffer, k)) {
            pr_err("[%s/%s] copy_from_user != 0\n", THIS_MODULE->name, __FUNCTION__);
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
