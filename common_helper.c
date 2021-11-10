#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "common_helper.h"

static void free_opt(struct opt *opt)
{
	if (!opt)
		return;
	free(opt->name);
	free(opt->val);
	free(opt);
}

static struct opt *new_opt(char *name, char *val)
{
	struct opt *opt = (struct opt*)malloc(sizeof(opt));
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

struct opt **parse_init(unsigned int size)
{
	struct opt **opts = (struct opt **)malloc(size * sizeof(struct opt *));
	if (opts)
		memset(opts, 0, size * sizeof(struct opt*));
	return opts;
}

void parse_fini(struct opt **opts, unsigned int size)
{
	if (!opts)
		return;

	for (unsigned int i = 0; i < size; i++)
		if (opts[i] != NULL)
			free_opt(opts[i]);
}

static int empty(char *s)
{
	while (isspace(*s))
		++s;
	return *s == 0;
}

static char *strstrip(char *s) 
{
    char *p; 
    while (isspace(*s))
        s++;
    p = s + strlen(s) - 1;
    if (p <= s)
        return s;
    while (isspace(*p) && p >= s)
        *p-- = 0;
    return s;
}

/* djb hash */
static unsigned hash(const char *str, unsigned int size)
{
	const unsigned char *s;
        unsigned hash = 5381;
	for (s = (const unsigned char *)str; *s; s++) 
		hash = (hash * 32) + hash + *s;
        return hash % size;
}

int parse_config_file(const char *conf_fn, struct opt **opts, unsigned int size)
{
	char *line = NULL;
	size_t linelen = 0;
	char *name;
	char *val;
	struct opt *opt;
	unsigned int h;
	unsigned int lineno = 1;
	int ret = 0;

	FILE *f = fopen(conf_fn, "r");
	if (!f) {
		log(LOG_ERR, "fopen config file(%s) failed\n", conf_fn);
		return -1;
	}

	while (getline(&line, &linelen, f) > 0) {
		char *s = strchr(line, '#');
		if (s)
			*s = 0;
		s = strstrip(line);
		if ((val = strchr(line, '=')) != NULL) {
			*val++ = 0;
			name = strstrip(s);
			val = strstrip(val);
			opt = new_opt(name, val);
			if (!opt) {
				ret = -1;
				log(LOG_ERR, "failed to alloc opt struct\n");
				goto cleanup;
			}
			h = hash(name, size);
			if (opts[h]) {
				struct opt *next = opts[h]->next;
				opts[h]->next = opt;
				opt->next = next;
			} else {
				opts[h] = opt;
			}
		} else if (!empty(s)) {
			ret = -1;
			log(LOG_ERR, "config file(%s) line(%u) is not field\n", conf_fn, lineno);
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

char* config_opt(struct opt **opts, unsigned int size, const char *name)
{
	unsigned int h = hash(name, size);
	struct opt *header = opts[h];
	while (header) {
		if (strcmp(header->name, name))
			header = header->next;
		else
			break;
	}
	return header ? header->val : NULL;
}

void bump_memlock_rlimit(void)
{
    struct rlimit rlim_new = {
        .rlim_cur   = RLIM_INFINITY,
        .rlim_max   = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        log(LOG_ERR, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        exit(1);
    }
}
