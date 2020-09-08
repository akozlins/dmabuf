/* SPDX-License-Identifier: GPL-2.0 */

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
    char* name;
    struct class* class;
    dev_t dev;
    int count;
    struct chrdev_device devices[];
};

static
void chrdev_device_del(struct chrdev_device* chrdev_device) {
    M_INFO("\n");

    if(IS_ERR_OR_NULL(chrdev_device)) return;

    if(!IS_ERR_OR_NULL(chrdev_device->device)) {
        device_destroy(chrdev_device->device->class, chrdev_device->cdev.dev);
        chrdev_device->device = NULL;
    }

    if(chrdev_device->cdev.count != 0) {
        cdev_del(&chrdev_device->cdev);
        chrdev_device->cdev.count = 0;
    }

    chrdev_device->private_data = NULL;
}

static
void chrdev_free(struct chrdev* chrdev) {
    M_INFO("\n");

    if(IS_ERR_OR_NULL(chrdev)) return;

    for(int i = 0; i < chrdev->count; i++) {
        chrdev_device_del(&chrdev->devices[i]);
    }

    if(chrdev->count != 0) {
        unregister_chrdev_region(chrdev->dev, chrdev->count);
        chrdev->count = 0;
    }

    if(!IS_ERR_OR_NULL(chrdev->class)) {
        class_destroy(chrdev->class);
        chrdev->class = NULL;
    }

    if(chrdev->name != NULL) kfree(chrdev->name);
    kfree(chrdev);
}

/**
 * \code
 * chrdev_device = chrdev->devices[minor]
 * cdev_add(&chrdev_device->cdev)
 * chrdev_device->device = device_create(parent, drvdata, "${name}${minor}")
 * \endcode
 *
 * @param chrdev - pointer to struct chrdev
 * @param minor - minor number for this device
 * @param parent - parent struct device
 *
 * @return - pointer to struct chrdev_device
 *
 * @retval -EINVAL - invalid minor number
 * @retval - errors from cdev_add and device_create
 */
static
struct chrdev_device* chrdev_device_add(struct chrdev* chrdev, int minor, const struct file_operations* fops, struct device* parent, void* private_data) {
    int error;
    struct chrdev_device* chrdev_device;

    M_INFO("minor = %d\n", minor);

    if(IS_ERR_OR_NULL(chrdev)) return ERR_PTR(-EFAULT);

    if(!(0 <= minor && minor < chrdev->count)) return ERR_PTR(-EINVAL);

    chrdev_device = &chrdev->devices[minor];

    cdev_init(&chrdev_device->cdev, fops);
    chrdev_device->cdev.owner = THIS_MODULE;
    chrdev_device->cdev.dev = MKDEV(MAJOR(chrdev->dev), MINOR(chrdev->dev) + minor);

    chrdev_device->private_data = private_data;

    error = cdev_add(&chrdev_device->cdev, chrdev_device->cdev.dev, 1);
    if(error) {
        M_ERR("cdev_add(minor = %d): error = %d\n", minor, error);
        kobject_put(&chrdev_device->cdev.kobj);
        chrdev_device->cdev.count = 0;
        goto err_out;
    }

    chrdev_device->device = device_create(chrdev->class, parent, chrdev_device->cdev.dev, NULL, "%s%d", chrdev->name, minor);
    if(IS_ERR_OR_NULL(chrdev_device->device)) {
        error = PTR_ERR(chrdev_device->device);
        chrdev_device->device = NULL;
        M_ERR("device_create(minor = %d): error = %d\n", minor, error);
        goto err_out;
    }

    return chrdev_device;

err_out:
    chrdev_device_del(chrdev_device);
    return ERR_PTR(error);
}

/**
 * \code
 * chrdev = kzalloc()
 * chrdev->class = create_calss(name)
 * alloc_chrdev_region(&chrdev->dev, count, name)
 * for_each(device, chrdev->devices) cdev_init(&device->cdev)
 * \endcode
 *
 * @param name - name of class and associated device or driver
 * @param count - required number of minor numbers
 * @param fops - file_operations for this device
 *
 * @return pointer to struct chrdev
 *
 * @retval -EINVAL - if name == NULL or count <= 0
 * @retval -ENOMEM - out of memory
 * @retval - errors from class_create and alloc_chrdev_region
 */
static
struct chrdev* chrdev_alloc(const char* name, int count) {
    int error;
    struct chrdev* chrdev;

    M_INFO("name = '%s', count = %d\n", name, count);

    if(name == NULL || count <= 0) return ERR_PTR(-EINVAL);

    chrdev = kzalloc(sizeof(*chrdev) + count * sizeof(chrdev->devices[0]), GFP_KERNEL);
    if(IS_ERR_OR_NULL(chrdev)) {
        error = PTR_ERR(chrdev);
        if(error == 0) error = -ENOMEM;
        chrdev = NULL;
        M_ERR("kzalloc: error = %d\n", error);
        goto err_out;
    }

    chrdev->name = kstrdup(name, GFP_KERNEL);
    if(IS_ERR_OR_NULL(chrdev->name)) {
        error = PTR_ERR(chrdev->name);
        if(error == 0) error = -ENOMEM;
        chrdev->name = NULL;
        M_ERR("kstrdup: error = %d\n", error);
        goto err_out;
    }

    chrdev->class = class_create(THIS_MODULE, chrdev->name);
    if(IS_ERR_OR_NULL(chrdev->class)) {
        error = PTR_ERR(chrdev->class);
        chrdev->class = NULL;
        M_ERR("class_create(name = '%s'): error = %d\n", name, error);
        goto err_out;
    }

    error = alloc_chrdev_region(&chrdev->dev, 0, count, chrdev->name);
    if(error) {
        M_ERR("alloc_chrdev_region(count = %d, name = '%s'): error = %d\n", count, name, error);
        goto err_out;
    }
    chrdev->count = count;

    return chrdev;

err_out:
    chrdev_free(chrdev);
    return ERR_PTR(error);
}
