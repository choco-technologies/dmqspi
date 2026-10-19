#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmqspi.h"
#include "dmqspi_port.h"

/*
 * The dmqspi bus driver: module entry, configuration and the dmdrvi
 * interface ("driver_name=dmqspi" in the board .ini). The node is
 * /dev/dmqspi<instance> (or /dev/<section name> for a named section).
 * _read/_write are not supported: the bus has no byte stream, only commands,
 * which go through DMQSPI_IOCTL_* (dmqspi_types.h).
 */

#include <errno.h>
#include <string.h>

#define DMQSPI_CONTEXT_MAGIC    0x44515350      /* "DQSP" */
#define DMQSPI_DEFAULT_SECTION  "dmqspi"

struct dmdrvi_context
{
    uint32_t            magic;
    dmqspi_config_t     config;
};

static bool is_valid_context(dmdrvi_context_t context)
{
    return context != NULL && context->magic == DMQSPI_CONTEXT_MAGIC;
}

static bool section_is_ours(dmini_context_t ini, const char *section)
{
    return dmini_has_key(ini, section, "flash_size") || dmini_has_key(ini, section, "clock_hz");
}

/* Name of the [section] that holds our keys: [dmqspi] or the first named
 * section (not [main]) with flash_size/clock_hz - same scheme as dmspi. */
static const char *detect_config_section(dmini_context_t ini, char *buf, size_t buf_size)
{
    if (section_is_ours(ini, DMQSPI_DEFAULT_SECTION))
        return DMQSPI_DEFAULT_SECTION;
    int needed = dmini_generate_string(ini, NULL, 0);
    char *text = needed > 1 ? (char *)Dmod_Malloc((size_t)needed + 1) : NULL;
    if (text == NULL || dmini_generate_string(ini, text, (size_t)needed + 1) <= 0)
    {
        Dmod_Free(text);
        return DMQSPI_DEFAULT_SECTION;
    }
    text[needed] = '\0';
    const char *result = DMQSPI_DEFAULT_SECTION;
    for (char *p = text; *p != '\0' && result == DMQSPI_DEFAULT_SECTION; p++)
    {
        if (*p != '[')
            continue;
        char *end = p + 1;
        while (*end != '\0' && *end != ']' && *end != '\n')
            end++;
        size_t len = (size_t)(end - p - 1);
        if (*end == ']' && len > 0 && len < buf_size)
        {
            memcpy(buf, p + 1, len);
            buf[len] = '\0';
            if (strcmp(buf, "main") != 0 && section_is_ours(ini, buf))
                result = buf;
        }
    }
    Dmod_Free(text);
    return result;
}

static void read_config(dmqspi_config_t *c, dmini_context_t ini, const char *section)
{
    c->instance          = (dmqspi_instance_t)dmini_get_int(ini, section, "instance", 1);
    c->bank              = dmini_get_int(ini, section, "bank", 1) == 2 ? dmqspi_bank_2 : dmqspi_bank_1;
    c->clock_hz          = (uint32_t)dmini_get_int(ini, section, "clock_hz", 50000000);
    c->flash_size_bytes  = (uint32_t)dmini_get_int(ini, section, "flash_size", 0);
    c->cs_high_cycles    = (uint8_t)dmini_get_int(ini, section, "cs_high_cycles", 2);
    c->clock_mode        = (uint8_t)dmini_get_int(ini, section, "clock_mode", 0);
    c->sample_shift_half = (uint8_t)dmini_get_int(ini, section, "sample_shift_half", 1);
}

