# dmqspi

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmqspi/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmqspi/actions/workflows/ci.yml)

dmqspi - QUADSPI (external NOR flash) driver for DMOD. **Status: skeleton** - interfaces and docs only, no implementation yet (see [docs/tasks.md](docs/tasks.md)).

## Description

dmqspi is the **QUADSPI bus** driver: it exposes the controller as a dmdrvi
node `/dev/dmqspi<N>` on which a client sends commands (instruction / address /
alternate / dummy / data phases on 1, 2, 4 or 8 lines), auto-polls a status
register, and switches on a memory-mapped window (0x90000000 on STM32). It is
chip-agnostic - like dmspi it knows nothing about what is attached. Flash
chips (e.g. the N25Q128A on the STM32F746G-DISCO) are handled by a **separate
module** (e.g. `dmnorflash`, own repo) that is a client of this bus and
exports the block device.

Two modules, same driver+port split as dmspi/dmsdio:

| Module | Contains | Hardware knowledge |
| --- | --- | --- |
| `dmqspi` | dmdrvi bus driver, .ini parsing, ioctl dispatch | none - speaks only to `dmqspi_port` |
| `dmqspi_port` | QUADSPI controller: send a command, move data, auto-poll, memory-mapped mode | `src/port/stm32_common` (shared), `stm32f4`, `stm32f7`, `esp32s3` (the memory SPI: the external RAM) |

Start with [docs/architecture.md](docs/architecture.md); the implementation
plan, in the order it should be done, is [docs/tasks.md](docs/tasks.md).

## Building

### Using CMake

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

Pass `-DDMOD_DIR=/path/to/local/dmod` to build against a local dmod checkout
instead of fetching `develop` from GitHub.

### Using Make

```bash
make DMOD_MODE=DMOD_MODULE DMOD_DIR=/path/to/dmod
```

## Testing

Tests are built automatically alongside the module (see `tests/`). Once built,
run them with `ctest`:

```bash
cd build
ctest --output-on-failure
```

`ctest` installs the test module's dependencies with `dmf-get` and then runs
it through `dmod_loader`. To run it manually instead:

```bash
export DMOD_DMF_DIR=$(pwd)/build/dmf
dmf-get install -d ${DMOD_DMF_DIR}/test_dmqspi-local.dmd -y
dmod_loader build/dmf/test_dmqspi.dmf
```

## Usage

<TBD>

This library module provides functions that can be used by other modules:

```c
#include "dmqspi.h"
```

## API

| Function | Description |
|----------|-------------|
| `dmqspi_validate_config()` | Check a configuration without touching hardware. |
| `dmqspi_transfer()`, `dmqspi_auto_poll()` | One command with its data phase; a status read repeated until it matches. |
| `dmqspi_map()`, `dmqspi_unmap()`, `dmqspi_get_mode()` | The memory-mapped window, read only (flash) or read/write (RAM). |
| `dmqspi_flush_mapped()`, `dmqspi_invalidate_mapped()` | Cache coherency of the window: write back, drop. |

The node `/dev/dmqspi<N>` takes the same operations as `DMQSPI_IOCTL_*` commands. The functions are for the drivers that use the controller while
they are configured (an external RAM has to be a heap before the next driver is created): a driver that requires the `dmqspi` module is configured
after the controller. The node is for what runs after the file system of the devices is there.

See [include/dmqspi.h](include/dmqspi.h) for the full
declarations and [docs/api-reference.md](docs/api-reference.md) for the
complete reference.

## Documentation

See the `docs/` directory:

- **[api-reference.md](docs/api-reference.md)** - Complete API documentation

View documentation using `dmf-man dmqspi`.

## Hardware Port

This module ships two DMOD modules: the architecture-independent
`dmqspi` and `dmqspi_port`, which contains the
architecture-specific implementation. The active architecture is selected via
`DMOD_CPU_FAMILY` (default: `stm32f7`; `stm32f4`, `esp32s3`, `x86_64` - a host stub):

```bash
cmake .. -DDMOD_CPU_FAMILY=stm32f7
```

On the **ESP32-S3** the port drives the memory SPI (MSPI) that the flash and the external RAM hang on - chip select 1, the RAM;
chip select 0 is the flash the firmware runs from, and the port refuses it. Its pads are fixed, so there are no pin sections in the ini
file. See [docs/port-implementation.md](docs/port-implementation.md) and `configs/board/` of [dmpsram](https://github.com/choco-technologies/dmpsram)
for the ini files of boards.

See [docs/port-implementation.md](docs/port-implementation.md) for how to add
another architecture. Port-specific files:

```
├── include/dmqspi_port.h
├── src/port/
│   ├── CMakeLists.txt
│   └── stm32f7/
│       ├── config.cmake
│       └── port.c
└── dmqspi_port.dmr
```
## Project Structure

```
dmqspi/
├── docs/              # Documentation (markdown format)
├── include/           # Public headers
│   └── dmqspi.h
├── src/
│   └── dmqspi.c
├── tests/
│   ├── CMakeLists.txt
│   └── dmqspi_test.c
├── CMakeLists.txt
├── Makefile
├── dmqspi.dmr
└── manifest.dmm
```

## Author

Patryk Kubiak

## License

MIT
