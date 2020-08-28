
#include "chrdev.h"
#include "dmabuf.h"

static
loff_t chrdev_llseek(struct file* file, loff_t loff, int whence) {
    struct dmabuf* dmabuf = file->private_data;
    size_t size = dmabuf_size(dmabuf);

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
ssize_t chrdev_read(struct file* file, char __user* user_buffer, size_t size, loff_t* offset) {
    struct dmabuf* dmabuf = file->private_data;
    ssize_t n = dmabuf_read(dmabuf, user_buffer, size, *offset);
    *offset += n;
    return n;
}

static
ssize_t chrdev_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* offset) {
    struct dmabuf* dmabuf = file->private_data;
    ssize_t n = dmabuf_write(dmabuf, user_buffer, size, *offset);
    *offset += n;
    return n;
}

static
int chrdev_mmap(struct file* file, struct vm_area_struct* vma) {
    struct dmabuf* dmabuf = file->private_data;
    return dmabuf_mmap(dmabuf, vma);
}

static
int chrdev_open(struct inode* inode, struct file* file) {
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
int chrdev_release(struct inode* inode, struct file* file) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
struct file_operations dmabuf_chrdev_fops = {
    .owner = THIS_MODULE,
    .llseek = chrdev_llseek,
    .read = chrdev_read,
    .write = chrdev_write,
    .mmap = chrdev_mmap,
    .open = chrdev_open,
    .release = chrdev_release,
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
