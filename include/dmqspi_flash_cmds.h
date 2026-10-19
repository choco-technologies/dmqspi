#ifndef DMQSPI_FLASH_CMDS_H
#define DMQSPI_FLASH_CMDS_H

/*
 * Opcodes of the JEDEC-style NOR flash command set, as used by the Micron
 * N25Q128A on the STM32F746G-DISCO. Other vendors mostly share these; a
 * chip that differs gets an override in its dmqspi_chip_t (src/dmqspi_chips.c).
 * Verify against the datasheet of the actual chip before relying on a value.
 */
#define DMQSPI_CMD_READ_ID              0x9F    /**< 1-line, 3 bytes: manufacturer, type, capacity */
#define DMQSPI_CMD_READ_SFDP            0x5A
#define DMQSPI_CMD_WRITE_ENABLE         0x06    /**< Required before every program/erase/write-status */
#define DMQSPI_CMD_WRITE_DISABLE        0x04
#define DMQSPI_CMD_READ_STATUS          0x05    /**< Bit 0 = WIP (write in progress), bit 1 = WEL */
#define DMQSPI_CMD_WRITE_STATUS         0x01
#define DMQSPI_CMD_READ_FLAG_STATUS     0x70    /**< Micron: bit 7 = ready, bits 5/4 = erase/program error */
#define DMQSPI_CMD_CLEAR_FLAG_STATUS    0x50
#define DMQSPI_CMD_READ_ENH_VOLATILE    0x65    /**< Micron: quad enable lives here */
#define DMQSPI_CMD_WRITE_ENH_VOLATILE   0x61
#define DMQSPI_CMD_READ_VOLATILE_CFG    0x85    /**< Micron: dummy cycles for fast read */
#define DMQSPI_CMD_WRITE_VOLATILE_CFG   0x81
#define DMQSPI_CMD_FAST_READ_QUAD_IO    0xEB    /**< Address on 4 lines, dummy cycles from volatile cfg */
#define DMQSPI_CMD_FAST_READ_QUAD_OUT   0x6B
#define DMQSPI_CMD_FAST_READ            0x0B    /**< 1-line fallback */
#define DMQSPI_CMD_PAGE_PROGRAM         0x02
#define DMQSPI_CMD_QUAD_PAGE_PROGRAM    0x32
#define DMQSPI_CMD_SUBSECTOR_ERASE_4K   0x20
#define DMQSPI_CMD_SECTOR_ERASE_64K     0xD8
#define DMQSPI_CMD_CHIP_ERASE           0xC7
#define DMQSPI_CMD_RESET_ENABLE         0x66
#define DMQSPI_CMD_RESET_MEMORY         0x99

#define DMQSPI_STATUS_WIP               (1u << 0)
#define DMQSPI_STATUS_WEL               (1u << 1)

#endif // DMQSPI_FLASH_CMDS_H
