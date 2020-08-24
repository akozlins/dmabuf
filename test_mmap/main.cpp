
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int fd = open("/dev/dmabuf", O_RDWR | O_CLOEXEC);
    if(fd < 0) {
        printf("F [] open: errno = %d\n", errno);
        return EXIT_FAILURE;
    }

    size_t size = 64 * 1024 * 4096;

    uint32_t* wbuffer = (uint32_t*)malloc(size);
    for(size_t i = 0; i < size/4; i++) wbuffer[i] = i;
    if(lseek(fd, 0, SEEK_SET) == -1) {
        printf("F [] lseek = -1\n");
        return EXIT_FAILURE;
    }
    int wn = write(fd, wbuffer, size);
    printf("wn = %d\n", wn);

    uint32_t* mmap_addr = (uint32_t*)mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
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

    uint32_t* rbuffer = (uint32_t*)malloc(size);
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
