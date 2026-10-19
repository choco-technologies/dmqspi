#include <errno.h>
#include "dmod.h"
#include "dmqspi_port.h"
#include "stm32_common.h"

/*
 * Shared QUADSPI controller implementation. NOT IMPLEMENTED - every function
 * is a stub returning -ENOSYS (they exist so the port module links).
 * Per-function register recipe: docs/port-implementation.md. Start with
 * _init, _command and _read (enough to send a command and read back data), then
 * _write/_auto_poll, then memory-mapped (docs/tasks.md, steps 1-3 and 6).
 * Registration (DMOD_ENABLE_REGISTRATION) is defined only in the family's port.c.
 */

dmod_dmqspi_port_api_declaration(1.0, int, _init, ( const dmqspi_config_t *config ))
{
    (void)config;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _deinit, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _command, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd ))
{
    (void)instance; (void)cmd;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _read, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, void *data ))
{
    (void)instance; (void)cmd; (void)data;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _write, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const void *data ))
{
    (void)instance; (void)cmd; (void)data;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _auto_poll, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const dmqspi_poll_t *poll ))
{
    (void)instance; (void)cmd; (void)poll;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _enter_memory_mapped, ( dmqspi_instance_t instance, const dmqspi_command_t *read_cmd ))
{
    (void)instance; (void)read_cmd;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, int, _exit_memory_mapped, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return -ENOSYS;
}

dmod_dmqspi_port_api_declaration(1.0, uintptr_t, _get_mapped_base, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, dmqspi_mode_t, _get_mode, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return dmqspi_mode_indirect;
}

dmod_dmqspi_port_api_declaration(1.0, void, _invalidate_mapped, ( dmqspi_instance_t instance, uint32_t offset, uint32_t size ))
{
    (void)instance; (void)offset; (void)size;
}
