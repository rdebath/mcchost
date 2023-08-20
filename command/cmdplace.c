
#include "cmdplace.h"

#if INTERFACE
#define UCMD_PLACE \
  {N"place", &cmd_place, CMD_HELPARG}, {N"pl", &cmd_place, CMD_ALIAS}
#endif

/*HELP place,pl H_CMD
&T/place [b] [x y z]
Places the Block &Tb&S at your feet or at &T[x y z]&S
Alias: &T/pl
*/

void
cmd_place(char * UNUSED(cmd), char * arg)
{
    int args[8] = {0};
    int cnt = 0;
    char * ar = arg;
    if (ar)
	for(int i = 0; i<8; i++) {
	    char * p = strarg(ar); ar = 0;
	    if (p == 0) break;
	    if (i == 0) {
		args[i] = block_id(p);
		if (args[i] == BLOCKNIL) {
		    printf_chat("&WUnknown block '%s'", p?p:"");
		    return;
		}
	    } else
		args[i] = atoi(p);
	    cnt = i+1;
	}
    if (cnt != 1 && cnt != 3 && cnt != 4) {
	printf_chat("&WUsage: /place [b] [x y z]");
	return;
    }
    if (cnt == 3) {
	args[3] = args[2]; args[2] = args[1]; args[1] = args[0];
	args[0] = player_held_block;
	if (args[0] >= BLOCKMAX) args[0] = 1;
	cnt++;
    } else if (cnt == 0) {
	args[cnt++] = player_held_block;
	if (args[0] >= BLOCKMAX) args[0] = 1;
    }

    if (!level_block_queue || !level_blocks) return;
    if (level_prop->disallowchange) { printf_chat("&WThis level cannot be changed"); return; }
    if (complain_bad_block(args[0])) return;

    // NB: Place is treated just like the client setblock, including any
    // Grass/Dirt/Slab conversions.
    pkt_setblock pkt;
    pkt.heldblock = args[0];
    pkt.mode = 3;
    pkt.block = pkt.mode?pkt.heldblock:Block_Air;
    if (cnt == 1) {
	pkt.coord.x = player_posn.x/32;	// check range [0..cells_x)
	pkt.coord.y = (player_posn.y+16)/32;// Add half a block for slabs
	pkt.coord.z = player_posn.z/32;
    } else if(cnt == 4) {
	pkt.coord.x = args[1];
	pkt.coord.y = args[2];
	pkt.coord.z = args[3];
    }

    if (!level_block_queue || !level_blocks) return; // !!!
    if (pkt.coord.x < 0 || pkt.coord.x >= level_prop->cells_x) return;
    if (pkt.coord.y < 0 || pkt.coord.y >= level_prop->cells_y) return;
    if (pkt.coord.z < 0 || pkt.coord.z >= level_prop->cells_z) return;

    block_t b = level_blocks[World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z)];
    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b)) {
	printf_chat("&WYou cannot change the block at location (%d,%d,%d)",
	    pkt.coord.x, pkt.coord.y, pkt.coord.z);
	return;
    }

    // Note: Specifically counting this as both a command and a "setblock" so
    // that there's no advantage to the client sending /place commands as well
    // as setblock packets.
    if (add_antispam_event(player_block_spam, server->block_spam_count, server->block_spam_interval, 0)) {
	print_player_spam_msg("&WYou're changing blocks too fast, slow down.");
	return;
    }

    update_block(pkt);
    return;
}
