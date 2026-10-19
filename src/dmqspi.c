#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmqspi.h"

/*
 * Module entry + configuration helpers. The two big pieces live elsewhere:
 *   src/dmqspi_flash.c  - flash-chip layer (public API in include/dmqspi.h)
 *   src/dmqspi_driver.c - dmdrvi driver ("dmqspi" in the .ini) exposing the
 *                         block-device node
 * See docs/architecture.md and docs/tasks.md.
 */

/* Module state (also guarantees the module has a data section, which the dmod
 * packer requires). TODO: hold the opened flash handles here. */
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
