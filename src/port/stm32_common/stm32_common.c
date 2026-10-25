#include <errno.h>
#include "dmod.h"
#include "dmosi.h"
#include "dmclk_port.h"
#include "dmqspi_port.h"
#include "stm32_common.h"

/*
 * Shared QUADSPI controller implementation (register recipe:
 * docs/port-implementation.md; names from RM0385 "Quad-SPI interface").
 * Indirect data is moved by polling the FIFO (the transfer is bounded by the
 * bus speed); auto-polling and memory-mapped aborts sleep on an IRQ semaphore.
 */

typedef struct
{
    volatile uint32_t CR, DCR, SR, FCR, DLR, CCR, AR, ABR, DR, PSMKR, PSMAR, PIR, LPTR;
} qspi_regs_t;

#define CR_EN           (1u << 0)
#define CR_ABORT        (1u << 1)
#define CR_DMAEN        (1u << 2)
#define CR_SSHIFT       (1u << 4)
#define CR_FSEL         (1u << 7)
#define CR_TEIE         (1u << 16)
#define CR_SMIE         (1u << 19)
#define CR_APMS         (1u << 22)
#define CR_PRESC_POS    24u
#define SR_TEF          (1u << 0)
#define SR_TCF          (1u << 1)
#define SR_FTF          (1u << 2)
#define SR_SMF          (1u << 3)
#define SR_BUSY         (1u << 5)
#define SR_FLEVEL(sr)   (((sr) >> 8) & 0x3Fu)
#define FCR_CTEF        (1u << 0)
#define FCR_CTCF        (1u << 1)
#define FCR_CSMF        (1u << 3)
#define FMODE_WRITE     0u
#define FMODE_READ      1u
#define FMODE_POLL      2u
#define FMODE_MAPPED    3u

#define RCC_BASE        0x40023800UL
#define RCC_AHB3RSTR    (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define RCC_AHB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x38))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x08))
#define RCC_QSPI_BIT    (1u << 1)
/* Waits are bounded by time (the RTOS tick); the spin count only ends them when the tick does not run -
 * before the scheduler starts. */
#define SPIN_LIMIT          4000000u
#define IDLE_TIMEOUT_MS     2u      /* BUSY at the start of a command: nothing of ours is running, so it is a leftover */
#define ABORT_TIMEOUT_MS    20u     /* the controller to let go after CR.ABORT */
#define FLAG_TIMEOUT_MS     200u    /* a flag or a byte of the FIFO the flash is expected to produce */

static struct
{
    bool                    ready;
    dmqspi_config_t         config;
    dmqspi_mode_t           mode;
    dmosi_mutex_t           lock;
    dmosi_semaphore_t       irq_sem;
} g_q;

static inline qspi_regs_t *regs(void)
{
    return (qspi_regs_t *)(uintptr_t)dmqspi_stm32_get_family()->regs_base;
}

/* The IRQ calls dmosi from the handler, so its priority must not be above what
 * FreeRTOS masks in critical sections (see dmspi's stm32_spi_nvic_enable_irq). */
static void nvic_enable(uint32_t irqn)
{
    volatile uint8_t  *ip   = (volatile uint8_t *)0xE000E400UL;
    volatile uint32_t *iser = (volatile uint32_t *)0xE000E100UL;
    ip[irqn] = (uint8_t)dmosi_get_min_interrupt_priority();
    iser[irqn >> 5u] = 1u << (irqn & 0x1Fu);
}

static bool valid_instance(dmqspi_instance_t instance)
{
    return g_q.ready && instance == g_q.config.instance;
}

/* --- helpers --- */

static uint32_t ahb_divider(void)
{
    uint32_t hpre = (RCC_CFGR >> 4) & 0xFu;
    if (hpre < 8u) return 1u;
    return hpre < 12u ? (2u << (hpre - 8u)) : (64u << (hpre - 12u));
}

static uint32_t kernel_clock_hz(void)
{
    return (uint32_t)dmclk_port_get_current_frequency() / ahb_divider();
}

static uint32_t log2_floor(uint32_t v)
{
    uint32_t n = 0;
    while (v >>= 1) n++;
    return n;
}

static uint32_t lines_to_mode(dmqspi_lines_t lines)
{
    switch (lines)
    {
        case dmqspi_lines_1: return 1u;
        case dmqspi_lines_2: return 2u;
        case dmqspi_lines_4: return 3u;
        default:             return 0u;
    }
}

