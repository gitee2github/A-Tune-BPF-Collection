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
#define MAX_HASH_TABLE_ENTRY 10000

char _license[] SEC("license") = "GPL";
__u32 _version SEC("version") = 1;

struct fs_file_read_ctx {
	const unsigned char *name;
	unsigned int f_mode;
	unsigned int rsvd;
	/* clear from f_mode */
	unsigned int clr_f_mode;
	/* set into f_mode */
	unsigned int set_f_mode;
	unsigned long key;
	/* file size */
	long long i_size;
	/* previous page index */
	long long prev_index;
	/* current page index */
	long long index;
};

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

struct bpf_map_def SEC("maps") htab = {
    .type = BPF_MAP_TYPE_HASH,
    .key_size = sizeof(long),
    .value_size = sizeof(struct file_rd_hnode),
    .max_entries = MAX_HASH_TABLE_ENTRY,
};

const volatile unsigned long long file_read_conf[CONF_NUM] = {
    DEFAULT_FILESZ,
    DEFAULT_READ_TIME,
    DEFAULT_TOTAL_READ,
    DEFAULT_LOWER_BOUND,
    DEFAULT_UPPER_BOUND
};

static __always_inline bool is_expected_file(void *name)
{
    char prefix[5];
    int err;

    err = bpf_probe_read_str(&prefix, sizeof(prefix), name);
    if (err <= 0)
        return false;
    return !__builtin_memcmp(prefix, PREFIX_PATTERN, sizeof(PREFIX_PATTERN) - 1);
}

SEC("raw_tracepoint.w/fs_file_read")
int fs_file_read(struct fs_file_read_args *args)
{
    const char fmt[] = "elapsed %llu, seq %u, tot %u\n";
    struct fs_file_read_ctx *rd_ctx = args->ctx;

    if (!is_expected_file((void *)rd_ctx->name))
        return 0;

    if (rd_ctx->i_size <= file_read_conf[CONF_FILESZ]) {
        rd_ctx->set_f_mode = FMODE_WILLNEED;
        return 0;
    }

    __u64 now = bpf_ktime_get_ns();
    __u64 key = rd_ctx->key;
    bool first = false;
    struct file_rd_hnode *hist = bpf_map_lookup_elem(&htab, &key);
    struct file_rd_hnode new_hist;
    if (!hist) {
        __builtin_memset(&new_hist, 0, sizeof(new_hist));
        new_hist.last_nsec = now;
        first = true;
        hist = &new_hist;
    }

    /* the consecutive read pos of the same file spatially local */
    if (rd_ctx->index >= rd_ctx->prev_index &&
        rd_ctx->index - rd_ctx->prev_index <= 1)
        hist->seq_nr += 1;
    hist->tot_nr += 1;

    bpf_trace_printk(fmt, sizeof(fmt), now - hist->last_nsec,
             hist->seq_nr, hist->tot_nr);

    if (first) {
        bpf_map_update_elem(&htab, &key, hist, 0);
        return 0;
    }

    if (now - hist->last_nsec >= file_read_conf[CONF_READ_TIME] || hist->tot_nr >= file_read_conf[CONF_TOTAL_READ]) {
        if (hist->tot_nr >= file_read_conf[CONF_TOTAL_READ]) {
            if (hist->seq_nr <= hist->tot_nr * file_read_conf[CONF_LOWER_BOUND] / HUNDRED_PERCENTAGE)
                rd_ctx->set_f_mode = FMODE_RANDOM;
            else if (hist->seq_nr >= hist->tot_nr * file_read_conf[CONF_UPPER_BOUND] / HUNDRED_PERCENTAGE)
                rd_ctx->clr_f_mode = FMODE_RANDOM;
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
    void *value;

    value = bpf_map_lookup_elem(&htab, &key);
    if (value)
        bpf_map_delete_elem(&htab, &key);

    return 0;
}
