/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __AKOZLINS_DMABUF_H
#define __AKOZLINS_DMABUF_H

#include "module.h"

#include <linux/dma-mapping.h>
#include <linux/list_sort.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0) // `dma_to_phys`
#include <linux/dma-direct.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) // `dma-map-ops.h`
#include <linux/dma-map-ops.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0) // `vm_flags_set`
static inline
void vm_flags_set(struct vm_area_struct* vma, vm_flags_t flags) {
    vma->vm_flags |= flags;
}
static inline
void vm_flags_clear(struct vm_area_struct* vma, vm_flags_t flags) {
    vma->vm_flags &= ~flags;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
static int ida_alloc_range(struct ida *ida, unsigned int min, unsigned int max, gfp_t gfp) {
    return ida_simple_get(ida, min, max + 1, gfp);
}

static void ida_free(struct ida* ida, unsigned int id) {
    ida_simple_remove(ida, id);
}

static int ida_alloc(struct ida* ida, gfp_t gfp) {
    return ida_alloc_range(ida, 0, ~0, gfp);
}
#endif

struct dmabuf_entry {
    size_t size;
    void* cpu_addr;
    dma_addr_t dma_handle;
    struct list_head list_head;
};

struct dmabuf {
    struct device* dev;
    size_t size;
    struct list_head entries;
};

static
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 70) // use const
int dmabuf_entry_cmp(void* priv, const struct list_head* a, const struct list_head* b) {
#else
int dmabuf_entry_cmp(void* priv, struct list_head* a, struct list_head* b) {
#endif
    dma_addr_t aa = container_of(a, struct dmabuf_entry, list_head)->dma_handle;
    dma_addr_t bb = container_of(b, struct dmabuf_entry, list_head)->dma_handle;
    if(aa < bb) return -1;
    if(aa > bb) return +1;
    return 0;
}

/**
 * Report contiguous DMA handles.
 *
 * @param dmabuf - pointer to struct dmabuf
 *
 * @return - number of contiguous DMA handles
 */
static
int dmabuf_report(struct dmabuf* dmabuf) {
    int n = 0;
    struct dmabuf_entry* entry;

    if(IS_ERR_OR_NULL(dmabuf)) return -EFAULT;

    list_for_each_entry(entry, &dmabuf->entries, list_head) {
        dma_addr_t dma_handle = entry->dma_handle;
        size_t size = entry->size;
        while(!list_is_last(&entry->list_head, &dmabuf->entries)) {
            typeof(entry) next = list_next_entry(entry, list_head);
            if(dma_handle + size != next->dma_handle) break;
            size += next->size;
            entry = next;
        }
        n++;
        M_INFO("dma_handle = 0x%pad, size = 0x%zx", &dma_handle, size);
    }

    return n;
}

static
void dmabuf_free(struct dmabuf* dmabuf) {
    struct dmabuf_entry* entry, *tmp;

    if(IS_ERR_OR_NULL(dmabuf)) return;

    M_INFO("\n");

    list_for_each_entry_safe(entry, tmp, &dmabuf->entries, list_head) {
        list_del(&entry->list_head);
        M_DEBUG("dma_free_coherent(dma_handle = 0x%pad, size = 0x%zx)\n", &entry->dma_handle, entry->size);
        dma_free_coherent(dmabuf->dev, entry->size, entry->cpu_addr, entry->dma_handle);
        kfree(entry);
    }

    kfree(dmabuf);
}

/**
 * Allocate DMA buffer.
 *
 * Use dma_alloc_coherent to allocate list of struct dmabuf_entry objects
 * that back the requested size of the DMA buffer.
 *
 * The list is sorted by dma_handle
 * such that contiguous ranges can be combined
 * when passing handle and size to the device.
 *
 * \code
 * dmabuf = kzalloc()
 * while(dmabuf->size < size) list_add(dma_alloc_coherent(), &dmabuf->entries)
 * list_sort(&dmabuf->entries, (a, b) { a->dma_handle < b->dma_handle })
 * \endcode
 *
 * @param dev - associated struct device pointer
 * @param size - required size of the buffer
 *
 * @return - pointer to struct dmabuf
 *
 * @retval -EINVAL - if size is 0 or not multiple of page size
 * @retval -ENOMEM - out of memory (kzalloc or dma_alloc_coherent)
 */
static
struct dmabuf* dmabuf_alloc(struct device* dev, size_t size) {
    int error;
    size_t entry_size = PAGE_SIZE << 10; // start from 1024 pages (4 MB)
    struct dmabuf* dmabuf;

    if(dev == NULL) return ERR_PTR(-EFAULT);

    M_INFO("size = 0x%zx\n", size);

    if(size == 0 || !IS_ALIGNED(size, PAGE_SIZE)) {
        return ERR_PTR(-EINVAL);
    }

    dmabuf = kzalloc(sizeof(*dmabuf), GFP_KERNEL);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        if(error == 0) error = -ENOMEM;
        M_ERR("kzalloc: error = %d\n", error);
        goto err_out;
    }

    dmabuf->dev = dev;
    INIT_LIST_HEAD(&dmabuf->entries);

    while(dmabuf->size < size) {
        struct dmabuf_entry* entry = kzalloc(sizeof(*entry), GFP_KERNEL);
        if(IS_ERR_OR_NULL(entry)) {
            error = PTR_ERR(entry);
            if(error == 0) error = -ENOMEM;
            M_ERR("kzalloc: error = %d\n", error);
            goto err_out;
        }

retry_alloc:
        entry->size = entry_size;
        M_DEBUG("dma_alloc_coherent(size = 0x%zx)\n", entry->size);
        entry->cpu_addr = dma_alloc_coherent(dmabuf->dev, entry->size, &entry->dma_handle, GFP_ATOMIC | __GFP_NOWARN); // see `pci_alloc_consistent`
        if(IS_ERR_OR_NULL(entry->cpu_addr)) {
            error = PTR_ERR(entry->cpu_addr);
            if(error == 0) error = -ENOMEM;
            M_ERR("dma_alloc_coherent(size = 0x%zx): error = %d\n", entry->size, error);
            if(entry_size > PAGE_SIZE) {
                // reduce allocation order and try again
                entry_size /= 2;
                goto retry_alloc;
            }
            kfree(entry);
            goto err_out;
        }

        INIT_LIST_HEAD(&entry->list_head);
        list_add(&entry->list_head, &dmabuf->entries);

        dmabuf->size += entry->size;
    }

    // sort by dma_handle
    list_sort(NULL, &dmabuf->entries, dmabuf_entry_cmp);

    dmabuf_report(dmabuf);

    return dmabuf;

err_out:
    dmabuf_free(dmabuf);
    return ERR_PTR(error);
}

