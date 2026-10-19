#ifndef DMQSPI_TYPES_H
#define DMQSPI_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmdrvi_ioctl.h"

/*
 * Types shared by the dmqspi core (flash-chip layer + dmdrvi driver) and the
 * dmqspi_port (QUADSPI controller). See docs/architecture.md.
 */

/** QUADSPI controller instance (1-based; every supported STM32 has only 1). */
typedef uint8_t dmqspi_instance_t;

/** Flash bank selected by the controller (STM32 QUADSPI: BK1 or BK2). */
typedef enum
{
    dmqspi_bank_1 = 0,      /**< Chip on BK1 pins (default, STM32F746G-DISCO) */
    dmqspi_bank_2,          /**< Chip on BK2 pins */
} dmqspi_bank_t;

/** Number of data lines used by one phase of a command. */
typedef enum
{
    dmqspi_lines_none = 0,  /**< Phase is skipped */
    dmqspi_lines_1,         /**< Single (SPI) */
    dmqspi_lines_2,         /**< Dual */
    dmqspi_lines_4,         /**< Quad */
} dmqspi_lines_t;

/** Address size of the address phase. */
typedef enum
{
    dmqspi_addr_8bit = 0,
    dmqspi_addr_16bit,
    dmqspi_addr_24bit,      /**< Up to 16 MB (N25Q128A) */
    dmqspi_addr_32bit,      /**< Bigger chips */
} dmqspi_addr_size_t;

/**
 * One QSPI transaction = instruction, address, alternate bytes, dummy
 * cycles, data - each phase with its own line count. This maps 1:1 onto the
 * STM32 QUADSPI CCR register (see docs/port-implementation.md).
 */
typedef struct
{
    uint8_t             instruction;        /**< Opcode, e.g. 0x9F (READ ID) */
    dmqspi_lines_t      instruction_lines;  /**< none = no instruction phase */
    uint32_t            address;            /**< Valid when address_lines != none */
    dmqspi_lines_t      address_lines;
    dmqspi_addr_size_t  address_size;
    uint32_t            alternate;          /**< Alternate bytes (mode bits), e.g. 0xA0 */
    dmqspi_lines_t      alternate_lines;
    uint8_t             alternate_size;     /**< In bytes 1..4 */
    uint8_t             dummy_cycles;       /**< 0..31 */
    dmqspi_lines_t      data_lines;         /**< none = no data phase */
    uint32_t            data_size;          /**< Bytes of the data phase */
    bool                ddr;                /**< Double data rate (unused by N25Q) */
} dmqspi_command_t;

/**
 * Controller configuration - filled by the core from the .ini file and
 * passed to dmqspi_port_init().
 */
typedef struct
{
    dmqspi_instance_t   instance;           /**< 1 */
    dmqspi_bank_t       bank;
    uint32_t            clock_hz;           /**< Requested CLK (port picks prescaler <= this) */
    uint32_t            flash_size_bytes;   /**< Power of 2; port programs FSIZE = log2(size)-1 */
    uint8_t             cs_high_cycles;     /**< Min CS high time between commands (1..8) */
    uint8_t             clock_mode;         /**< 0 = CLK low when idle (mode 0), 3 = high */
    uint8_t             sample_shift_half;  /**< 1 = sample half a cycle later (SSHIFT) */
} dmqspi_config_t;

/** Controller functional mode. */
typedef enum
{
    dmqspi_mode_indirect = 0,   /**< Commands issued by software (read/write/erase) */
    dmqspi_mode_memory_mapped,  /**< Flash visible at the QSPI base (0x90000000), read only */
} dmqspi_mode_t;

/** What the status-poll (auto-polling) waits for: (status & mask) == match. */
typedef struct
{
    uint32_t mask;
    uint32_t match;
    uint32_t timeout_ms;
} dmqspi_poll_t;

/** Identification returned by READ ID (0x9F). */
typedef struct
{
    uint8_t manufacturer;   /**< 0x20 = Micron */
    uint8_t memory_type;    /**< 0xBA = 3V N25Q */
    uint8_t capacity;       /**< 0x18 = 128 Mbit */
} dmqspi_jedec_id_t;

/**
 * Geometry + command set of one flash chip. The table of known chips lives
 * in src/dmqspi_chips.c (TODO); unknown JEDEC IDs fall back to SFDP.
 */
typedef struct
{
    char                name[16];           /**< "N25Q128A" (array, not a pointer: a module cannot hold relocated const data) */
    dmqspi_jedec_id_t   id;
    uint32_t            size_bytes;         /**< 16 MiB */
    uint32_t            page_size;          /**< 256 */
    uint32_t            sector_size;        /**< 4096 - the erase unit exposed as erase_block_size */
    uint8_t             read_dummy_cycles;  /**< For the quad fast-read command */
    bool                needs_quad_enable;  /**< Must set quad bit in a config register */
} dmqspi_chip_t;

/** Driver-specific ioctl commands (numbered from DMDRVI_IOCTL_CUSTOM_BASE). */
#define DMQSPI_IOCTL_GET_JEDEC_ID       (DMDRVI_IOCTL_CUSTOM_BASE + 0)  /**< arg: dmqspi_jedec_id_t* */
#define DMQSPI_IOCTL_SET_MEMORY_MAPPED  (DMDRVI_IOCTL_CUSTOM_BASE + 1)  /**< arg: const bool* (true = enter XIP) */
#define DMQSPI_IOCTL_GET_MAPPED_BASE    (DMDRVI_IOCTL_CUSTOM_BASE + 2)  /**< arg: uintptr_t* (0x90000000 on STM32) */
#define DMQSPI_IOCTL_ERASE_CHIP         (DMDRVI_IOCTL_CUSTOM_BASE + 3)  /**< arg: none */

#endif // DMQSPI_TYPES_H
