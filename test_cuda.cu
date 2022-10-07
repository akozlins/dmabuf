//

#include "test.h"

inline
void cuda_assert(cudaError_t cudaError, const char* function, const char* file, int line, bool abort = true) {
    if(cudaError == cudaSuccess) return;

    fprintf(stderr, "F [%s] %s:%d, cudaError = %d (%s)\n", function, file, line, cudaError, cudaGetErrorString(cudaError));
    if(abort) exit(EXIT_FAILURE);
}

#define CUDA_ASSERT(cudaError) do { cuda_assert((cudaError), __FUNCTION__, __FILE__, __LINE__); } while(0)

struct cuda_t {
    int device = 0;

    cudaDeviceProp properties;

    cuda_t() {
        CUDA_ASSERT(cudaSetDevice(device));
        CUDA_ASSERT(cudaGetDeviceProperties(&properties, device));
    }
};

__global__
void kernel1(uint32_t* values) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;

    values[i] = ~values[i];
}

__host__
int main() {
    cuda_t cuda;
    CUDA_ASSERT(cudaSetDeviceFlags(cudaDeviceMapHost));

    INFO("pageableMemoryAccess = %d\n", cuda.properties.pageableMemoryAccess);
    int hostRegisterSupported = 0;
    CUDA_ASSERT(cudaDeviceGetAttribute(&hostRegisterSupported, cudaDevAttrHostRegisterSupported, cuda.device));
    INFO("hostRegisterSupported = %d\n", hostRegisterSupported);

    test_t test;
    ssize_t size = test.seek_end(), offset = 0;
    test.mmap(size, offset);

    int nThreadsPerBlock = 1;
    while(2 * nThreadsPerBlock <= cuda.properties.maxThreadsPerBlock) nThreadsPerBlock *= 2;
    int nBlocks = size/4 / nThreadsPerBlock;
    INFO("nThreadsPerBlock = %d, nBlocks = %d\n", nThreadsPerBlock, nBlocks);

    uint32_t* wvalues;
//    wvalues = (uint32_t*)malloc(size);
//    cudaMallocHost(&wvalues, size);
    wvalues = test.addr;
    for(int i = 0; i < size/4; i++) wvalues[i] = i;
//    CUDA_ASSERT(cudaHostRegister(wvalues, size, cudaHostRegisterDefault));

    // allocate device memory
    uint32_t* values_d;
    INFO("cudaMalloc\n");
    cudaMalloc(&values_d, size);

    INFO("cudaMemcpy\n");
    cudaMemcpy(values_d, wvalues, size, cudaMemcpyHostToDevice);

    // call kernel
    INFO("kernel1\n");
    kernel1<<<nBlocks, nThreadsPerBlock>>>(values_d);

    // allocate host memory
    uint32_t* rvalues;
    rvalues = (uint32_t*)malloc(size);
//    cudaMallocHost(&rvalues, size);

    // copy values from device to host
    INFO("cudaMemcpy\n");
    cudaMemcpy(rvalues, values_d, size, cudaMemcpyDeviceToHost);

    // check values
    int error = 0;
    for(int i = 0; i < size/4; i++) {
        if(rvalues[i] == ~wvalues[i]) continue;
        error = 1;
        ERR("rvalues[%d] = %d\n", i, rvalues[i]);
    }
    if(error == 0) INFO("OK\n");

    return 0;
}