static uint32_t build_ccr(const dmqspi_command_t *c, uint32_t fmode)
{
    uint32_t ccr = c->instruction;
    ccr |= lines_to_mode(c->instruction_lines) << 8;
    ccr |= lines_to_mode(c->address_lines) << 10;
    ccr |= (uint32_t)c->address_size << 12;
    ccr |= lines_to_mode(c->alternate_lines) << 14;
    ccr |= (uint32_t)(c->alternate_size ? c->alternate_size - 1u : 0u) << 16;
    ccr |= ((uint32_t)c->dummy_cycles & 0x1Fu) << 18;
    ccr |= lines_to_mode(c->data_lines) << 24;
    ccr |= fmode << 26;
    return ccr;
}

/* A wait in progress: when it started (RTOS tick) and how many times it looked. */
typedef struct
{
    uint32_t start;
    uint32_t spins;
} wait_t;

static wait_t wait_begin(void)
{
    wait_t w = { dmosi_get_tick_count(), 0 };
    return w;
}

/* True when a wait of @p timeout_ms is over. The tick is read only now and then. */
static bool wait_expired(wait_t *w, uint32_t timeout_ms)
{
    if ((++w->spins & 63u) != 0u)
        return false;
    return w->spins >= SPIN_LIMIT || (uint32_t)(dmosi_get_tick_count() - w->start) > timeout_ms;
}

static int wait_flag(uint32_t flag)
{
    qspi_regs_t *q = regs();
    for (wait_t w = wait_begin(); !wait_expired(&w, FLAG_TIMEOUT_MS); )
    {
        uint32_t sr = q->SR;
        if (sr & SR_TEF) { q->FCR = FCR_CTEF; return -EIO; }
        if (sr & flag) return 0;
    }
    return -ETIMEDOUT;
}

static bool wait_idle(uint32_t timeout_ms)
{
    qspi_regs_t *q = regs();
    for (wait_t w = wait_begin(); ; )
    {
        if (!(q->SR & SR_BUSY)) return true;
        if (wait_expired(&w, timeout_ms)) return false;
    }
}

static int wait_not_busy(void)
{
    return wait_idle(FLAG_TIMEOUT_MS) ? 0 : -ETIMEDOUT;
}

/* Program the controller: pulse its reset, then clock, bank and flash size from @p config. */
static void configure_controller(const dmqspi_config_t *config)
{
    RCC_AHB3ENR |= RCC_QSPI_BIT;
    RCC_AHB3RSTR |= RCC_QSPI_BIT;
    RCC_AHB3RSTR &= ~RCC_QSPI_BIT;
    uint32_t kclk = kernel_clock_hz();
    uint32_t presc = kclk == 0 ? 255u : (kclk + config->clock_hz - 1u) / config->clock_hz - 1u;
    if (presc > 255u) presc = 255u;
    qspi_regs_t *q = regs();
    q->DCR = (config->clock_mode ? 1u : 0u) | ((uint32_t)(config->cs_high_cycles - 1u) << 8) |
             ((log2_floor(config->flash_size_bytes) - 1u) << 16);
    q->CR = (presc << CR_PRESC_POS) | (config->sample_shift_half ? CR_SSHIFT : 0u) |
            (config->bank == dmqspi_bank_2 ? CR_FSEL : 0u) | CR_EN;
}

/* Take what an indirect read left in the FIFO out of it, so that a held-up transfer can end. */
static void drain_fifo(void)
{
    qspi_regs_t *q = regs();
    for (uint32_t i = 0; i < 64u && SR_FLEVEL(q->SR) != 0u; i++)
        (void)*(volatile uint8_t *)&q->DR;
}

/*
 * Bring the controller back to idle, whatever it is doing: stop the DMA, abort, and when it does not let go (a
 * read stalls on a full FIFO nobody empties) empty the FIFO and abort again; as the last resort reset the
 * peripheral. Called with the lock held - no transfer of ours is going on - and in indirect mode.
 * @return 0, or -EIO when even the reset did not help.
 */
