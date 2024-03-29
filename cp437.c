
#include "cp437.h"

#if INTERFACE
typedef struct emconv_t emconv_t;
struct emconv_t {
    char * name;
    int code;
};
typedef struct to_cp437_t to_cp437_t;
struct to_cp437_t {
    int utf;
    uint8_t cp437;
};
#endif

emconv_t emconv[];

/*HELP chars,cp437
___0123456789ABCDEF0123456789ABCDEF_
0:|␀☺☻♥♦♣♠•◘○◙♂♀♪♫☼▶◀↕‼¶§▬↨↑↓→←∟↔▲▼|
1:| !"#$%&'()*+,-./0123456789:;<=>?|
2:|@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_|
3:|`abcdefghijklmnopqrstuvwxyz{|}~⌂|
4:|ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ|
5:|áíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐|
6:|└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀|
7:|αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ |
*/

void
cp437_prt(FILE* ofd, int ch)
{
    int uc = cp437rom[ch & 0xFF];
    int c1, c2, c3;
    if (ch == 0) uc = 0x2400; // Only debug/display output

    c2 = (uc/64);
    c1 = uc - c2*64;
    c3 = (c2/64);
    c2 = c2 - c3 * 64;
    if (uc < 128 && uc >= 0)
	putc(uc, ofd);
    else if (uc < 2048)
        fprintf(ofd, "%c%c", c2+192, c1+128);
    else if (uc < 65536)
        fprintf(ofd, "%c%c%c", c3+224, c2+128, c1+128);
    else
        fprintf(ofd, "%c%c%c", 0xef, 0xbf, 0xbd);
}

void
convert_to_cp437(char *buf, int *l)
{
    int s = 0, d = 0;
    int utfstate[1] = {0};

    for(s=0; s<*l; s++)
    {
        int ch = -1;
        if (*utfstate>=0) ch= (buf[s] & 0xFF); else { s--; ch = -1; }

        if (ch <= 0x7F && *utfstate == 0)
            ;
        else {
            ch = decodeutf8(ch, utfstate);
            if (ch == UTFNIL)
                continue;
            if (ch >= 0x80) {
                int utf = ch;
                ch = 0;
                for(int n=0; n<256; n++) {
                    if (cp437rom[(n+128)&0xFF] == utf) { ch=((n+128)&0xFF) | 0x100; break; }
                }
		if (ch == 0)
		    for(int n=0; cp437_extras[n].utf; n++) {
			if (utf == cp437_extras[n].utf) {
			    ch = cp437_extras[n].cp437;
			    break;
			}
		    }
		if (ch == 0)
		    ch = 0xA8; // CP437 ¿
            }
        }

        buf[d++] = ch;
    }
    *l = d;
}

void
convert_to_utf8(char *buf, int len, char *s)
{
    char * d = buf;
    int ch;
    for(; (ch=*s) != 0; s++)
    {
	int uc = cp437rom[ch & 0xFF];
	int c1, c2, c3;
	if (d-buf>len-4) break;

	c2 = (uc/64);
	c1 = uc - c2*64;
	c3 = (c2/64);
	c2 = c2 - c3 * 64;
	if (uc < 128 && uc >= 0) {
	    *d++ = uc;
	} else if (uc < 2048) {
	    *d++ = c2+192; *d++ = c1+128;
	} else if (uc < 65536) {
	    *d++ = c3+224; *d++ = c2+128; *d++ = c1+128;
	} else {
	    *d++ = 0xef; *d++ = 0xbf; *d++ = 0xbd;
	}
    }
    *d = 0;
}

void
convert_from_paren(char *buf, int *l)
{
    char * d = buf;
    int len = *l;

    for(int i=0; i<len; i++) {
	if (buf[i] == '(') {
	    for(int em = 0; emconv[em].name; em++) {
		int nl = strlen(emconv[em].name);
		if (i+nl+1 > len) continue;
		if (strncmp(buf+i+1, emconv[em].name, nl) == 0 && buf[i+nl+1] == ')') {
		    *d++ = emconv[em].code;
		    i += nl+1;
		    goto break_continue;
		}
	    }
	} else if (buf[i] == '\\' && (buf[i+1] == '\\' || buf[i+1] == '(')) {
	    if (i!=len) i++;
	}
	*d++ = buf[i];

	break_continue:;
    }
    *l = d-buf;
}

