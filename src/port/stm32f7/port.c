#define DMOD_ENABLE_REGISTRATION    ON
#include "dmqspi_port.h"
#include "dmod.h"

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("dmqspi port module initialized (stm32f7)\n");
    return 0;
}

int dmod_deinit(void)
{
    Dmod_Printf("dmqspi port module deinitialized (stm32f7)\n");
    return 0;
}

/* ---- API implementation ----
 *
 * Implement the dmod_dmqspi_port_api_declaration(...) functions
 * declared in include/dmqspi_port.h here. Register an interrupt
 * handler if needed, e.g.:
 *
 *   DMOD_IRQ_HANDLER(SOME_IRQn)
 *   {
 *       // handle interrupt
 *   }
 */
