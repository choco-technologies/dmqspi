#include "dmod.h"
#include "dmqspi_types.h"
#include <string.h>

/*
 * qspitest - exercises a dmqspi bus node with a Micron N25Q128A on it:
 *   qspitest <node>            READ ID, status, read 4 KiB (DMA path), quad read; read-only
 *   qspitest <node> --write    additionally: erase + program + verify the LAST 4 KiB sector
 *                              (offset 0xFFF000) - everything stored there is lost.
 * <node> is the dmdevfs path of the bus, e.g. /dev/qspi or /dev/dmqspi1.
 * Exit code = number of failed steps.
 */

#define SECTOR_SIZE     4096u
#define PAGE_SIZE       256u
#define TEST_OFFSET     0x00FFF000u
#define OP_READ_ID      0x9F
#define OP_WREN         0x06
#define OP_STATUS       0x05
#define OP_FAST_READ    0x0B
#define OP_QUAD_IO_READ 0xEB
#define OP_PROGRAM      0x02
#define OP_ERASE_4K     0x20

static int g_failed;

static void check(bool ok, const char *what)
{
    Dmod_Printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        g_failed++;
}

static dmqspi_command_t make_cmd(uint8_t op, dmqspi_lines_t addr_lines, uint32_t addr,
                                 uint8_t dummy, dmqspi_lines_t data_lines, uint32_t size)
{
    dmqspi_command_t c;
    memset(&c, 0, sizeof(c));
    c.instruction = op;
    c.instruction_lines = dmqspi_lines_1;
    c.address_lines = addr_lines;
    c.address_size = dmqspi_addr_24bit;
    c.address = addr;
    c.dummy_cycles = dummy;
    c.data_lines = data_lines;
    c.data_size = size;
    return c;
}

static int transfer(void *fp, const dmqspi_command_t *cmd, dmqspi_dir_t dir, void *data)
{
    dmqspi_transfer_t t;
    memset(&t, 0, sizeof(t));
    t.cmd = *cmd;
    t.dir = dir;
    t.data = data;
    return Dmod_Ioctl(fp, DMQSPI_IOCTL_TRANSFER, &t);
}

static int wait_ready(void *fp, uint32_t timeout_ms)
{
    dmqspi_auto_poll_t p;
    memset(&p, 0, sizeof(p));
    p.cmd = make_cmd(OP_STATUS, dmqspi_lines_none, 0, 0, dmqspi_lines_1, 1);
    p.poll.mask = 1;        /* WIP */
    p.poll.match = 0;
    p.poll.timeout_ms = timeout_ms;
    return Dmod_Ioctl(fp, DMQSPI_IOCTL_AUTO_POLL, &p);
}

static int read_flash(void *fp, uint32_t offset, void *buf, uint32_t size, bool quad)
{
    dmqspi_command_t c = quad
        ? make_cmd(OP_QUAD_IO_READ, dmqspi_lines_4, offset, 10, dmqspi_lines_4, size)
        : make_cmd(OP_FAST_READ, dmqspi_lines_1, offset, 8, dmqspi_lines_1, size);
    int ret = transfer(fp, &c, dmqspi_dir_read, buf);
    if (ret != 0)
        Dmod_Printf("  read of %u bytes at 0x%x (%s) failed: %d\n", (unsigned)size, (unsigned)offset, quad ? "quad" : "1-line", ret);
    return ret;
}

static int write_enable(void *fp)
{
    dmqspi_command_t c = make_cmd(OP_WREN, dmqspi_lines_none, 0, 0, dmqspi_lines_none, 0);
    return transfer(fp, &c, dmqspi_dir_none, NULL);
}

static uint8_t read_status(void *fp)
{
    uint8_t st = 0xEE;
    dmqspi_command_t c = make_cmd(OP_STATUS, dmqspi_lines_none, 0, 0, dmqspi_lines_1, 1);
    (void)transfer(fp, &c, dmqspi_dir_read, &st);
    return st;
}

static void test_id_and_status(void *fp)
{
    uint8_t id[3] = { 0 };
    dmqspi_command_t c = make_cmd(OP_READ_ID, dmqspi_lines_none, 0, 0, dmqspi_lines_1, 3);
    int ret = transfer(fp, &c, dmqspi_dir_read, id);
    Dmod_Printf("READ ID: ret=%d  %02x %02x %02x (expect 20 ba 18)\n", ret, id[0], id[1], id[2]);
    check(ret == 0 && id[0] == 0x20 && id[1] == 0xBA && id[2] == 0x18, "JEDEC id is Micron N25Q128A");

    uint8_t st = 0xFF;
    c = make_cmd(OP_STATUS, dmqspi_lines_none, 0, 0, dmqspi_lines_1, 1);
    ret = transfer(fp, &c, dmqspi_dir_read, &st);
    Dmod_Printf("status register = 0x%02x\n", st);
    check(ret == 0 && (st & 1) == 0, "status read, chip not busy");
    check(wait_ready(fp, 100) == 0, "auto-poll WIP==0 returns at once");
}

static void test_sizes(void *fp, uint8_t *buf)
{
    /* below 64 bytes the FIFO loop is used, from 64 on (aligned buffer) the DMA */
    static const uint32_t sizes[] = { 1, 16, 63, 64, 96, 256, 1024, 4096 };
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
        check(read_flash(fp, 0, buf, sizes[i], false) == 0, sizes[i] < 64 ? "FIFO read" : "DMA read");
}

