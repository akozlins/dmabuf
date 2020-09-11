/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __KMODULE_H__
#define __KMODULE_H__

#include <linux/module.h>

#define M_ERR(fmt, ...) \
    pr_err("[%s/%s] " pr_fmt(fmt), THIS_MODULE->name, __FUNCTION__, ##__VA_ARGS__)
#define M_INFO(fmt, ...) \
    pr_info("[%s/%s] " pr_fmt(fmt), THIS_MODULE->name, __FUNCTION__, ##__VA_ARGS__)
#define M_DEBUG(fmt, ...) \
    pr_debug("[%s/%s] " pr_fmt(fmt), THIS_MODULE->name, __FUNCTION__, ##__VA_ARGS__)

#endif // __KMODULE_H__