static int recover(void)
{
    qspi_regs_t *q = regs();
    uint32_t sr = q->SR, cr = q->CR, ccr = q->CCR;
    unsigned stage = 1;

    dmqspi_stm32_dma_cancel();
    q->CR &= ~CR_DMAEN;
    q->CR |= CR_ABORT;
    if (!wait_idle(ABORT_TIMEOUT_MS))
    {
        stage = 2;
        drain_fifo();
        q->CR |= CR_ABORT;
        if (!wait_idle(ABORT_TIMEOUT_MS))
        {
            stage = 3;
            configure_controller(&g_q.config);
            if (!wait_idle(ABORT_TIMEOUT_MS))
                stage = 4;
        }
    }
    q->FCR = FCR_CTEF | FCR_CTCF | FCR_CSMF;
    if (stage > 1)
    {
        DMOD_LOG_WARN("dmqspi_port: the controller held on to a transfer (SR=0x%x CR=0x%x CCR=0x%x) - %s\n",
                      (unsigned)sr, (unsigned)cr, (unsigned)ccr,
                      stage == 2 ? "let go after the FIFO was emptied" :
                      stage == 3 ? "reset" : "NOT recovered, even by a reset");
    }
    else
    {
        DMOD_LOG_INFO("dmqspi_port: aborted a transfer left behind (SR=0x%x CR=0x%x CCR=0x%x)\n",
                      (unsigned)sr, (unsigned)cr, (unsigned)ccr);
    }
    return stage == 4 ? -EIO : 0;
}

/* Abort the current transfer and wait for the controller to go idle. */
static void abort_transfer(void)
{
    (void)recover();
}

/* Program DLR/ABR/CCR. Without an address phase this write also starts the transfer. */
static void prepare_command(const dmqspi_command_t *c, uint32_t fmode)
{
    qspi_regs_t *q = regs();
    if (c->data_lines != dmqspi_lines_none && c->data_size > 0 && fmode != FMODE_MAPPED)
        q->DLR = c->data_size - 1u;
    if (c->alternate_lines != dmqspi_lines_none)
        q->ABR = c->alternate;
    q->CCR = build_ccr(c, fmode);
}

/* With an address phase, writing AR starts the transfer. */
static void kick_command(const dmqspi_command_t *c, uint32_t fmode)
{
    if (c->address_lines != dmqspi_lines_none && fmode != FMODE_MAPPED)
        regs()->AR = c->address;
}

static void start_command(const dmqspi_command_t *c, uint32_t fmode)
{
    prepare_command(c, fmode);
    kick_command(c, fmode);
}

static int begin(dmqspi_instance_t instance)
{
    if (!valid_instance(instance)) return -EINVAL;
    dmosi_mutex_lock(g_q.lock);
    if (g_q.mode == dmqspi_mode_memory_mapped)
    {
        dmosi_mutex_unlock(g_q.lock);
        return -EBUSY;
    }
    /* With the lock taken no transfer of ours is running, so a busy controller was left behind. */
    if (!wait_idle(IDLE_TIMEOUT_MS) && recover() != 0)
    {
        dmosi_mutex_unlock(g_q.lock);
        return -EBUSY;
    }
    return 0;
}

static int finish(int result)
{
    dmosi_mutex_unlock(g_q.lock);
    return result;
}

static int read_data(uint8_t *out, uint32_t size)
{
    qspi_regs_t *q = regs();
    uint32_t got = 0;
    wait_t w = wait_begin();
    while (got < size)
    {
        uint32_t sr = q->SR;
        if (sr & SR_TEF) { q->FCR = FCR_CTEF; return -EIO; }
        uint32_t level = SR_FLEVEL(sr);
        if (level == 0)
        {
            if (wait_expired(&w, FLAG_TIMEOUT_MS)) return -ETIMEDOUT;
            continue;
        }
        w = wait_begin();           /* the flash is delivering: the wait starts again */
        while (level-- && got < size)
            out[got++] = *(volatile uint8_t *)&q->DR;
    }
    return 0;
}

static int write_data(const uint8_t *in, uint32_t size)
{
    qspi_regs_t *q = regs();
    for (uint32_t i = 0; i < size; i++)
    {
        int ret = wait_flag(SR_FTF);
        if (ret != 0) return ret;
        *(volatile uint8_t *)&q->DR = in[i];
    }
    return 0;
}

/* Longest a transfer of `size` bytes can take on one line at the requested clock, in ms. */
__attribute__((unused)) static uint32_t transfer_budget_ms(uint32_t size)
{
    uint32_t khz = g_q.config.clock_hz / 1000u;
    return khz == 0 ? 1000u : (uint32_t)(((uint64_t)size * 8u * 2u) / khz) + 1u;
}

/* Indirect read: CCR first (FMODE=read - a leftover FMODE=write from the previous command
 * keeps FTF set and the DMA would drain an empty FIFO at once), then the DMA stream and
 * DMAEN, the address write starts the transfer.
 * 1 = DMA not used (CCR is programmed, the caller kicks the command), 0 = done, <0 = error. */
