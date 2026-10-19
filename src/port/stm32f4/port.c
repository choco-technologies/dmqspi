#define DMOD_ENABLE_REGISTRATION    ON
#include "dmqspi_port.h"
#include "dmod.h"
#include "../stm32_common/stm32_common.h"

/*
 * STM32F4: thin wrapper over src/port/stm32_common. NOT IMPLEMENTED yet:
 * it must define the dmqspi_stm32_family_t for this family, forward the
 * IRQ, and (in dmod_init) nothing else - the controller is brought up by
 * dmqspi_port_init() when the dmqspi driver is created.
 *
 *   static const dmqspi_stm32_family_t g_family = {
 *       .regs_base   = 0xA0001000,
 *       .mapped_base = 0x90000000,
 *       .irq         = QUADSPI_IRQn,
 *       .has_dcache  = false,
 *   };
 *
 *   DMOD_IRQ_HANDLER(QUADSPI_IRQn) { dmqspi_stm32_irq_handler(); }
 */

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    Dmod_Printf("dmqspi_port (stm32f4): skeleton, not implemented yet\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
