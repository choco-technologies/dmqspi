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

DMOD_TEST_STEP(the_functions_check_their_arguments)
{
    /* what a driver configured before the file system of the devices calls instead of opening the node */
    DMOD_TEST_EXPECT_EQ(dmqspi_transfer(1, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmqspi_auto_poll(1, NULL), -EINVAL);
    DMOD_TEST_EXPECT_EQ(dmqspi_map(1, NULL, NULL), -EINVAL);
}

DMOD_TEST_STEP(a_transfer_needs_a_buffer_for_its_data_phase)
{
    dmqspi_transfer_t t;
    memset(&t, 0, sizeof(t));
    t.dir = dmqspi_dir_read;
    DMOD_TEST_EXPECT_EQ(dmqspi_transfer(1, &t), -EINVAL);
    t.dir = (dmqspi_dir_t)9;
    DMOD_TEST_EXPECT_EQ(dmqspi_transfer(1, &t), -EINVAL);
}

DMOD_TEST_STEP(a_controller_that_cannot_map_says_so)
{
    /* the host stub maps nothing, as a controller without the window would */
    dmqspi_map_request_t r;
    dmqspi_map_info_t info;
    memset(&r, 0, sizeof(r));
    DMOD_TEST_EXPECT_EQ(dmqspi_map(1, &r, &info), -ENOTSUP);
    r.writable = true;
    DMOD_TEST_EXPECT_EQ(dmqspi_map(1, &r, &info), -ENOTSUP);
    DMOD_TEST_EXPECT_EQ((int)dmqspi_get_mode(1), (int)dmqspi_mode_indirect);
}
