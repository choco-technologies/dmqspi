# Implementing dmqspi_port (STM32 QUADSPI)

Register recipe for `src/port/stm32_common/stm32_common.c` (names from RM0385
chapter "Quad-SPI interface (QUADSPI)"; verify against the reference manual).
Base `0xA0001000`, mapped window `0x90000000`. Registers: CR, DCR, SR, FCR,
DLR, CCR, AR, ABR, DR, PSMKR, PSMAR, PIR, LPTR.

## Adding another family
1. `src/port/<family>/config.cmake` (`DMOD_TOOLS_NAME`), `src/port/<family>/port.c`
   (family constants + `DMOD_IRQ_HANDLER`); shared logic stays in `stm32_common`.
2. Build with `-DDMOD_CPU_FAMILY=<family>`. Do not invent module-specific variables.
F4 parts with QUADSPI: F412, F413, F423, F446, F469, F479 (the other F4s have no controller - the port must fail `_init` with `-ENOTSUP`, not crash).

## `_init`
1. Enable the peripheral clock via `dmclk_port` (QSPI is on AHB3; kernel clock = HCLK).
2. Reset the peripheral (RCC AHB3RSTR) to a known state.
3. `DCR`: `FSIZE = log2(flash_size) - 1`, `CSHT = cs_high_cycles - 1`, `CKMODE = clock_mode`.
4. `CR`: `PRESCALER = ceil(f_kernel / clock_hz) - 1`, `SSHIFT`, `FSEL` (bank), `EN = 1`.
   Create the mutex and the completion semaphore (dmosi), enable the NVIC IRQ (TCIE/TEIE/SMIE flags are set per transfer, not here).

## `_command` / `_read` / `_write` (indirect)
1. Take the mutex, then wait briefly (2 ms) for `SR.BUSY == 0`. With the mutex taken no transfer of ours is going on,
   so a controller that stays busy was left with a transfer nobody finishes (seen on the STM32F746G-DISCO: an indirect
   read stalled on a full FIFO, `DMAEN` off, every later command failing with `-EBUSY` for good). It is recovered:
   stop the DMA stream, `CR.ABORT`; if it does not let go, empty the FIFO and abort again; as the last resort reset the
   peripheral and program it again. `-EBUSY` only when even the reset did not help.
2. `DLR = data_size - 1` (skip when no data phase); `CCR` = functional mode
   (`FMODE`: 00 indirect write, 01 indirect read), `IMODE/ADMODE/ABMODE/DMODE` from the
   `*_lines` fields (none=0, 1 line=1, 2=2, 4=3), `ADSIZE`, `ABSIZE`, `DCYC`, `INSTRUCTION`.
3. Write `ABR` (if alternate), then `AR` (if address phase) - **writing AR starts the
   transfer** (or writing CCR when there is no address phase).
4. Data: read/write `DR` byte-wise (`*(volatile uint8_t*)&DR`) while `SR.FTF`/FIFO level
   permits; short transfers may poll, long ones wait on the IRQ semaphore (FTIE).
   DMA (F7, `stm32_qspi_dma.c`): transfers of 64 B..64 KiB with a 32-byte aligned buffer reachable by DMA2 go through
   a dmdma lease of DMA2 stream 7 / channel 3 (one request line, both directions), `CR.DMAEN` set before the command is
   started; the thread sleeps on a semaphore, the D-cache is cleaned/invalidated around the transfer. Anything else, and
   F4, uses the polled FIFO loop. The stream is leased on first use and released in `_deinit`.
5. Wait `SR.TCF`, clear it in `FCR.CTCF`. On `SR.TEF` (transfer error) clear `CTEF`, return `-EIO`.

## `_auto_poll`
`PSMKR = mask`, `PSMAR = match`, `PIR = 0x10` (interval), `CR.APMS = 1` (stop on match),
`CCR.FMODE = 10` (auto-polling), start via CCR/AR as above, wait `SR.SMF` (IRQ, `SMIE`)
with `timeout_ms` (on timeout: `CR.ABORT`, return `-ETIMEDOUT`), clear `FCR.CSMF`.

## memory-mapped
`CCR.FMODE = 11` with the read command (instruction, address lines, alternate bytes,
dummy cycles, data lines) - then the window at 0x90000000 reads the chip directly.
Exit with `CR.ABORT = 1` and wait `SR.BUSY == 0`. F7: after `_exit_memory_mapped` and any
indirect write/erase, `SCB_InvalidateDCache_by_Addr` the touched range (`_invalidate_mapped`);
the window is normally cacheable - keep write-through/no-allocate attributes in mind (MPU).