static
loff_t dmabuf_llseek(struct dmabuf* dmabuf, struct file* file, loff_t loff, int whence) {
    loff_t loff_new;

    if(dmabuf == NULL) return -EFAULT;

    switch(whence) {
    case SEEK_END:
        loff_new = dmabuf->size + loff;
        break;
    case SEEK_SET:
        loff_new = loff;
        break;
    default:
        loff_new = -1;
    }

    if(!(0 <= loff_new && loff_new <= dmabuf->size)) {
        M_ERR("loff = 0x%llx, whence = %d\n", loff, whence);
        return -EINVAL;
    }

    file->f_pos = loff_new;
    return file->f_pos;
}

/**
 * Map DMA buffer to user address space.
 *
 * Use pgprot_noncached to set page protection
 * and map each dmabuf_entry with remap_pfn_range.
 *
 * \code
 * vma->vm_page_prot = pgprot_dmacoherent()
 * for_each(entry : dmabuf->entries) remap_pfn_range(pfn(entry->dma_handle))
 * \endcode
 *
 * @param dmabuf - pointer to struct dmabuf
 * @param vma - pointer to struct vm_area_struct
 *
 * @return - 0 on success
 *
 * @retval -EINVAL - if out of range
 * @retval - errors from remap_pfn_range
 */
