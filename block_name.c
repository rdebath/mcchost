#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "block_name.h"

block_t
block_name(char * name)
{
    char bnb[256];
    char *s, *d;
    block_t b = BLOCKMAX;
    int non_digit = 0;

    for(s=name, d=bnb; d<bnb+sizeof(bnb)-1 && *s; s++) {
	int ch = *s & 0xFF;
	if (ch <= ' ') continue;
	if (ch < '0' || ch > '9') non_digit = 1;
	if (ch & 0x80) ch = cp437_ascii[ch & 0x7F];
	if (ch == '_') continue;
	ch = tolower(ch);
	*d++ = ch;
    }
    *d = 0;
    if (!*bnb) return b;

    if (!non_digit) {
	b = atoi(bnb);
	if (b < BLOCKMAX)
	    return b;
    }

    for(int i=0; i < BLOCKMAX; i++) {
	if (!level_prop->blockdef[i].defined) continue;
	if (!level_prop->blockdef[i].name.c[0]) continue;
	if (block_name_match(bnb, level_prop->blockdef[i].name.c))
	    return i;
    }

    for(int i=0; oldblock_names[i]; i++)
	if (block_name_match(bnb, oldblock_names[i]))
	    return i;

    for(int i=0; i < Block_CPE; i++)
	if (block_name_match(bnb, default_blocks[i].name.c))
	    return i;

    return BLOCKMAX;
}

LOCAL int
block_name_match(char * s, volatile char * d)
{
    while(*s && *d) {
	int ch1 = *s;
	if (ch1 == ' ' || ch1 == '_') {s++; continue; }
	int ch2 = *d;
	if (ch2 == ' ' || ch2 == '_') {d++; continue; }
	ch2 = tolower(ch2);
	if (ch1 != ch2) break;
	s++; d++;
    }
    return (*s == 0 && *d == 0);
}
