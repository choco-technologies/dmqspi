# Flash chips

| Chip | Board | JEDEC (mfr, type, cap) | Size | Page | Erase | Quad enable |
| --- | --- | --- | --- | --- | --- | --- |
| Micron N25Q128A (3 V) | STM32F746G-DISCO | 20 BA 18 | 16 MiB | 256 B | 4 KB / 64 KB | volatile config reg (0x81/0x85) |

Adding a chip: append a `dmqspi_chip_t` row to `src/dmqspi_chips.c`; if its
quad-enable or dummy-cycle procedure differs from Micron's, add a field to
`dmqspi_chip_t` (e.g. an enum of quad-enable styles: Micron VCR, Winbond/ISSI
status-register QE bit, Macronix) and a branch in `dmqspi_flash_open`.
Always confirm opcodes, dummy cycles at the chosen `clock_hz` and timings from the datasheet.
