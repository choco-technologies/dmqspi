#define DMOD_ENABLE_REGISTRATION    ON
#include "dmod.h"
#include "dmqspi_port.h"
#include "esp32s3_mspi.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * The memory SPI controller (MSPI) of the ESP32-S3 as a dmqspi port, for the chip on its chip select 1: the external RAM.
 *
 *  - Commands (_command/_read/_write) are sent by SPI1, the way ESP-IDF does it (the routines of the ROM program the
 *    registers; this port picks the mode of the lines and keeps the registers it does not own intact).
 *  - The memory-mapped window (_enter_memory_mapped_rw) is SPI0 + the MMU: the registers that tell the cache what to send for
 *    a read and for a write access, then the entries of the MMU that point the window at the chip.
 *
 * Chip select 0 is the flash the firmware runs from: this port never touches it. Everything between the "critical" marks
 * runs with the interrupts masked and calls nothing but the ROM and this file: the firmware is executed from the flash through
 * the same cache that is reconfigured.
 */

#define MAX_DATA_BYTES      64U         /* the W0..W15 registers of the controller */
#define CS1_MASK            BIT(1)
#define MAX_CLOCK_HZ        40000000U   /* no timing tuning of the MSPI here: what ESP-IDF runs without it */
#define MAX_OCTAL_CLOCK_HZ  20000000U   /* the DDR bus needs the tuning from 40 MHz up */

typedef struct
{
    uint16_t  cmd;
    uint16_t  cmd_bit_len;
    uint32_t *addr;
    uint32_t  addr_bit_len;
    uint32_t *tx_data;
    uint32_t  tx_data_bit_len;
    uint32_t *rx_data;
    uint32_t  rx_data_bit_len;
    uint32_t  dummy_bit_len;
} rom_spi_cmd_t;

static struct
{
    bool                ready;
    dmqspi_config_t     config;
    dmqspi_mode_t       mode;
    uint32_t            lines;              /* data lines of the chip: 4 or 8 */
    uint32_t            first_entry;
    uint32_t            entries;
} s_q;

/* the other four data lines and DQS of an octal chip (the reset state is GPIO) */
static const struct { uint8_t gpio; uint8_t func; } s_pads_octal[] =
{
    { 33, 4 }, { 34, 4 }, { 35, 4 }, { 36, 4 },     /* SPIIO4-7 */
    { 37, 4 },                                      /* SPIDQS */
};

static const struct { uint8_t gpio; uint8_t func; } s_pads[] =
{
    { 26, 0 },  /* SPICS1: chip select of the RAM */
    { 27, 0 },  /* SPIHD (D3) */
    { 28, 0 },  /* SPIWP (D2) */
    { 31, 0 },  /* SPIQ  (D1) */
    { 32, 0 },  /* SPID  (D0) */
};

static inline volatile uint32_t *reg(uint32_t address)
{
    return (volatile uint32_t *)address;
}

static inline void reg_set_bits(uint32_t address, uint32_t mask)
{
    *reg(address) |= mask;
}

static inline void reg_clear_bits(uint32_t address, uint32_t mask)
{
    *reg(address) &= ~mask;
}

static inline void reg_set_field(uint32_t address, uint32_t mask, uint32_t shift, uint32_t value)
{
    *reg(address) = (*reg(address) & ~(mask << shift)) | ((value & mask) << shift);
}

static uint32_t address_bits(dmqspi_addr_size_t size)
{
    return 8U * ((uint32_t)size + 1U);
}

/* ---- the clock ---- */

static uint32_t clock_divider(uint32_t clock_hz)
{
    uint32_t div = (APB_FREQUENCY_HZ + clock_hz - 1U) / clock_hz;   /* rounded up: never above what was asked */
    return div < 1U ? 1U : div;
}

static void set_clock(uint32_t div)
{
    /* the register is shared by the cache (SPI0) and the commands (SPI1) */
    if (div == 1U)
        *reg(MSPI_SRAM_CLK_REG(0)) = SRAM_CLK_EQU_SYSCLK;
    else
        *reg(MSPI_SRAM_CLK_REG(0)) = ((div - 1U) << SRAM_CLK_CNT_N_S) | ((div / 2U - 1U) << SRAM_CLK_CNT_H_S) | ((div - 1U) << SRAM_CLK_CNT_L_S);
}

