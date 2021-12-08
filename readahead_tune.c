// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2021. Huawei Technologies Co., Ltd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <argp.h>
#include <signal.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include "readahead_tune.h"
#include "common_helper.h"

#define DEFAULT_CONF "/etc/sysconfig/readahead_tune.conf"
#define READ_TP_NAME "fs_file_read"
#define RELEASE_TP_NAME "fs_file_release"

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

static int parse_config(unsigned int where, int conf_fd, const char *conf_fn)
{
    unsigned long long parse_conf[CONF_NUM] = {0};
    struct opt **opts = parse_init(SHASH);
    if (!opts) {
        log(where, LOG_ERR, "parse_init failed, all the option use default value\n");
        goto use_default;
    }

    if (parse_config_file(where, conf_fn, opts, SHASH)) {
        log(where, LOG_ERR, "parse_config_file failed, all the option use default value\n");
        goto use_default;
    }

    for (int i = 0; i < CONF_NUM; i++) {
        char *env_str = config_opt(opts, SHASH, confs[i].name);
        if (env_str && strlen(env_str)) {
            char *endptr;
            errno = 0;
            long long val = strtoll(env_str, &endptr, 10);

            if ((errno == ERANGE && (val == LLONG_MAX || val == LLONG_MIN))
                    || (errno != 0 && val == 0)
                    || endptr == env_str
                    || *endptr != '\0'
                    || val < 0) {
                log(where, LOG_ERR, "Option %s value is %s, use default!\n", confs[i].name, env_str);
                parse_conf[i] = confs[i].default_val;
            } else {
                parse_conf[i] = (unsigned long long)val;
            }
        } else {
            parse_conf[i] = confs[i].default_val;
        }
    }

    if (parse_conf[CONF_LOWER_BOUND] >= parse_conf[CONF_UPPER_BOUND]
            || parse_conf[CONF_LOWER_BOUND] <= ZERO_PERCENTAGE
            || parse_conf[CONF_UPPER_BOUND] >= HUNDRED_PERCENTAGE) {
        log(where, LOG_ERR, "lower-bound-percentage(%llu), upper-bound-percentage(%llu) is invalid, use default\n",
                parse_conf[CONF_LOWER_BOUND], parse_conf[CONF_UPPER_BOUND]);
        parse_conf[CONF_LOWER_BOUND] = DEFAULT_LOWER_BOUND;
        parse_conf[CONF_UPPER_BOUND] = DEFAULT_UPPER_BOUND;
    }

    if (parse_conf[CONF_TOTAL_READ] * parse_conf[CONF_LOWER_BOUND] < parse_conf[CONF_TOTAL_READ]
            || parse_conf[CONF_TOTAL_READ] * parse_conf[CONF_UPPER_BOUND] < parse_conf[CONF_TOTAL_READ]) {
        log(where, LOG_ERR, "total-read-threshold(%llu) is too large, use default\n", parse_conf[CONF_TOTAL_READ]);
        parse_conf[CONF_TOTAL_READ] = DEFAULT_TOTAL_READ;
    }
    goto success_parse;

use_default:
    for (int i = 0; i < CONF_NUM; i++) {
        parse_conf[i] = confs[i].default_val;
    }

success_parse:
    parse_fini(opts, SHASH);
    for (unsigned int i = 0; i < CONF_NUM; i++) {
        if (bpf_map_update_elem(conf_fd, &i, parse_conf + i, BPF_ANY)) {
            return -1;
        }
    }
    log(where, LOG_INFO, "All the file_read_conf finally set as following:\n");
    for (int i = 0; i < CONF_NUM; i++) {
        log(where, LOG_INFO, "Config %s = %llu\n", confs[i].name, parse_conf[i]);
    }
    return 0;
}

bool verbose;
bool foreground;
const char * config_file = DEFAULT_CONF;
static const struct argp_option opts[] = {
    { "verbose", 'v', NULL, 0, "Verbose debug output" },
    { "foreground", 'f', NULL, 0, "Run foreground, not daemonize" },
    { "config", 'c', "CONFIG_FILE", 0, "Config file to specify" },
    {},
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
    switch (key) {
    case 'v':
        verbose = true;
        break;
    case 'f':
        foreground = true;
        break;
    case 'c':
        config_file = !arg ? DEFAULT_CONF : arg;
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
    if (level >= LIBBPF_DEBUG && !verbose) {
        return 0;
    }
    /* syslog output is Mojibake, and No log_buf to check kernel BPF verifier log */
    if (foreground) {
        return vfprintf(stderr, format, args);
    }
    syslog(LOG_ERR, format, args);
    return 0;
}

static volatile bool exiting = false;

static void sig_handler(int sig)
{
    log((foreground ? TERM : SYSLOG), LOG_INFO, "Gracefully exit...\n");
    exiting = true;
}

int main(int argc, char *argv[])
{
    const char *file = "readahead_tune.bpf.o";
    struct bpf_object *obj = NULL;
    unsigned int where = TERM;
    int prog_fd = -1;

    int err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err) {
        return err;
    }

    if (!foreground) {
        /* file is relative path, nochdir can't be zero */
        err = daemon(1, 0);
        where = SYSLOG;
        if (err) {
            perror("Failed to daemon\n");
            return err;
        }
    }

    signal(SIGTERM, sig_handler);

    /* Set up libbpf errors and debug info callback */
    libbpf_set_print(libbpf_print_fn);

    /* Bump RLIMIT_MEMLOCK to create BPF maps */
    bump_memlock_rlimit(where);

    err = bpf_prog_load(file, BPF_PROG_TYPE_UNSPEC, &obj, &prog_fd);
    if (err) {
        log(where, LOG_ERR, "Failed to load BPF program\n");
        return err;
    }

    int conf_fd = bpf_object__find_map_fd_by_name(obj, "arraymap");
    if (conf_fd < 0) {
        log(where, LOG_ERR, "Failed to get arraymap fd\n");
        err = conf_fd;
        goto cleanup;
    }

    err = parse_config(where, conf_fd, config_file);
    if (err) {
        log(where, LOG_ERR, "Failed to write config value into BPF ARRAY MAP\n");
        goto cleanup;
    }

    const char *prog_name = "raw_tracepoint.w/" READ_TP_NAME;
    struct bpf_program *prog = bpf_object__find_program_by_title(obj, prog_name);
    if (!prog) {
        log(where, LOG_ERR, "Failed to find program %s\n", prog_name);
        err = -EINVAL;
        goto cleanup;
    }

    int read_fd = bpf_raw_tracepoint_open(READ_TP_NAME, bpf_program__fd(prog));
    if (read_fd < 0) {
        err = -errno;
        log(where, LOG_ERR, "Failed to attach raw tracepoint %s\n", READ_TP_NAME);
        goto cleanup;
    }

    prog_name = "raw_tracepoint/" RELEASE_TP_NAME;
    prog = bpf_object__find_program_by_title(obj, prog_name);
    if (!prog) {
        log(where, LOG_ERR, "Failed to find program %s\n", prog_name);
        err = -EINVAL;
        goto close_read_fd;
    }

    int release_fd = bpf_raw_tracepoint_open(RELEASE_TP_NAME, bpf_program__fd(prog));
    if (release_fd < 0) {
        err = -errno;
        log(where, LOG_ERR, "Failed to attach raw tracepoint %s\n", RELEASE_TP_NAME);
        goto close_read_fd;
    }

    while (!exiting) {
        pause();
    }

    close(release_fd);
close_read_fd:
    close(read_fd);

cleanup:
    bpf_object__close(obj);
    return err;
}