static int run_dma_read(const dmqspi_command_t *cmd, uint32_t fmode, void *rx)
{
    qspi_regs_t *q = regs();
    prepare_command(cmd, fmode);
    int ret = dmqspi_stm32_dma_start(true, rx, cmd->data_size, (uint32_t)(uintptr_t)&q->DR);
    if (ret != 0) return ret;
    q->CR |= CR_DMAEN;
    kick_command(cmd, fmode);
    ret = dmqspi_stm32_dma_wait(transfer_budget_ms(cmd->data_size));
    q->CR &= ~CR_DMAEN;
    return ret;
}

/* Indirect write: the command starts first (the clock waits for data), then the
 * DMA feeds the FIFO - like the ST HAL; DMA before the command pushes bytes in
 * before the controller is configured. Falls back to the FIFO loop. */
static int run_write(const dmqspi_command_t *cmd, uint32_t fmode, const void *tx)
{
    qspi_regs_t *q = regs();
    start_command(cmd, fmode);
    int ret = dmqspi_stm32_dma_start(false, (void *)tx, cmd->data_size, (uint32_t)(uintptr_t)&q->DR);
    if (ret == 1) return write_data(tx, cmd->data_size);
    if (ret != 0) return ret;
    q->CR |= CR_DMAEN;
    ret = dmqspi_stm32_dma_wait(transfer_budget_ms(cmd->data_size));
    q->CR &= ~CR_DMAEN;
    return ret;
}

/* Common indirect transfer: fmode READ/WRITE (or write with no data phase). */
static int run_indirect(dmqspi_instance_t instance, const dmqspi_command_t *cmd,
                        uint32_t fmode, void *rx, const void *tx)
{
    int ret = begin(instance);
    if (ret != 0) return ret;
    qspi_regs_t *q = regs();
    q->FCR = FCR_CTEF | FCR_CTCF;
    if (tx != NULL)
    {
        ret = run_write(cmd, fmode, tx);
    }
    else
    {
        ret = rx != NULL ? run_dma_read(cmd, fmode, rx) : 1;
        if (ret == 1 && rx != NULL)
        {
            kick_command(cmd, fmode);
            ret = read_data(rx, cmd->data_size);
        }
        else if (ret == 1)
        {
            start_command(cmd, fmode);
            ret = 0;
        }
    }
    if (ret == 0) ret = wait_flag(SR_TCF);
    q->FCR = FCR_CTCF;
    if (ret != 0) abort_transfer();
    return finish(ret);
}

/* --- Port API --- */

dmod_dmqspi_port_api_declaration(1.0, int, _init, ( const dmqspi_config_t *config ))
{
    if (config == NULL || g_q.ready) return config == NULL ? -EINVAL : -EBUSY;
    if (config->instance != 1 || config->clock_hz == 0 ||
        config->flash_size_bytes < 2 || (config->flash_size_bytes & (config->flash_size_bytes - 1)))
        return -EINVAL;
    g_q.lock = dmosi_mutex_create(false);
    g_q.irq_sem = dmosi_semaphore_create(0, 1);
    if (g_q.lock == NULL || g_q.irq_sem == NULL) return -ENOMEM;
    configure_controller(config);
    g_q.config = *config;
    g_q.mode = dmqspi_mode_indirect;
    g_q.ready = true;
    nvic_enable(dmqspi_stm32_get_family()->irq);
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, int, _deinit, ( dmqspi_instance_t instance ))
{
    if (!valid_instance(instance)) return -EINVAL;
    if (g_q.mode == dmqspi_mode_memory_mapped) dmqspi_port_exit_memory_mapped(instance);
    dmosi_mutex_lock(g_q.lock);
    regs()->CR = 0;
    RCC_AHB3ENR &= ~RCC_QSPI_BIT;
    g_q.ready = false;
    dmqspi_stm32_dma_release();
    dmosi_mutex_unlock(g_q.lock);
    dmosi_mutex_destroy(g_q.lock);
    dmosi_semaphore_destroy(g_q.irq_sem);
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, int, _command, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd ))
{
    if (cmd == NULL || cmd->data_lines != dmqspi_lines_none) return -EINVAL;
    return run_indirect(instance, cmd, FMODE_WRITE, NULL, NULL);
}

