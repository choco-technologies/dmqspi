# stm32f7 port

QUADSPI at 0xA0001000, mapped window 0x90000000, IRQ `QUADSPI_IRQn`, Cortex-M7
D-cache present (invalidate the mapped window after writes/erases). Target board:
STM32F746G-DISCO (N25Q128A, bank 1). Thin wrapper over `../stm32_common`.
