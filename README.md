# dmabuf

This is a kernel driver that allows to allocate buffer
that can be used for DMA and mapped to user space.

The buffer consist of smaller contiguous entries of up to 4 MB in size
that are allocated with `dma_alloc_coherent`.
The buffer can be mapped to user space through `mmap`
where each contiguous entry is mapped with `remap_pfn_range`.

Buffer de/allocation and stub implementations of `fops`
(`mmap`, `llseek`, `read` and `write`)
can be found in `dmabuf.h`.

The rest of the code implements the driver:

- `chrdev.h` - char device handling (de/allocation)
- `dmabuf_fops.h` - impl char device `fops` using stubs (from `dmabuf.h`)
- `dmabuf_platform_device.h` - dummy device
- `dmabuf_platform_driver.h` - driver probe (set DMA mask and create misc device)
