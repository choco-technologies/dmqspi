# dmqspi

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![CI](https://github.com/choco-technologies/dmqspi/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmqspi/actions/workflows/ci.yml)

dmqspi DMOD library module.

## Description

TODO: describe what this module does.

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
| `dmqspi_create()` | Create a new `dmqspi_t` instance. |
| `dmqspi_destroy()` | Destroy an instance created by `_create()`. |
| `dmqspi_is_valid()` | Check whether a handle is a valid instance. |

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
`DMOD_CPU_FAMILY` (default: `stm32f7`):

```bash
cmake .. -DDMOD_CPU_FAMILY=stm32f7
```

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
