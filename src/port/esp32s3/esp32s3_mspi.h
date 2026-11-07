#ifndef ESP32S3_MSPI_H
#define ESP32S3_MSPI_H

#include <stdint.h>

/*
 * ESP32-S3: the memory SPI controller (MSPI) that the flash and the external RAM of the chip hang on. It is two blocks that
 * share the pads and the clock: SPI1 (the CPU sends commands: the registers the ROM routines below program) and SPI0 (the
 * cache: what it sends for an access to the window of the data/instruction bus). Chip select 0 is the flash (the firmware runs
 * from it), chip select 1 the external RAM; this port serves the second only.
 */

#define BIT(n)                          (1UL << (n))

/* ---- MSPI registers (i = 0: the cache, i = 1: the commands) ---- */
#define MSPI_BASE(i)                    (0x60003000UL - (i) * 0x1000UL)
#define MSPI_USER_REG(i)                (MSPI_BASE(i) + 0x18UL)
#define MSPI_USER1_REG(i)               (MSPI_BASE(i) + 0x1CUL)
#define MSPI_USER2_REG(i)               (MSPI_BASE(i) + 0x20UL)
#define MSPI_CTRL_REG(i)                (MSPI_BASE(i) + 0x08UL)
#define MSPI_MISC_REG(i)                (MSPI_BASE(i) + 0x34UL)
#define MSPI_CACHE_SCTRL_REG(i)         (MSPI_BASE(i) + 0x40UL)
#define MSPI_SRAM_CMD_REG(i)            (MSPI_BASE(i) + 0x44UL)
#define MSPI_SRAM_DRD_CMD_REG(i)        (MSPI_BASE(i) + 0x48UL)
#define MSPI_SRAM_DWR_CMD_REG(i)        (MSPI_BASE(i) + 0x4CUL)
#define MSPI_SRAM_CLK_REG(i)            (MSPI_BASE(i) + 0x50UL)
#define MSPI_SMEM_AC_REG(i)             (MSPI_BASE(i) + 0xDCUL)
#define MSPI_DDR_REG(i)                 (MSPI_BASE(i) + 0xE0UL)
#define MSPI_SMEM_DDR_REG(i)            (MSPI_BASE(i) + 0xE4UL)
#define MSPI_DATE_REG(i)                (MSPI_BASE(i) + 0x3FCUL)

/* MSPI_DDR_REG(1) */
#define DDR_FMEM_VAR_DUMMY              BIT(1)
/* MSPI_SMEM_DDR_REG */
#define SMEM_DDR_EN                     BIT(0)
#define SMEM_VAR_DUMMY                  BIT(1)
#define SMEM_DDR_RDAT_SWP               BIT(2)
#define SMEM_DDR_WDAT_SWP               BIT(3)
/* MSPI_SRAM_CMD_REG: the phases of the cache on eight lines */
#define SRAM_CMD_SDIN_OCT               BIT(18)
#define SRAM_CMD_SDOUT_OCT              BIT(19)
#define SRAM_CMD_SADDR_OCT              BIT(20)
#define SRAM_CMD_SCMD_OCT               BIT(21)
#define SRAM_CMD_SDUMMY_OUT             BIT(22)
/* MSPI_CTRL_REG */
#define CTRL_FCMD_QUAD                  BIT(8)
/* MSPI_MISC_REG: bit set = the chip select is NOT used by the cache */
#define MISC_CS1_DIS                    BIT(1)
/* MSPI_SMEM_AC_REG */
#define SMEM_CS_SETUP                   BIT(0)
#define SMEM_CS_HOLD                    BIT(1)
#define SMEM_CS_SETUP_TIME_S            2U
#define SMEM_CS_HOLD_TIME_S             7U
#define SMEM_CS_HOLD_DELAY_S            25U
/* MSPI_DATE_REG: the drive strength of the clock pads is set by this register, not by the IO MUX */
#define DATE_SMEM_SPICLK_FUN_DRV_S      0U
#define DATE_FMEM_SPICLK_FUN_DRV_S      2U
#define DATE_SPICLK_PAD_DRV_CTL_EN      BIT(4)
/* MSPI_CACHE_SCTRL_REG */
#define SCTRL_USR_SCMD_4BYTE            BIT(0)
#define SCTRL_USR_SRAM_DIO              BIT(1)
#define SCTRL_USR_SRAM_QIO              BIT(2)
#define SCTRL_USR_WR_SRAM_DUMMY         BIT(3)
#define SCTRL_USR_RD_SRAM_DUMMY         BIT(4)
#define SCTRL_SRAM_USR_RCMD             BIT(5)
#define SCTRL_SRAM_RDUMMY_CYCLELEN_S    6U
#define SCTRL_SRAM_ADDR_BITLEN_S        14U
#define SCTRL_SRAM_USR_WCMD             BIT(20)
#define SCTRL_SRAM_OCT                  BIT(21)
#define SCTRL_SRAM_WDUMMY_CYCLELEN_S    22U
/* MSPI_SRAM_DRD_CMD_REG / MSPI_SRAM_DWR_CMD_REG: [31:28] the length of the command - 1 (in bits), [15:0] the command */
#define SRAM_CMD_BITLEN_S               28U
/* MSPI_SRAM_CLK_REG */
#define SRAM_CLK_CNT_L_S                0U
#define SRAM_CLK_CNT_H_S                8U
#define SRAM_CLK_CNT_N_S                16U
#define SRAM_CLK_EQU_SYSCLK             BIT(31)

