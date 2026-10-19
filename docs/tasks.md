# Implementation plan (do it in this order, one PR per step)

Context: STM32F746G-DISCO, N25Q128A. Build/flash/test workflow: see the
`dmod-board-debugging` skill of the dmod-boot repo. Read `docs/architecture.md`
and `docs/port-implementation.md` first. Unit tests (host, `ctest`) only cover pure
logic; everything else is verified on the board.

1. **Port: read JEDEC ID.** `stm32_common.c`: `_init`, `_deinit`, `_command`, `_read`;
   `stm32f7/port.c` (family constants + IRQ). Wire a throw-away test in the driver
   `_create` that logs the ID. Pass criterion: log shows `20 BA 18`.
2. **Core flash layer.** `dmqspi_flash.c`: `_flash_open` (reset, ID, chip match, quad
   enable, dummy cycles), `_flash_read` (0xEB). Needs the port's `_write` + `_auto_poll`
   for quad enable. Pass: read 256 B at offset 0 and compare with a known pattern.
3. **Write + erase.** Port `_write`, `_auto_poll`; core `_flash_write` (page split),
   `_flash_erase`, `_flash_erase_chip`. Pass: erase sector, read all 0xFF, program a
   pattern across a page boundary, read it back.
4. **dmdrvi driver.** `dmqspi_driver.c` per the comment in the file + `docs/configuration.md`.
   Pass: `/dev/dmqspi1` exists; `dd`-style read/write from the shell; `BLOCK_GET_INFO` sane.
5. **Board integration (dmod-boot).** `dmqspi dmqspi_port` + `dmqspi driver=board/stm32f746g-disco/qspi.ini`
   in the board `flash.dmd` after dmclk/dmgpio; check pin conflicts (see note in `qspi.ini`).
6. **Memory-mapped mode** (+ D-cache invalidate). Pass: read through 0x90000000 equals read via `/dev/dmqspi1`.
7. **File system** (separate repo, later): `dmlfs` (littlefs) on the block node; automount rule + blkid signature.
8. **Tests/CI:** extend `tests/` with host tests of anything pure (chip table lookup, page-split
   arithmetic - factor it into a pure function to make it testable).

Conventions: heap-allocate with `Dmod_Malloc`, no fixed buffers for paths/strings,
functions under ~50 lines, `_create/_destroy` with magic guard (see `dmod-coding-conventions`),
negative errno returns, `DMOD_LOG_*` for logging.
