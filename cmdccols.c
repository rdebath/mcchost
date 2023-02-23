
#include "cmdccols.h"

#if INTERFACE
#define CMD_TEXTCOLOUR \
    {N"ccols", &cmd_textcolour, CMD_PERM_ADMIN, CMD_HELPARG}, \
    {N"CustomColors", &cmd_textcolour, .dup=1}, \
    {N"CustomColours", &cmd_textcolour, .dup=1}

#endif

/*HELP ccols,customcolours,customcolors H_CMD
&T/CustomColors add [code] [name] [fallback] #[hex]
  code is a single character.
  fallback is the color code shown to non-supporting clients.
&T/CustomColors remove [code] &S- Removes that custom color.
*/

void
cmd_textcolour(char * UNUSED(cmd), char *arg)
{
    char * subcmd = strtok(arg, " ");
    char * txcode = strtok(0, " ");
    char * txname = strtok(0, " ");
    char * txfallback = strtok(0, " ");
    char * txcolour = strtok(0, " ");

    if (strcasecmp(subcmd, "remove") == 0 || strcasecmp(subcmd, "rm") == 0 ||
	    strcasecmp(subcmd, "del") == 0) {
	if (!txcode || !*txcode) {
	    printf_chat("&SMissing argument to remove");
	    return;
	}
	uint8_t c = *txcode;
	if (txcode[1] != 0) c = 0;
	if (c==0 || !textcolour[c].defined) {
	    printf_chat("&SText code %s is not defined", txcode);
	    return;
	}
	textcolour[c].defined = 0;
    } else if (strcasecmp(subcmd, "add") == 0) {
	if (!txcode || !txname || !txfallback || !txcolour) {
	    printf_chat("&SMissing argument to add");
	    return;
	}
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
    } else {
	printf_chat("&SUse add or remove subcommand");
	return;
    }

    send_textcolours();
    save_system_ini_file();
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
