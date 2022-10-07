//

#include "test.h"

#include <memory>

int main() {
    test_t test;
    size_t size = test.seek_end(), offset = 0;

    // init write buffer
    auto wbuffer = std::make_unique<uint32_t[]>(size/4);
    for(int i = 0; i < size/4; i++) wbuffer[i] = i;

    // write to DMA buffer
    test.seek_set(offset);
    test.write(wbuffer.get(), size);

    // mmap and check that mmap'd DMA buffer == write buffer
    test.mmap(size, offset);
    for(int i = 0; i < size/4; i++) {
        if(test.addr[i] == wbuffer[i]) continue;
        ERR("mmap_addr[0x%x] != wbuffer[0x%x]\n", i, i);
    }

    // init read buffer
    auto rbuffer = std::make_unique<uint32_t[]>(size/4);
    for(int i = 0; i < size/4; i++) rbuffer[i] = 0;

    // read from DMA buffer
    test.seek_set(offset);
    test.read(rbuffer.get(), size);

    // check that read buffer == write buffer
    for(int i = 0; i < size/4; i++) {
        if(rbuffer[i] == wbuffer[i]) continue;
        ERR("rbuffer[0x%x] != wbuffer[0x%x]\n", i, i);
    }

    // cleanup
    munmap(test.addr, size);

    return 0;
}
