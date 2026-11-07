#ifndef DMQSPI_H
#define DMQSPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmqspi_defs.h"
#include "dmqspi_types.h"

/*
 * dmqspi = the QUADSPI *bus* driver. It owns the controller and exposes it
 * as a dmdrvi node /dev/dmqspi<N> driven through the DMQSPI_IOCTL_* commands
 * of dmqspi_types.h (send a command with optional data, auto-poll a status
 * register, memory-mapped mode). It does not know flash chips, opcodes or
 * block devices - that is a separate module on top (e.g. dmnorflash),
 * exactly as dmsdio/dmspi relate to what is attached to them.
 *
 * Besides the node, the controller can be driven by a function call (dmqspi_ioctl): for the clients that start BEFORE the file
 * system of the devices is there - the external RAM (dmpsram) has to be a heap before the drivers that follow it are created.
 */

/** Check a configuration without touching hardware. */
dmod_dmqspi_api(1.0, bool, _validate_config, ( const dmqspi_config_t *config ));

/**
 * The commands of the node (DMQSPI_IOCTL_*) as a function: what a client that cannot open the node yet calls, with the number of the
 * controller (the `instance` of its configuration) instead of a file. The controller has to be configured (driver_order, or the
 * client requires the dmqspi module): -EINVAL until then.
 *
 * @return what the command returns (0, or a negative errno)
 */
dmod_dmqspi_api(1.0, int, _ioctl, ( dmqspi_instance_t instance, int command, void *arg ));

#endif // DMQSPI_H
