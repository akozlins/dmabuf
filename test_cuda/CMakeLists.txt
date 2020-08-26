project(test_cuda)

find_package(CUDA 11)

if(CUDA_FOUND)
    set(CUDA_HOST_COMPILER "/bin/g++-9")
    set(CMAKE_CUDA_ARCHITECTURES 61)

    cuda_add_executable(test_cuda main.cu)
endif()
