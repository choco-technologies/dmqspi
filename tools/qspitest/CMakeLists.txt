# qspitest - on-board test of a dmqspi bus node with a Micron N25Q-style flash on it.
# Added via add_subdirectory() from the top-level CMakeLists.txt (shares its dmod setup).
set(DMOD_DMR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/qspitest.dmr)

dmod_add_executable(qspitest ${DMOD_MODULE_VERSION}
    main.c
)

# dmdrvi: headers only (DMDRVI_IOCTL_CUSTOM_BASE, the base of the DMQSPI_IOCTL_* numbers).
dmod_link_modules(qspitest
    dmdrvi@>=2.0
)

target_include_directories(qspitest PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
