
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>

struct chrdev {
    dev_t dev;
    struct class* class;
    struct cdev cdev;
    struct device* device;
};

static
void chrdev_free(struct chrdev* chrdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(chrdev == NULL) return;

    if(chrdev->device != NULL) {
        device_destroy(chrdev->class, chrdev->dev);
        chrdev->device = NULL;
    }
    if(chrdev->cdev.dev != 0) {
        cdev_del(&chrdev->cdev);
        chrdev->cdev.dev = 0;
    }
    if(chrdev->class != NULL) {
        class_destroy(chrdev->class);
        chrdev->class = NULL;
    }
    if(chrdev->dev != 0) {
        unregister_chrdev_region(chrdev->dev, 1);
        chrdev->dev = 0;
    }

    kfree(chrdev);
}

static
struct chrdev* chrdev_alloc(struct file_operations* fops) {
    long error = 0;
    struct chrdev* chrdev = NULL;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    chrdev = kzalloc(sizeof(struct chrdev), GFP_KERNEL);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        chrdev = NULL;
        pr_err("[%s/%s] kzalloc: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    error = alloc_chrdev_region(&chrdev->dev, 0, 1, THIS_MODULE->name);
    if(error) {
        chrdev->dev = 0;
        pr_err("[%s/%s] alloc_chrdev_region: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    chrdev->class = class_create(THIS_MODULE, THIS_MODULE->name);
    if(IS_ERR_OR_NULL(chrdev->class)) {
        error = PTR_ERR(chrdev->class);
        chrdev->class = NULL;
        pr_err("[%s/%s] class_create: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    cdev_init(&chrdev->cdev, fops);
    error = cdev_add(&chrdev->cdev, chrdev->dev, 1);
    if(error) {
        chrdev->cdev.dev = 0;
        pr_err("[%s/%s] cdev_add: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }
    chrdev->device = device_create(chrdev->class, NULL, chrdev->dev, NULL, "%s", THIS_MODULE->name);
    if(IS_ERR_OR_NULL(chrdev->device)) {
        error = PTR_ERR(chrdev->device);
        chrdev->device = NULL;
        pr_err("[%s/%s] device_create: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    return chrdev;

err_out:
    chrdev_free(chrdev);
    return ERR_PTR(error);
}
