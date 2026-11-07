#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmqspi.h"
#include <errno.h>
#include <string.h>

/* Host-side tests: only logic that needs no hardware (config validation,
 * and later the chip table / command building). Hardware is tested on the
 * board with the commands in docs/tasks.md. */

static dmqspi_config_t valid_config(void)
{
    dmqspi_config_t c = {
        .instance = 1, .bank = dmqspi_bank_1, .clock_hz = 50000000,
        .flash_size_bytes = 16u * 1024u * 1024u, .cs_high_cycles = 2,
    };
    return c;
}

DMOD_TEST_STEP(validate_config_ok)
{
    dmqspi_config_t c = valid_config();
    DMOD_TEST_EXPECT_TRUE(dmqspi_validate_config(&c));
}

DMOD_TEST_STEP(validate_config_null)
{
    DMOD_TEST_EXPECT_FALSE(dmqspi_validate_config(NULL));
}

DMOD_TEST_STEP(validate_config_size_not_power_of_two)
{
    dmqspi_config_t c = valid_config();
    c.flash_size_bytes = 3000000;
    DMOD_TEST_EXPECT_FALSE(dmqspi_validate_config(&c));
}

DMOD_TEST_STEP(validate_config_zero_clock)
{
    dmqspi_config_t c = valid_config();
    c.clock_hz = 0;
    DMOD_TEST_EXPECT_FALSE(dmqspi_validate_config(&c));
}

DMOD_TEST_STEP(validate_config_data_lines)
{
    static const uint8_t good[] = { 0, 1, 2, 4, 8 };
    for (unsigned i = 0; i < sizeof(good); i++)
    {
        dmqspi_config_t c = valid_config();
        c.data_lines = good[i];
        DMOD_TEST_EXPECT_TRUE(dmqspi_validate_config(&c));
    }
    dmqspi_config_t c = valid_config();
    c.data_lines = 3;
    DMOD_TEST_EXPECT_FALSE(dmqspi_validate_config(&c));
    c.data_lines = 16;
    DMOD_TEST_EXPECT_FALSE(dmqspi_validate_config(&c));
}

DMOD_TEST_STEP(ioctl_function_checks_its_arguments)
{
    /* the function a client calls instead of opening the node: the same answers as the node gives */
    DMOD_TEST_EXPECT_EQ(dmqspi_ioctl(1, DMQSPI_IOCTL_TRANSFER, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmqspi_ioctl(1, DMQSPI_IOCTL_ENTER_MAPPED_RW, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmqspi_ioctl(1, 0x7FFF, NULL), -ENOTTY);
}

DMOD_TEST_STEP(a_controller_that_cannot_map_a_writable_window_says_so)
{
    /* the host stub is a controller without the memory-mapped RAM window (as the STM32 QUADSPI is) */
    dmqspi_mapped_rw_t m;
    memset(&m, 0, sizeof(m));
    m.request.writable = true;
    DMOD_TEST_EXPECT_EQ(dmqspi_ioctl(1, DMQSPI_IOCTL_ENTER_MAPPED_RW, &m), -ENOTSUP);
}
