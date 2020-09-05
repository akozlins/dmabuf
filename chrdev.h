
#include "kmodule.h"

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>

struct chrdev_device {
    struct cdev cdev;
    struct device* device;
    void* private_data;
};

struct chrdev {
    char name[64];
    struct class* class;
    dev_t dev;
    int count;
    struct chrdev_device devices[];
};

static
void chrdev_device_del(struct chrdev* chrdev, int i) {
    struct chrdev_device* chrdev_device;

    M_INFO("i = %d\n", i);

    if(IS_ERR_OR_NULL(chrdev)) return;
    if(!(0 <= i && i < chrdev->count)) return;

    chrdev_device = &chrdev->devices[i];

    if(!IS_ERR_OR_NULL(chrdev_device->device)) {
        device_destroy(chrdev->class, chrdev_device->cdev.dev);
        chrdev_device->device = NULL;
    }

    if(chrdev_device->cdev.count != 0) {
        cdev_del(&chrdev_device->cdev);
        chrdev_device->cdev.count = 0;
    }
}

static
void chrdev_free(struct chrdev* chrdev) {
    M_INFO("\n");

    if(IS_ERR_OR_NULL(chrdev)) return;

    for(int i = 0; i < chrdev->count; i++) {
        chrdev_device_del(chrdev, i);
    }

    if(chrdev->dev != 0) {
        unregister_chrdev_region(chrdev->dev, 1);
        chrdev->dev = 0;
    }

    if(!IS_ERR_OR_NULL(chrdev->class)) {
        class_destroy(chrdev->class);
        chrdev->class = NULL;
    }

    kfree(chrdev);
}

static
int chrdev_device_add(struct chrdev* chrdev, int i, struct device *parent) {
    int error;
    struct chrdev_device* chrdev_device;

    M_INFO("i = %d\n", i);

    if(IS_ERR_OR_NULL(chrdev)) return -EINVAL;
    if(!(0 <= i && i < chrdev->count)) return -EINVAL;

    chrdev_device = &chrdev->devices[i];

    error = cdev_add(&chrdev_device->cdev, chrdev_device->cdev.dev, 1);
    if(error) {
        chrdev_device->cdev.count = 0;
        M_ERR("cdev_add: error = %d\n", error);
        goto err_out;
    }

    chrdev_device->device = device_create(chrdev->class, parent, chrdev_device->cdev.dev, NULL, "%s%d", chrdev->name, i);
    if(IS_ERR_OR_NULL(chrdev_device->device)) {
        error = PTR_ERR(chrdev_device->device);
        chrdev_device->device = NULL;
        M_ERR("device_create: error = %d\n", error);
        goto err_out;
    }

    return 0;

err_out:
    chrdev_device_del(chrdev, i);
    return error;
}

static
struct chrdev* chrdev_alloc(const char* name, int count, const struct file_operations* fops) {
    long error;
    struct chrdev* chrdev;

    M_INFO("name = %s, count = %d\n", name, count);

    chrdev = kzalloc(sizeof(*chrdev) + count * sizeof(chrdev->devices[0]), GFP_KERNEL);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        if(error == 0) error = -ENOMEM;
        chrdev = NULL;
        M_ERR("kzalloc: error = %ld\n", error);
        goto err_out;
    }

    strlcpy(chrdev->name, name, sizeof(chrdev->name));

    chrdev->class = class_create(THIS_MODULE, chrdev->name);
    if(IS_ERR_OR_NULL(chrdev->class)) {
        error = PTR_ERR(chrdev->class);
        chrdev->class = NULL;
        M_ERR("class_create: error = %ld\n", error);
        goto err_out;
    }

    error = alloc_chrdev_region(&chrdev->dev, 0, count, chrdev->name);
    if(error) {
        chrdev->dev = 0;
        M_ERR("alloc_chrdev_region: error = %ld\n", error);
        goto err_out;
    }

    chrdev->count = count;

    for(int i = 0; i < chrdev->count; i++) {
        struct chrdev_device* device = &chrdev->devices[i];

        cdev_init(&device->cdev, fops);
        device->cdev.owner = THIS_MODULE;
        device->cdev.dev = MKDEV(MAJOR(chrdev->dev), MINOR(chrdev->dev) + i);
    }

    return chrdev;

err_out:
    chrdev_free(chrdev);
    return ERR_PTR(error);
}
