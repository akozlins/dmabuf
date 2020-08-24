cmake_minimum_required(VERSION 3.8)

project(dmabuf LANGUAGES C)

execute_process(OUTPUT_VARIABLE KERNEL_RELEASE
    COMMAND uname --kernel-release
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(KERNEL_RELEASE STREQUAL "")
    message(FATAL_ERROR "E [] `uname --kernel-release`")
endif()

set(KDIR "/lib/modules/${KERNEL_RELEASE}/build")
message("I [] KERNEL = '${KERNEL}'")

set(MODULE_NAME dmabuf)

file(GLOB MODULE_SOURCES
    module.c
    *.h
)

add_custom_command(OUTPUT ${MODULE_NAME}.ko
    COMMAND $(MAKE) -C ${KDIR} modules
        M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR}
        -E "MODULE_NAME := ${MODULE_NAME}"
    VERBATIM
    DEPENDS ${MODULE_SOURCES}
)

add_custom_target(dmabuf
    ALL
    DEPENDS ${MODULE_NAME}.ko
)



add_definitions(-Wall)

add_subdirectory(test_cuda)
add_subdirectory(test_mmap)