dmod_dmqspi_api_declaration(1.0, bool, _validate_config, ( const dmqspi_config_t *config ))
{
    if (config == NULL || config->instance == 0)
        return false;
    if (config->flash_size_bytes == 0 || (config->flash_size_bytes & (config->flash_size_bytes - 1)) != 0)
        return false;       /* FSIZE needs a power of two */
    if (config->clock_hz == 0)
        return false;
    if (config->cs_high_cycles < 1 || config->cs_high_cycles > 8)
        return false;
    if (config->bank != dmqspi_bank_1 && config->bank != dmqspi_bank_2)
        return false;
    return true;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    DMOD_LOG_INFO("dmqspi: QUADSPI bus driver initialized\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}

/* ---- dmdrvi interface ---- */

static void fill_dev_num(dmdrvi_dev_num_t *dev_num, const dmqspi_config_t *c, const char *section)
{
    dev_num->flags = DMDRVI_NUM_MINOR;
    dev_num->major = 0;
    dev_num->minor = (dmdrvi_dev_id_t)c->instance;
    size_t len = strlen(section);
    if (strcmp(section, DMQSPI_DEFAULT_SECTION) != 0 && len <= DMDRVI_ALT_NAME_MAX_LEN)
    {
        dev_num->flags |= DMDRVI_NUM_ALT_NAME;
        memcpy(dev_num->alt_name, section, len + 1);
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if (config == NULL || dev_num == NULL)
        return NULL;
    dmdrvi_context_t context = Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context == NULL)
        return NULL;
    memset(context, 0, sizeof(*context));
    context->magic = DMQSPI_CONTEXT_MAGIC;

    char section_buf[DMDRVI_ALT_NAME_MAX_LEN + 1];
    const char *section = detect_config_section(config, section_buf, sizeof(section_buf));
    read_config(&context->config, config, section);
    if (!dmqspi_validate_config(&context->config))
    {
        DMOD_LOG_ERROR("dmqspi: invalid configuration in [%s] (flash_size must be a power of two)\n", section);
        Dmod_Free(context);
        return NULL;
    }
    int ret = dmqspi_port_init(&context->config);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("dmqspi: controller init failed (%d)\n", ret);
        Dmod_Free(context);
        return NULL;
    }
    fill_dev_num(dev_num, &context->config, section);
    DMOD_LOG_INFO("dmqspi: instance %u, clock <= %u Hz, window %u bytes\n",
                  context->config.instance, (unsigned)context->config.clock_hz,
                  (unsigned)context->config.flash_size_bytes);
    return context;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, void, _free, ( dmdrvi_context_t context ))
{
    if (!is_valid_context(context))
        return;
    dmqspi_port_deinit(context->config.instance);
    context->magic = 0;
    Dmod_Free(context);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t *dev_num ))
{
    (void)flags; (void)dev_num;
    return is_valid_context(context) ? context : NULL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    (void)context; (void)handle;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    (void)context; (void)handle; (void)buffer; (void)size; (void)offset;
    return -ENOTSUP;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    (void)context; (void)handle; (void)buffer; (void)size; (void)offset;
    return -ENOTSUP;
}

static int ioctl_transfer(dmqspi_instance_t inst, const dmqspi_transfer_t *t)
{
    switch (t->dir)
    {
        case dmqspi_dir_none:  return dmqspi_port_command(inst, &t->cmd);
        case dmqspi_dir_read:  return t->data != NULL ? dmqspi_port_read(inst, &t->cmd, t->data) : -EINVAL;
        case dmqspi_dir_write: return t->data != NULL ? dmqspi_port_write(inst, &t->cmd, t->data) : -EINVAL;
        default:               return -EINVAL;
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    (void)handle;
    if (!is_valid_context(context))
        return -EINVAL;
    dmqspi_instance_t inst = context->config.instance;
    bool needs_arg = command != DMQSPI_IOCTL_EXIT_MAPPED;
    if (needs_arg && arg == NULL && command >= DMQSPI_IOCTL_TRANSFER && command <= DMQSPI_IOCTL_INVALIDATE_MAPPED)
        return -EINVAL;
    switch (command)
    {
        case DMQSPI_IOCTL_TRANSFER:
            return ioctl_transfer(inst, (const dmqspi_transfer_t *)arg);
        case DMQSPI_IOCTL_AUTO_POLL:
        {
            const dmqspi_auto_poll_t *p = (const dmqspi_auto_poll_t *)arg;
            return dmqspi_port_auto_poll(inst, &p->cmd, &p->poll);
        }
        case DMQSPI_IOCTL_ENTER_MAPPED:
            return dmqspi_port_enter_memory_mapped(inst, (const dmqspi_command_t *)arg);
        case DMQSPI_IOCTL_EXIT_MAPPED:
            return dmqspi_port_exit_memory_mapped(inst);
        case DMQSPI_IOCTL_GET_MAPPED_BASE:
            *(uintptr_t *)arg = dmqspi_port_get_mapped_base(inst);
            return 0;
        case DMQSPI_IOCTL_GET_MODE:
            *(dmqspi_mode_t *)arg = dmqspi_port_get_mode(inst);
            return 0;
        case DMQSPI_IOCTL_INVALIDATE_MAPPED:
            dmqspi_port_invalidate_mapped(inst, ((const uint32_t *)arg)[0], ((const uint32_t *)arg)[1]);
            return 0;
        default:
            return -ENOTTY;     /* e.g. the class probes dmdevfs sends to every node */
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    (void)handle;
    return is_valid_context(context) ? 0 : -EINVAL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmqspi, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    (void)path;
    if (!is_valid_context(context) || stat == NULL)
        return -EINVAL;
    stat->size = (dmdrvi_size_t)0;
    stat->mode = 0666;
    return 0;
}
