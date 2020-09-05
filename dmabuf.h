
#include "kmodule.h"

#include <linux/dma-direct.h>

struct dmabuf_entry {
    size_t size;
    void* cpu_addr;
    dma_addr_t dma_handle;
};

struct dmabuf {
    struct device* dev;
    size_t size;
    int count;
    struct dmabuf_entry entries[];
};

static
void dmabuf_free(struct dmabuf* dmabuf) {
    struct dmabuf_entry* entry;

    M_INFO("\n");

    if(IS_ERR_OR_NULL(dmabuf)) return;

    for(int i = 0; i < dmabuf->count; i++) {
        entry = &dmabuf->entries[i];
        if(entry->size == 0) continue;

        M_DEBUG("dma_free_coherent(dma_handle = 0x%llx))\n", entry->dma_handle);
        dma_free_coherent(dmabuf->dev, entry->size, entry->cpu_addr, entry->dma_handle);
    }

    kfree(dmabuf);
}

static
struct dmabuf* dmabuf_alloc(struct device* dev, size_t size) {
    int error;
    struct dmabuf* dmabuf;
    int count = ALIGN(size, PAGE_SIZE << 10) / (PAGE_SIZE << 10);

    M_INFO("size = 0x%lx\n", size);

    dmabuf = kzalloc(sizeof(*dmabuf) + count * sizeof(dmabuf->entries[0]), GFP_KERNEL);
    if(IS_ERR_OR_NULL(dmabuf)) {
        error = PTR_ERR(dmabuf);
        if(error == 0) error = -ENOMEM;
        dmabuf = NULL;
        M_ERR("kzalloc: error = %d\n", error);
        goto err_out;
    }

    dmabuf->dev = dev;
    dmabuf->count = count;

    for(int i = 0; i < dmabuf->count; i++) {
        struct dmabuf_entry* entry = &dmabuf->entries[i];

        entry->size = PAGE_SIZE << 10;

        M_DEBUG("dma_alloc_coherent(size = 0x%lx)\n", entry->size);
        entry->cpu_addr = dma_alloc_coherent(dmabuf->dev, entry->size, &entry->dma_handle, GFP_ATOMIC); // see `pci_alloc_consistent`
        if(IS_ERR_OR_NULL(entry->cpu_addr)) {
            error = PTR_ERR(entry->cpu_addr);
            if(error == 0) error = -ENOMEM;
            M_ERR("dma_alloc_coherent: error = %d\n", error);
            entry->size = 0;
            goto err_out;
        }

        dmabuf->size += entry->size;
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

    for(int i = 0; i < dmabuf->count; i++) {
        struct dmabuf_entry* entry = &dmabuf->entries[i];
        unsigned long pfn = PHYS_PFN(dma_to_phys(dmabuf->dev, entry->dma_handle)); // see `dma_direct_mmap`

        M_DEBUG("remap_pfn_range(pfn = 0x%lx, size = 0x%lx)\n", pfn, entry->size);
        error = remap_pfn_range(vma,
            vma->vm_start + offset,
            pfn,
            entry->size, vma->vm_page_prot
        );
        if(error) {
            M_ERR("remap_pfn_range: error = %d\n", error);
            break;
        }

        offset += entry->size;
    }

    return error;
}

static
ssize_t dmabuf_read(struct dmabuf* dmabuf, char __user* user_buffer, size_t size, loff_t offset) {
    ssize_t n = 0;
    struct dmabuf_entry* entry;

    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -EFAULT;
    }

    for(int i = 0; i < dmabuf->count; i++) {
        size_t k;
        entry = &dmabuf->entries[i];
        if(size == 0) break;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }

        k = entry->size - offset;
        if(k > size) k = size;

        M_DEBUG("copy_to_user(size = 0x%lx)\n", k);
        if(copy_to_user(user_buffer, entry->cpu_addr + offset, k)) {
            M_ERR("copy_to_user != 0\n");
            return -EFAULT;
        }
        n += k;
        user_buffer += k;
        size -= k;
        offset = 0;
    }

    return n;
}

static
ssize_t dmabuf_write(struct dmabuf* dmabuf, const char __user* user_buffer, size_t size, loff_t offset) {
    ssize_t n = 0;
    struct dmabuf_entry* entry;

    if(dmabuf == NULL) {
        M_ERR("dmabuf == NULL\n");
        return -EFAULT;
    }

    for(int i = 0; i < dmabuf->count; i++) {
        size_t k;
        entry = &dmabuf->entries[i];
        if(size == 0) break;
        if(offset >= entry->size) {
            offset -= entry->size;
            continue;
        }

        k = entry->size - offset;
        if(k > size) k = size;

        M_DEBUG("copy_from_user(size = 0x%lx)\n", k);
        if(copy_from_user(entry->cpu_addr + offset, user_buffer, k)) {
            M_ERR("copy_from_user != 0\n");
            return -EFAULT;
        }
        n += k;
        user_buffer += k;
        size -= k;
        offset = 0;
    }

    return n;
}
