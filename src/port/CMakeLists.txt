# ======================================================================
#               dmqspi_port Module Configuration
# ======================================================================

# Name of the module
set(DMOD_MODULE_NAME        dmqspi_port)

# Version is already set above and used in project()

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Path to the DMR file for this module
set(DMOD_DMR_PATH           ${CMAKE_SOURCE_DIR}/dmqspi_port.dmr)

# Every STM32 family with a QUADSPI shares the register layout, so the
# controller logic lives in stm32_common/ and each family's port.c is a thin
# wrapper (lifecycle + IRQ handler + family constants). See
# src/port/stm32_common/README.md.
set(COMMON_SOURCES "")
if(DMOD_CPU_FAMILY MATCHES "^stm32")
    set(COMMON_SOURCES stm32_common/stm32_common.c)
endif()

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    ${DMOD_CPU_FAMILY}/port.c
    ${COMMON_SOURCES}
)

# dmclk_port: peripheral clock enable + the QSPI kernel clock frequency;
# dmosi: mutex/semaphore for the IRQ-driven transfers; dmdrvi: headers only.
# One dmod_link_modules() call (each call rewrites the requirements file).
dmod_link_modules(${DMOD_MODULE_NAME}
    dmclk_port
    dmosi
    dmdrvi@>=2.0
)

target_include_directories(${DMOD_MODULE_NAME} BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)
