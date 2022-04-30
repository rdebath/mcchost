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

/*HELP help H_CMD
Commands can be listed with &T/commands
To see help for a command type &T/help CommandName
Also see /faq, /news, /view and /rules
*/

/*HELP other
Other help files: chars, inifile, edlin
*/
/* Other generic help texts at the end of this file */

#define H_CMD	1	// Help for a command

#define CMD_HELP \
    {N"help", &cmd_help}, {N"faq", &cmd_help}, \
    {N"news", &cmd_help}, {N"view", &cmd_help}, {N"rules", &cmd_help}

#endif

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

/*HELP view H_CMD
The view command show special files for this system,
none are currently installed.
*/

/*HELP faq H_CMD
&cFAQ&f:
&fExample: What does this server run on? This server runs on MCCHost
*/

/*HELP news H_CMD
No news today.
*/

/*HELP rules H_CMD
No rules found.
*/