/* ---- a command by SPI1 ---- */

/** How the lines of the phases map onto the modes of the controller: -1 when it has none for them. */
static int command_mode(const dmqspi_command_t *c, bool *fcmd_quad)
{
    *fcmd_quad = false;
    bool addr = c->address_lines != dmqspi_lines_none;
    bool data = c->data_lines != dmqspi_lines_none;

    if (c->instruction_lines == dmqspi_lines_1 &&
        (!addr || c->address_lines == dmqspi_lines_1) && (!data || c->data_lines == dmqspi_lines_1))
        return ROM_SPIFLASH_SLOWRD_MODE;

    if (c->instruction_lines == dmqspi_lines_4 &&
        (!addr || c->address_lines == dmqspi_lines_4) && (!data || c->data_lines == dmqspi_lines_4))
    {
        *fcmd_quad = true;      /* the chip is in the quad (QPI) mode: the instruction goes out on four lines too */
        return ROM_SPIFLASH_QIO_MODE;
    }

    if (c->instruction_lines == dmqspi_lines_1 &&
        (!addr || c->address_lines == dmqspi_lines_4) && (!data || c->data_lines == dmqspi_lines_4))
        return ROM_SPIFLASH_QIO_MODE;

    return -1;
}

/** An octal command: DDR, every phase on eight lines, a 32-bit address (what the octal chips take in the OPI DTR mode). */
static bool is_octal_command(const dmqspi_command_t *c)
{
    return c->ddr && c->instruction_lines == dmqspi_lines_8 &&
           (c->address_lines == dmqspi_lines_none || (c->address_lines == dmqspi_lines_8 && c->address_size == dmqspi_addr_32bit)) &&
           (c->data_lines == dmqspi_lines_none || c->data_lines == dmqspi_lines_8);
}

/** The dummy cycles of an octal DDR phase as the ROM counts them: two bits for every cycle but the first. */
static uint32_t octal_dummy_bits(uint8_t cycles)
{
    return cycles != 0U ? 2U * ((uint32_t)cycles - 1U) : 0U;
}

static int check_command(const dmqspi_command_t *c)
{
    bool quad;
    if (c->instruction_lines == dmqspi_lines_none || c->alternate_lines != dmqspi_lines_none)
        return -ENOTSUP;
    if (c->data_size > MAX_DATA_BYTES)
        return -EINVAL;
    if (s_q.lines == 8U)
        return is_octal_command(c) ? 0 : -ENOTSUP;     /* an octal chip is in the OPI DTR mode: that is all it understands */
    if (c->ddr || c->instruction_lines == dmqspi_lines_8 || c->address_lines == dmqspi_lines_8 || c->data_lines == dmqspi_lines_8)
        return -ENOTSUP;
    if (command_mode(c, &quad) < 0)
        return -ENOTSUP;
    return 0;
}

typedef void (*opiflash_exec_cmd_t)(int spi_num, int mode, uint32_t cmd, int cmd_bit_len, uint32_t addr, int addr_bit_len,
                                    int dummy_bits, uint8_t *mosi_data, int mosi_bit_len, uint8_t *miso_data,
                                    int miso_bit_len, uint32_t cs_mask, bool is_write_erase_operation);

static void __attribute__((noinline)) execute_octal_critical(const dmqspi_command_t *c, uint8_t *tx, uint8_t *rx)
{
    /* the byte of the command goes out twice: in the DDR mode there are two bytes of it */
    uint32_t cmd = ((uint32_t)c->instruction << 8) | c->instruction;
    uint32_t bits = c->data_size * 8U;
    ((opiflash_exec_cmd_t)ROM_OPIFLASH_EXEC_CMD)(1, ROM_SPIFLASH_OPI_DTR_MODE, cmd, 16, c->address,
        c->address_lines != dmqspi_lines_none ? 32 : 0, (int)octal_dummy_bits(c->dummy_cycles),
        tx, tx != NULL ? (int)bits : 0, rx, rx != NULL ? (int)bits : 0, CS1_MASK, false);
}

