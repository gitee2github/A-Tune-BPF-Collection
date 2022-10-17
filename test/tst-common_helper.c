#include <string.h>
#include "common_helper.h"

#define CONFIG_FILE "tst-conf-file"

struct expected_option {
    const char *name;
    const char *expected_val;
};

struct expected_option test_options[] = {
    {"space_integer", "23"},
    {"space_float", "3.1415926"},
    {"no_space_str", "hello world"},
    {"space_str", "hello world"},
    {"empty_field", ""}
};

static int do_test(void)
{
    int ret = 0;
    struct opt **opts = parse_init(SHASH);
    if (!opts) {
        log(TERM, LOG_ERR, "parse_init failed\n");
        return 1;
    }

    if (parse_config_file(TERM, CONFIG_FILE, opts, SHASH)) {
        log(TERM, LOG_ERR, "parse_config_file failed\n");
        return 1;
    }

    for (int i = 0; i < sizeof(test_options) / sizeof(struct expected_option); i++) {
        const char *res = config_opt(opts, SHASH, test_options[i].name);
        if (strcmp(test_options[i].expected_val, res)) {
            ret = 1;
            goto out;
        }
    }

out:
    parse_fini(opts, SHASH);
    return ret;
}

#include "test/test-driver.c"
