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
    dmqspi_lines_8,         /**< Octal (8 data lines, usually with `ddr`); not in dmqspi_command_t of the released ABI on every controller */
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
    uint8_t             dummy_cycles;       /**< Clock cycles of the bus between the address and the data (a DDR bus counts cycles, not edges) */
    dmqspi_lines_t      data_lines;         /**< none = no data phase */
    uint32_t            data_size;          /**< Bytes of the data phase */
    bool                ddr;                /**< Double data rate (unused by N25Q) */
} dmqspi_command_t;

/*
 * The structures above and the DMQSPI_IOCTL_* commands that take them (TRANSFER, AUTO_POLL, ENTER_MAPPED) are the released ABI of the
 * node: dmspiflash and the tools are built against them, and their layout depends on the compiler (short enums on ARM). They do not
 * change. What a newer controller needs - a 16-bit instruction - is in the "2" structures below, which have their own ioctl numbers; the
 * driver converts the old ones to the new ones, so the controllers (ports) know only the new ones.
 */

/** Size of the instruction phase (values of dmqspi_command2_t.instruction_size). */
typedef enum
{
    dmqspi_instr_8bit = 0,  /**< One byte (the default; every controller): dmqspi_command_t.instruction */
    dmqspi_instr_16bit,     /**< Two bytes (octal chips in the DDR mode), both in dmqspi_command2_t.instruction */
} dmqspi_instr_size_t;

/** A transaction: the phases of dmqspi_command_t, with an instruction of 8 or 16 bits. */
typedef struct
{
    dmqspi_command_t    cmd;                /**< The phases; cmd.instruction is the instruction of an 8-bit instruction phase */
    uint16_t            instruction;        /**< The instruction of a 16-bit instruction phase (both bytes: how the chip wants them) */
    uint8_t             instruction_size;   /**< dmqspi_instr_8bit (0, the default) or dmqspi_instr_16bit */
} dmqspi_command2_t;

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
    uint8_t             data_lines;         /**< Data lines the attached chip is wired with: 1, 2, 4 or 8 (0 = what the controller has). Ports whose pads are not
                                                 free to choose (ESP32-S3: the pads of the flash) set up that many */
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

/** Argument of DMQSPI_IOCTL_TRANSFER2: one command with its data buffer. */
typedef struct
{
    dmqspi_command2_t   cmd;
    dmqspi_dir_t        dir;
    void               *data;       /**< cmd.cmd.data_size bytes; ignored for dir_none */
} dmqspi_transfer2_t;

/** Argument of DMQSPI_IOCTL_AUTO_POLL2: repeat `cmd` (status read) until the match. */
typedef struct
{
    dmqspi_command2_t   cmd;
    dmqspi_poll_t       poll;
} dmqspi_auto_poll2_t;

/**
 * What a memory-mapped window is made of. The controller sends `read_cmd` when the CPU reads the window and, when `writable`, `write_cmd`
 * when it writes (a RAM chip). Flash windows are read only (`writable` false). The phases of both are those of a command
 * (instruction, address, dummy cycles, data lines); `data_size` is ignored.
 */
typedef struct
{
    dmqspi_command2_t   read_cmd;
    dmqspi_command2_t   write_cmd;      /**< Used only when writable */
    bool                writable;       /**< false = read only window */
} dmqspi_map_request_t;

/** Where the window is. */
typedef struct
{
    uintptr_t           base;           /**< Address of the window as the CPU sees it */
    uint32_t            size;           /**< Bytes of the window (flash_size of the configuration, or less when the controller cannot map that much) */
    uint32_t            clock_hz;       /**< Clock the controller runs the chip at (not above the clock_hz of the configuration; 0 = the port does not report it) */
} dmqspi_map_info_t;

/** Argument of DMQSPI_IOCTL_MAP: the request goes in, the window comes out. */
typedef struct
{
    dmqspi_map_request_t request;
    dmqspi_map_info_t    info;
} dmqspi_map_t;

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
#define DMQSPI_IOCTL_INVALIDATE_MAPPED  (DMDRVI_IOCTL_CUSTOM_BASE + 6)  /**< arg: const uint32_t[2] {offset, size}: drop the range from the cache of the CPU, the next read comes from the chip */
/* The commands above are the released ones (dmqspi_command_t: an 8-bit instruction). From here on: dmqspi_command2_t. */
#define DMQSPI_IOCTL_TRANSFER2          (DMDRVI_IOCTL_CUSTOM_BASE + 7)  /**< arg: dmqspi_transfer2_t* */
#define DMQSPI_IOCTL_AUTO_POLL2         (DMDRVI_IOCTL_CUSTOM_BASE + 8)  /**< arg: dmqspi_auto_poll2_t* */
#define DMQSPI_IOCTL_MAP                (DMDRVI_IOCTL_CUSTOM_BASE + 9)  /**< arg: dmqspi_map_t* (the window, read only or read/write: ENTER_MAPPED with the write command and the answer) */
#define DMQSPI_IOCTL_FLUSH_MAPPED       (DMDRVI_IOCTL_CUSTOM_BASE + 10) /**< arg: const uint32_t[2] {offset, size}: write what the CPU changed in the range back to the chip */

#endif // DMQSPI_TYPES_H
