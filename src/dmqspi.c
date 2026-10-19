#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmqspi.h"
#include "dmqspi_port.h"

/*
 * The dmqspi bus driver: module entry, configuration and the dmdrvi
 * interface ("driver_name=dmqspi" in the board .ini). Copy the structure of
 * dmspi/src/dmspi.c. NOT IMPLEMENTED yet - the dmdrvi functions below are
 * to be written (docs/tasks.md, step 3):
 *
 *   _create   : read [section] keys (docs/configuration.md) -> dmqspi_config_t,
 *               dmqspi_validate_config, dmqspi_port_init; dev_num minor = instance.
 *               The node appears as /dev/dmqspi<instance>.
 *   _free     : dmqspi_port_deinit, free the context
 *   _open/_close/_flush/_stat : trivial (no per-handle state)
 *   _read/_write : not supported (-ENOTSUP) - the bus has no byte stream; use ioctl.
 *   _ioctl    : DMQSPI_IOCTL_TRANSFER      -> dmqspi_port_command / _read / _write
 *               DMQSPI_IOCTL_AUTO_POLL     -> dmqspi_port_auto_poll
 *               DMQSPI_IOCTL_ENTER_MAPPED  -> dmqspi_port_enter_memory_mapped
 *               DMQSPI_IOCTL_EXIT_MAPPED   -> dmqspi_port_exit_memory_mapped
 *               DMQSPI_IOCTL_GET_MAPPED_BASE / GET_MODE / INVALIDATE_MAPPED -> port getters
 *               Validate every pointer; the buffers belong to the caller.
 */

/* Module state (also guarantees the module has a data section, which the dmod
 * packer requires). TODO: hold the bus contexts here. */
static int g_dmqspi_ref_count = 1;

dmod_dmqspi_api_declaration(1.0, bool, _validate_config, ( const dmqspi_config_t *config ))
{
    if (config == NULL || config->instance == 0)
        return false;
    if (config->flash_size_bytes == 0 || (config->flash_size_bytes & (config->flash_size_bytes - 1)) != 0)
        return false;       /* FSIZE needs a power of two */
    if (config->clock_hz == 0)
        return false;
    if (config->cs_high_cycles < 1 || config->cs_high_cycles > 8)
        return false;
    if (config->bank != dmqspi_bank_1 && config->bank != dmqspi_bank_2)
        return false;
    return true;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    Dmod_Printf("dmqspi: skeleton, not implemented yet\n");
    return g_dmqspi_ref_count > 0 ? 0 : -1;
}

int dmod_deinit(void)
{
    return 0;
}
