#ifndef DMQSPI_STM32_COMMON_H
#define DMQSPI_STM32_COMMON_H

#include <stdint.h>
#include "dmqspi_types.h"

/*
 * Shared STM32 QUADSPI controller code (F4 with QUADSPI: F412/F413/F423/
 * F446/F469/F479; F7: all with QUADSPI). Register layout is identical.
 * The family port.c passes in only what differs - see
 * dmqspi_stm32_family_t and src/port/stm32_common/README.md.
 */

/** What a family's port.c tells the common code. */
typedef struct
{
    uintptr_t   regs_base;      /**< QUADSPI registers: 0xA0001000 */
    uintptr_t   mapped_base;    /**< Memory-mapped window: 0x90000000 */
    int         irq;            /**< QUADSPI_IRQn */
    bool        has_dcache;     /**< F7: invalidate/clean D-cache around the mapped window */
} dmqspi_stm32_family_t;

int  dmqspi_stm32_init(const dmqspi_stm32_family_t *family, const dmqspi_config_t *config);
int  dmqspi_stm32_deinit(dmqspi_instance_t instance);
void dmqspi_stm32_irq_handler(void);    /**< Call from the family's DMOD_IRQ_HANDLER */

#endif
