#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "help.h"

#if INTERFACE
typedef struct help_text_t help_text_t;
struct help_text_t {
    char * item;
    int class;
    char ** lines;
};

/*HELP Welcome 0 */

/*HELP license H_CMD
This software is licensed under the AFGPL and other licenses.
Details and original source available at
    &Thttps://github.com/rdebath/mcchost
*/

/*HELP help H_CMD
Commands can be listed with &T/commands
To see help for a command type &T/help CommandName
Examples include &T/goto&S, &T/maps&S.
Also see /faq, /news and /help license.
*/

/*HELP other
Other help files: license, chars, inifile, edlin, ...
*/
/* Other generic help texts at the end of this file */

#define H_CMD	1	// Help for a command

#define CMD_HELP \
    {N"help", &cmd_help}, {N"faq", &cmd_help}, {N"news", &cmd_help}, \
    {N"clear", &cmd_clear}, {N"cls", &cmd_clear, .dup=1}
#endif

void
cmd_help(char * prefix, char *cmdargs)
{
    char helpbuf[BUFSIZ];
    char helptopic[128];
    if (prefix && (*prefix == 0 || strcasecmp(prefix, "help") == 0)) prefix = 0;
    if (!cmdargs && !prefix)
	strcpy(helptopic, "help");
    else {
	while (cmdargs && *cmdargs == ' ') cmdargs++;
	saprintf(helptopic,"%s%s%s",
	    prefix?prefix:"",
	    !prefix||!cmdargs?"":" ",
	    cmdargs?cmdargs:"");

	char * d = helptopic;
	for(char *p = helptopic; *p; p++) {
	    int ch = *p;
	    if (ch <= ' ' || ch > '~' || ch == '\'' || ch == '"'
		|| ch == '.' || ch == '_' || ch == '/') {
		if (d == helptopic || d[-1] != '_')
		    ch = '_';
		else
		    ch = 0;
	    }
	    if (ch >= 'A' && ch <= 'Z')
		ch = ch - 'A' + 'a';
	    if (ch) *d++ = ch;
	}
	*d = 0;
    }

    saprintf(helpbuf, "help/%s.txt", helptopic);
    FILE * hfd = fopen(helpbuf, "r");
    if (hfd) {
	int ln = 0;
	while(fgets(helpbuf, sizeof(helpbuf), hfd)) {
	    int l = strlen(helpbuf);
	    char * p = helpbuf+l;
	    if (l != 0 && p[-1] == '\n') { p[-1] = 0; l--; }
	    if (ln++ == 0) {
		if (strcasecmp(helpbuf, "!clear") == 0)
		    return cmd_clear("", 0);
	    }

	    convert_to_cp437(helpbuf, &l);
	    post_chat(-1, 0, 0, helpbuf, l);
	}
	fclose(hfd);
	return;
    }

    {
	int l = strlen(helptopic);
	if (l>0) {
	    helpbuf[l-4] = 0;
	    for(int hno = 0; helptext[hno].item; hno++) {
		if (strcasecmp(helptext[hno].item, helptopic) != 0)
		    continue;
		char ** ln = helptext[hno].lines;
		for(;*ln; ln++) {
		    strcpy(helpbuf, *ln);
		    l = strlen(helpbuf);
		    convert_to_cp437(helpbuf, &l);
		    post_chat(-1, 0, 0, helpbuf, l);
		}
		return;
	    }
	}
    }

    fprintf_logfile("Failed /help \"%s\",\"%s\" file \"%s.txt\"",
	prefix?prefix:"", cmdargs, helptopic);

    if (prefix == 0)
	printf_chat("&WNo help found for topic '%s'", cmdargs);
    else
	printf_chat("&WNo %s file found for '%s'", prefix, cmdargs);
}

/*HELP clear,cls H_CMD
&T/Clear &SClear your chat.
Alias: &T/cls
*/

void
cmd_clear(char * UNUSED(cmd), char * UNUSED(arg))
{
    for(int i = 0; i<30; i++)
	printf_chat(" ");;
    printf_chat("&WCleared");;
}

/*HELP faq H_CMD
&cFAQ&f:
&fExample: What does this server run on?
    This server runs on MCCHost
&fWhere is the source?
    &eSource is at &Thttps://github.com/rdebath/mcchost
*/

/*HELP news H_CMD
No news is good news.
*/
