#ifndef DMQSPI_TYPES_H
#define DMQSPI_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmdrvi_ioctl.h"

/*
 * Types shared by the dmqspi core (dmdrvi bus driver) and the
 * dmqspi_port (QUADSPI controller). The bus is chip-agnostic. See docs/architecture.md.
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
    uint32_t            flash_size_bytes;   /**< Addressable window of the attached device (power of 2; FSIZE = log2-1) */
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

/** Direction of the data phase in an ioctl transfer. */
typedef enum
{
    dmqspi_dir_none = 0,    /**< No data phase (cmd.data_lines must be none) */
    dmqspi_dir_read,        /**< Controller reads cmd.data_size bytes into `data` */
    dmqspi_dir_write,       /**< Controller writes cmd.data_size bytes from `data` */
} dmqspi_dir_t;

/** Argument of DMQSPI_IOCTL_TRANSFER: one command + its data buffer. */
typedef struct
{
    dmqspi_command_t    cmd;
    dmqspi_dir_t        dir;
    void               *data;       /**< cmd.data_size bytes; ignored for dir_none */
} dmqspi_transfer_t;

/** Argument of DMQSPI_IOCTL_AUTO_POLL: repeat `cmd` (status read) until the match. */
typedef struct
{
    dmqspi_command_t    cmd;
    dmqspi_poll_t       poll;
} dmqspi_auto_poll_t;

/*
 * Ioctl commands of the bus node /dev/dmqspi<N> (numbered from
 * DMDRVI_IOCTL_CUSTOM_BASE). They are the whole user-visible interface: the
 * bus knows nothing about what sits on it. A flash-chip driver (separate
 * module, e.g. dmnorflash) is a client of these.
 */
#define DMQSPI_IOCTL_TRANSFER           (DMDRVI_IOCTL_CUSTOM_BASE + 0)  /**< arg: dmqspi_transfer_t* */
#define DMQSPI_IOCTL_AUTO_POLL          (DMDRVI_IOCTL_CUSTOM_BASE + 1)  /**< arg: dmqspi_auto_poll_t* */
#define DMQSPI_IOCTL_ENTER_MAPPED       (DMDRVI_IOCTL_CUSTOM_BASE + 2)  /**< arg: const dmqspi_command_t* (read cmd) */
#define DMQSPI_IOCTL_EXIT_MAPPED        (DMDRVI_IOCTL_CUSTOM_BASE + 3)  /**< arg: none */
#define DMQSPI_IOCTL_GET_MAPPED_BASE    (DMDRVI_IOCTL_CUSTOM_BASE + 4)  /**< arg: uintptr_t* (0x90000000 on STM32) */
#define DMQSPI_IOCTL_GET_MODE           (DMDRVI_IOCTL_CUSTOM_BASE + 5)  /**< arg: dmqspi_mode_t* */
#define DMQSPI_IOCTL_INVALIDATE_MAPPED  (DMDRVI_IOCTL_CUSTOM_BASE + 6)  /**< arg: const uint32_t[2] {offset, size} */

#endif // DMQSPI_TYPES_H
