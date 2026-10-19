#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmqspi.h"

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
