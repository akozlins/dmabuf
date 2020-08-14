
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    size_t size = 16 * 1024 * 4096;

    int fd = open("/dev/dmabuf2", O_RDWR);
    printf("fd = %d, errno = %d\n", fd, errno);

    uint32_t* wbuffer = (uint32_t*)malloc(size);
    for(int i = 0; i < size/4; i++) wbuffer[i] = i;
//    lseek(fd, 0, SEEK_SET);
    int wn = write(fd, wbuffer, size);
    printf("wn = %d\n", wn);

    void* mmap_addr = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if(mmap_addr == MAP_FAILED) {
        printf("E []\n");
        return 1;
    }
    printf("mmap_addr = %p, errno = %d\n", mmap_addr, errno);
    for(int i = 0; i < size/4; i++) {
        if(((uint32_t*)mmap_addr)[i] == wbuffer[i]) continue;
        printf("E [] mmap_addr[%d] != wbuffer[%d]\n", i, i);
    }
    munmap(mmap_addr, size);

    uint32_t* rbuffer = (uint32_t*)malloc(size);
    for(int i = 0; i < size/4; i++) rbuffer[i] = 0;
//    lseek(fd, 0, SEEK_SET);
    int rn = read(fd, rbuffer, size);
    printf("rn = %d\n", rn);
    for(int i = 0; i < size/4; i++) {
        if(rbuffer[i] == wbuffer[i]) continue;
        printf("E [] rbuffer[%d] != wbuffer[%d]\n", i, i);
    }

    close(fd);

    return 0;
}
