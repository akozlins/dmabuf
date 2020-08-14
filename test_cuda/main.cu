
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>

inline
void cuda_assert(cudaError_t cudaError, const char* file, int line, bool abort = true) {
    if(cudaError == cudaSuccess) return;

    fprintf(stderr, "[%s] %s:%d, cudaError = %d (%s)\n", __FUNCTION__, file, line, cudaError, cudaGetErrorString(cudaError));
    if(abort) exit(EXIT_FAILURE);
}

#define CUDA_ASSERT(val) { cuda_assert((val), __FILE__, __LINE__); }

__global__
void kernel1(uint32_t* values) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    values[i] = ~i;
}

__host__
int main() {
    CUDA_ASSERT(cudaSetDevice(0));
    CUDA_ASSERT(cudaSetDeviceFlags(cudaDeviceMapHost))

    int fd = open("/dev/dmabuf2", O_RDWR);
    if(fd == -1) {
        printf("F [] open: errno = %d\n", errno);
        return EXIT_FAILURE;
    }

    size_t size = 64 * 1024 * 4096;

    const int nThreadsPerBlock = 1024;
    const int nBlocks = size/4 / nThreadsPerBlock;

    uint32_t* wvalues;
//    wvalues = (uint32_t*)malloc(size);
    cudaMallocHost(&wvalues, size);
    for(int i = 0; i < size/4; i++) wvalues[i] = i;
    write(fd, wvalues, size);
    printf("I [] mmap\n");
    wvalues = (uint32_t*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(wvalues == MAP_FAILED) {
        printf("F [] mmap: errno = %d\n", errno);
        return EXIT_FAILURE;
    }
//    if(int error = mlock(wvalues, size)) {
//        printf("F [] mlock: error = %d\n", errno);
//        return EXIT_FAILURE;
//    }
//    CUDA_ASSERT(cudaHostRegister(wvalues, size, cudaHostRegisterPortable));

    // allocate device memory
    uint32_t* values_d;
    printf("I [] cudaMalloc\n");
    cudaMalloc(&values_d, size);

    printf("I [] cudaMemcpy\n");
    cudaMemcpy(values_d, wvalues, size, cudaMemcpyHostToDevice);

    // call kernel
    kernel1<<<nBlocks, nThreadsPerBlock>>>(values_d);

    // allocate host memory
    uint32_t* rvalues;
    rvalues = (uint32_t*)malloc(size);
//    cudaMallocHost(&rvalues, size);

    // copy values from device to host
    cudaMemcpy(rvalues, values_d, size, cudaMemcpyDeviceToHost);

    // check values
    int error = 0;
    for(int i = 0; i < size/4; i++) {
        if(rvalues[i] == ~wvalues[i]) continue;
        error = 1;
        printf("E [%s] rvalues[%d] = %d\n", __FUNCTION__, i, rvalues[i]);
    }
    if(error == 0) printf("I [%s] OK\n", __FUNCTION__);

    return 0;
}
