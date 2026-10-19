# stm32_common

QUADSPI controller code shared by all STM32 families that have the peripheral
(identical register layout). `stm32_common.c` implements every function of
`include/dmqspi_port.h`; `stm32f4/port.c` and `stm32f7/port.c` only supply a
`dmqspi_stm32_family_t` (register/window base, IRQ, D-cache flag) and the
`DMOD_IRQ_HANDLER` that forwards to `dmqspi_stm32_irq_handler()`.
Register recipe: `docs/port-implementation.md`. Status: not implemented.
