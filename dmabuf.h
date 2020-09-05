
#include "kmodule.h"

#include <linux/dma-direct.h>

struct dmabuf_entry {
    size_t size;
    void* cpu_addr;
    dma_addr_t dma_handle;
    struct list_head list_head;
};

#define dmabuf_entry_for_each(pos, head) \
    for(struct dmabuf_entry* pos = NULL; pos == NULL; pos = ERR_PTR(-EINVAL)) \
    list_for_each_entry(pos, head, list_head)

struct dmabuf {
    struct device* dev;
    size_t size;
    struct list_head entries;
};

#include <linux/list_sort.h>

static
int dmabuf_entry_cmp(void* priv, struct list_head* a, struct list_head* b) {
    dma_addr_t aa = container_of(a, struct dmabuf_entry, list_head)->dma_handle;
    dma_addr_t bb = container_of(b, struct dmabuf_entry, list_head)->dma_handle;
    if(aa < bb) return -1;
    if(aa > bb) return +1;
    return 0;
}

static
void dmabuf_free(struct dmabuf* dmabuf) {
    M_INFO("\n");

    if(IS_ERR_OR_NULL(dmabuf)) return;

    dmabuf_entry_for_each(entry, &dmabuf->entries) {
        M_DEBUG("dma_free_coherent(dma_handle = 0x%llx, size = 0x%lx)\n", entry->dma_handle, entry->size);
        dma_free_coherent(dmabuf->dev, entry->size, entry->cpu_addr, entry->dma_handle);
        kfree(entry);
    }

    kfree(dmabuf);
}

static
struct dmabuf* dmabuf_alloc(struct device* dev, size_t size) {
    int error;
    size_t entry_size = PAGE_SIZE << 10;
    struct dmabuf* dmabuf = NULL;

    M_INFO("size = 0x%lx\n", size);

    if(dev == NULL || size == 0 || !IS_ALIGNED(size, PAGE_SIZE)) {
        error = -EINVAL;
        goto err_out;
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


retry:
        entry->size = entry_size;
        M_DEBUG("dma_alloc_coherent(size = 0x%lx)\n", entry->size);
        entry->cpu_addr = dma_alloc_coherent(dmabuf->dev, entry->size, &entry->dma_handle, GFP_ATOMIC | __GFP_NOWARN); // see `pci_alloc_consistent`
        if(IS_ERR_OR_NULL(entry->cpu_addr)) {
            error = PTR_ERR(entry->cpu_addr);
            if(error == 0) error = -ENOMEM;
            M_ERR("dma_alloc_coherent(size = 0x%lx): error = %d\n", entry->size, error);
            if(entry_size > PAGE_SIZE) { entry_size /= 2; goto retry; }
            kfree(entry);
            goto err_out;
        }

        INIT_LIST_HEAD(&entry->list_head);
        list_add(&entry->list_head, &dmabuf->entries);

        dmabuf->size += entry->size;
    }

    // sort by dma_handle
    list_sort(NULL, &dmabuf->entries, dmabuf_entry_cmp);

    // report contiguous DMA handles
    dmabuf_entry_for_each(entry, &dmabuf->entries) {
        dma_addr_t dma_handle = entry->dma_handle;
        size_t dma_size = 0;
        while(dma_handle + dma_size == entry->dma_handle) {
            dma_size += entry->size;
            if(list_is_last(&entry->list_head, &dmabuf->entries)) break;
            entry = list_next_entry(entry, list_head);
        }
        M_INFO("dma_handle = 0x%llx, dma_size = 0x%lx", dma_handle, dma_size);
    }

    return dmabuf;

err_out:
    dmabuf_free(dmabuf);
    return ERR_PTR(error);
}

static
loff_t dmabuf_llseek(struct dmabuf* dmabuf, struct file* file, loff_t loff, int whence) {
    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -EFAULT;
    }

    if(whence == SEEK_END && 0 <= -loff && -loff <= dmabuf->size) {
        file->f_pos = dmabuf->size + loff;
        return file->f_pos;
    }

    if(whence == SEEK_SET && 0 <= loff && loff <= dmabuf->size) {
        file->f_pos = loff;
        return file->f_pos;
    }

    M_ERR("loff = 0x%llx, whence = %d\n", loff, whence);
    return -EINVAL;
}

static
int dmabuf_mmap(struct dmabuf* dmabuf, struct vm_area_struct* vma) {
    int error = 0;
    size_t offset = 0;

    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -EFAULT;
    }

    M_INFO("vm_start = 0x%lx, vm_pgoff = 0x%lx, vma_pages = 0x%lx\n", vma->vm_start, vma->vm_pgoff, vma_pages(vma));

    if(vma_pages(vma) != PAGE_ALIGN(dmabuf->size) >> PAGE_SHIFT) {
        return -EINVAL;
    }
    if(vma->vm_pgoff != 0) {
        return -EINVAL;
    }

    vma->vm_flags |= VM_LOCKED | VM_IO | VM_DONTEXPAND;
    // see `https://www.kernel.org/doc/html/latest/x86/pat.html#advanced-apis-for-drivers`
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); // see `pgprot_dmacoherent`

    dmabuf_entry_for_each(entry, &dmabuf->entries) {
        unsigned long pfn = PHYS_PFN(dma_to_phys(dmabuf->dev, entry->dma_handle)); // see `dma_direct_mmap`

        M_DEBUG("remap_pfn_range(pfn = 0x%lx, size = 0x%lx)\n", pfn, entry->size);
        error = remap_pfn_range(vma,
            vma->vm_start + offset,
            pfn,
            entry->size, vma->vm_page_prot
        );
        if(error) {
            M_ERR("remap_pfn_range(pfn = 0x%lx, size = 0x%lx): error = %d\n", pfn, entry->size, error);
            break;
        }

        offset += entry->size;
    }

    return error;
}

static
ssize_t dmabuf_read(struct dmabuf* dmabuf, char __user* user_buffer, size_t user_size, loff_t offset) {
    ssize_t n = 0;

    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -EFAULT;
    }

    dmabuf_entry_for_each(entry, &dmabuf->entries) {
        size_t size;
        if(user_size == 0) break;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }

        size = entry->size - offset;
        if(size > user_size) size = user_size;

        M_DEBUG("copy_to_user(size = 0x%lx)\n", size);
        if(copy_to_user(user_buffer, entry->cpu_addr + offset, size)) {
            M_ERR("copy_to_user(size = 0x%lx) != 0\n", size);
            return -EFAULT;
        }
        n += size;
        user_buffer += size;
        user_size -= size;
        offset = 0;
    }

    return n;
}

static
ssize_t dmabuf_write(struct dmabuf* dmabuf, const char __user* user_buffer, size_t user_size, loff_t offset) {
    ssize_t n = 0;

    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -EFAULT;
    }

    dmabuf_entry_for_each(entry, &dmabuf->entries) {
        size_t size;
        if(user_size == 0) break;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }

        size = entry->size - offset;
        if(size > user_size) size = user_size;

        M_DEBUG("copy_from_user(size = 0x%lx)\n", size);
        if(copy_from_user(entry->cpu_addr + offset, user_buffer, size)) {
            M_ERR("copy_from_user(size = 0x%lx) != 0\n", size);
            return -EFAULT;
        }
        n += size;
        user_buffer += size;
        user_size -= size;
        offset = 0;
    }

    return n;
}
