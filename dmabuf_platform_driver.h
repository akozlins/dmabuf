
#include <linux/cdev.h>
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
void dmabuf_free(struct platform_device *pdev) {
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
int dmabuf_alloc(struct platform_device *pdev) {
    int error = 0;

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
    }

    return 0;

err_free:
    dmabuf_free(pdev);
    return error;
}



struct dmabuf_chrdev {
    dev_t dev;
    struct class* class;
    struct cdev cdev;
    struct device* device;
};
static struct dmabuf_chrdev dmabuf_chrdev;

static
int dmabuf_chrdev_open(struct inode *inode, struct file *file) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
int dmabuf_chrdev_mmap(struct file *filp, struct vm_area_struct *vma) {
    int error = 0;
    size_t size = 0;
    size_t offset = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);
    pr_info("  vm_start = %lu\n", vma->vm_start);
    pr_info("  vm_end = %lu\n", vma->vm_end);
    pr_info("  vm_pgoff = %lu\n", vma->vm_pgoff);

    if(dmabuf == NULL) {
        return -ENOMEM;
    }

    for(int i = 0; i < dmabuf_n; i++) size += dmabuf[i].size;
    pr_info("  size = %lu\n", size);
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
struct file_operations dmabuf_chrdev_fops = {
    .owner = THIS_MODULE,
    .open = dmabuf_chrdev_open,
    .mmap = dmabuf_chrdev_mmap,
};

static
void dmabuf_chrdev_free(void) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(&dmabuf_chrdev.cdev.dev != 0) {
        cdev_del(&dmabuf_chrdev.cdev);
        dmabuf_chrdev.cdev.dev = 0;
    }

    if(dmabuf_chrdev.device != NULL) {
        device_destroy(dmabuf_chrdev.class, dmabuf_chrdev.dev);
        dmabuf_chrdev.device = NULL;
    }

    if(dmabuf_chrdev.class != NULL) {
        class_destroy(dmabuf_chrdev.class);
        dmabuf_chrdev.class = NULL;
    }
    if(dmabuf_chrdev.dev != 0) {
        unregister_chrdev_region(dmabuf_chrdev.dev, 1);
        dmabuf_chrdev.dev = 0;
    }
}

static
int dmabuf_chrdev_alloc(void) {
    int error = 0;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    error = alloc_chrdev_region(&dmabuf_chrdev.dev, 0, 1, THIS_MODULE->name);
    if(error) {
        dmabuf_chrdev.dev = 0;
        pr_err("[%s/%s] alloc_chrdev_region: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    dmabuf_chrdev.class = class_create(THIS_MODULE, THIS_MODULE->name);
    if(IS_ERR_OR_NULL(dmabuf_chrdev.class)) {
        error = PTR_ERR(dmabuf_chrdev.class);
        dmabuf_chrdev.class = NULL;
        pr_err("[%s/%s] class_create: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    cdev_init(&dmabuf_chrdev.cdev, &dmabuf_chrdev_fops);
    error = cdev_add(&dmabuf_chrdev.cdev, dmabuf_chrdev.dev, 1);
    if(error) {
        dmabuf_chrdev.cdev.dev = 0;
        pr_err("[%s/%s] cdev_add: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }
    dmabuf_chrdev.device = device_create(dmabuf_chrdev.class, NULL, dmabuf_chrdev.dev, NULL, THIS_MODULE->name, 0);
    if(IS_ERR_OR_NULL(dmabuf_chrdev.device)) {
        error = PTR_ERR(dmabuf_chrdev.device);
        dmabuf_chrdev.device = NULL;
        pr_err("[%s/%s] device_create: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    return 0;

err_free:
    dmabuf_chrdev_free();
    return error;
}



static
int dmabuf_platform_driver_probe(struct platform_device *pdev) {
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

    error = dmabuf_chrdev_alloc();
    if(error) {
        goto err_out;
    }

    return 0;

err_out:
    return error;
}

static
int dmabuf_platform_driver_remove(struct platform_device *pdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    dmabuf_chrdev_free();
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
