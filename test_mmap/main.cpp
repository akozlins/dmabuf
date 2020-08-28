
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
    int fd = open("/dev/dmabuf", O_RDWR | O_CLOEXEC);
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
    printf("wn = %d\n", wn);

    auto mmap_addr = (uint32_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(mmap_addr == MAP_FAILED) {
        printf("F [] mmap: errno = %d\n", errno);
        return EXIT_FAILURE;
    }
    printf("mmap_addr = %p, errno = %d\n", mmap_addr, errno);
    for(size_t i = 0; i < size/4; i++) {
        if(mmap_addr[i] == wbuffer[i]) continue;
        printf("E [] mmap_addr[%ld] != wbuffer[%ld]\n", i, i);
    }
    munmap(mmap_addr, size);

    auto rbuffer = (uint32_t*)malloc(size);
    for(size_t i = 0; i < size/4; i++) rbuffer[i] = 0;
    if(lseek(fd, 0, SEEK_SET) == -1) {
        printf("F [] lseek = -1\n");
        return EXIT_FAILURE;
    }
    int rn = read(fd, rbuffer, size);
    printf("rn = %d\n", rn);
    for(size_t i = 0; i < size/4; i++) {
        if(rbuffer[i] == wbuffer[i]) continue;
        printf("E [] rbuffer[%ld] != wbuffer[%ld]\n", i, i);
    }

    close(fd);

    return 0;
}
