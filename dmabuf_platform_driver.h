
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
void dmabuf_free(struct platform_device* pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(dmabuf == NULL) return;

    for(int i = 0; i < dmabuf_n; i++) {
        if(dmabuf[i].cpu_addr == NULL) continue;
        pr_info("[%s/%s] dma_free_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dma_free_coherent(&pdev->dev, dmabuf[i].size, dmabuf[i].cpu_addr, dmabuf[i].dma_addr);
    }

    kfree(dmabuf);
    dmabuf = NULL;
}

static
int dmabuf_alloc(struct platform_device* pdev) {
    int error = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    dmabuf = kzalloc(dmabuf_n * sizeof(struct dmabuf), 0);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        dmabuf = NULL;
        pr_err("[%s/%s] kzalloc: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    for(int i = 0; i < dmabuf_n; i++) {
        dmabuf[i].size = 1024 * PAGE_SIZE;
        pr_info("[%s/%s] dma_alloc_coherent: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        dmabuf[i].cpu_addr = dma_alloc_coherent(&pdev->dev, dmabuf[i].size, &dmabuf[i].dma_addr, 0);
        if(IS_ERR_OR_NULL(dmabuf[i].cpu_addr)) {
            error = PTR_ERR(dmabuf[i].cpu_addr);
            dmabuf[i].cpu_addr = NULL;
            pr_err("[%s/%s] dma_alloc_coherent: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
            goto err_free;
        }
        pr_info("  cpu_addr = %px\n", dmabuf[i].cpu_addr);
    }

    return 0;

err_free:
    dmabuf_free(pdev);
    return error;
}



#include "chrdev.h"
static struct chrdev_struct* dmabuf_chrdev;

static
ssize_t dmabuf_chrdev_read(struct file* file, char __user* user_buffer, size_t size, loff_t* loff) {
    ssize_t n = 0;
    loff_t offset = *loff;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    for(int i = 0; i < dmabuf_n; i++, offset -= dmabuf[i].size) {
        size_t k;

        if(offset > dmabuf[i].size) continue;

        k = dmabuf[i].size - offset;
        if(k > size) k = size;

        copy_to_user(user_buffer, dmabuf[i].cpu_addr, k);
        n += k;
        user_buffer += k;
        size -= k;
        offset += k;

        if(size == 0) break;
    }

    *loff += n;
    return n;
}

static
ssize_t dmabuf_chrdev_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* loff) {
    ssize_t n = 0;
    loff_t offset = *loff;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    for(int i = 0; i < dmabuf_n; i++, offset -= dmabuf[i].size) {
        size_t k;

        if(offset > dmabuf[i].size) continue;

        k = dmabuf[i].size - offset;
        if(k > size) k = size;

        copy_from_user(dmabuf[i].cpu_addr, user_buffer, k);
        n += k;
        user_buffer += k;
        size -= k;
        offset += k;

        if(size == 0) break;
    }

    *loff += n;
    return n;
}

static
int dmabuf_chrdev_mmap(struct file* filp, struct vm_area_struct* vma) {
    int error = 0;
    size_t size = 0;
    size_t offset = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);
    pr_info("  vm_start = %lx\n", vma->vm_start);
    pr_info("  vm_end = %lx\n", vma->vm_end);
    pr_info("  vm_pgoff = %lx\n", vma->vm_pgoff);

    if(dmabuf == NULL) {
        return -ENOMEM;
    }

    for(int i = 0; i < dmabuf_n; i++) size += dmabuf[i].size;
    pr_info("  size = %lx\n", size);
    if(vma->vm_end - vma->vm_start != size) {
        return -EINVAL;
    }
    if(vma->vm_pgoff != 0) {
        return -EINVAL;
    }

    // TODO: use set_memory_uc
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    for(int i = 0; i < dmabuf_n; i++) {
        if(dmabuf[i].cpu_addr == NULL) {
            return -ENOMEM;
        }

        pr_info("[%s/%s] remap_pfn_range: i = %d\n", THIS_MODULE->name, __FUNCTION__, i);
        error = remap_pfn_range(vma,
            vma->vm_start + offset,
            virt_to_phys(dmabuf[i].cpu_addr) >> PAGE_SHIFT,
            dmabuf[i].size, vma->vm_page_prot
        );
        if(error) {
            pr_err("[%s/%s] remap_pfn_range: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
            goto err_unmap;
        }
        offset += dmabuf[i].size;
    }

    return 0;

err_unmap:
    // TODO: unmap
    return error;
}

static
int dmabuf_chrdev_open(struct inode* inode, struct file* file) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
int dmabuf_chrdev_release(struct inode* inode, struct file* file) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
struct file_operations dmabuf_chrdev_fops = {
    .owner = THIS_MODULE,
    .read = dmabuf_chrdev_read,
    .write = dmabuf_chrdev_write,
    .mmap = dmabuf_chrdev_mmap,
    .open = dmabuf_chrdev_open,
    .release = dmabuf_chrdev_release,
};



static
int dmabuf_platform_driver_probe(struct platform_device* pdev) {
    int error = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    error = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if(error) {
        pr_err("[%s/%s] dma_set_mask_and_coherent: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    error = dmabuf_alloc(pdev);
    if(error) {
        goto err_out;
    }

    dmabuf_chrdev = chrdev_alloc(&dmabuf_chrdev_fops);
    if(IS_ERR_OR_NULL(dmabuf_chrdev)) {
        dmabuf_chrdev = NULL;
        pr_err("[%s/%s] chrdev_alloc: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    return 0;

err_out:
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device* pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    chrdev_free(dmabuf_chrdev);
    dmabuf_free(pdev);

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
