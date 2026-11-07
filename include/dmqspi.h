#ifndef DMQSPI_H
#define DMQSPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmqspi_defs.h"
#include "dmqspi_types.h"

/*
 * dmqspi = the QUADSPI *bus* driver. It owns the controller and offers it two ways:
 *
 *  - the node /dev/dmqspi<N>, driven through the DMQSPI_IOCTL_* commands of dmqspi_types.h - for what runs after the file system of
 *    the devices is there;
 *  - the functions below, keyed by the number of the controller (the `instance` of its configuration) - for the drivers that use the
 *    controller while they are being configured (an external RAM has to be a heap before the next driver is created). A driver that
 *    requires the dmqspi module is configured after the controller (dmdevfs resolves the dependencies ahead of driver_order).
 *
 * Both do the same: send a command with optional data, auto-poll a status register, switch on a memory-mapped window. The bus does not
 * know flash chips, opcodes or block devices - that is a separate module on top (e.g. dmnorflash, dmpsram), exactly as dmsdio/dmspi
 * relate to what is attached to them.
 */

/** Check a configuration without touching hardware. */
dmod_dmqspi_api(1.0, bool, _validate_config, ( const dmqspi_config_t *config ));

/** One command with its data phase (what DMQSPI_IOCTL_TRANSFER2 does). @return 0 or a negative errno (-EINVAL: no such controller) */
dmod_dmqspi_api(1.0, int, _transfer, ( dmqspi_instance_t instance, const dmqspi_transfer2_t *transfer ));

/** Repeat a status read until (status & mask) == match or the time runs out (DMQSPI_IOCTL_AUTO_POLL2). */
dmod_dmqspi_api(1.0, int, _auto_poll, ( dmqspi_instance_t instance, const dmqspi_auto_poll2_t *poll ));

/** Switch the memory-mapped window on: read only (flash) or read/write (RAM); `info` gets the window (DMQSPI_IOCTL_MAP). */
dmod_dmqspi_api(1.0, int, _map, ( dmqspi_instance_t instance, const dmqspi_map_request_t *request, dmqspi_map_info_t *info ));

/** Switch the window off (DMQSPI_IOCTL_EXIT_MAPPED). */
dmod_dmqspi_api(1.0, int, _unmap, ( dmqspi_instance_t instance ));

/** Whether the controller is in the indirect or in the memory-mapped mode (DMQSPI_IOCTL_GET_MODE). */
dmod_dmqspi_api(1.0, dmqspi_mode_t, _get_mode, ( dmqspi_instance_t instance ));

/** Write what the CPU changed in the range of a writable window back to the chip (DMQSPI_IOCTL_FLUSH_MAPPED). */
dmod_dmqspi_api(1.0, void, _flush_mapped, ( dmqspi_instance_t instance, uint32_t offset, uint32_t size ));

/** Drop the range of the window from the data cache of the CPU, the next read comes from the chip (DMQSPI_IOCTL_INVALIDATE_MAPPED). */
dmod_dmqspi_api(1.0, void, _invalidate_mapped, ( dmqspi_instance_t instance, uint32_t offset, uint32_t size ));

#endif // DMQSPI_H
