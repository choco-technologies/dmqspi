# dmqspi architecture

```
 littlefs / FAT / automount / shell
              |  open/read/write/ioctl on /dev/dmqspi1
        dmdrvi (dmdevfs node)
              |
   +----------+-----------+
   | dmqspi (core)        |   src/dmqspi_driver.c   dmdrvi DIF: create/free/open/close/read/write/ioctl/flush/stat
   |                      |   src/dmqspi_flash.c    flash-chip layer: open, read, write(page split), erase, XIP
   |                      |   src/dmqspi_chips.c    table of known chips (JEDEC ID -> geometry)
   +----------+-----------+
              |  include/dmqspi_port.h   (generic "send this command" API)
   +----------+-----------+
   | dmqspi_port          |   src/port/stm32_common  shared QUADSPI register code
   |                      |   src/port/stm32f7|f4    thin wrappers (IRQ, base addresses)
   +----------+-----------+
              |  registers, 0xA0001000      mapped window, 0x90000000
           hardware
```

**Rule:** the core never touches a register; the port never knows an opcode.
A new chip = a row in `dmqspi_chips.c`. A new MCU = a new `src/port/<family>`.

## Pins

Pins are NOT configured by dmqspi. The board `.ini` lists each pin as a
`dmgpio` friend (alternate function) in the same `friends_group` as the
`[qspi]` section - exactly like `dmspi` (see `configs/board/stm32f746g-disco/qspi.ini`).
The port only enables the QUADSPI clock (through `dmclk_port`) and programs the
controller.

## Block-device contract (what the dmdrvi node does)

Offsets are **bytes**. NOR flash differs from an SD card: a write can only
clear bits, so the area must be erased before it is written.

| dmdrvi call | Behaviour |
| --- | --- |
| `_read(buf, size, offset)` | `dmqspi_flash_read` - any alignment, any size |
| `_write(buf, size, offset)` | `dmqspi_flash_write` - area must be erased; returns `-EIO` if the chip flags a program error. Does not erase for the caller |
| `_ioctl(DMDRVI_IOCTL_BLOCK_GET_INFO)` | `logical_block_size = 512`, `erase_block_size = chip->sector_size` (4096), `block_count = size/512`, `flags = DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED` |
| `_ioctl(DMDRVI_IOCTL_BLOCK_ERASE)` | `dmqspi_flash_erase` (range must be sector aligned, else `-EINVAL`) |
| `_ioctl(DMQSPI_IOCTL_*)` | JEDEC ID, memory-mapped on/off, mapped base, chip erase (`dmqspi_types.h`) |
| `_flush` | no-op (writes finish before returning) |

Concurrency: a dmosi mutex in the port makes each command atomic. The core
holds its own mutex over a whole read/write/erase so two threads cannot
interleave a WREN with another thread's command.

## Flash operations (core, using only the port API)

All commands are `dmqspi_command_t`; opcodes are in `dmqspi_flash_cmds.h`.

* **open:** `port_init` -> reset (0x66, 0x99, wait ~30 us) -> READ ID 0x9F on 1 line
  -> look up `g_dmqspi_chips` (not found: `-ENODEV`; SFDP fallback is optional) ->
  enable quad / set dummy cycles (Micron: volatile config reg 0x81, enh. volatile 0x61).
* **read:** one 0xEB (quad IO fast read) command; address 24 bit on 4 lines, alternate
  byte 0xFF on 4 lines, `dummy_cycles` from the chip row, data on 4 lines. The controller
  streams any length - no page limit.
* **write:** for each chunk not crossing a 256 B page boundary: WREN (0x06),
  page program (0x32 quad or 0x02), then `auto_poll` of status 0x05 until WIP=0
  (Micron: also check flag status 0x70 for program error, clear with 0x50).
* **erase:** per 4 KB sector: WREN, 0x20, auto_poll WIP=0 (timeout 400 ms; worst case
  from datasheet). Aligned 64 KB runs may use 0xD8.
* **memory-mapped:** `port_enter_memory_mapped` with the same 0xEB read command. While
  mapped, indirect operations first call `_exit_memory_mapped` (the core re-enters it
  afterwards if it was on). After a write/erase the D-cache lines of the window must be
  invalidated (`port_invalidate_mapped`).

## Why a block device and not "mtd"

dmdrvi already has block ioctls including ERASE (dmdrvi >= 2.x), automount and
`blkid` already understand block nodes, and a file system with wear levelling
(littlefs, separate module `dmlfs`, later) sits on top using exactly
read/write/erase. No new interface is needed.
