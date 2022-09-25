#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "textcolour.h"

#if INTERFACE
typedef struct textcolour_t textcolour_t;
struct textcolour_t {
    nbtstr_t name;
    int32_t colour;
    uint8_t a;
    char fallback;
    uint8_t defined;
};

#define CMD_TEXTCOLOUR \
    {N"ccols", &cmd_textcolour}, \
    {N"CustomColors", &cmd_textcolour, .dup=1}, \
    {N"CustomColours", &cmd_textcolour, .dup=1}

#endif

textcolour_t textcolour[256];

/*HELP ccols,customcolours,customcolors H_CMD
&T/CustomColors add [code] [name] [fallback] #[hex]
  code is a single character.
  fallback is the color code shown to non-supporting clients.
&T/CustomColors remove [code] &S- Removes that custom color.
*/

void
cmd_textcolour(char * cmd, char *arg)
{
    char * subcmd = strtok(arg, " ");
    char * txcode = strtok(0, " ");
    char * txname = strtok(0, " ");
    char * txfallback = strtok(0, " ");
    char * txcolour = strtok(0, " ");

    if (!subcmd) return cmd_help("", cmd);

    if (!client_trusted) {
	printf_chat("&WPermission denied, need to be admin.");
	return;
    }

    if (strcasecmp(subcmd, "remove") == 0 || strcasecmp(subcmd, "rm") == 0 ||
	    strcasecmp(subcmd, "del") == 0) {
	if (!txcode || !*txcode) return cmd_help("", cmd);
	uint8_t c = *txcode;
	if (txcode[1] != 0) c = 0;
	if (c==0 || !textcolour[c].defined) {
	    printf_chat("&SText code %s is not defined", txcode);
	    return;
	}
	textcolour[c].defined = 0;
    } else if (strcasecmp(subcmd, "add") == 0) {
	if (!txcode || !txname || !txfallback || !txcolour) return cmd_help("", cmd);
	uint8_t c = *txcode;
	if (txcode[1] != 0) c = 0;
	if (c == '%' || c == '&' || c == ' ') c = 0;
	if (c==0) {
	    printf_chat("&SText code %s can not be defined", txcode);
	    return;
	}

	printf_chat("Text code %s %sdefined.", txcode,
	    textcolour[c].defined?"re":"");
	textcolour[c].defined = 1;
	snprintf(textcolour[c].name.c, MB_STRLEN, "%s", txname);
	textcolour[c].a = 255;
	textcolour[c].fallback = *txfallback;
	int base = 0;
	if (*txcolour == '#') { base = 16; txcolour++; }
	textcolour[c].colour = strtol(txcolour, 0, base);
    } else
	return cmd_help("", cmd);

    send_textcolours();
    save_system_ini_file();
}

void
init_textcolours() {
    for (int i = 0; i<256; i++) {
	textcolour[i].defined = 0;
	textcolour[i].name.c[0] = 0;
	textcolour[i].fallback = 'f';
	textcolour[i].colour = -1;
	textcolour[i].a = 255;
    }

    set_alias('H', 'e'); // HelpDescriptionColor
    set_alias('I', '5'); // IRCColor
    set_alias('S', 'e'); // DefaultColor
    set_alias('T', 'a'); // HelpSyntaxColor
    set_alias('W', 'c'); // WarningErrorColor

    for(int i = 'A'; i<='F'; i++)
	set_alias(i, i-'A'+'a');
}

void
set_alias(int from, int to)
{
    if (from > 255 || from < 0) return;
    textcolour[from].name.c[0] = 0;
    textcolour[from].defined = 1;
    textcolour[from].fallback = to;
    textcolour[from].colour = -1;
    textcolour[from].a = -1;
}

void
send_textcolours()
{
    if (!extn_textcolours) return;

    for(int i = 1; i<255; i++) {
	if (i == '%' || i == '&' || i == ' ') continue;
	if (textcolour[i].defined && textcolour[i].colour != -1)
	    send_settextcolour_pkt(i, textcolour[i].colour, textcolour[i].a);
    }
}