static
int dmabuf_mmap(struct dmabuf* dmabuf, struct vm_area_struct* vma) {
    int error;
    typeof(vma->vm_start) vma_addr = vma->vm_start;
    size_t vma_size = vma->vm_end - vma->vm_start;
    size_t offset = vma->vm_pgoff << PAGE_SHIFT;
    struct dmabuf_entry* entry;

    if(dmabuf == NULL) return -EFAULT;

    M_INFO("vma_size = 0x%zx, offset = 0x%zx\n", vma_size, offset);

    if(offset > dmabuf->size) return -EINVAL;
    if(vma_size > dmabuf->size - offset) return -EINVAL;

    vm_flags_set(vma, VM_LOCKED | VM_IO | VM_DONTEXPAND);
    M_DEBUG("vma->vm_flags = %pGv", &vma->vm_flags);
    // <https://www.kernel.org/doc/html/latest/x86/pat.html>
    // <https://elixir.bootlin.com/linux/latest/source/include/linux/dma-map-ops.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) // `dma-map-ops.h`
    vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
#else
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

    // the mm semaphore is already held (by mmap)
    list_for_each_entry(entry, &dmabuf->entries, list_head) {
        size_t size;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }
        size = entry->size - offset;
        if(vma_size < size) size = vma_size;
        if(size == 0) break;

        unsigned long pfn = PHYS_PFN(dma_to_phys(dmabuf->dev, entry->dma_handle + offset)); // see `dma_direct_mmap`

        M_DEBUG("remap_pfn_range(pfn = 0x%lx, size = 0x%zx)\n", pfn, size);
        error = remap_pfn_range(vma, vma_addr, pfn, size, vma->vm_page_prot);
        if(error) {
            M_ERR("remap_pfn_range(pfn = 0x%lx, size = 0x%zx): error = %d\n", pfn, size, error);
            goto err_out;
        }

        vma_addr += size;
        vma_size -= size;
        offset = 0; // offset is 0 for next entry
    }

    if(vma_size != 0) return -EINVAL;

    return 0;

err_out:
    // kernel does the unmap in case of error
    return error;
}

static
ssize_t dmabuf_read(struct dmabuf* dmabuf, char __user* user_buffer, size_t user_size, loff_t offset) {
    ssize_t n = 0;
    struct dmabuf_entry* entry;

    if(dmabuf == NULL) return -EFAULT;

    list_for_each_entry(entry, &dmabuf->entries, list_head) {
        size_t size;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }
        size = entry->size - offset;
        if(user_size < size) size = user_size;
        if(size == 0) break;

        M_DEBUG("copy_to_user(size = 0x%zx)\n", size);
        if(copy_to_user(user_buffer, entry->cpu_addr + offset, size)) {
            M_ERR("copy_to_user(size = 0x%zx) != 0\n", size);
            return -EFAULT;
        }
        n += size;
        user_buffer += size;
        user_size -= size;
        offset = 0; // offset is 0 for next entry
    }

    return n;
}

static
ssize_t dmabuf_write(struct dmabuf* dmabuf, const char __user* user_buffer, size_t user_size, loff_t offset) {
    ssize_t n = 0;
    struct dmabuf_entry* entry;

    if(dmabuf == NULL) return -EFAULT;

    list_for_each_entry(entry, &dmabuf->entries, list_head) {
        size_t size;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }
        size = entry->size - offset;
        if(user_size < size) size = user_size;
        if(size == 0) break;

        M_DEBUG("copy_from_user(size = 0x%zx)\n", size);
        if(copy_from_user(entry->cpu_addr + offset, user_buffer, size)) {
            M_ERR("copy_from_user(size = 0x%zx) != 0\n", size);
            return -EFAULT;
        }
        n += size;
        user_buffer += size;
        user_size -= size;
        offset = 0; // offset is 0 for next entry
    }

    return n;
}

#endif // __AKOZLINS_DMABUF_H
