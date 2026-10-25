#ifndef DMQSPI_STM32_COMMON_H
#define DMQSPI_STM32_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include "dmqspi_types.h"

/*
 * Shared STM32 QUADSPI controller code (F4 with QUADSPI: F412/F413/F423/
 * F446/F469/F479; F7: all with QUADSPI). Register layout is identical.
 * The family's port.c supplies only what differs (dmqspi_stm32_get_family)
 * and forwards the IRQ to dmqspi_stm32_irq_handler().
 */

/** What a family's port.c tells the common code. */
typedef struct
{
    uint32_t    regs_base;      /**< QUADSPI registers: 0xA0001000 */
    uint32_t    mapped_base;    /**< Memory-mapped window: 0x90000000 */
    uint32_t    irq;            /**< QUADSPI IRQ number (NVIC), 92 on F4/F7 */
    bool        has_dcache;     /**< F7: invalidate the D-cache over the mapped window */
} dmqspi_stm32_family_t;

/** Implemented by each family's port.c. */
const dmqspi_stm32_family_t *dmqspi_stm32_get_family(void);

/** Call from the family's DMOD_IRQ_HANDLER(<irq>). */
void dmqspi_stm32_irq_handler(void);

/*
 * DMA hooks (stm32_qspi_dma.c, built only with DMQSPI_PORT_DMA - F7 with dmdma).
 * dma_start: 0 = stream running, 1 = not used (the caller moves the bytes), <0 = error.
 * dma_wait: sleeps until the transfer ends; also stops the stream and fixes the D-cache.
 * dma_cancel: stops the stream whatever it is doing (recovering the controller).
 */
#ifdef DMQSPI_PORT_DMA
int  dmqspi_stm32_dma_start(bool read, void *buffer, uint32_t size, uint32_t dr_address);
int  dmqspi_stm32_dma_wait(uint32_t timeout_ms);
void dmqspi_stm32_dma_release(void);
void dmqspi_stm32_dma_cancel(void);
#else
#define dmqspi_stm32_dma_start(read, buffer, size, dr)  (1)
#define dmqspi_stm32_dma_wait(timeout_ms)               (0)
#define dmqspi_stm32_dma_release()                      ((void)0)
#define dmqspi_stm32_dma_cancel()                       ((void)0)
#endif

#endif
