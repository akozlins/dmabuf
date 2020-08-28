
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>

struct chrdev_minor {
    struct cdev cdev;
    struct device* device;
};

struct chrdev {
    dev_t dev;
    struct class* class;
    int count;
    struct chrdev_minor* minors;
};

static
void chrdev_free(struct chrdev* chrdev) {
    pr_info("[%s/%s]\n", THIS_MODULE->name, __FUNCTION__);

    if(chrdev == NULL) return;

    if(chrdev->minors == NULL) {
        goto err_free_chrdev;
    }

    for(int i = 0; i < chrdev->count; i++) {
        struct chrdev_minor* minor = &chrdev->minors[i];

        if(minor->device != NULL) {
            device_destroy(chrdev->class, chrdev->dev);
            minor->device = NULL;
        }

        if(minor->cdev.dev != 0) {
            cdev_del(&minor->cdev);
            minor->cdev.dev = 0;
        }
    }

    if(chrdev->class != NULL) {
        class_destroy(chrdev->class);
        chrdev->class = NULL;
    }
    if(chrdev->dev != 0) {
        unregister_chrdev_region(chrdev->dev, 1);
        chrdev->dev = 0;
    }

    kfree(chrdev->minors);
err_free_chrdev:
    kfree(chrdev);
}

static
struct chrdev* chrdev_alloc(int count, struct file_operations* fops) {
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
    chrdev->minors = kzalloc(count * sizeof(struct chrdev_minor), GFP_KERNEL);
    if(IS_ERR_OR_NULL(chrdev->minors)) {
        error = PTR_ERR(chrdev->minors);
        chrdev->minors = NULL;
        pr_err("[%s/%s] kzalloc: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
        goto err_out;
    }
    chrdev->count = count;

    error = alloc_chrdev_region(&chrdev->dev, 0, count, THIS_MODULE->name);
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

    for(int i = 0; i < count; i++) {
        struct chrdev_minor* minor = &chrdev->minors[i];

        cdev_init(&minor->cdev, fops);
        minor->cdev.owner = THIS_MODULE;
        error = cdev_add(&minor->cdev, chrdev->dev, 1);
        if(error) {
            minor->cdev.dev = 0;
            pr_err("[%s/%s] cdev_add: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
            goto err_out;
        }

        minor->device = device_create(chrdev->class, NULL, chrdev->dev, NULL, "%s%d", THIS_MODULE->name, i);
        if(IS_ERR_OR_NULL(minor->device)) {
            error = PTR_ERR(minor->device);
            minor->device = NULL;
            pr_err("[%s/%s] device_create: error = %ld\n", THIS_MODULE->name, __FUNCTION__, error);
            goto err_out;
        }
    }

    return chrdev;

err_out:
    chrdev_free(chrdev);
    return ERR_PTR(error);
}
