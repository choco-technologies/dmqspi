/* No DMOD_ENABLE_REGISTRATION here - see the family's port.c. */
#include <errno.h>
#include "dmod.h"
#include "dmosi.h"
#include "dmdma_lease.h"
#include "stm32_common.h"

/*
 * Indirect data of a few dozen bytes or more by DMA (page program, reads of
 * file-system blocks). QUADSPI has ONE request line for both directions:
 * DMA2 stream 7, channel 3 (RM0385 table 27). The CPU sleeps on a semaphore
 * while the bus is clocked; the polled FIFO loop in stm32_common.c holds the
 * CPU for the whole transfer.
 *
 * Used only when it can be done right, otherwise dmqspi_stm32_dma_start()
 * returns 1 and the caller moves the bytes itself:
 *  - 64 bytes <= size < 64 KiB;
 *  - the buffer is reachable by DMA2 (not DTCM/ITCM) and starts on and spans
 *    whole D-cache lines (32 bytes): the cache is cleaned before the DMA reads
 *    it, cleaned+invalidated before it writes it, invalidated again after;
 *  - dmdma has the stream (leased at first use, released at deinit); not
 *    having it is not an error.
 * Element width is a byte, FIFO threshold 1 byte (the controller's default).
 */

#define DMA_CONTROLLER      1U      /* DMA2 */
#define DMA_STREAM          7U
#define DMA_CHANNEL         3U
#define DMA_MIN_BYTES       64U
#define DMA_MAX_BYTES       0xFFFFU
#define DMA_ALIGNMENT       32U
#define DMA_SLACK_MS        100U

#define SCB_CCR             (*(volatile uint32_t *)0xE000ED14UL)
#define SCB_CCR_DC          (1UL << 16)
#define SCB_DCIMVAC         (*(volatile uint32_t *)0xE000EF5CUL)
#define SCB_DCCMVAC         (*(volatile uint32_t *)0xE000EF68UL)
#define SCB_DCCIMVAC        (*(volatile uint32_t *)0xE000EF70UL)

static struct
{
    dmdma_lease_t       lease;
    dmosi_semaphore_t   done;
    volatile bool       active;
    volatile uint32_t   result;         /* 0 running, 1 complete, 2 failed */
    volatile uint32_t   last_event;     /* The failing one, for the log */
    bool                unavailable;    /* No stream: do not look again */
    void               *buffer;         /* Of the transfer in progress, for the cache */
    uint32_t            size;
    bool                read;
    bool                warned;         /* The accepted-error note was logged */
} g_dma;

static void dcache(volatile uint32_t *operation, const void *buffer, uint32_t length)
{
    if ((SCB_CCR & SCB_CCR_DC) == 0U || length == 0U)
        return;
    uintptr_t line = (uintptr_t)buffer & ~(uintptr_t)(DMA_ALIGNMENT - 1U);
    uintptr_t end  = (uintptr_t)buffer + length;
    __asm__ volatile ("dsb" ::: "memory");
    for (; line < end; line += DMA_ALIGNMENT)
        *operation = (uint32_t)line;
    __asm__ volatile ("dsb\n\tisb" ::: "memory");
}

static bool buffer_usable(const void *buffer, uint32_t size)
{
    uintptr_t start = (uintptr_t)buffer;
    if (start % DMA_ALIGNMENT != 0U || size % DMA_ALIGNMENT != 0U)
        return false;
    /* ITCM (< 0x00080000) and DTCM (the first 128 KiB of 0x20000000) are not reached */
    return !(start < 0x00080000UL || (start < 0x20020000UL && start + size > 0x20000000UL));
}

/* Lease callback: interrupt context (or synchronous from abort, then !active).
 * An error flag together with "complete" is accepted: the hardware also
 * flags a FIFO/direct-mode error on a transfer that went through (all the
 * elements moved - checked in dmqspi_stm32_dma_wait() by the remaining
 * count, and by the data compare of tools/qspitest). A failure is an
 * error flag WITHOUT complete (the stream was stopped). */
static void dma_callback(dmdma_lease_t lease, dmdma_event_t event, void *user_ptr)
{
    (void)lease; (void)user_ptr;
    if (!g_dma.active)
        return;
    if ((event & dmdma_event_error) != 0)
        g_dma.last_event = (uint32_t)event;
    if ((event & dmdma_event_complete) != 0 && g_dma.result == 0U)
        g_dma.result = 1U;
    else if ((event & dmdma_event_error) != 0)
        g_dma.result = 2U;
    else
        return;
    (void)dmosi_semaphore_post(g_dma.done, 1);
}

