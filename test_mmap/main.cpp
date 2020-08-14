
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

    int fd = open("/dev/dmabuf", O_RDWR);
    printf("fd = %d, errno = %d\n", fd, errno);

    void* addr = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED) {
    }
    printf("addr = %p, errno = %d\n", addr, errno);
    munmap(addr, size);

    uint32_t* buffer = (uint32_t*)malloc(size);
    for(int i = 0; i < size/4; i++) buffer[i] = i;
    write(fd, buffer, size);

    read(fd, buffer, size);
    for(int i = 0; i < 1024; i++) {
        printf(" %08X", buffer[i]);
    }
    printf("\n");

    close(fd);
}
