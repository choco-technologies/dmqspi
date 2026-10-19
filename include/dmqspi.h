#ifndef DMQSPI_H
#define DMQSPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmqspi_defs.h"
#include "dmqspi_types.h"

/*
 * Public API of dmqspi = the flash-chip layer. It turns the raw controller
 * (dmqspi_port) into "a NOR flash you can read, program and erase".
 *
 * Normal clients (littlefs, automount, shell) do NOT use this header: they
 * open the block-device node /dev/dmqspi<N> through dmdrvi (read/write at
 * byte offsets + DMDRVI_IOCTL_BLOCK_GET_INFO / _ERASE). This API exists for
 * the driver itself, for tests, and for tools that already hold a handle.
 *
 * All functions return 0 / bytes on success, negative errno on failure.
 * Offsets are bytes from the start of the chip. See docs/architecture.md.
 */

/** Opaque flash handle - the struct lives in src/dmqspi_flash.c */
typedef struct dmqspi_flash* dmqspi_flash_t;

/** Check a configuration without touching hardware (safe in host tests). */
dmod_dmqspi_api(1.0, bool, _validate_config, ( const dmqspi_config_t *config ));

/** Init the controller (port), read the JEDEC ID, match it in the chip table
 *  (fallback: SFDP), enable quad mode. NULL on failure. */
dmod_dmqspi_api(1.0, dmqspi_flash_t, _flash_open, ( const dmqspi_config_t *config ));

/** Release the chip and deinit the controller. NULL-safe. */
dmod_dmqspi_api(1.0, void, _flash_close, ( dmqspi_flash_t flash ));

/** Identified chip description (never NULL for a valid handle). */
dmod_dmqspi_api(1.0, const dmqspi_chip_t*, _flash_get_chip, ( dmqspi_flash_t flash ));

/** Read `size` bytes at `offset`. Uses quad fast-read; transparently leaves
 *  memory-mapped mode if active. Returns bytes read. */
dmod_dmqspi_api(1.0, int, _flash_read, ( dmqspi_flash_t flash, uint32_t offset, void *buffer, size_t size ));

/** Program `size` bytes at `offset`. The area MUST be erased first. Splits
 *  at page boundaries (WREN + page program + wait WIP each). Returns bytes written. */
dmod_dmqspi_api(1.0, int, _flash_write, ( dmqspi_flash_t flash, uint32_t offset, const void *buffer, size_t size ));

/** Erase [offset, offset+size); both must be multiples of chip->sector_size (-EINVAL otherwise). */
dmod_dmqspi_api(1.0, int, _flash_erase, ( dmqspi_flash_t flash, uint32_t offset, size_t size ));

dmod_dmqspi_api(1.0, int, _flash_erase_chip, ( dmqspi_flash_t flash ));

/** Enter/leave memory-mapped mode (XIP window, read only). */
dmod_dmqspi_api(1.0, int, _flash_set_memory_mapped, ( dmqspi_flash_t flash, bool enable ));

#endif // DMQSPI_H
