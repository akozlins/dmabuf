
#include "test.h"

inline
void cuda_assert(cudaError_t cudaError, const char* function, const char* file, int line, bool abort = true) {
    if(cudaError == cudaSuccess) return;

    fprintf(stderr, "[%s] %s:%d, cudaError = %d (%s)\n", function, file, line, cudaError, cudaGetErrorString(cudaError));
    if(abort) exit(EXIT_FAILURE);
}

#define CUDA_ASSERT(cudaError) do { cuda_assert((cudaError), __FUNCTION__, __FILE__, __LINE__); } while(0)

__global__
void kernel1(uint32_t* values) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    values[i] = ~values[i];
}

__host__
int main() {
    int device = 0;
    CUDA_ASSERT(cudaSetDevice(device));
    CUDA_ASSERT(cudaSetDeviceFlags(cudaDeviceMapHost));
    cudaDeviceProp deviceProperties;
    CUDA_ASSERT(cudaGetDeviceProperties(&deviceProperties, device));

    test_t test;
    ssize_t size = test.seek_end(), offset = 0;
    test.mmap(size, offset);

    int nThreadsPerBlock = 1;
    while(2 * nThreadsPerBlock <= deviceProperties.maxThreadsPerBlock) nThreadsPerBlock *= 2;
    int nBlocks = size/4 / nThreadsPerBlock;
    printf("I [] nThreadsPerBlock = %d, nBlocks = %d\n", nThreadsPerBlock, nBlocks);

    uint32_t* wvalues;
//    wvalues = (uint32_t*)malloc(size);
//    cudaMallocHost(&wvalues, size);
    wvalues = test.addr;
    for(int i = 0; i < size/4; i++) wvalues[i] = i;
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