static void __attribute__((noinline)) execute_critical(const dmqspi_command_t *c, int mode, bool fcmd_quad, const void *tx, void *rx)
{
    uint32_t addr = c->address;
    rom_spi_cmd_t rc;
    memset(&rc, 0, sizeof(rc));
    rc.cmd = c->instruction;
    rc.cmd_bit_len = 8;
    rc.addr = &addr;
    rc.addr_bit_len = c->address_lines != dmqspi_lines_none ? address_bits(c->address_size) : 0U;
    rc.dummy_bit_len = c->dummy_cycles;     /* the clock cycles on the lines of the data phase */
    rc.tx_data = (uint32_t *)tx;
    rc.tx_data_bit_len = tx != NULL ? c->data_size * 8U : 0U;
    rc.rx_data = (uint32_t *)rx;
    rc.rx_data_bit_len = rx != NULL ? c->data_size * 8U : 0U;

    uint32_t usr = *reg(MSPI_USER_REG(1)), usr1 = *reg(MSPI_USER1_REG(1)), usr2 = *reg(MSPI_USER2_REG(1)), ctrl = *reg(MSPI_CTRL_REG(1));

    ((void (*)(int, int))ROM_SPI_SET_OP_MODE)(1, mode);
    if (fcmd_quad)
        reg_set_bits(MSPI_CTRL_REG(1), CTRL_FCMD_QUAD);
    ((void (*)(int, rom_spi_cmd_t *))ROM_SPI_CMD_CONFIG)(1, &rc);
    ((void (*)(int, uint8_t *, uint16_t, uint8_t, bool))ROM_SPI_CMD_START)(1, (uint8_t *)rx, (uint16_t)(rx != NULL ? c->data_size : 0U), CS1_MASK, false);

    *reg(MSPI_USER_REG(1)) = usr;
    *reg(MSPI_USER1_REG(1)) = usr1;
    *reg(MSPI_USER2_REG(1)) = usr2;
    *reg(MSPI_CTRL_REG(1)) = ctrl;
}

static int execute(const dmqspi_command_t *c, const void *tx, void *rx)
{
    if (!s_q.ready) return -EINVAL;
    if (c == NULL) return -EINVAL;
    if (s_q.mode != dmqspi_mode_indirect) return -EBUSY;
    int ret = check_command(c);
    if (ret != 0) return ret;

    bool fcmd_quad = false;
    int mode = s_q.lines == 8U ? 0 : command_mode(c, &fcmd_quad);
    /* the ROM routines move words: a buffer of its own, aligned */
    uint32_t tx_words[MAX_DATA_BYTES / 4U], rx_words[MAX_DATA_BYTES / 4U];
    if (tx != NULL) memcpy(tx_words, tx, c->data_size);

    Dmod_EnterCritical();
    if (s_q.lines == 8U)
        execute_octal_critical(c, tx != NULL ? (uint8_t *)tx_words : NULL, rx != NULL ? (uint8_t *)rx_words : NULL);
    else
        execute_critical(c, mode, fcmd_quad, tx != NULL ? (const void *)tx_words : NULL, rx != NULL ? (void *)rx_words : NULL);
    Dmod_ExitCritical();

    if (rx != NULL) memcpy(rx, rx_words, c->data_size);
    return 0;
}

/* ---- the memory-mapped window: SPI0 and the MMU ---- */

static int check_mapped_command(const dmqspi_command_t *c)
{
    if (s_q.lines == 8U)
    {
        if (!c->ddr || c->instruction_lines != dmqspi_lines_8 || c->address_lines != dmqspi_lines_8 || c->data_lines != dmqspi_lines_8 ||
            c->address_size != dmqspi_addr_32bit || c->alternate_lines != dmqspi_lines_none)
            return -ENOTSUP;
        return c->dummy_cycles == 0U || c->dummy_cycles > 32U ? -EINVAL : 0;     /* the dummy phase is always there on the octal bus */
    }
    if (c->instruction_lines != dmqspi_lines_1 && c->instruction_lines != dmqspi_lines_4) return -ENOTSUP;
    if (c->address_lines != dmqspi_lines_4 || c->data_lines != dmqspi_lines_4) return -ENOTSUP;
    if (c->alternate_lines != dmqspi_lines_none || c->ddr) return -ENOTSUP;
    if (c->dummy_cycles > 63U) return -EINVAL;
    return 0;
}

