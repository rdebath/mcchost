
#include "cmdsethotbar.h"

/*HELP sethotbar,chb,shb H_CMD
&T/sethotbar [Slot] [Block]
Alias &T/shb
&T/chb &S -- Clear hotbar
*/

#if INTERFACE
#define UCMD_SHB \
    {N"sethotbar", &cmd_sethotbar}, {N"shb", &cmd_sethotbar, CMD_ALIAS}, \
    {N"clearhotbar", &cmd_sethotbar, CMD_ALIAS, .nodup=1}, \
    {N"chb", &cmd_sethotbar, CMD_ALIAS}
#endif

void
cmd_sethotbar(char * cmd, char * arg)
{
    if (!cpe_support(cmd, extn_sethotbar)) return;

    block_t block = Block_Air;
    reset_hotbar_on_mapload = 0;

    if (!level_prop || strcasecmp("clearhotbar", cmd) == 0) {
	for(int id = 0; id<9; id++)
	    send_sethotbar_pkt(id, block);
	return;
    }

    char * arg1 = strarg(arg);
    char * arg2 = strarg(0);
    if (arg1 == 0) { reset_hotbar(); return; }
    int slot = atoi(arg1);
    if (slot < 1 || slot > 9) {
	printf_chat("Hotbar slot number should be 1..9");
	return;
    }
    if (arg2)
	block = block_id(arg2);

    if (block == (block_t)-1) {
	printf_chat("Block %s is unknown", arg2?arg2:"");
	return;
    }
    send_sethotbar_pkt(slot-1, block);
}

