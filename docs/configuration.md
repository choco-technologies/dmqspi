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

Unknown keys: log a warning, do not fail. Missing `flash_size`: fail `_create`.
Parsing lives in `src/dmqspi_driver.c`; the result must pass
`dmqspi_validate_config()` (implemented, unit tested).