/** The same for an octal chip: 16-bit commands (the byte twice), a 32-bit address, DDR, the dummy phases counted in bits. */
static void config_cache_phases_octal(const dmqspi_mapped_request_t *r)
{
    uint32_t rd_dummy = octal_dummy_bits(r->read_cmd.dummy_cycles), wr_dummy = octal_dummy_bits(r->write_cmd.dummy_cycles);

    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_SRAM_USR_WCMD);
    reg_set_field(MSPI_SRAM_DWR_CMD_REG(0), 0xFU, SRAM_CMD_BITLEN_S, 16U - 1U);
    reg_set_field(MSPI_SRAM_DWR_CMD_REG(0), 0xFFFFU, 0U, ((uint32_t)r->write_cmd.instruction << 8) | r->write_cmd.instruction);

    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_SRAM_USR_RCMD);
    reg_set_field(MSPI_SRAM_DRD_CMD_REG(0), 0xFU, SRAM_CMD_BITLEN_S, 16U - 1U);
    reg_set_field(MSPI_SRAM_DRD_CMD_REG(0), 0xFFFFU, 0U, ((uint32_t)r->read_cmd.instruction << 8) | r->read_cmd.instruction);

    reg_set_field(MSPI_CACHE_SCTRL_REG(0), 0x3FU, SCTRL_SRAM_ADDR_BITLEN_S, 32U - 1U);
    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_SCMD_4BYTE);

    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_RD_SRAM_DUMMY | SCTRL_USR_WR_SRAM_DUMMY);
    reg_set_field(MSPI_CACHE_SCTRL_REG(0), 0x3FU, SCTRL_SRAM_RDUMMY_CYCLELEN_S, rd_dummy - 1U);
    reg_set_bits(MSPI_SMEM_DDR_REG(0), SMEM_VAR_DUMMY);
    reg_set_field(MSPI_CACHE_SCTRL_REG(0), 0x3FU, SCTRL_SRAM_WDUMMY_CYCLELEN_S, wr_dummy - 1U);

    reg_clear_bits(MSPI_SMEM_DDR_REG(0), SMEM_DDR_WDAT_SWP | SMEM_DDR_RDAT_SWP);
    reg_set_bits(MSPI_SMEM_DDR_REG(0), SMEM_DDR_EN);

    reg_set_bits(MSPI_SRAM_CMD_REG(0), SRAM_CMD_SDUMMY_OUT | SRAM_CMD_SCMD_OCT | SRAM_CMD_SADDR_OCT | SRAM_CMD_SDOUT_OCT | SRAM_CMD_SDIN_OCT);
    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_SRAM_OCT);

    reg_clear_bits(MSPI_MISC_REG(0), MISC_CS1_DIS);     /* the cache uses the chip select 1: CS0 is the flash */
}

/** What the cache needs to know to read and write the chip: the commands, the address, the dummy cycles, the lines. */
static void config_cache_phases(const dmqspi_mapped_request_t *r)
{
    if (s_q.lines == 8U)
    {
        config_cache_phases_octal(r);
        return;
    }

    reg_clear_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_SRAM_DIO);
    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_SRAM_QIO);

    reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_SRAM_USR_RCMD);
    reg_set_field(MSPI_SRAM_DRD_CMD_REG(0), 0xFU, SRAM_CMD_BITLEN_S, 8U - 1U);
    reg_set_field(MSPI_SRAM_DRD_CMD_REG(0), 0xFFFFU, 0U, r->read_cmd.instruction);

    if (r->writable)
    {
        reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_SRAM_USR_WCMD);
        reg_set_field(MSPI_SRAM_DWR_CMD_REG(0), 0xFU, SRAM_CMD_BITLEN_S, 8U - 1U);
        reg_set_field(MSPI_SRAM_DWR_CMD_REG(0), 0xFFFFU, 0U, r->write_cmd.instruction);
    }

    reg_set_field(MSPI_CACHE_SCTRL_REG(0), 0x3FU, SCTRL_SRAM_ADDR_BITLEN_S, address_bits(r->read_cmd.address_size) - 1U);

    if (r->read_cmd.dummy_cycles != 0U)
    {
        reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_RD_SRAM_DUMMY);
        reg_set_field(MSPI_CACHE_SCTRL_REG(0), 0x3FU, SCTRL_SRAM_RDUMMY_CYCLELEN_S, r->read_cmd.dummy_cycles - 1U);
    }
    else
        reg_clear_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_RD_SRAM_DUMMY);

    if (r->writable && r->write_cmd.dummy_cycles != 0U)
    {
        reg_set_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_WR_SRAM_DUMMY);
        reg_set_field(MSPI_CACHE_SCTRL_REG(0), 0x3FU, SCTRL_SRAM_WDUMMY_CYCLELEN_S, r->write_cmd.dummy_cycles - 1U);
    }
    else
        reg_clear_bits(MSPI_CACHE_SCTRL_REG(0), SCTRL_USR_WR_SRAM_DUMMY);

    reg_clear_bits(MSPI_MISC_REG(0), MISC_CS1_DIS);     /* the cache uses the chip select 1: CS0 is the flash */
}