## Errors / pitfalls
* Never leave `CS` low after an error: `CR.ABORT` then `EN` off/on.
* Waits for flags and for the FIFO are bounded by time (the RTOS tick, 200 ms), not by a count of polls - a count
  says nothing about time when the thread is preempted; the count only ends a wait before the scheduler runs.
* 24-bit addressing covers 16 MB; bigger chips need `ADSIZE=32 bit` and the chip's 4-byte mode.
* Prescaler rounding must be *up* (never exceed `clock_hz`).

---

# Implementing dmqspi_port (ESP32-S3, the memory SPI)

`src/port/esp32s3/port.c`, registers and ROM addresses in `esp32s3_mspi.h`. The ESP32-S3 has no QUADSPI peripheral; the flash and the
external RAM hang on the **memory SPI (MSPI)**: SPI1 (the CPU sends commands) and SPI0 (the cache: what it sends for an access to the
window) share the pads and the clock. Chip select 0 is the flash (the firmware runs from it), chip select 1 the external RAM. This port
serves chip select 1 only (`bank=2`; `bank=1` is refused with `-EACCES`).

## `_init`
1. The pads: the fixed pads of the bus (GPIO26 CS1, GPIO27/28 SPIHD/SPIWP, GPIO31/32 SPIQ/SPID: function 0; for 8 lines GPIO33-37
   SPIIO4-7 and DQS: function 4), all with the strongest drive, and the drive of the clock pads (the DATE register). Eight lines are refused
   (`-ENODEV`) on a package whose eFuses say it has no PSRAM inside: GPIO33-37 are free pins there.
2. The timing of the chip select (shared by SPI0 and SPI1) and the clock of the chip (`SRAM_CLK`: APB 80 MHz / divider, rounded up; up to
   40 MHz for 4 lines, 20 MHz for 8: no timing tuning of the MSPI here).
3. 8 lines: no ECC (`SYSCON_SRAM_ACE0_ATTR` bit 8), SPI1 in the DTR mode (`DDR_FMEM_VAR_DUMMY`, `esp_rom_spi_set_dtr_swap_mode`).

Everything that reconfigures the cache runs in a critical section and calls nothing but the ROM and this file: the firmware is executed
from the flash through the cache that is being changed.

## `_command` / `_read` / `_write`
SPI1, the way ESP-IDF does it (the ROM `esp_rom_spi_cmd_config` / `esp_rom_spi_cmd_start`), the registers of the mode saved and restored.
Lines of the phases: all 1 (SPI), instruction 1 with address and data 4 (QIO), or all 4 (a chip in the QPI mode: `FCMD_QUAD`). 8 lines:
`ddr` with every phase on 8 lines and a 32-bit address, through the ROM `esp_rom_opiflash_exec_cmd` (OPI DTR; the byte of the instruction
goes out twice; the dummy cycles are counted as the ROM does: two bits for every cycle but the first). At most 64 bytes of data (the
`W0..W15` registers). `_auto_poll` and `_enter_memory_mapped` (read-only) are `-ENOTSUP`.

## `_enter_memory_mapped_rw`
The window of a RAM chip is read AND written by the CPU, so the request carries both commands (`write_cmd`). The port programs SPI0:
the read and write commands (`SRAM_DRD_CMD`, `SRAM_DWR_CMD`), the address length, the dummy cycles, the lines (`USR_SRAM_QIO`; for 8
lines `SRAM_OCT`, DDR and the 16-bit commands), the data cache mode (32 KB, 8 ways, 32-byte lines: as ESP-IDF sets it for the external
RAM), then the MMU entries that point the window at the chip: 8 MB at the end of the 32 MB window of the cache (entries 384..511),
`0x3D800000` for the data bus and `0x43800000` for the instruction bus (`info.code_base`). `_invalidate_mapped` writes the lines of
a range back to the chip and drops them from the cache (ROM `Cache_WriteBack_Addr` + `Cache_Invalidate_Addr`).

## Pitfalls
* `s32c1i` (the atomic compare-and-set of the Xtensa) does not work on external RAM.
* The window is behind the cache: DMA cannot use it, and it must not be touched with the cache disabled (the flash operations of the firmware).
* A command executed between the start of the firmware and the first `_enter_memory_mapped_rw` of a chip that has not been woken up
  (the chip is in the state it was left in) may be misunderstood: the driver of the chip (dmpsram) sends the reset first.
