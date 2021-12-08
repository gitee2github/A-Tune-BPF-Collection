// SPDX-License-Identifier: GPL-2.0 OR MulanPSL-2.0
/* Copyright (C) 2021. Huawei Technologies Co., Ltd */
#define BPF_NO_PRESERVE_ACCESS_INDEX

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include "readahead_tune.h"

/*
 * Need to keep consistent with definitions in include/linux/fs.h
 * vmlinux.h does not contain macro
 */
#define FMODE_RANDOM 0x1000
#define FMODE_WILLNEED 0x400000

#define PREFIX_PATTERN "blk_"
#define MAX_HASH_TABLE_ENTRY 100000
#define MAP_ARRAY_SIZE 10

char g_license[] SEC("license") = "GPL";
__u32 g_version SEC("version") = 1;

struct fs_file_read_args {
    struct fs_file_read_ctx *ctx;
    int version;
};

struct fs_file_release_args {
    void *inode;
    void *filp;
};

struct file_rd_hnode {
    __u64 last_nsec;
    __u64 seq_nr;
    __u64 tot_nr;
};

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, unsigned long long);
    __uint(max_entries, MAP_ARRAY_SIZE);
} arraymap SEC(".maps");

struct bpf_map_def SEC("maps") htab = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(long),
    .value_size = sizeof(struct file_rd_hnode),
    .max_entries = MAX_HASH_TABLE_ENTRY,
};

/*
 * This first paramater should be the exact position of variable, otherwise the start
 * position of array, and then access the variable by array[index], BPF verifier will
 * warn: "load bpf program failed: Permission denied ... variable stack access var_off"
 */
static __always_inline void get_conf(unsigned long long *file_read_conf, unsigned int index)
{
    const char conf_fmt[] = "option is %llu\n";
    void *value = bpf_map_lookup_elem(&arraymap, &index);
    if (value) {
        *file_read_conf = *(unsigned long long *)value;
    }
}

static __always_inline bool is_expected_file(void *name)
{
    char prefix[5];
    int err = bpf_probe_read_str(&prefix, sizeof(prefix), name);
    if (err <= 0) {
        return 0;
    }
    return !__builtin_memcmp(prefix, PREFIX_PATTERN, sizeof(PREFIX_PATTERN) - 1);
}

SEC("raw_tracepoint.w/fs_file_read")
int fs_file_read(struct fs_file_read_args *args)
{
    const char fmt[] = "elapsed %llu, seq %u, tot %u\n";
    struct fs_file_read_ctx *rd_ctx = args->ctx;
    unsigned long long file_read_conf[CONF_NUM] = {
        DEFAULT_FILESZ,
        DEFAULT_READ_TIME,
        DEFAULT_TOTAL_READ,
        DEFAULT_LOWER_BOUND,
        DEFAULT_UPPER_BOUND
    };

    if (!is_expected_file((void *)rd_ctx->name)) {
        return 0;
    }

    /*
     * Get user configuration, 4.19 kernel does not support
     * BPF program for-loop
     */
    get_conf(file_read_conf + CONF_FILESZ, CONF_FILESZ);

    if (rd_ctx->i_size <= file_read_conf[CONF_FILESZ]) {
        rd_ctx->set_f_mode = FMODE_WILLNEED;
        return 0;
    }

    __u64 now = bpf_ktime_get_ns();
    __u64 key = rd_ctx->key;
    bool first = 0;
    struct file_rd_hnode *hist = bpf_map_lookup_elem(&htab, &key);
    struct file_rd_hnode new_hist;
    if (!hist) {
        __builtin_memset(&new_hist, 0, sizeof(new_hist));
        new_hist.last_nsec = now;
        first = 1;
        hist = &new_hist;
    }

    /* the consecutive read pos of the same file spatially local */
    if (rd_ctx->index >= rd_ctx->prev_index &&
        rd_ctx->index - rd_ctx->prev_index <= 1) {
        hist->seq_nr += 1;
    }
    hist->tot_nr += 1;

    if (first) {
        bpf_map_update_elem(&htab, &key, hist, 0);
        return 0;
    }

    get_conf(file_read_conf + CONF_READ_TIME, CONF_READ_TIME);
    get_conf(file_read_conf + CONF_TOTAL_READ, CONF_TOTAL_READ);
    get_conf(file_read_conf + CONF_LOWER_BOUND, CONF_LOWER_BOUND);
    get_conf(file_read_conf + CONF_UPPER_BOUND, CONF_UPPER_BOUND);

    if (now - hist->last_nsec >= file_read_conf[CONF_READ_TIME] || hist->tot_nr >= file_read_conf[CONF_TOTAL_READ]) {
        if (hist->tot_nr >= file_read_conf[CONF_TOTAL_READ]) {
            if (hist->seq_nr <= hist->tot_nr * file_read_conf[CONF_LOWER_BOUND] / HUNDRED_PERCENTAGE) {
                rd_ctx->set_f_mode = FMODE_RANDOM;
            } else if (hist->seq_nr >= hist->tot_nr * file_read_conf[CONF_UPPER_BOUND] / HUNDRED_PERCENTAGE) {
                rd_ctx->clr_f_mode = FMODE_RANDOM;
            }
        }

        hist->last_nsec = now;
        hist->tot_nr = 0;
        hist->seq_nr = 0;
    }

    return 0;
}

SEC("raw_tracepoint/fs_file_release")
int fs_file_release(struct fs_file_release_args *args)
{
    __u64 key = (unsigned long)args->filp;
    void *value = bpf_map_lookup_elem(&htab, &key);
    if (value) {
        bpf_map_delete_elem(&htab, &key);
    }

    return 0;
}
