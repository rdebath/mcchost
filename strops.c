
#include "strops.h"

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

    strcat(list, ",");
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
