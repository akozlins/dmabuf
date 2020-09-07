cmake_minimum_required(VERSION 3.18)

project(dmabuf LANGUAGES C CXX)

file(GLOB MODULE_SOURCES
    kmodule.c
    *.h
)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(kmodule REQUIRED)
add_kmodule(dmabuf
    NAME dmabuf
    ${MODULE_SOURCES}
)
add_dependencies(dmabuf-insmod dmabuf-rmmod)

add_custom_target(insmod
    COMMAND sudo chmod a+rw /dev/dmabuf0
    VERBATIM
    DEPENDS dmabuf-insmod
)



add_executable(test_mmap test_mmap.cpp test.h)

find_package(CUDA 11)
if(CUDA_FOUND)
    set(CUDA_HOST_COMPILER "/bin/g++-9")
    set(CMAKE_CUDA_ARCHITECTURES 61)
    cuda_add_executable(test_cuda test_cuda.cu)
endif()
