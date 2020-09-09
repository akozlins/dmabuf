/* SPDX-License-Identifier: GPL-2.0 */

#include "dmabuf.h"

static
loff_t dmabuf_fops_llseek(struct file* file, loff_t loff, int whence) {
    struct dmabuf* dmabuf = file->private_data;
    return dmabuf_llseek(dmabuf, file, loff, whence);
}

static
ssize_t dmabuf_fops_read(struct file* file, char __user* user_buffer, size_t size, loff_t* offset) {
    struct dmabuf* dmabuf = file->private_data;
    ssize_t n = dmabuf_read(dmabuf, user_buffer, size, *offset);
    *offset += n;
    return n;
}

static
ssize_t dmabuf_fops_write(struct file* file, const char __user* user_buffer, size_t size, loff_t* offset) {
    struct dmabuf* dmabuf = file->private_data;
    ssize_t n = dmabuf_write(dmabuf, user_buffer, size, *offset);
    *offset += n;
    return n;
}

static
int dmabuf_fops_mmap(struct file* file, struct vm_area_struct* vma) {
    struct dmabuf* dmabuf = file->private_data;
    return dmabuf_mmap(dmabuf, vma);
}

/**
 * Set file->private_data pointer.
 *
 * \code
 * chrdev_device = container_of(inode->cdev)
 * dmabuf = get_drvdata(chrdev_device->device)
 * file->private_data = dmabuf
 * \endcode
 *
 * @param inode
 * @param file
 *
 * @return
 */
static
int dmabuf_fops_open(struct inode* inode, struct file* file) {
    struct dmabuf_device* dmabuf_device;
    struct dmabuf* dmabuf;

    M_INFO("\n");

    dmabuf_device  = container_of(file->private_data, struct dmabuf_device, miscdevice);
    if(dmabuf_device == NULL) {
        M_ERR("dmabuf_device == NULL\n");
        return -ENODEV;
    }

    dmabuf = dmabuf_device->dmabuf;
    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -ENODEV;
    }

    file->private_data = dmabuf;

    return 0;
}

static
int dmabuf_fops_release(struct inode* inode, struct file* file) {
    M_INFO("\n");

    return 0;
}

static const
struct file_operations dmabuf_fops = {
    .owner = THIS_MODULE,
    .llseek = dmabuf_fops_llseek,
    .read = dmabuf_fops_read,
    .write = dmabuf_fops_write,
    .mmap = dmabuf_fops_mmap,
    .open = dmabuf_fops_open,
    .release = dmabuf_fops_release,
};
