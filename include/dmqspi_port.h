#ifndef DMQSPI_PORT_H
#define DMQSPI_PORT_H

#include "dmod_types.h"
#include "dmqspi_port_defs.h"
#include "dmqspi_types.h"

/*
 * Port API = the QUADSPI *controller* only. It knows nothing about flash
 * chips: no opcodes, no JEDEC, no page sizes (that is the core's job, see
 * src/dmqspi_flash.c). All functions return 0 on success or a negative
 * errno-style value (-EINVAL, -EBUSY, -ETIMEDOUT, -EIO, -ENOTSUP).
 *
 * Each declaration needs a definition written with
 * dmod_dmqspi_port_api_declaration(...) in src/port/<family>/port.c or in
 * src/port/stm32_common/ (shared). Do NOT declare _init/_deinit lifecycle
 * hooks here - dmod_init()/dmod_deinit() are separate (see
 * docs/port-implementation.md).
 *
 * Thread safety: the port serializes callers with a dmosi mutex per
 * instance; a single command (including its data phase) is atomic.
 */

/* --- Lifecycle --- */

/** Enable clocks (via dmclk_port), program prescaler/FSIZE/CS time, enable the controller.
 *  Pins are NOT touched - the board .ini configures them through dmgpio friends. */
dmod_dmqspi_port_api(1.0, int, _init,   ( const dmqspi_config_t *config ) );

/** Disable the controller and its clock. Leaves memory-mapped mode first. */
dmod_dmqspi_port_api(1.0, int, _deinit, ( dmqspi_instance_t instance ) );

/* --- Indirect mode: one transaction per call --- */

/** Command without data phase (WREN, erase, write status). Waits for completion (TCF). */
dmod_dmqspi_port_api(1.0, int, _command, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd ) );

/** Command with an indirect read data phase; reads cmd->data_size bytes. */
dmod_dmqspi_port_api(1.0, int, _read,    ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, void *data ) );

/** Command with an indirect write data phase; writes cmd->data_size bytes. */
dmod_dmqspi_port_api(1.0, int, _write,   ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const void *data ) );

/**
 * Hardware auto-polling: repeat `cmd` (a status-register read, data_size 1..4)
 * until (status & poll->mask) == poll->match, or poll->timeout_ms passes
 * (-ETIMEDOUT). Used for "write in progress" after program/erase. The
 * calling thread should sleep (IRQ + semaphore), not spin, for long erases.
 */
dmod_dmqspi_port_api(1.0, int, _auto_poll, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const dmqspi_poll_t *poll ) );

/* --- Memory-mapped mode (XIP / read-only window) --- */

/** Enter memory-mapped mode using `read_cmd` as the read command (data_size ignored).
 *  In this mode indirect calls return -EBUSY until _exit_memory_mapped(). */
dmod_dmqspi_port_api(1.0, int, _enter_memory_mapped, ( dmqspi_instance_t instance, const dmqspi_command_t *read_cmd ) );

/** Abort memory-mapped mode (ABORT bit), back to indirect mode. */
dmod_dmqspi_port_api(1.0, int, _exit_memory_mapped,  ( dmqspi_instance_t instance ) );

/** Base address of the mapped window (0x90000000 on STM32F4/F7). */
dmod_dmqspi_port_api(1.0, uintptr_t, _get_mapped_base, ( dmqspi_instance_t instance ) );

/* --- Status --- */

dmod_dmqspi_port_api(1.0, dmqspi_mode_t, _get_mode, ( dmqspi_instance_t instance ) );

/** Cache coherency helper: invalidate the D-cache range of the mapped window
 *  (call after any write/erase when the mapped window was read before). */
dmod_dmqspi_port_api(1.0, void, _invalidate_mapped, ( dmqspi_instance_t instance, uint32_t offset, uint32_t size ) );

#endif // DMQSPI_PORT_H