static bool dma_ensure(void)
{
    if (g_dma.lease != NULL)
        return true;
    if (g_dma.unavailable)
        return false;
    if (g_dma.done == NULL)
        g_dma.done = dmosi_semaphore_create(0, 1);
    if (g_dma.done != NULL)
        g_dma.lease = dmdma_lease_acquire(DMA_CONTROLLER, DMA_STREAM);
    if (g_dma.lease != NULL && dmdma_lease_set_callback(g_dma.lease, dma_callback, NULL) != 0)
    {
        dmdma_lease_release(g_dma.lease);
        g_dma.lease = NULL;
    }
    if (g_dma.lease == NULL)
    {
        DMOD_LOG_WARN("dmqspi_port: no DMA2 stream 7 - data moved by the CPU\n");
        g_dma.unavailable = true;
        return false;
    }
    return true;
}

void dmqspi_stm32_dma_release(void)
{
    g_dma.active = false;
    if (g_dma.lease != NULL)
        dmdma_lease_release(g_dma.lease);
    g_dma.lease = NULL;
    if (g_dma.done != NULL)
        dmosi_semaphore_destroy(g_dma.done);
    g_dma.done = NULL;
    g_dma.unavailable = false;
}

int dmqspi_stm32_dma_start(bool read, void *buffer, uint32_t size, uint32_t dr_address)
{
    if (size < DMA_MIN_BYTES || size > DMA_MAX_BYTES || !buffer_usable(buffer, size) || !dma_ensure())
        return 1;

    (void)dmosi_semaphore_wait(g_dma.done, 1, 0);       /* A late wake-up of an earlier transfer */
    g_dma.result = 0;
    g_dma.active = true;
    g_dma.buffer = buffer;
    g_dma.size = size;
    g_dma.read = read;
    dcache(read ? &SCB_DCCIMVAC : &SCB_DCCMVAC, buffer, size);

    dmdma_transfer_config_t cfg = {
        .direction             = read ? dmdma_direction_peripheral_to_memory : dmdma_direction_memory_to_peripheral,
        .request               = DMA_CHANNEL,
        .source_address        = read ? (const void *)(uintptr_t)dr_address : (const void *)buffer,
        .destination_address   = read ? buffer : (void *)(uintptr_t)dr_address,
        .source_width          = dmdma_data_width_byte,
        .destination_width     = dmdma_data_width_byte,
        .source_increment      = !read,
        .destination_increment = read,
        .circular              = false,
        .priority              = dmdma_priority_high,
        .element_count         = size,
        .timeout_ms            = 0,     /* The semaphore wait bounds it (see dmspi's note on the dmdma watchdog) */
    };
    if (dmdma_lease_start(g_dma.lease, &cfg) != 0)
    {
        g_dma.active = false;
        return -EIO;
    }
    return 0;
}

int dmqspi_stm32_dma_wait(uint32_t timeout_ms)
{
    int ret = 0;
    if (dmosi_semaphore_wait(g_dma.done, 1, (int32_t)(timeout_ms + DMA_SLACK_MS)) != 0)
        ret = -ETIMEDOUT;
    else if (g_dma.result != 1U || dmdma_lease_get_remaining(g_dma.lease) != 0)
        ret = -EIO;
    if (ret == 0 && g_dma.last_event != 0U && !g_dma.warned)
    {
        g_dma.warned = true;
        DMOD_LOG_WARN("dmqspi_port: DMA flagged an error on a completed transfer (event 0x%x) - accepted\n",
                      (unsigned)g_dma.last_event);
    }
    g_dma.last_event = 0U;
    if (ret != 0)
        DMOD_LOG_ERROR("dmqspi_port: DMA transfer of %u bytes failed (%d): event 0x%x, remaining %u\n",
                       (unsigned)g_dma.size, ret, (unsigned)g_dma.last_event,
                       (unsigned)dmdma_lease_get_remaining(g_dma.lease));
    g_dma.active = false;
    dmdma_lease_abort(g_dma.lease);
    if (g_dma.read)
        dcache(&SCB_DCIMVAC, g_dma.buffer, g_dma.size);
    return ret;
}
