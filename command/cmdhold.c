
#include "cmdhold.h"

#if INTERFACE
#define UCMD_HOLD \
		   {N"hold", &cmd_hold, CMD_HELPARG}, {N"holdthis", &cmd_hold, CMD_ALIAS}
#endif

/*HELP hold,holdthis H_CMD
&T/hold
Shows information about a block location
Alias: &T/holdthis
*/

void
cmd_hold(char * UNUSED(cmd), char * arg)
{
    char * block = strarg(arg);
    char * flag = strarg(0);

    block_t b = block_id(block);
    if (b == BLOCKNIL) {
	printf_chat("&WUnknown block '%s'", block?block:"");
	return;
    }
    if (complain_bad_block(b)) return;
    int f = flag?ini_read_bool(flag):0;

    player_held_block = b;

    if (!extn_heldblock) {
	printf_chat("&SYour held block for &T/place&S is &T%s", block_name(b));
	return;
    }

    if (b >= client_block_limit) {
	printf_chat("&SYour client doesn't support block &T%s.", block_name(b));
	return;
    }

    printf_chat("&SSet your held block to &T%s", block_name(b));
    send_holdthis_pkt(b, f);
}
