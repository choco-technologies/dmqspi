#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmqspi.h"

/*
 * dmdrvi driver skeleton ("driver_name=dmqspi" in the board .ini). NOT
 * IMPLEMENTED. Copy the structure of dmspi/src/dmspi.c (create/free/open/
 * close/read/write/ioctl/flush/stat) and of dmsdio/src/dmsdio.c (block
 * ioctls). What each function must do is in docs/architecture.md; the .ini
 * keys are in docs/configuration.md. Summary:
 *
 *   _create   : read [section] keys -> dmqspi_config_t, dmqspi_validate_config,
 *               dmqspi_flash_open; dev_num = { major from dmdevfs, minor = instance }.
 *               The node appears as /dev/dmqspi<instance>.
 *   _read     : dmqspi_flash_read(offset, buf, size)  (offset = byte offset)
 *   _write    : dmqspi_flash_write - caller erased first (block-device contract)
 *   _ioctl    : DMDRVI_IOCTL_BLOCK_GET_INFO  -> logical_block_size = 512 (or page),
 *                  erase_block_size = chip->sector_size, block_count, flags =
 *                  DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED;
 *               DMDRVI_IOCTL_BLOCK_ERASE -> dmqspi_flash_erase;
 *               DMQSPI_IOCTL_* (see dmqspi_types.h).
 *   _flush    : nothing (writes complete before returning)
 *   _free     : dmqspi_flash_close, free the context
 */
