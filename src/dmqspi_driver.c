#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmqspi.h"
#include "dmqspi_port.h"

/*
 * dmdrvi bus driver skeleton ("driver_name=dmqspi" in the board .ini). NOT
 * IMPLEMENTED. Copy the structure of dmspi/src/dmspi.c. Summary:
 *
 *   _create   : read [section] keys (docs/configuration.md) -> dmqspi_config_t,
 *               dmqspi_validate_config, dmqspi_port_init; dev_num minor = instance.
 *               The node appears as /dev/dmqspi<instance>.
 *   _free     : dmqspi_port_deinit, free the context
 *   _open/_close/_flush/_stat : trivial (one exclusive-or-shared handle, no state)
 *   _read/_write : not supported (-ENOTSUP) - the bus has no byte stream; use ioctl.
 *               (Optional later: _read at offset = memory-mapped read.)
 *   _ioctl    : DMQSPI_IOCTL_TRANSFER      -> dmqspi_port_command / _read / _write
 *               DMQSPI_IOCTL_AUTO_POLL     -> dmqspi_port_auto_poll
 *               DMQSPI_IOCTL_ENTER_MAPPED  -> dmqspi_port_enter_memory_mapped
 *               DMQSPI_IOCTL_EXIT_MAPPED   -> dmqspi_port_exit_memory_mapped
 *               DMQSPI_IOCTL_GET_MAPPED_BASE / GET_MODE / INVALIDATE_MAPPED -> port getters
 *               Validate every pointer; copy nothing - the buffers belong to the caller.
 */
