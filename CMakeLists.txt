cmake_minimum_required(VERSION 3.17)

project(dmabuf LANGUAGES C CXX)

set(MODULE_NAME dmabuf)

file(GLOB MODULE_SOURCES
    module.c
    *.h
)

# `uname --kernel-release`
#set(KMODULE_KERNEL_RELEASE ...)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(kmodule REQUIRED)
add_kmodule(${PROJECT_NAME}
    NAME ${MODULE_NAME}
    ${MODULE_SOURCES}
)
add_dependencies(${PROJECT_NAME}-insmod ${PROJECT_NAME}-rmmod)

get_directory_property(parent_dir PARENT_DIRECTORY)
if(NOT parent_dir)
    add_custom_target(insmod
        COMMAND sudo chmod a+rw /dev/${MODULE_NAME}0
        VERBATIM
        DEPENDS ${PROJECT_NAME}-insmod
    )
endif()



add_executable(test_mmap test_mmap.cpp test.h)

find_package(CUDA 11)
if(CUDA_FOUND)
    set(CMAKE_CUDA_ARCHITECTURES 61)
    cuda_add_executable(test_cuda test_cuda.cu test.h)
endif()
