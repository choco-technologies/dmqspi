#include "dmod.h"
#include "dmqspi.h"

/*
 * Table of known flash chips, matched by the JEDEC ID read in
 * dmqspi_flash_open(). Add a chip = add a row (and a note in
 * docs/flash-chips.md). Chips not in the table: try SFDP (TODO, optional).
 */
const dmqspi_chip_t g_dmqspi_chips[] =
{
    /* STM32F746G-DISCO: Micron N25Q128A, 3 V, 128 Mbit */
    {
        .name = "N25Q128A",
        .id = { .manufacturer = 0x20, .memory_type = 0xBA, .capacity = 0x18 },
        .size_bytes = 16u * 1024u * 1024u,
        .page_size = 256,
        .sector_size = 4096,
        .read_dummy_cycles = 10,        /* TODO: verify vs. datasheet for EB at the chosen clock */
        .needs_quad_enable = true,
    },
};

const size_t g_dmqspi_chips_count = sizeof(g_dmqspi_chips) / sizeof(g_dmqspi_chips[0]);
