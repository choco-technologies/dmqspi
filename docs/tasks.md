# Implementation plan (do it in this order, one PR per step)

Scope of THIS repo: the bus (`dmqspi` + `dmqspi_port`). The flash-chip module
(`dmnorflash`, N25Q128A, block device) and the file system are **separate repos**,
listed at the end only to show who the consumer is.

Context: STM32F746G-DISCO. Build/flash/test workflow: `dmod-board-debugging` skill of
the dmod-boot repo. Read `docs/architecture.md` and `docs/port-implementation.md` first.
Host tests (`ctest`) cover pure logic only; the rest is verified on the board.

1. **Port: indirect mode.** `stm32_common.c`: `_init`, `_deinit`, `_command`, `_read`, `_write`;
   `stm32f7/port.c` (family constants + IRQ). Quick bring-up check: a throw-away call in
   `dmod_init`/driver `_create` that sends READ ID (0x9F, 1 line, 3 data bytes) and logs it.
   Pass: log shows `20 BA 18` (N25Q128A).
2. **Port: `_auto_poll`.** Pass: WREN (0x06) then poll status 0x05 until WEL==1 (mask 2, match 2).
3. **Driver `dmqspi.c`.** Per the comment in the file + `docs/configuration.md`.
   Pass: `/dev/dmqspi1` appears; TRANSFER/AUTO_POLL ioctls from a small test tool (like dmspi's `tools/spitest`)
   reproduce steps 1-2. Remove the throw-away code.
4. **Port: memory-mapped mode** (+ D-cache invalidate). Pass: after the flash module (or a test tool)
   programs a pattern, reading at 0x90000000 matches.
5. **Board integration (dmod-boot).** `dmqspi dmqspi_port` + `dmqspi driver=board/stm32f746g-disco/qspi.ini`
   in `flash.dmd` after dmclk/dmgpio; check pin conflicts (note in `qspi.ini`).
6. **F4 port** when a board is available (compile-check earlier).

Elsewhere (new repos): `dmnorflash` (chip table, block device on top of the ioctls above),
`dmlfs` (littlefs), automount rule + blkid signature.

Conventions: heap-allocate with `Dmod_Malloc`, no fixed buffers for strings, functions under
~50 lines, negative errno returns, `DMOD_LOG_*` for logging; a module must print something in
`dmod_init` (packer rejects an "empty" module); `DMOD_ENABLE_REGISTRATION` in exactly one .c file.
