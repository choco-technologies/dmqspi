#include <errno.h>
#include "dmod.h"
#include "dmqspi.h"
#include "dmqspi_port.h"
#include "dmqspi_flash_cmds.h"

/*
 * Flash-chip layer. NOT IMPLEMENTED - every function below is a stub that
 * returns an error. Implementation order and per-function algorithms are in
 * docs/tasks.md (step 2) and docs/flash-chips.md.
 */

struct dmqspi_flash
{
    dmqspi_config_t         config;
    const dmqspi_chip_t    *chip;      /* matched row of g_dmqspi_chips */
    bool                    mapped;    /* memory-mapped mode active */
    /* TODO: a dmosi mutex if the core (not only the port) needs one */
};

dmod_dmqspi_api_declaration(1.0, dmqspi_flash_t, _flash_open, ( const dmqspi_config_t *config ))
{
    /* TODO: validate -> dmqspi_port_init -> reset chip (66/99) -> READ ID ->
     *       match g_dmqspi_chips -> enable quad -> set dummy cycles. */
    (void)config;
    return NULL;
}

dmod_dmqspi_api_declaration(1.0, void, _flash_close, ( dmqspi_flash_t flash ))
{
    /* TODO: leave memory-mapped mode, dmqspi_port_deinit, free. */
    (void)flash;
}

dmod_dmqspi_api_declaration(1.0, const dmqspi_chip_t*, _flash_get_chip, ( dmqspi_flash_t flash ))
{
    return flash != NULL ? flash->chip : NULL;
}

dmod_dmqspi_api_declaration(1.0, int, _flash_read, ( dmqspi_flash_t flash, uint32_t offset, void *buffer, size_t size ))
{
    /* TODO: bounds check; exit memory-mapped if needed; one EB (quad IO fast
     *       read) command with data_size = size (the controller streams; no page limit). */
    (void)flash; (void)offset; (void)buffer; (void)size;
    return -ENOSYS;
}

dmod_dmqspi_api_declaration(1.0, int, _flash_write, ( dmqspi_flash_t flash, uint32_t offset, const void *buffer, size_t size ))
{
    /* TODO: loop per page: WREN, 0x32/0x02 with <= (page_size - offset%page_size) bytes,
     *       auto_poll WIP==0, check flag status for errors; invalidate mapped cache. */
    (void)flash; (void)offset; (void)buffer; (void)size;
    return -ENOSYS;
}

dmod_dmqspi_api_declaration(1.0, int, _flash_erase, ( dmqspi_flash_t flash, uint32_t offset, size_t size ))
{
    /* TODO: require sector alignment; per 4 KB sector: WREN, 0x20, auto_poll WIP==0
     *       (timeout ~ 400 ms/sector worst case); use 0xD8 for aligned 64 KB runs. */
    (void)flash; (void)offset; (void)size;
    return -ENOSYS;
}

dmod_dmqspi_api_declaration(1.0, int, _flash_erase_chip, ( dmqspi_flash_t flash ))
{
    /* TODO: WREN, 0xC7, auto_poll with a long timeout (tens of seconds). */
    (void)flash;
    return -ENOSYS;
}

dmod_dmqspi_api_declaration(1.0, int, _flash_set_memory_mapped, ( dmqspi_flash_t flash, bool enable ))
{
    /* TODO: enable -> dmqspi_port_enter_memory_mapped(read cmd EB); disable -> exit. */
    (void)flash; (void)enable;
    return -ENOSYS;
}
