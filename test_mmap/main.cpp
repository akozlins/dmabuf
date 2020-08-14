
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int fd = open("/dev/dmabuf", O_RDWR);
    if(fd == -1) {
        printf("F [] open: errno = %d\n", fd, errno);
        return EXIT_FAILURE;
    }

    size_t size = 64 * 1024 * 4096;

    uint32_t* wbuffer = (uint32_t*)malloc(size);
    for(int i = 0; i < size/4; i++) wbuffer[i] = i;
    lseek(fd, 0, SEEK_SET);
    int wn = write(fd, wbuffer, size);
    printf("wn = %d\n", wn);

    uint32_t* mmap_addr = (uint32_t*)mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if(mmap_addr == MAP_FAILED) {
        printf("F [] mmap: errno = %d\n", errno);
        return EXIT_FAILURE;
    }
    printf("mmap_addr = %p, errno = %d\n", mmap_addr, errno);
    for(int i = 0; i < size/4; i++) {
        if(mmap_addr[i] == wbuffer[i]) continue;
        printf("E [] mmap_addr[%d] != wbuffer[%d]\n", i, i);
    }
    munmap(mmap_addr, size);

    uint32_t* rbuffer = (uint32_t*)malloc(size);
    for(int i = 0; i < size/4; i++) rbuffer[i] = 0;
    lseek(fd, 0, SEEK_SET);
    int rn = read(fd, rbuffer, size);
    printf("rn = %d\n", rn);
    for(int i = 0; i < size/4; i++) {
        if(rbuffer[i] == wbuffer[i]) continue;
        printf("E [] rbuffer[%d] != wbuffer[%d]\n", i, i);
    }

    close(fd);

    return 0;
}
