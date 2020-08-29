
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <memory>

int main() {
    int fd = open("/dev/dmabuf0", O_RDWR | O_CLOEXEC);
    if(fd < 0) {
        printf("F [] open: errno = %d\n", errno);
        return EXIT_FAILURE;
    }

    ssize_t size = lseek(fd, 0, SEEK_END);
    printf("I [] size = %ld\n", size);
    if(lseek(fd, 0, SEEK_SET) < 0 || size < 0) {
        printf("F [] lseek < 0\n");
        return EXIT_FAILURE;
    }

    auto wbuffer = std::make_unique<uint32_t[]>(size/4);
    for(int i = 0; i < size/4; i++) wbuffer[i] = i;
    int wn = write(fd, wbuffer.get(), size);
    printf("I [] wn = %d\n", wn);

    auto mmap_addr = (uint32_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(mmap_addr == MAP_FAILED) {
        printf("F [] mmap: errno = %d\n", errno);
        return EXIT_FAILURE;
    }
    printf("I [] mmap_addr = %p\n", mmap_addr);
    for(int i = 0; i < size/4; i++) {
        if(mmap_addr[i] == wbuffer[i]) continue;
        printf("E [] mmap_addr[%d] != wbuffer[%d]\n", i, i);
    }
    munmap(mmap_addr, size);

    auto rbuffer = std::make_unique<uint32_t[]>(size/4);
    for(int i = 0; i < size/4; i++) rbuffer[i] = 0;
    if(lseek(fd, 0, SEEK_SET) < 0) {
        printf("F [] lseek < 0\n");
        return EXIT_FAILURE;
    }
    int rn = read(fd, rbuffer.get(), size);
    printf("I [] rn = %d\n", rn);
    for(int i = 0; i < size/4; i++) {
        if(rbuffer[i] == wbuffer[i]) continue;
        printf("E [] rbuffer[%d] != wbuffer[%d]\n", i, i);
    }

    close(fd);

    return 0;
}
