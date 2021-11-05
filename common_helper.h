#ifndef _COMMON_HELPER_H
#define _COMMON_HELPER_H

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/resource.h>

#define log(level, fmt, args...) do {	\
	syslog(level, fmt, ##args);	\
} while (0)

static inline void bump_memlock_rlimit(void)
{
    struct rlimit rlim_new = {
        .rlim_cur   = RLIM_INFINITY,
        .rlim_max   = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}
#endif