static void __attribute__((noinline)) map_critical(const dmqspi_mapped_request_t *r, uint32_t first_entry, uint32_t entries)
{
    uint32_t autoload = ((uint32_t (*)(void))ROM_CACHE_SUSPEND_DCACHE)();

    ((void (*)(uint32_t, uint32_t, uint32_t))ROM_CONFIG_DATA_CACHE_MODE)(DCACHE_SIZE_BYTES, DCACHE_WAYS, DCACHE_LINE_BYTES);
    config_cache_phases(r);

    for (uint32_t i = 0; i < entries; i++)
        *reg(MMU_TABLE + 4U * (first_entry + i)) = MMU_ACCESS_SPIRAM | i;       /* valid, the page i of the chip */

    ((void (*)(void))ROM_CACHE_INVALIDATE_DCACHE_ALL)();
    ((void (*)(uint32_t))ROM_CACHE_RESUME_DCACHE)(autoload);
}

static void __attribute__((noinline)) unmap_critical(uint32_t first_entry, uint32_t entries)
{
    for (uint32_t i = 0; i < entries; i++)
        *reg(MMU_TABLE + 4U * (first_entry + i)) = MMU_INVALID;
    ((void (*)(void))ROM_CACHE_INVALIDATE_DCACHE_ALL)();
}

static void do_unmap(void)
{
    Dmod_EnterCritical();
    unmap_critical(s_q.first_entry, s_q.entries);
    Dmod_ExitCritical();
    s_q.mode = dmqspi_mode_indirect;
}

/* ---- Port API ---- */

