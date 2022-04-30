#include <stdio.h>
#include <string.h>

#include "help.h"

#if INTERFACE
typedef struct help_text_t help_text_t;
struct help_text_t {
    char * item;
    int class;
    char ** lines;
};

#define H_CMD	1	// Help for a command

#define CMD_HELP \
    {N"help", &cmd_help}, {N"faq", &cmd_help}, \
    {N"news", &cmd_help}, {N"view", &cmd_help}, {N"rules", &cmd_help}

#endif

/*HELPTEXT help
Commands can be listed with &T/commands
To see help for a command type &T/help CommandName
*/

/*HELPTEXT view
The view command show special files for this system,
none are currently installed.
*/

/*HELPTEXT faq
&cFAQ&f:
&fExample: What does this server run on? This server runs on MCCHost
*/

/*HELPTEXT news
No news today.
*/

/*HELPTEXT rules
No rules found.
*/

void
cmd_help(char * prefix, char *cmdargs)
{
    char helpbuf[BUFSIZ];
    if (prefix && strcasecmp(prefix, "help") == 0) prefix = 0;
    if (!cmdargs && !prefix)
	strcpy(helpbuf, "help/help.txt");
    else {
	while (cmdargs && *cmdargs == ' ') cmdargs++;
	snprintf(helpbuf, sizeof(helpbuf),
	    "help/%s%s%.64s.txt", 
	    prefix?prefix:"",
	    !prefix||!cmdargs?"":" ",
	    cmdargs?cmdargs:"");

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

    {
	int l = strlen(helpbuf);
	if (l>9) {
	    helpbuf[l-4] = 0;
	    for(int hno = 0; helptext[hno].item; hno++) {
		if (strcasecmp(helptext[hno].item, helpbuf+5) != 0)
		    continue;
		char ** ln = helptext[hno].lines;
		for(;*ln; ln++) {
		    strcpy(helpbuf, *ln);
		    l = strlen(helpbuf);
		    convert_to_cp437(helpbuf, &l);
		    post_chat(1, helpbuf, l);
		}
		return;
	    }
	}
    }

    if (prefix == 0)
	post_chat(1, "&cNo help found for that topic", 0);
    else
	post_chat(1, "&cNo file found for that topic", 0);
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
