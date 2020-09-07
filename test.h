#ifndef __TEST_H__
#define __TEST_H__

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

#define INFO(fmt, ...) printf("I [%s] " fmt, __FUNCTION__, ##__VA_ARGS__)
#define ERR(fmt, ...) printf("E [%s] " fmt, __FUNCTION__, ##__VA_ARGS__)
#define FATAL(fmt, ...) printf("F [%s] " fmt, __FUNCTION__, ##__VA_ARGS__)

struct test_t {
    int fd = -1;
    uint32_t* addr = nullptr;

    void open(const char* file) {
        INFO("file = %s\n", file);
        fd = ::open(file, O_RDWR | O_CLOEXEC);
        if(fd < 0) {
            FATAL("open: errno = %d\n", errno);
            exit(EXIT_FAILURE);
        }
    }

    test_t() {
        open("/dev/dmabuf0");
    }
    ~test_t() {
        close(fd);
    }

    ssize_t seek_set(ssize_t offset = 0) const {
        INFO("offset = 0x%lx\n", offset);
        ssize_t pos = lseek(fd, offset, SEEK_SET);
        if(pos < 0) {
            FATAL("lseek(SEEK_SET) < 0\n");
            exit(EXIT_FAILURE);
        }
        return pos;
    }

    ssize_t seek_end(ssize_t offset = 0) const {
        INFO("offset = 0x%lx\n", offset);
        ssize_t pos = lseek(fd, offset, SEEK_END);
        if(pos < 0) {
            FATAL("lseek(SEEK_END) < 0\n");
            exit(EXIT_FAILURE);
        }
        return pos;
    }

    void mmap(size_t size, size_t offset) {
        INFO("size = 0x%lx, offset = 0x%lx\n", size, offset);
        addr = (uint32_t*)::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
        if(addr == MAP_FAILED) {
            FATAL("mmap: errno = %d\n", errno);
            exit(EXIT_FAILURE);
        }
    }

    void read(void* buffer, size_t size) const {
        INFO("size = 0x%lx\n", size);
        ssize_t n = ::read(fd, buffer, size);
        if(n != size) {
            FATAL("read: n = 0x%lx\n", n);
            exit(EXIT_FAILURE);
        }
    }

    void write(const void* buffer, size_t size) const {
        INFO("size = 0x%lx\n", size);
        ssize_t n = ::write(fd, buffer, size);
        if(n != size) {
            FATAL("write: n = 0x%lx\n", n);
            exit(EXIT_FAILURE);
        }
    }
};

#endif // __TEST_H__
