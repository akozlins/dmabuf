
#include <linux/cdev.h>

struct chrdev_struct {
    dev_t dev;
    struct class* class;
    struct cdev cdev;
    struct device* device;
};

static
void chrdev_free(struct chrdev_struct* chrdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(chrdev == NULL) return;

    if(&chrdev->cdev.dev != 0) {
        cdev_del(&chrdev->cdev);
        chrdev->cdev.dev = 0;
    }

    if(chrdev->device != NULL) {
        device_destroy(chrdev->class, chrdev->dev);
        chrdev->device = NULL;
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
struct chrdev_struct* chrdev_alloc(struct file_operations* fops) {
    int error = 0;
    struct chrdev_struct* chrdev;

    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    chrdev = kzalloc(sizeof(struct chrdev_struct), 0);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        chrdev = NULL;
        pr_err("[%s/%s] kzalloc: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }

    error = alloc_chrdev_region(&chrdev->dev, 0, 1, THIS_MODULE->name);
    if(error) {
        chrdev->dev = 0;
        pr_err("[%s/%s] alloc_chrdev_region: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    chrdev->class = class_create(THIS_MODULE, THIS_MODULE->name);
    if(IS_ERR_OR_NULL(chrdev->class)) {
        error = PTR_ERR(chrdev->class);
        chrdev->class = NULL;
        pr_err("[%s/%s] class_create: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    cdev_init(&chrdev->cdev, fops);
    error = cdev_add(&chrdev->cdev, chrdev->dev, 1);
    if(error) {
        chrdev->cdev.dev = 0;
        pr_err("[%s/%s] cdev_add: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }
    chrdev->device = device_create(chrdev->class, NULL, chrdev->dev, NULL, THIS_MODULE->name, 0);
    if(IS_ERR_OR_NULL(chrdev->device)) {
        error = PTR_ERR(chrdev->device);
        chrdev->device = NULL;
        pr_err("[%s/%s] device_create: error = %d\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_free;
    }

    return chrdev;

err_free:
    chrdev_free(chrdev);
err_out:
    return ERR_PTR(error);
}
