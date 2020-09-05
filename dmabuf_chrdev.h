
#include "chrdev.h"
#include "dmabuf.h"

static
loff_t dmabuf_chrdev_llseek(struct file* file, loff_t loff, int whence) {
    struct dmabuf* dmabuf = file->private_data;
    return dmabuf_llseek(dmabuf, file, loff, whence);
}

static
ssize_t dmabuf_chrdev_read(struct file* file, char __user* user_buffer, size_t size, loff_t* offset) {
    struct dmabuf* dmabuf = file->private_data;
    ssize_t n = dmabuf_read(dmabuf, user_buffer, size, *offset);
    *offset += n;
    return n;
}

static
ssize_t dmabuf_chrdev_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* offset) {
    struct dmabuf* dmabuf = file->private_data;
    ssize_t n = dmabuf_write(dmabuf, user_buffer, size, *offset);
    *offset += n;
    return n;
}

static
int dmabuf_chrdev_mmap(struct file* file, struct vm_area_struct* vma) {
    struct dmabuf* dmabuf = file->private_data;
    return dmabuf_mmap(dmabuf, vma);
}

static
int dmabuf_chrdev_open(struct inode* inode, struct file* file) {
    struct chrdev_device* chrdev_device;
    struct dmabuf* dmabuf;

    M_INFO("\n");

    chrdev_device = container_of(inode->i_cdev, struct chrdev_device, cdev);
    if(chrdev_device == NULL) {
        M_ERR("chrdev_device == NULL\n");
        return -ENODEV;
    }

    dmabuf = chrdev_device->private_data;
    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -ENODEV;
    }

    file->private_data = dmabuf;

    return 0;
}

static
int dmabuf_chrdev_release(struct inode* inode, struct file* file) {
    M_INFO("\n");

    return 0;
}

static const
struct file_operations dmabuf_chrdev_fops = {
    .owner = THIS_MODULE,
    .llseek = dmabuf_chrdev_llseek,
    .read = dmabuf_chrdev_read,
    .write = dmabuf_chrdev_write,
    .mmap = dmabuf_chrdev_mmap,
    .open = dmabuf_chrdev_open,
    .release = dmabuf_chrdev_release,
};
