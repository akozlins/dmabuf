cmake_minimum_required(VERSION 3.8)

project(dmabuf LANGUAGES C)

add_definitions(-Wall)

add_subdirectory(driver)
add_subdirectory(test_cuda)
add_subdirectory(test_mmap)