/* ---- SYSCON: the attributes of the address range 0 of the external RAM (ECC) ---- */
#define SYSCON_SRAM_ACE0_ATTR_REG       0x60026058UL
#define SYSCON_SRAM_ECC_ENABLE          BIT(8)

/* ---- eFuse: what the package is said to have inside (EFUSE_RD_MAC_SPI_SYS_4_REG) ---- */
#define EFUSE_RD_MAC_SPI_SYS_4_REG      0x60007054UL
#define EFUSE_PSRAM_CAP_S               3U      /* 2 bits: 0 none, 1 8 MB, 2 2 MB */

/* ---- The pads of the bus and the IO MUX ---- */
#define MSPI_CS1_GPIO                   26U
#define IO_MUX_REG(n)                   (0x60009004UL + 4UL * (n))
#define IO_MUX_MCU_SEL_S                12U
#define IO_MUX_FUN_DRV_S                10U

/* ---- The memory management unit of the cache: 512 entries of 64 KB pages, shared by both buses ---- */
#define MMU_TABLE                       0x600C5000UL
#define MMU_ENTRIES                     512U
#define MMU_PAGE_SIZE                   0x10000UL
#define MMU_INVALID                     BIT(14)
#define MMU_ACCESS_SPIRAM               BIT(15)
#define MMU_DBUS_BASE                   0x3C000000UL    /* the window of the data bus, 32 MB */
#define MMU_IBUS_BASE                   0x42000000UL    /* the same window seen by the instruction bus */
#define MAX_MAPPED_PAGES                128U            /* 8 MB: the end of the window; flash has the lower entries */

/* ---- ROM: esp32s3.rom.ld of ESP-IDF ---- */
#define ROM_SPI_CMD_CONFIG              0x4000087cUL
#define ROM_SPI_CMD_START               0x40000888UL
#define ROM_SPI_SET_OP_MODE             0x400008a0UL
#define ROM_OPIFLASH_EXEC_CMD           0x400008b8UL
#define ROM_SPI_SET_DTR_SWAP_MODE       0x4000093cUL
#define ROM_CONFIG_DATA_CACHE_MODE      0x40001a28UL    /* rom_config_data_cache_mode(size, ways, line size) */
#define ROM_CACHE_INVALIDATE_ADDR       0x400016b0UL
#define ROM_CACHE_WRITEBACK_ADDR        0x400016c8UL
#define ROM_CACHE_INVALIDATE_DCACHE_ALL 0x400016e0UL
#define ROM_CACHE_SUSPEND_DCACHE        0x400018b4UL
#define ROM_CACHE_RESUME_DCACHE         0x400018c0UL
/* esp_rom_spiflash_read_mode_t */
#define ROM_SPIFLASH_QIO_MODE           0
#define ROM_SPIFLASH_SLOWRD_MODE        5
#define ROM_SPIFLASH_OPI_DTR_MODE       7

/* The data cache as ESP-IDF sets it up for the external RAM (CONFIG_ESP32S3_DATA_CACHE_*): with the 16-byte lines of the ROM
   the data that comes back from the chip is wrong from time to time */
#define DCACHE_SIZE_BYTES               (32U * 1024U)
#define DCACHE_WAYS                     8U
#define DCACHE_LINE_BYTES               32U

#define APB_FREQUENCY_HZ                80000000U

#endif /* ESP32S3_MSPI_H */
