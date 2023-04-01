
#include "block_name.h"

char *
block_name(block_t block)
{
#define BBUFLEN 4
    static int bid = 0;
    static char name[BBUFLEN][NB_SLEN];
    char * n = 0;
    if (block >= BLOCKMAX || (server->cpe_disabled && block >= Block_CP))
	return "Invalid Block";

    if (level_prop->blockdef[block].defined)
	if (level_prop->blockdef[block].name.c[0])
	    n = level_prop->blockdef[block].name.c;

    if (n == 0 && block < Block_CPE)
	if (default_blocks[block].name.c[0])
	    n = default_blocks[block].name.c;

    bid++; if (bid>=BBUFLEN) bid = 0;
    char *d = name[bid];
    if (n)
	while (*n && d<name[bid]+NB_SLEN-1)
	    *d++ = *n++;
    *d = 0;
    if (!name[bid][0])
	snprintf(name[bid], NB_SLEN, "@%d", block);
    return name[bid];
}

block_t
block_id(char * name)
{
    char bnb[256];
    char *s, *d;
    block_t b = -1;
    int non_digit = 0;

    if (!name || !*name) return player_held_block;

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

#ifdef OLDBLOCKNAMES
    for(int i=0; oldblock_names[i]; i++)
	if (block_name_match(bnb, oldblock_names[i]))
	    return i;
#endif

    for(int i=0; i < Block_CPE; i++)
	if (block_name_match(bnb, default_blocks[i].name.c))
	    return i;

    return -1;
}

LOCAL int
block_name_match(char * s, char * d)
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
