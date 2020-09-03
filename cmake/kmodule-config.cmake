
execute_process(OUTPUT_VARIABLE KMODULE_KERNEL_RELEASE
    COMMAND uname --kernel-release
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(KMODULE_KERNEL_RELEASE STREQUAL "")
    message(FATAL_ERROR "`uname --kernel-release`")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(kmodule
    REQUIRED_VARS KMODULE_KERNEL_RELEASE
)

set(KMODULE_KDIR "/lib/modules/${KMODULE_KERNEL_RELEASE}/build")

set(KMODULE_INCLUDE_DIRECTORIES
    ${KMODULE_KDIR}/include
    ${KMODULE_KDIR}/include/uapi
    ${KMODULE_KDIR}/include/generated
    ${KMODULE_KDIR}/include/generated/uapi
    ${KMODULE_KDIR}/arch/x86/include
    ${KMODULE_KDIR}/arch/x86/include/uapi
    ${KMODULE_KDIR}/arch/x86/include/generated
    ${KMODULE_KDIR}/arch/x86/include/generated/uapi
)
set(KMODULE_COMPILE_DEFINITIONS
    -include ${KMODULE_KDIR}/include/linux/kconfig.h
    -include ${KMODULE_KDIR}/include/linux/compiler_types.h
    -D__KERNEL__
    -DMODULE
)

function(add_kmodule TARGET_NAME)

    add_custom_command(OUTPUT ${TARGET_NAME}.ko
        COMMAND
            $(MAKE) -C ${KMODULE_KDIR} clean modules
            M=${CMAKE_CURRENT_BINARY_DIR} src=${CMAKE_CURRENT_SOURCE_DIR}
            -E "MODULE_NAME := ${TARGET_NAME}"
            W=1
        VERBATIM
        DEPENDS Kbuild ${ARGN}
    )

    add_custom_target(${TARGET_NAME}
        ALL
        DEPENDS ${TARGET_NAME}.ko
    )

    add_custom_target(${TARGET_NAME}-insmod
        COMMAND sudo insmod ${TARGET_NAME}.ko
        VERBATIM
        DEPENDS ${TARGET_NAME}.ko
    )

    add_custom_target(${TARGET_NAME}-rmmod
        COMMAND sudo rmmod ${TARGET_NAME} || true
        VERBATIM
    )

    add_library(${TARGET_NAME}-ide MODULE EXCLUDE_FROM_ALL
        ${ARGN}
    )
    target_include_directories(${TARGET_NAME}-ide SYSTEM PRIVATE
        ${KMODULE_INCLUDE_DIRECTORIES}
    )
    target_compile_options(${TARGET_NAME}-ide PRIVATE
        ${KMODULE_COMPILE_DEFINITIONS}
        -DKBUILD_BASENAME="${TARGET_NAME}"
        -DKBUILD_MODNAME="${TARGET_NAME}"
    )

endfunction()
