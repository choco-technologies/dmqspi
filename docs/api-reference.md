# API reference

* `include/dmqspi.h` - `dmqspi_validate_config` only. Clients use the bus node `/dev/dmqspi<N>` through the `DMQSPI_IOCTL_*` commands (docs/architecture.md).
* `include/dmqspi_port.h` - controller API implemented by `dmqspi_port`.
* `include/dmqspi_types.h` - command descriptor, config, transfer structs, ioctl numbers.

Return convention everywhere: `0` / byte count on success, negative errno on failure.
Every declaration carries its doc comment in the header - read those first.
