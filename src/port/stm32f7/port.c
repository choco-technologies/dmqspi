#define DMOD_ENABLE_REGISTRATION    ON
#include "dmqspi_port.h"
#include "dmod.h"
#include "../stm32_common/stm32_common.h"

/* STM32F7: thin wrapper over src/port/stm32_common (family constants + IRQ). */

static const dmqspi_stm32_family_t g_family =
{
    .regs_base   = 0xA0001000UL,
    .mapped_base = 0x90000000UL,
    .irq         = 92,
    .has_dcache  = true,
};

const dmqspi_stm32_family_t *dmqspi_stm32_get_family(void)
{
    return &g_family;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    Dmod_Printf("dmqspi port module initialized (stm32f7)\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}

/* QUADSPI global interrupt, IRQ 92 */
DMOD_IRQ_HANDLER(92) { dmqspi_stm32_irq_handler(); }