static void test_reads(void *fp, uint8_t *buf, uint8_t *buf2)
{
    int ret = read_flash(fp, 0, buf, SECTOR_SIZE, false);
    Dmod_Printf("first bytes @0: %02x %02x %02x %02x %02x %02x %02x %02x\n",
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
    check(ret == 0, "1-line fast read of 4 KiB (aligned buffer: DMA path)");
    ret = read_flash(fp, 0, buf2, SECTOR_SIZE, true);
    check(ret == 0 && memcmp(buf, buf2, SECTOR_SIZE) == 0, "quad I/O read (0xEB) equals the 1-line read");
    bool same = true;       /* 4 KiB again in 16-byte pieces: the FIFO path, no DMA */
    for (uint32_t off = 0; off < SECTOR_SIZE && same; off += 16)
        same = read_flash(fp, off, buf2 + off, 16, false) == 0;
    check(same && memcmp(buf, buf2, SECTOR_SIZE) == 0, "4 KiB by DMA equals 4 KiB in 16-byte FIFO reads");
}

static int program_sector(void *fp, const uint8_t *pattern)
{
    for (uint32_t page = 0; page < SECTOR_SIZE / PAGE_SIZE; page++)
    {
        uint32_t off = TEST_OFFSET + page * PAGE_SIZE;
        dmqspi_command_t c = make_cmd(OP_PROGRAM, dmqspi_lines_1, off, 0, dmqspi_lines_1, PAGE_SIZE);
        int ret = write_enable(fp);
        if (ret == 0)
            ret = transfer(fp, &c, dmqspi_dir_write, (void *)(pattern + page * PAGE_SIZE));
        if (ret != 0)
        {
            Dmod_Printf("  page %u: WREN/program ret=%d status=0x%02x\n", (unsigned)page, ret, read_status(fp));
            return -1;
        }
        ret = wait_ready(fp, 100);
        if (ret != 0)
        {
            Dmod_Printf("  page %u: poll ret=%d status=0x%02x\n", (unsigned)page, ret, read_status(fp));
            return -2;
        }
    }
    return 0;
}

static void report_diff(const uint8_t *want, const uint8_t *got, uint32_t size)
{
    uint32_t bad = 0, first = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        if (want[i] != got[i] && bad++ == 0)
            first = i;
    }
    if (bad != 0)
        Dmod_Printf("  %u of %u bytes differ, first at %u: want %02x got %02x\n", (unsigned)bad, (unsigned)size,
                    (unsigned)first, want[first], got[first]);
}

static void test_write(void *fp, uint8_t *buf, uint8_t *buf2)
{
    Dmod_Printf("--write: erasing the sector at 0x%06x\n", (unsigned)TEST_OFFSET);
    dmqspi_command_t c = make_cmd(OP_ERASE_4K, dmqspi_lines_1, TEST_OFFSET, 0, dmqspi_lines_none, 0);
    int ret = write_enable(fp);
    Dmod_Printf("  WREN ret=%d, status after WREN = 0x%02x (bit1 WEL expected)\n", ret, read_status(fp));
    if (ret == 0)
        ret = transfer(fp, &c, dmqspi_dir_none, NULL);
    Dmod_Printf("  erase cmd ret=%d, status right after = 0x%02x (bit0 WIP expected)\n", ret, read_status(fp));
    if (ret == 0)
        ret = wait_ready(fp, 2000);
    Dmod_Printf("  poll ret=%d, status after = 0x%02x\n", ret, read_status(fp));
    check(ret == 0, "sector erase + auto-poll");
    check(read_flash(fp, TEST_OFFSET, buf, SECTOR_SIZE, false) == 0, "read the erased sector");
    bool blank = true;
    for (uint32_t i = 0; i < SECTOR_SIZE; i++)
        blank = blank && buf[i] == 0xFF;
    check(blank, "erased sector is all 0xFF");

    for (uint32_t i = 0; i < SECTOR_SIZE; i++)
        buf[i] = (uint8_t)(i * 7u + (i >> 8) + 1u);
    check(program_sector(fp, buf) == 0, "16 page programs (each page: WREN + program + poll)");
    memset(buf2, 0, SECTOR_SIZE);
    int rret = read_flash(fp, TEST_OFFSET, buf2, SECTOR_SIZE, false);
    report_diff(buf, buf2, SECTOR_SIZE);
    Dmod_Printf("  status after the read = 0x%02x\n", read_status(fp));
    check(rret == 0 && memcmp(buf, buf2, SECTOR_SIZE) == 0, "programmed sector reads back (DMA read)");
    memset(buf2, 0, SECTOR_SIZE);
    check(read_flash(fp, TEST_OFFSET, buf2, SECTOR_SIZE, true) == 0 && memcmp(buf, buf2, SECTOR_SIZE) == 0,
          "programmed sector reads back by quad I/O");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        Dmod_Printf("usage: qspitest <node> [--write]\n");
        return 1;
    }
    void *fp = Dmod_FileOpen(argv[1], "r+");
    if (fp == NULL)
    {
        Dmod_Printf("qspitest: cannot open '%s'\n", argv[1]);
        return 1;
    }
    /* 32-byte aligned buffers so the DMA path is used (see stm32_qspi_dma.c) */
    uint8_t *raw = Dmod_Malloc(2 * SECTOR_SIZE + 64);
    if (raw == NULL)
    {
        Dmod_FileClose(fp);
        return 1;
    }
    uint8_t *buf = (uint8_t *)(((uintptr_t)raw + 31u) & ~(uintptr_t)31u);
    uint8_t *buf2 = buf + SECTOR_SIZE;

    test_id_and_status(fp);
    test_sizes(fp, buf);
    test_reads(fp, buf, buf2);
    if (argc >= 3 && strcmp(argv[2], "--write") == 0)
        test_write(fp, buf, buf2);
    Dmod_Printf("qspitest: %d failed step(s)\n", g_failed);
    Dmod_Free(raw);
    Dmod_FileClose(fp);
    return g_failed;
}
