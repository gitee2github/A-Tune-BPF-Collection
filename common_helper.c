#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "common_helper.h"

static void free_opt(struct opt *opt)
{
    if (!opt) {
        return;
    }
    free(opt->name);
    free(opt->val);
    free(opt);
}

static struct opt *new_opt(char *name, char *val)
{
    struct opt *opt = (struct opt*)malloc(sizeof(struct opt));
    if (opt) {
        opt->name = strdup(name);
        opt->val = strdup(val);
        opt->next = NULL;
        if (!opt->name || !opt->val) {
            free_opt(opt);
            opt = NULL;
        }
    }
    return opt;
}

/* alloc option hash table */
struct opt **parse_init(unsigned int size)
{
    struct opt **opts = (struct opt **)malloc(size * sizeof(struct opt *));
    if (opts)
        memset(opts, 0, size * sizeof(struct opt*));
    return opts;
}

/* free option hash table and inside options */
void parse_fini(struct opt **opts, unsigned int size)
{
    if (!opts) {
        return;
    }

    for (unsigned int i = 0; i < size; i++) {
        if (opts[i] != NULL) {
            struct opt *opt = opts[i];
            while (opt) {
                struct opt *next_opt = opt->next;
                free_opt(opt);
                opt = next_opt;
            }
        }
    }
    free(opts);
}

static int empty(char *s)
{
    while (isspace(*s)) {
        ++s;
    }
    return *s == 0;
}

static char *strstrip(char *s) 
{
    while (isspace(*s)) {
        s++;
    }
    char *p = s + strlen(s) - 1;
    if (p <= s) {
        return s;
    }
    while (isspace(*p) && p >= s) {
        *p-- = 0;
    }
    return s;
}

/* djb hash */
static unsigned hash(const char *str, unsigned int size)
{
    unsigned hash = 5381;

    for (const unsigned char *s = (const unsigned char *)str; *s; s++) {
        hash = (hash * 32) + hash + *s;
    }
    return hash % size;
}

/*
 * parse config file by key-value way from config file, the result will be added to option hash table
 * @where: where the log output
 * @conf_fn: config file name
 * @opts: option hash table(option pointer array)
 * @size: option hash table size
 */
int parse_config_file(unsigned int where, const char *conf_fn, struct opt **opts, unsigned int size)
{
    char *line = NULL;
    size_t linelen = 0;
    char *val = NULL;
    unsigned int lineno = 1;
    int ret = 0;

    FILE *f = fopen(conf_fn, "r");
    if (!f) {
        log(where, LOG_ERR, "fopen config file(%s) failed\n", conf_fn);
        return -1;
    }

    while (getline(&line, &linelen, f) > 0) {
        char *s = strchr(line, '#');
        if (s) {
            *s = 0;
        }
        s = strstrip(line);
        if ((val = strchr(line, '=')) != NULL) {
            *val++ = 0;
            char *name = strstrip(s);
            val = strstrip(val);
            struct opt * opt = new_opt(name, val);
            if (!opt) {
                ret = -1;
                log(where, LOG_ERR, "failed to alloc opt struct\n");
                goto cleanup;
            }
            unsigned int h = hash(name, size);
            if (opts[h]) {
                struct opt *next = opts[h]->next;
                opts[h]->next = opt;
                opt->next = next;
            } else {
                opts[h] = opt;
            }
        } else if (!empty(s)) {
            ret = -1;
            log(where, LOG_ERR, "config file(%s) line(%u) is not field\n", conf_fn, lineno);
            goto cleanup;
        }
        lineno++;
    }
    goto ret;

cleanup:
    parse_fini(opts, size);

ret:
    fclose(f);
    free(line);
    return ret;
}

/* get config value string by config name */
char* config_opt(struct opt **opts, unsigned int size, const char *name)
{
    unsigned int h = hash(name, size);
    struct opt *header = opts[h];
    while (header) {
        if (strcmp(header->name, name)) {
            header = header->next;
        } else {
            break;
        }
    }
    return header ? header->val : NULL;
}

void bump_memlock_rlimit(unsigned int where)
{
    struct rlimit rlim_new = {
        .rlim_cur   = RLIM_INFINITY,
        .rlim_max   = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        log(where, LOG_ERR, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}
