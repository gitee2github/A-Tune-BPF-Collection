#ifndef _COMMON_HELPER_H
#define _COMMON_HELPER_H

#include <syslog.h>

#define SHASH 11
#define log(level, fmt, args...) do {	\
	syslog(level, fmt, ##args);	\
} while (0)

struct opt {
	struct opt *next;
	char *name;
	char *val;
};

struct opt **parse_init(unsigned int size);
void parse_fini(struct opt **opts, unsigned int size);
int parse_config_file(const char *conf_fn, struct opt **opts, unsigned int size);
char* config_opt(struct opt **opts, unsigned int size, const char *name);
void bump_memlock_rlimit(void);

#endif
