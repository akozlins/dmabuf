/* SPDX-License-Identifier: GPL-2.0 */
#pragma once

#include <linux/module.h>

// TODO: rename `M_ERR` -> `M_DMABUF_ERROR`, etc.
#define M_ERR(format, ...) \
    pr_err("[%s:%d,%s] " pr_fmt(format), strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define M_INFO(format, ...) \
    pr_info("[%s:%d,%s] " pr_fmt(format), strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define M_DEBUG(format, ...) \
    pr_debug("[%s:%d,%s] " pr_fmt(format), strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
