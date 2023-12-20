
#include "strops.h"

#if INTERFACE
inline static int strtoi(const char *nptr, char **endptr, int base)
{
    long v = strtol(nptr, endptr, base);
    if (sizeof(int) != sizeof(long)) {
	if (v < INT_MIN) { v = INT_MIN; errno = ERANGE; }
	if (v > INT_MAX) { v = INT_MAX; errno = ERANGE; }
    }
    return v;
}
#endif

char *
my_strcasestr(char *haystack, char *needle)
{
    int nlen = strlen(needle);
    int slen = strlen(haystack) - nlen + 1;
    int i;

    for (i = 0; i < slen; i++) {
	int j;
	for (j = 0; j < nlen; j++) {
	    if (toupper((unsigned char)haystack[i+j]) != toupper((unsigned char)needle[j]))
		goto break_continue;
	}
	return haystack + i;
    break_continue: ;
    }
    return 0;
}

static char * argv = 0;
static char * rbuf = 0;
static char * rfree = 0;
static int buflen = 0;

/* Chops a command line up into arguments. */
char *
strarg(char * nargv)
{
    if (nargv) {
	int l = strlen(nargv) + 1;
	if (l > buflen) {
	    if (rbuf) free(rbuf);
	    rbuf = 0; argv = 0; buflen = 0;
	    if (l < 256) l = 256;
	    rbuf = malloc(l);
	    if (!rbuf) return 0;
	    buflen = l;
	}
	argv = nargv;
	rfree = rbuf;
    }
    if (!argv) return 0;

    while(*argv == ' ') argv++;
    if (*argv == '\0') return argv = 0;

    char * arg = rfree;
    char * ret = arg;
    int in_quote = 0;
    for(;;argv++) {
	if (*argv == '\0') break;
	if (*argv == '"') {
	    if (in_quote && argv[1] == '"') {
		*arg++ = *argv++;
	    } else if (arg != rbuf && argv[1] == '"')
		*arg++ = *argv++;
	    else
		in_quote = !in_quote;
	    continue;
	}
	if (*argv == ' ') {
	    if (in_quote) *arg++ = ' ';
	    else if (arg == rbuf)
		continue;
	    else break;
	}
	*arg++ = *argv;
    }
    *arg = 0;
    rfree = arg+1;
    if (*argv == 0) argv = 0;
    return ret;
}

char *
strarg_rest()
{
    if (argv) {
	while(*argv == ' ') argv++;
	char * arg = rfree;
	while(*argv) *arg++ = *argv++;
	argv = 0;
	*arg = 0;
	return rfree;
    } else
	return 0;
}

int
in_strlist(char * list, char * entry)
{
    char * p = list;
    while (p && *p) {
	p = strstr(p, entry);
	int rv = 1;
	if (!p) rv = 0;
	if (rv && p != list) {
	    if (p[-1] != ' ' && p[-1] != ',') rv = 0;
	}
	if (rv) {
	    char * e = p + strlen(entry);
	    if (*e != 0 && *e != ' ' && *e != ',') rv = 0;
	}
	if (rv) return 1;
    }
    return 0;
}

int
add_strlist(char * list, int l, char * entry)
{
    if (strlen(list) + strlen(entry) + 2 > l) return -1;
    if (in_strlist(list, entry)) return 0;

    if (*list) strcat(list, ",");
    strcat(list, entry);
    return 1;
}

int
del_strlist(char * list, char * entry)
{
    char * p = list;
    while (p && *p) {
	p = strstr(p, entry);
	int rv = 1;
	if (!p) rv = 0;
	if (rv && p != list) {
	    if (p[-1] != ' ' && p[-1] != ',') rv = 0;
	}
	if (rv) {
	    char * e = p + strlen(entry);
	    if (*e != 0 && *e != ' ' && *e != ',') rv = 0;
	    if (rv) {
		// Don't use strcpy() as there is overlap.
		while (p > list && (p[-1] == ',' || p[-1] == ' ')) p--;
		if (p == list && (*e == ' ' || *e == ',')) e++;
		memmove(p, e, strlen(e)+1);
		return 1;
	    }
	}
    }
    return 0;
}