dmod_dmqspi_port_api_declaration(1.0, int, _read, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, void *data ))
{
    if (cmd == NULL || data == NULL || cmd->data_size == 0) return -EINVAL;
    return run_indirect(instance, cmd, FMODE_READ, data, NULL);
}

dmod_dmqspi_port_api_declaration(1.0, int, _write, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const void *data ))
{
    if (cmd == NULL || data == NULL || cmd->data_size == 0) return -EINVAL;
    return run_indirect(instance, cmd, FMODE_WRITE, NULL, data);
}

dmod_dmqspi_port_api_declaration(1.0, int, _auto_poll, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const dmqspi_poll_t *poll ))
{
    if (cmd == NULL || poll == NULL || cmd->data_size < 1 || cmd->data_size > 4) return -EINVAL;
    int ret = begin(instance);
    if (ret != 0) return ret;
    qspi_regs_t *q = regs();
    while (dmosi_semaphore_wait(g_q.irq_sem, 1, 0) == 0) { }    /* drop a stale signal */
    q->FCR = FCR_CTEF | FCR_CSMF;
    q->PSMKR = poll->mask;
    q->PSMAR = poll->match;
    q->PIR = 0x10u;
    q->CR = (q->CR & ~(1u << 23)) | CR_APMS | CR_SMIE | CR_TEIE;
    start_command(cmd, FMODE_POLL);
    ret = dmosi_semaphore_wait(g_q.irq_sem, 1, (int32_t)poll->timeout_ms) == 0 ? 0 : -ETIMEDOUT;
    if (ret == 0 && (q->SR & SR_TEF)) ret = -EIO;
    q->CR &= ~(CR_SMIE | CR_TEIE | CR_APMS);
    if (ret != 0) abort_transfer(); else { q->FCR = FCR_CSMF; ret = wait_not_busy(); }
    return finish(ret);
}

dmod_dmqspi_port_api_declaration(1.0, int, _enter_memory_mapped, ( dmqspi_instance_t instance, const dmqspi_command_t *read_cmd ))
{
    if (read_cmd == NULL) return -EINVAL;
    int ret = begin(instance);
    if (ret != 0) return ret == -EBUSY && g_q.mode == dmqspi_mode_memory_mapped ? 0 : ret;
    start_command(read_cmd, FMODE_MAPPED);
    g_q.mode = dmqspi_mode_memory_mapped;
    return finish(0);
}

dmod_dmqspi_port_api_declaration(1.0, int, _exit_memory_mapped, ( dmqspi_instance_t instance ))
{
    if (!valid_instance(instance)) return -EINVAL;
    dmosi_mutex_lock(g_q.lock);
    if (g_q.mode == dmqspi_mode_memory_mapped)
    {
        abort_transfer();
        g_q.mode = dmqspi_mode_indirect;
    }
    return finish(0);
}

dmod_dmqspi_port_api_declaration(1.0, uintptr_t, _get_mapped_base, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return (uintptr_t)dmqspi_stm32_get_family()->mapped_base;
}

dmod_dmqspi_port_api_declaration(1.0, dmqspi_mode_t, _get_mode, ( dmqspi_instance_t instance ))
{
    return valid_instance(instance) ? g_q.mode : dmqspi_mode_indirect;
}

dmod_dmqspi_port_api_declaration(1.0, void, _invalidate_mapped, ( dmqspi_instance_t instance, uint32_t offset, uint32_t size ))
{
    (void)instance;
    const dmqspi_stm32_family_t *f = dmqspi_stm32_get_family();
    if (!f->has_dcache || size == 0) return;
    volatile uint32_t *dcimvac = (volatile uint32_t *)0xE000EF5CUL;   /* SCB->DCIMVAC */
    uint32_t addr = (f->mapped_base + offset) & ~31u;
    uint32_t end = f->mapped_base + offset + size;
    __asm volatile ("dsb 0xF" ::: "memory");
    for (; addr < end; addr += 32u) *dcimvac = addr;
    __asm volatile ("dsb 0xF\n isb 0xF" ::: "memory");
}

/* --- ISR: wake the thread waiting in _auto_poll --- */

void dmqspi_stm32_irq_handler(void)
{
    qspi_regs_t *q = regs();
    uint32_t sr = q->SR;
    if (sr & (SR_SMF | SR_TEF))
    {
        q->CR &= ~(CR_SMIE | CR_TEIE);      /* the thread clears the flags */
        dmosi_semaphore_post(g_q.irq_sem, 1);
    }
}
