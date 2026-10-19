# API reference

* `include/dmqspi.h` - flash-chip layer (`dmqspi_flash_*`), `dmqspi_validate_config`.
  Normal clients use the block node `/dev/dmqspi<N>` instead (docs/architecture.md).
* `include/dmqspi_port.h` - controller API implemented by `dmqspi_port`.
* `include/dmqspi_types.h` - command descriptor, config, chip description, ioctl numbers.
* `include/dmqspi_flash_cmds.h` - NOR opcodes and status bits.

Return convention everywhere: `0` / byte count on success, negative errno on failure.
Every declaration carries its doc comment in the header - read those first.
