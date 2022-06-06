#include <stdlib.h>
#include <string.h>

#include "sethotbar.h"

/*HELP sethotbar,chb H_CMD
&T/sethotbar Slot [Block]
Alias &T/shb
&T/chb &S -- Clear hotbar
*/

#if INTERFACE
#define CMD_SHB \
    {N"sethotbar", &cmd_sethotbar}, {N"shb", &cmd_sethotbar, .dup=1}, \
    {N"chb", &cmd_sethotbar, .dup=1}
#endif

void
cmd_sethotbar(char * cmd, char * arg)
{
    if (!extn_sethotbar) {
	printf_chat("Your client does not support the command \"%s\"", cmd);
	return;
    }

    block_t block = Block_Air;

    if (strcasecmp("chb", cmd) == 0) {
	for(int id = 0; id<9; id++)
	    send_sethotbar_pkt(id, block);
	return;
    }

    char * arg1 = strtok(arg, " ");
    char * arg2 = strtok(0, " ");
    if (arg1 == 0) {
	cmd_help(0, cmd);
	return;
    }
    int slot = atoi(arg1);
    if (arg2)
	block = atoi(arg2);

    send_sethotbar_pkt(slot, block);
}

