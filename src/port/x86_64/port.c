#define DMOD_ENABLE_REGISTRATION    ON
#include <errno.h>
#include "dmqspi_port.h"
#include "dmod.h"

/*
 * Host stub of the QUADSPI controller: every call fails with -ENOTSUP. It only lets
 * the core dmqspi module (and its tests) load on a x86_64 machine. Not a simulation.
 */

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    Dmod_Printf("dmqspi port module initialized (x86_64 host stub)\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, int, _init, ( const dmqspi_config_t *config ))
{
    (void)config;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _deinit, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _command, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd ))
{
    (void)instance;
    (void)cmd;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _read, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, void *data ))
{
    (void)instance;
    (void)cmd;
    (void)data;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _write, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const void *data ))
{
    (void)instance;
    (void)cmd;
    (void)data;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _auto_poll, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const dmqspi_poll_t *poll ))
{
    (void)instance;
    (void)cmd;
    (void)poll;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _enter_memory_mapped, ( dmqspi_instance_t instance, const dmqspi_command_t *read_cmd ))
{
    (void)instance;
    (void)read_cmd;
    return -ENOTSUP;
}

dmod_dmqspi_port_api_declaration(1.0, int, _exit_memory_mapped, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return -ENOTSUP;
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
    (void)instance;
    (void)offset;
    (void)size;

}
