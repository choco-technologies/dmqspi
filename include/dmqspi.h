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
 * The only function exported here is a pure helper, usable in host tests.
 */

/** Check a configuration without touching hardware. */
dmod_dmqspi_api(1.0, bool, _validate_config, ( const dmqspi_config_t *config ));

#endif // DMQSPI_H
