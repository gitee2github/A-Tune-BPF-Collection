#ifndef _COMMON_HELPER_H
#define _COMMON_HELPER_H

#include <syslog.h>

#define SHASH 11
#define SYSLOG  (1 << 0)
#define TERM    (1 << 1)
#define ALL (SYSLOG | TERM)

#define log(where, level, fmt, args...) do {    \
    if (where & SYSLOG) \
        syslog(level, fmt, ##args); \
    if (where & TERM) { \
        fprintf(stderr, fmt, ##args);   \
        fflush(stderr); \
    }   \
} while (0)

struct opt {
    struct opt *next;
    char *name;
    char *val;
};

struct opt **parse_init(unsigned int size);
void parse_fini(struct opt **opts, unsigned int size);
int parse_config_file(unsigned int where, const char *conf_fn, struct opt **opts, unsigned int size);
char* config_opt(struct opt **opts, unsigned int size, const char *name);
void bump_memlock_rlimit(unsigned int where);

#endif
