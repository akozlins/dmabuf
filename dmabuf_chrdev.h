
#include "chrdev.h"
#include "dmabuf.h"

static
loff_t dmabuf_chrdev_llseek(struct file* file, loff_t loff, int whence) {
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
int dmabuf_chrdev_release(struct inode* inode, struct file* file) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    return 0;
}

static
struct file_operations dmabuf_chrdev_fops = {
    .owner = THIS_MODULE,
    .llseek = dmabuf_chrdev_llseek,
    .read = dmabuf_chrdev_read,
    .write = dmabuf_chrdev_write,
    .mmap = dmabuf_chrdev_mmap,
    .open = dmabuf_chrdev_open,
    .release = dmabuf_chrdev_release,
};
