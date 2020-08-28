cmake_minimum_required(VERSION 3.18)

project(dmabuf LANGUAGES C)

set(MODULE_NAME ${PROJECT_NAME})

execute_process(OUTPUT_VARIABLE KERNEL_RELEASE
    COMMAND uname --kernel-release
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(KERNEL_RELEASE STREQUAL "")
    message(FATAL_ERROR "E [] `uname --kernel-release`")
endif()

set(KDIR "/lib/modules/${KERNEL_RELEASE}/build")
message("I [] KDIR = '${KDIR}'")

file(GLOB MODULE_SOURCES
    ${MODULE_NAME}.c
    *.h
)

add_custom_command(OUTPUT ${MODULE_NAME}.ko
    COMMAND $(MAKE) -C ${KDIR} clean modules
        M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR}
        -E "MODULE_NAME := ${MODULE_NAME}"
    VERBATIM
    DEPENDS Kbuild ${MODULE_SOURCES}
)

add_custom_target(${MODULE_NAME}_module
    ALL
    DEPENDS ${MODULE_NAME}.ko
)



add_custom_target(${MODULE_NAME}_insmod
    COMMAND sudo insmod ${MODULE_NAME}.ko
    COMMAND sudo chmod a+rw /dev/${MODULE_NAME}0
    VERBATIM
    DEPENDS ${MODULE_NAME}.ko
)

add_custom_target(${MODULE_NAME}_rmmod
    COMMAND sudo rmmod ${MODULE_NAME}
    VERBATIM
)

add_subdirectory(test_cuda)
add_subdirectory(test_mmap)



add_executable(${MODULE_NAME}_module_clion
    ${MODULE_SOURCES}
)
target_include_directories(${MODULE_NAME}_module_clion SYSTEM PRIVATE
    ${KDIR}/include
    ${KDIR}/include/uapi
    ${KDIR}/include/generated
    ${KDIR}/include/generated/uapi
    ${KDIR}/arch/x86/include
    ${KDIR}/arch/x86/include/uapi
    ${KDIR}/arch/x86/include/generated
    ${KDIR}/arch/x86/include/generated/uapi
)
target_compile_options(${MODULE_NAME}_module_clion PRIVATE
    -include ${KDIR}/include/linux/kconfig.h
    -include ${KDIR}/include/linux/compiler_types.h
    -D__KERNEL__
    -DMODULE
)
set_target_properties(${MODULE_NAME}_module_clion PROPERTIES
    EXCLUDE_FROM_ALL 1
)
