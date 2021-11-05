// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021. Huawei Technologies Co., Ltd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <argp.h>
#include <bpf/libbpf.h>
#include "readahead_tune.h"
#include "common_helper.h"
#include "readahead_tune.skel.h"

struct env_conf {
    const char *name;
    unsigned long long default_val;
};

const struct env_conf confs[CONF_NUM] = {
    {"filesz-threshold", DEFAULT_FILESZ},
    {"read-time-threshold", DEFAULT_READ_TIME},
    {"total-read-threshold", DEFAULT_TOTAL_READ},
    {"lower-bound-percentage", DEFAULT_LOWER_BOUND},
    {"upper-bound-percentage", DEFAULT_UPPER_BOUND}
};

static void parse_config_env(struct readahead_tune_bpf *skel)
{
    for (int i = 0; i < CONF_NUM; i++) {
        char *env_str = getenv(confs[i].name);
        if (env_str && strlen(env_str)) {
            char *endptr;
            errno = 0;
            long long val = strtoll(env_str, &endptr, 10);

            if ((errno == ERANGE && (val == LLONG_MAX || val == LLONG_MIN))
                    || (errno != 0 && val == 0)
                    || endptr == env_str
                    || *endptr != '\0'
                    || val < 0) {
                log(LOG_ERR, "Invalid env %s, use default!\n", env_str);
                skel->rodata->file_read_conf[i] = confs[i].default_val;
            } else {
                skel->rodata->file_read_conf[i] = (unsigned long long)val;
            }
        } else {
            skel->rodata->file_read_conf[i] = confs[i].default_val;
        }
    }

    if (skel->rodata->file_read_conf[CONF_LOWER_BOUND] >= skel->rodata->file_read_conf[CONF_UPPER_BOUND]
            || skel->rodata->file_read_conf[CONF_LOWER_BOUND] <= ZERO_PERCENTAGE
            || skel->rodata->file_read_conf[CONF_UPPER_BOUND] >= HUNDRED_PERCENTAGE) {
        log(LOG_ERR, "lower-bound-percentage(%llu), upper-bound-percentage(%llu) is invalid, use default\n",
                skel->rodata->file_read_conf[CONF_LOWER_BOUND], skel->rodata->file_read_conf[CONF_UPPER_BOUND]);
        skel->rodata->file_read_conf[CONF_LOWER_BOUND] = DEFAULT_LOWER_BOUND;
        skel->rodata->file_read_conf[CONF_UPPER_BOUND] = DEFAULT_UPPER_BOUND;
    }

    if (skel->rodata->file_read_conf[CONF_TOTAL_READ] * skel->rodata->file_read_conf[CONF_LOWER_BOUND] < skel->rodata->file_read_conf[CONF_TOTAL_READ]
            || skel->rodata->file_read_conf[CONF_TOTAL_READ] * skel->rodata->file_read_conf[CONF_UPPER_BOUND] < skel->rodata->file_read_conf[CONF_TOTAL_READ]) {
        log(LOG_ERR, "total-read-threshold(%llu) is too large, use default\n", skel->rodata->file_read_conf[CONF_TOTAL_READ]);
        skel->rodata->file_read_conf[CONF_TOTAL_READ] = DEFAULT_TOTAL_READ;
    }

    printf("All the file_read_conf finally set as following:\n");
    for (int i = 0; i < CONF_NUM; i++)
        log(LOG_INFO, "Config %s = %llu\n", confs[i].name, skel->rodata->file_read_conf[i]);
}

bool verbose;
static const struct argp_option opts[] = {
    { "verbose", 'v', NULL, 0, "Verbose debug output" },
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
    switch (key) {
    case 'v':
        verbose = true;
        break;
    case ARGP_KEY_ARG:
        argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static const struct argp argp = {
    .options = opts,
    .parser = parse_arg,
};

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    if (level == LIBBPF_DEBUG && !verbose)
        return 0;
	syslog(LOG_ERR, format, args);
	return 0;
}

int main(int argc, char *argv[])
{
    struct readahead_tune_bpf *skel;

    int err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err)
        return err;

    err = daemon(0, 0);
    if (err) {
        perror("Failed to daemon\n");
        return err;
    }

    /* Set up libbpf errors and debug info callback */
    libbpf_set_print(libbpf_print_fn);

    /* Bump RLIMIT_MEMLOCK to create BPF maps */
    bump_memlock_rlimit();
    
    skel = readahead_tune_bpf__open();
    if (!skel) {
        log(LOG_ERR, "Failed to open BPF skeleton\n");
        return -1;
    }

    parse_config_env(skel);

    err = readahead_tune_bpf__load(skel);
    if (err) {
        log(LOG_ERR, "Failed to load and verify BPF skeleton\n");
        goto cleanup;
    }

    err = readahead_tune_bpf__attach(skel);
    if (err) {
        log(LOG_ERR, "Failed to attach BPF skeleton\n");
        goto cleanup;
    }

    pause();

cleanup:
    readahead_tune_bpf__destroy(skel);
    return err;
}
