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
1. Wait until `SR.BUSY == 0` (else `-EBUSY`); take the mutex.
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
* 24-bit addressing covers 16 MB; bigger chips need `ADSIZE=32 bit` and the chip's 4-byte mode.
* Prescaler rounding must be *up* (never exceed `clock_hz`).