char cp437_ascii[] =
	"CueaaaaceeeiiiAAE**ooouuyOUc$YPs"
	"aiounNao?++**!<>###||||++||+++++"
	"+--|-+||++--|-+----++++++++##||#"
	"aBTPEsyt******EN=+><++-=... n2* ";

/* NOTE: The zero entry could be U+2400, this would be reasonable for
 *       display and debugging. */

int cp437rom[256] = {
    0x0000, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
    0x25b6, 0x25c0, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
    0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
    0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
    0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
};

to_cp437_t cp437_extras[] = {
    { 0x2215, '/' }, // DIVISION SLASH;
    { 0x2216, '\\' }, // SET MINUS
    { 0x1390, '.' }, // ETHIOPIC TONAL MARK YIZET
    {0,0}
};

emconv_t emconv[] = {
{ "darksmile", 0x01 },
{ "smile", 0x02 },
{ "heart", 0x03 }, { "hearts", 0x03 },
{ "diamond", 0x04 }, { "diamonds", 0x04 }, { "rhombus", 0x04 },
{ "club", 0x05 }, { "clubs", 0x05 }, { "clover", 0x05 }, { "shamrock", 0x05 },
{ "spade", 0x06 }, { "spades", 0x06 },
{ "dot", 0x07 }, { "*", 0x07 }, { "bullet", 0x07 }, { "point", 0x07 },
{ "hole", 0x08 },
{ "o", 0x09 }, { "circle", 0x09 },
{ "inversecircle", 0x0A },
{ "male", 0x0B }, { "mars", 0x0B },
{ "female", 0x0C }, { "venus", 0x0C },
{ "note", 0x0D }, { "quaver", 0x0D },
{ "notes", 0x0E }, { "music", 0x0E },
{ "sun", 0x0F }, { "celestia", 0x0F },
{ ">>", 0x10 }, { "right", 0x10 }, { "|>", 0x10 },
{ "<<", 0x11 }, { "left", 0x11 }, { "<|", 0x11 },
{ "updown", 0x12 }, { "^v", 0x12 },
{ "!!", 0x13 },
{ "p", 0x14 }, { "para", 0x14 }, { "pilcrow", 0x14 }, { "paragraph", 0x14 },
{ "sect", 0x15 }, { "section", 0x15 },
{ "-", 0x16 }, { "_", 0x16 }, { "bar", 0x16 }, { "half", 0x16 },
{ "updown2", 0x17 }, { "^v_", 0x17 },
{ "^", 0x18 }, { "uparrow", 0x18 },
{ "v", 0x19 }, { "downarrow", 0x19 },
{ "->", 0x1A }, { "rightarrow", 0x1A },
{ "<-", 0x1B }, { "leftarrow", 0x1B },
{ "l", 0x1C }, { "angle", 0x1C }, { "corner", 0x1C },
{ "<>", 0x1D }, { "<->", 0x1D }, { "leftright", 0x1D },
{ "^^", 0x1E }, { "up", 0x1E },
{ "vv", 0x1F }, { "down", 0x1F },
{ "house", 0x7F },

{ "perc", '%' }, { "amp", '&' },
{ "bra", '(' }, { "ket", ')' },

{0,0}
};


#if TEST
int
main(int argc, char ** argv)
{
    int fn = 0;
    if (argc>1 && strcmp(argv[1], "-4") == 0) fn=0;
    if (fn == 0) {
	char buf[8192];
	while(fgets(buf, sizeof(buf), stdin)) {
	    int l = strlen(buf)+1;
	    convert_to_cp437(buf, &l);
	    printf("%s", buf);
	}
    }
}
#endif
