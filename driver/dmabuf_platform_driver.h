
#include "chrdev.h"
#include "dmabuf.h"

static
loff_t dmabuf_llseek(struct file* file, loff_t loff, int whence) {
    if(whence != SEEK_SET) return -EINVAL;
    file->f_pos = loff;
    return loff;
}

static
ssize_t dmabuf_read(struct file* file, char __user* user_buffer, size_t size, loff_t* loff) {
    ssize_t n = 0;
    loff_t offset = *loff;
    struct dmabuf* dmabuf = file->private_data;

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

    *loff += n;
    return n;
}

static
ssize_t dmabuf_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* loff) {
    ssize_t n = 0;
    loff_t offset = *loff;
    struct dmabuf* dmabuf = file->private_data;

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

    *loff += n;
    return n;
}

static
int dmabuf_mmap(struct file* file, struct vm_area_struct* vma) {
    int error = 0;
    size_t size = 0;
    size_t offset = 0;
    struct dmabuf* dmabuf = file->private_data;

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
int dmabuf_open(struct inode* inode, struct file* file) {
    struct chrdev* chrdev;
    struct dmabuf* dmabuf;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    chrdev = container_of(inode->i_cdev, struct chrdev, cdev);
    dmabuf = dev_get_drvdata(chrdev->device);
    if(dmabuf == NULL) {
        return -ENOMEM;
    }

    file->private_data = dmabuf;

    return 0;
}

static
int dmabuf_release(struct inode* inode, struct file* file) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
struct file_operations dmabuf_chrdev_fops = {
    .owner = THIS_MODULE,
    .llseek = dmabuf_llseek,
    .read = dmabuf_read,
    .write = dmabuf_write,
    .mmap = dmabuf_mmap,
    .open = dmabuf_open,
    .release = dmabuf_release,
};



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

    dmabuf = dmabuf_alloc(&pdev->dev, 64);
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