dmod_dmqspi_port_api_declaration(1.0, int, _init, ( const dmqspi_config_t *config ))
{
    if (config == NULL) return -EINVAL;
    if (s_q.ready) return -EBUSY;
    if (config->instance != 1U) return -EINVAL;
    if (config->bank != dmqspi_bank_2)
    {
        DMOD_LOG_ERROR("dmqspi: the chip select 0 of the MSPI is the flash of the chip (the firmware runs from it): use bank=2\n");
        return -EACCES;
    }
    uint32_t lines = config->data_lines != 0U ? config->data_lines : 4U;
    if (lines != 4U && lines != 8U)
        return -ENOTSUP;
    uint32_t max_clock_hz = lines == 8U ? MAX_OCTAL_CLOCK_HZ : MAX_CLOCK_HZ;
    if (config->clock_hz > max_clock_hz)
    {
        DMOD_LOG_ERROR("dmqspi: %u Hz is above the %u Hz this port runs the chip at\n", (unsigned)config->clock_hz, (unsigned)max_clock_hz);
        return -ENOTSUP;
    }
    if (lines == 8U && ((*reg(EFUSE_RD_MAC_SPI_SYS_4_REG) >> EFUSE_PSRAM_CAP_S) & 0x3U) == 0U)
    {
        /* The pads of the octal bus (GPIO33-37) are free pins of the chip without the octal PSRAM inside: they must be left alone */
        DMOD_LOG_ERROR("dmqspi: this package has no octal PSRAM (the eFuses say so): the pads of the eight lines are not touched\n");
        return -ENODEV;
    }

    /* The pads are the pads of the MSPI (what the first stage bootloader sets for the flash, plus the chip select 1 and the
       data lines D2/D3 whose reset state is GPIO), all with the strongest drive. */
    Dmod_EnterCritical();
    for (uint32_t i = 0; i < sizeof(s_pads) / sizeof(s_pads[0]); i++)
    {
        reg_set_field(IO_MUX_REG(s_pads[i].gpio), 7U, IO_MUX_MCU_SEL_S, s_pads[i].func);
        reg_set_field(IO_MUX_REG(s_pads[i].gpio), 3U, IO_MUX_FUN_DRV_S, 3U);
    }
    if (lines == 8U)
    {
        for (uint32_t i = 0; i < sizeof(s_pads_octal) / sizeof(s_pads_octal[0]); i++)
        {
            reg_set_field(IO_MUX_REG(s_pads_octal[i].gpio), 7U, IO_MUX_MCU_SEL_S, s_pads_octal[i].func);
            reg_set_field(IO_MUX_REG(s_pads_octal[i].gpio), 3U, IO_MUX_FUN_DRV_S, 3U);
        }
        /* no ECC on the external RAM (the ACE0 attribute of the SYSCON: bit 8 turns it on; the reset value has it on) */
        reg_clear_bits(SYSCON_SRAM_ACE0_ATTR_REG, SYSCON_SRAM_ECC_ENABLE);
    }
    reg_set_bits(MSPI_DATE_REG(0), DATE_SPICLK_PAD_DRV_CTL_EN);
    reg_set_field(MSPI_DATE_REG(0), 3U, DATE_FMEM_SPICLK_FUN_DRV_S, 3U);
    reg_set_field(MSPI_DATE_REG(0), 3U, DATE_SMEM_SPICLK_FUN_DRV_S, 3U);

    /* the timing of the chip select (shared by SPI0 and SPI1) and the clock of the chip */
    uint32_t cs_time = lines == 8U ? 3U : 0U;
    reg_set_bits(MSPI_SMEM_AC_REG(0), SMEM_CS_HOLD | SMEM_CS_SETUP);
    reg_set_field(MSPI_SMEM_AC_REG(0), 0x1FU, SMEM_CS_HOLD_TIME_S, cs_time);
    reg_set_field(MSPI_SMEM_AC_REG(0), 0x1FU, SMEM_CS_SETUP_TIME_S, cs_time);
    if (lines == 8U)
        reg_set_field(MSPI_SMEM_AC_REG(0), 0x3FU, SMEM_CS_HOLD_DELAY_S, 2U);
    set_clock(clock_divider(config->clock_hz));
    if (lines == 8U)
    {
        /* the commands of SPI1 in the DTR mode */
        reg_set_bits(MSPI_DDR_REG(1), DDR_FMEM_VAR_DUMMY);
        ((void (*)(int, bool, bool))ROM_SPI_SET_DTR_SWAP_MODE)(1, false, false);
    }
    Dmod_ExitCritical();

    s_q.config = *config;
    s_q.lines = lines;
    s_q.mode = dmqspi_mode_indirect;
    s_q.ready = true;
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, int, _deinit, ( dmqspi_instance_t instance ))
{
    if (instance != 1U || !s_q.ready) return 0;
    if (s_q.mode == dmqspi_mode_memory_mapped)
        do_unmap();
    s_q.ready = false;
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, int, _command, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd ))
{
    if (instance != 1U || cmd == NULL || cmd->data_lines != dmqspi_lines_none) return -EINVAL;
    return execute(cmd, NULL, NULL);
}

dmod_dmqspi_port_api_declaration(1.0, int, _read, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, void *data ))
{
    if (instance != 1U || cmd == NULL || data == NULL || cmd->data_lines == dmqspi_lines_none || cmd->data_size == 0U) return -EINVAL;
    return execute(cmd, NULL, data);
}

dmod_dmqspi_port_api_declaration(1.0, int, _write, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const void *data ))
{
    if (instance != 1U || cmd == NULL || data == NULL || cmd->data_lines == dmqspi_lines_none || cmd->data_size == 0U) return -EINVAL;
    return execute(cmd, data, NULL);
}

