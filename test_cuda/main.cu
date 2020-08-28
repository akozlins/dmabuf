
#include <cerrno>
#include <cstdint>
#include <cstdio>

#include <fcntl.h>

#include <sys/mman.h>

inline
void cuda_assert(cudaError_t cudaError, const char* function, const char* file, int line, bool abort = true) {
    if(cudaError == cudaSuccess) return;

    fprintf(stderr, "[%s] %s:%d, cudaError = %d (%s)\n", function, file, line, cudaError, cudaGetErrorString(cudaError));
    if(abort) exit(EXIT_FAILURE);
}

#define CUDA_ASSERT(val) do { cuda_assert((val), __FUNCTION__, __FILE__, __LINE__); } while(0)

__global__
void kernel1(uint32_t* values) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    values[i] = ~values[i];
}

__host__
int main() {
    CUDA_ASSERT(cudaSetDevice(0));
    CUDA_ASSERT(cudaSetDeviceFlags(cudaDeviceMapHost));

    printf("I [] open('/dev/dmabuf')\n");
    int fd = open("/dev/dmabuf", O_RDWR);
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

    const int nThreadsPerBlock = 1024;
    const int nBlocks = size/4 / nThreadsPerBlock;

    uint32_t* wvalues;
//    wvalues = (uint32_t*)malloc(size);
    cudaMallocHost(&wvalues, size);
    for(int i = 0; i < size/4; i++) wvalues[i] = i;
    write(fd, wvalues, size);
    printf("I [] mmap\n");
    wvalues = (uint32_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(wvalues == MAP_FAILED) {
        printf("F [] mmap: errno = %d\n", errno);
        return EXIT_FAILURE;
    }
//    CUDA_ASSERT(cudaHostRegister(wvalues, size, cudaHostRegisterDefault));

    // allocate device memory
    uint32_t* values_d;
    printf("I [] cudaMalloc\n");
    cudaMalloc(&values_d, size);

    printf("I [] cudaMemcpy\n");
    cudaMemcpy(values_d, wvalues, size, cudaMemcpyHostToDevice);

    // call kernel
    printf("I [] kernel1\n");
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
