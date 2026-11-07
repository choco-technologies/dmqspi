# dmqspi configuration (.ini)

Section with `driver_name=dmqspi` (pins are separate dmgpio sections in the
same `friends_group`, see `configs/board/stm32f746g-disco/qspi.ini`).

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `instance` | int | 1 | QUADSPI instance (node becomes `/dev/dmqspi<instance>`) |
| `bank` | 1 \| 2 | 1 | Which bank's pins the chip is on |
| `clock_hz` | int | 50000000 | Max CLK; the port picks the largest `f_kernel/(P+1)` not above it |
| `flash_size` | int | required | Size of the address window of the attached device, power of two (-> FSIZE) |
| `cs_high_cycles` | 1..8 | 2 | Min CS high time (N25Q needs >= 20 ns) |
| `clock_mode` | 0 \| 3 | 0 | CLK level when idle |
| `sample_shift_half` | 0 \| 1 | 1 | Sample half a cycle later (needed above ~40 MHz) |
| `data_lines` | 1 \| 2 \| 4 \| 8 | 0 | Data lines the chip is wired with (0: what the controller has). The ESP32-S3 port sets up the pads of 4 (quad) or 8 (octal) lines; STM32 refuses 8 |

ESP32-S3: `bank=2` (the chip select 1 - the chip select 0 is the flash of the chip, the port refuses it), `clock_hz` up to 40 MHz for 4 lines and 20 MHz for 8
(no timing tuning of the memory SPI), `flash_size` is the size of the RAM window (up to 8 MB map), `data_lines=8` only on the packages that have the octal
PSRAM inside (the eFuses say so: otherwise `-ENODEV` and the pads GPIO33-37 are left alone).

Unknown keys: log a warning, do not fail. Missing `flash_size`: fail `_create`.
Parsing lives in `src/dmqspi.c`; the result must pass
`dmqspi_validate_config()` (implemented, unit tested).