dmod_dmqspi_port_api_declaration(1.0, int, _auto_poll, ( dmqspi_instance_t instance, const dmqspi_command_t *cmd, const dmqspi_poll_t *poll ))
{
    (void)instance; (void)cmd; (void)poll;
    return -ENOTSUP;        /* the chips on this controller (RAM) have no busy status to wait for */
}

dmod_dmqspi_port_api_declaration(1.0, int, _enter_memory_mapped, ( dmqspi_instance_t instance, const dmqspi_command_t *read_cmd ))
{
    (void)instance; (void)read_cmd;
    return -ENOTSUP;        /* the window of the external RAM is read and written: _enter_memory_mapped_rw */
}

dmod_dmqspi_port_api_declaration(1.0, int, _enter_memory_mapped_rw, ( dmqspi_instance_t instance, const dmqspi_mapped_request_t *request, dmqspi_mapped_info_t *info ))
{
    if (instance != 1U || request == NULL || info == NULL || !s_q.ready) return -EINVAL;
    if (s_q.mode == dmqspi_mode_memory_mapped) return -EBUSY;
    if (!request->writable) return -ENOTSUP;        /* the cache of the ESP32-S3 sends the write command for a write access */
    int ret = check_mapped_command(&request->read_cmd);
    if (ret == 0) ret = check_mapped_command(&request->write_cmd);
    if (ret != 0) return ret;

    uint32_t pages = s_q.config.flash_size_bytes / MMU_PAGE_SIZE;
    if (pages == 0U) return -EINVAL;
    if (pages > MAX_MAPPED_PAGES) pages = MAX_MAPPED_PAGES;
    s_q.first_entry = MMU_ENTRIES - pages;        /* the end of the window */
    s_q.entries = pages;

    Dmod_EnterCritical();
    map_critical(request, s_q.first_entry, s_q.entries);
    Dmod_ExitCritical();

    info->base = MMU_DBUS_BASE + s_q.first_entry * MMU_PAGE_SIZE;
    info->code_base = MMU_IBUS_BASE + s_q.first_entry * MMU_PAGE_SIZE;
    info->size = pages * MMU_PAGE_SIZE;
    info->clock_hz = APB_FREQUENCY_HZ / clock_divider(s_q.config.clock_hz);
    s_q.mode = dmqspi_mode_memory_mapped;
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, int, _exit_memory_mapped, ( dmqspi_instance_t instance ))
{
    if (instance != 1U || !s_q.ready) return -EINVAL;
    if (s_q.mode != dmqspi_mode_memory_mapped) return 0;
    do_unmap();
    return 0;
}

dmod_dmqspi_port_api_declaration(1.0, uintptr_t, _get_mapped_base, ( dmqspi_instance_t instance ))
{
    if (instance != 1U || !s_q.ready || s_q.mode != dmqspi_mode_memory_mapped) return 0;
    return MMU_DBUS_BASE + s_q.first_entry * MMU_PAGE_SIZE;
}

dmod_dmqspi_port_api_declaration(1.0, dmqspi_mode_t, _get_mode, ( dmqspi_instance_t instance ))
{
    (void)instance;
    return s_q.mode;
}

/** Writes the lines of the range that the CPU changed out to the chip, then forgets them: the next read comes from the chip.
 *  The offset is in the window, as the CPU sees it. */
dmod_dmqspi_port_api_declaration(1.0, void, _invalidate_mapped, ( dmqspi_instance_t instance, uint32_t offset, uint32_t size ))
{
    if (instance != 1U || !s_q.ready || s_q.mode != dmqspi_mode_memory_mapped) return;
    uint32_t window = s_q.entries * (uint32_t)MMU_PAGE_SIZE;
    if (offset >= window) return;
    if (size > window - offset) size = window - offset;

    uint32_t address = (uint32_t)(MMU_DBUS_BASE + s_q.first_entry * MMU_PAGE_SIZE) + offset;
    ((int (*)(uint32_t, uint32_t))ROM_CACHE_WRITEBACK_ADDR)(address, size);
    ((int (*)(uint32_t, uint32_t))ROM_CACHE_INVALIDATE_ADDR)(address, size);
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    DMOD_LOG_INFO("dmqspi port module initialized (ESP32-S3 MSPI, chip select 1)\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
