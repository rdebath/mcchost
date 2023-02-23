
#include "strcasestr.h"

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
