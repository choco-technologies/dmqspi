# dmqspi architecture

dmqspi is a **bus** driver (like dmspi, dmi2c), not a flash driver.

```
 littlefs / FAT / automount / shell
              |  block device /dev/dmnorflash0         <- separate module (own repo)
        dmnorflash: JEDEC id, chip table, quad enable, page program, erase
              |  ioctl on /dev/dmqspi1  (DMQSPI_IOCTL_*)
        dmdrvi (dmdevfs node)
              |
   +----------+-----------+
   | dmqspi (this repo)   |   src/dmqspi.c   dmdrvi DIF: create/free/open/close/ioctl/...
   |                      |   src/dmqspi.c          validate_config, module entry
   +----------+-----------+
              |  include/dmqspi_port.h   (generic "send this command" API)
   +----------+-----------+
   | dmqspi_port          |   src/port/stm32_common  shared QUADSPI register code
   |                      |   src/port/stm32f7|f4    thin wrappers (IRQ, base addresses)
   +----------+-----------+
              |  registers, 0xA0001000      mapped window, 0x90000000
           hardware
```

**Rule:** neither module here knows an opcode, a JEDEC id or a page size.
A new chip = a row in the *flash module*. A new MCU = a new `src/port/<family>`.

## Pins

Not configured by dmqspi. The board `.ini` lists each pin as a `dmgpio`
friend (alternate function) in the same `friends_group` as the `[qspi]`
section - exactly like dmspi (`configs/board/stm32f746g-disco/qspi.ini`).
The port only enables the QUADSPI clock (via `dmclk_port`) and programs the controller.

## Bus node contract

`/dev/dmqspi<instance>`; `_read`/`_write` are not supported (`-ENOTSUP`) - there
is no byte stream, only commands. Everything goes through ioctl
(`dmqspi_types.h`):

| ioctl | arg | Does |
| --- | --- | --- |
| `DMQSPI_IOCTL_TRANSFER` | `dmqspi_transfer_t*` | one command with optional read/write data phase (`dir`) |
| `DMQSPI_IOCTL_AUTO_POLL` | `dmqspi_auto_poll_t*` | hardware status polling until `(status & mask) == match` or timeout |
| `DMQSPI_IOCTL_ENTER_MAPPED` / `_EXIT_MAPPED` | read command / none | memory-mapped window on/off |
| `DMQSPI_IOCTL_GET_MAPPED_BASE`, `_GET_MODE`, `_INVALIDATE_MAPPED` | see header | helpers for the XIP window and D-cache |

Concurrency: one command (including its data phase) is atomic - a dmosi mutex
in the port. A multi-command sequence (WREN + program + poll) is the client's
problem: the flash module serializes its own callers. While memory-mapped,
indirect ioctls return `-EBUSY`. A command that finds the controller busy although no
command of the port is running aborts what was left behind (and resets the controller
if that does not help) and goes on - see `docs/port-implementation.md`.

## What the flash module needs from this bus (for reference)

Everything it does maps onto the three primitives: `TRANSFER` (READ ID 0x9F,
WREN 0x06, quad enable, page program, sector erase, quad fast read 0xEB),
`AUTO_POLL` (status WIP=0 after program/erase) and `ENTER_MAPPED` (XIP). It
exports `DMDRVI_IOCTL_BLOCK_*` (dmdrvi already has GET_INFO/ERASE) so that
automount, blkid and a file system with wear levelling (littlefs, later) work on top.
