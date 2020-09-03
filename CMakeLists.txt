cmake_minimum_required(VERSION 3.18)

project(dmabuf LANGUAGES C CXX)

file(GLOB MODULE_SOURCES
    kmodule.c
    *.h
)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(kmodule REQUIRED)
add_kmodule(${PROJECT_NAME} ${MODULE_SOURCES})



add_executable(test_mmap test_mmap.cpp)

find_package(CUDA 11)
if(CUDA_FOUND)
    set(CUDA_HOST_COMPILER "/bin/g++-9")
    set(CMAKE_CUDA_ARCHITECTURES 61)
    cuda_add_executable(test_cuda test_cuda.cu)
endif()
