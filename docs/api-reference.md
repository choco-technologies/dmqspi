# API reference

* `include/dmqspi.h` - `dmqspi_validate_config` and the operations of the bus as functions keyed by the number of the controller (`dmqspi_transfer`, `dmqspi_auto_poll`, `dmqspi_map`, `dmqspi_unmap`, `dmqspi_get_mode`, `dmqspi_flush_mapped`, `dmqspi_invalidate_mapped`) - for the drivers configured before the file system of the devices is there (the external RAM). Everything else uses the node `/dev/dmqspi<N>` through the `DMQSPI_IOCTL_*` commands (docs/architecture.md): the same operations.
* `include/dmqspi_port.h` - controller API implemented by `dmqspi_port`.
* `include/dmqspi_types.h` - command descriptor, config, transfer structs, ioctl numbers.

Return convention everywhere: `0` / byte count on success, negative errno on failure.
Every declaration carries its doc comment in the header - read those first.
