#ifndef __KMODULE_H__
#define __KMODULE_H__

#include <linux/printk.h>

#define M_ERR(fmt, ...) \
    printk(KERN_ERR   "[%s/%s] " pr_fmt(fmt), THIS_MODULE->name, __FUNCTION__, ##__VA_ARGS__)
#define M_INFO(fmt, ...) \
    printk(KERN_INFO  "[%s/%s] " pr_fmt(fmt), THIS_MODULE->name, __FUNCTION__, ##__VA_ARGS__)
#define M_DEBUG(fmt, ...) \
    printk(KERN_DEBUG "[%s/%s] " pr_fmt(fmt), THIS_MODULE->name, __FUNCTION__, ##__VA_ARGS__)

#endif // __KMODULE_H__
