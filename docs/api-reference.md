# API reference

* `include/dmqspi.h` - `dmqspi_validate_config`, and `dmqspi_ioctl(instance, command, arg)`: the commands of the bus node as a function. Clients use the node `/dev/dmqspi<N>` through the `DMQSPI_IOCTL_*` commands (docs/architecture.md), or the function when they start before the file system of the devices is there (the external RAM).
* `include/dmqspi_port.h` - controller API implemented by `dmqspi_port`.
* `include/dmqspi_types.h` - command descriptor, config, transfer structs, ioctl numbers.

Return convention everywhere: `0` / byte count on success, negative errno on failure.
Every declaration carries its doc comment in the header - read those first.
