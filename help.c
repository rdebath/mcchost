#include <stdio.h>
#include <string.h>

#include "help.h"

typedef struct help_text_t help_text_t;
struct help_text_t {
    char * item;
    int class;
    char ** lines;
};

/*HELPTEXT help
&cHelp command help, TODO.
*/

void
cmd_help(char *cmdargs)
{
    char helpbuf[BUFSIZ];
    if (!cmdargs)
	strcpy(helpbuf, "help/help.txt");
    else {
	snprintf(helpbuf, sizeof(helpbuf), "help/%.64s.txt", cmdargs);
	for(char * p = helpbuf; *p; p++) {
	    if (*p <= ' ' || *p > '~' || *p == '\'' || *p == '"')
		*p = '_';
	    if (*p >= 'A' && *p <= 'Z')
		*p = *p - 'A' + 'a';
	}
    }

    FILE * hfd = fopen(helpbuf, "r");
    if (hfd) {
	while(fgets(helpbuf, sizeof(helpbuf), hfd)) {
	    int l = strlen(helpbuf);
	    char * p = helpbuf+l;
	    if (l != 0 && p[-1] == '\n') { p[-1] = 0; l--; }

	    convert_to_cp437(helpbuf, &l);

	    post_chat(1, helpbuf, l);
	}
	fclose(hfd);
	return;
    }

#ifdef HAS_HELP
#endif

    post_chat(1, "&cNo help found for that topic", 0);
    // /faq -> /help faq...
    // /news -> /help news...
    // /view -> /help view...

}

#ifdef UTFNIL
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
                ch = 0xA8; // CP437 ¿
                for(int n=0; n<256; n++) {
                    if (cp437rom[(n+128)&0xFF] == utf) { ch=((n+128)&0xFF) | 0x100; break; }
                }
            }
        }

	buf[d++] = ch;
    }
    *l = d;
}
#endif
