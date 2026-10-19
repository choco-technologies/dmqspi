# The x86_64 "port" is a host stub: it exists only so the core dmqspi module (which requires
# dmqspi_port) can be loaded by dmod_loader in the CI tests. It never ships as a release package.
set(DMOD_TOOLS_NAME "arch/x86_64" CACHE STRING "Name of the tools configuration")
