project(driver LANGUAGES C)

set(MODULE_NAME dmabuf)

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
    COMMAND $(MAKE) -C ${KDIR} modules
        M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR}
        -E "MODULE_NAME := ${MODULE_NAME}"
    VERBATIM
    DEPENDS Kbuild ${MODULE_SOURCES}
)

add_custom_target(driver
    ALL
    DEPENDS ${MODULE_NAME}.ko
)



add_executable(clion_driver
    ${MODULE_SOURCES}
)
target_include_directories(clion_driver SYSTEM PRIVATE
    ${KDIR}/include
    ${KDIR}/include/uapi
    ${KDIR}/include/generated
    ${KDIR}/include/generated/uapi
    ${KDIR}/arch/x86/include
    ${KDIR}/arch/x86/include/uapi
    ${KDIR}/arch/x86/include/generated
    ${KDIR}/arch/x86/include/generated/uapi
)
target_compile_options(clion_driver PRIVATE
    -include ${KDIR}/include/linux/kconfig.h
    -include ${KDIR}/include/linux/compiler_types.h
    -D__KERNEL__
    -DMODULE
)
set_target_properties(clion_driver PROPERTIES
    EXCLUDE_FROM_ALL 1
)
