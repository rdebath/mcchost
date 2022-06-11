#include <stdlib.h>
#include <string.h>

#include "place.h"

int player_mode_paint = 0;

/*HELP place,pl H_CMD
&T/place b [x y z] [X Y Z]
Places the Block numbered &Tb&S at your feet or at &T[x y z]&S
With both &T[x y z]&S and &T[X Y Z]&S it places a
cuboid between those points.
Alias: &T/pl
*/
/*HELP paint,p H_CMD
&T/paint
When paint mode is on your held block replaces any deleted block.
Alias: &T/p
*/

#if INTERFACE
#define CMD_PLACE  {N"place", &cmd_place}, {N"pl", &cmd_place, .dup=1}, \
                   {N"paint", &cmd_paint}, {N"p", &cmd_paint, .dup=1}
#endif
void
cmd_place(UNUSED char * cmd, char * arg)
{
    int args[8] = {0};
    int cnt = 0;
    char * ar = arg;
    if (ar)
	for(int i = 0; i<8; i++) {
	    char * p = strtok(ar, " "); ar = 0;
	    if (p == 0) break;
	    // if (i == 0) check block name; else
	    // check for relative x/y/z posn
	    args[i] = atoi(p);
	    cnt = i+1;
	}
    if (cnt != 1 && cnt != 4 && cnt != 7) {
	printf_chat("&WUsage: /place b [x y z] [X Y Z]");
	return;
    }

    pkt_setblock pkt;
    pkt.heldblock = args[0];
    pkt.mode = 3;
    pkt.block = pkt.mode?pkt.heldblock:Block_Air;
    if (cnt == 1) {
	pkt.coord.x = player_posn.x/32;	// check range [0..cells_x)
	pkt.coord.y = (player_posn.y+16)/32;// Add half a block for slabs
	pkt.coord.z = player_posn.z/32;
	update_block(pkt);
    } else if(cnt == 4) {
	pkt.coord.x = args[1];
	pkt.coord.y = args[2];
	pkt.coord.z = args[3];
	update_block(pkt);
    } else {
	int i, x, y, z;
	int max[3] = { level_prop->cells_x-1, level_prop->cells_y-1, level_prop->cells_z-1};
	for(i=0; i<3; i++) {
	    if (args[i+1]<0 && args[i+4]<0) return; // Off the map.
	    if (args[i+1]>max[i] && args[i+4]>max[i]) return; // Off the map.
	}
	for(i=0; i<6; i++) { // Crop to map.
	    if (args[i+1]<0) args[i+1] = 0;
	    if (args[i+1]>max[i%3]) args[i+1] = max[i%3];
	}
	for(i=0; i<3; i++) {
	    if (args[i+4] < args[i+1])
		{int t=args[i+1]; args[i+1]=args[i+4]; args[i+4]=t; }
	}

	// Cuboid does not "adjust" blocks so call
	// send_update(), not update_block().
	//
	// Lock may be very slow, so update all the blocks
	// in one run using unlocked_update().
	//
	// Too large and this will trigger a /reload as it'll
	// run too fast so buzzing the lock isn't needed
	lock_shared();
	block_t b = args[0];
	if (b >= BLOCKMAX) b = BLOCKMAX-1;
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    uintptr_t index = World_Pack(x, y, z);
		    if (level_blocks[index] == b) continue;
		    level_blocks[index] = b;
		    unlocked_update(x, y, z, b);
		}
	unlock_shared();
    }
    return;
}

void
cmd_paint(UNUSED char * cmd, UNUSED char * arg)
{
    player_mode_paint = !player_mode_paint;

    printf_chat("&SPainting mode: &T%s.", player_mode_paint?"ON":"OFF");
}

void
process_player_setblock(pkt_setblock pkt)
{
    if (level_prop->readonly) {
	revert_client(pkt);
	return;
    }

    if (player_mode_paint) {
	pkt.block = pkt.heldblock;
	pkt.mode = 2;
    }

    update_block(pkt);
}

void
revert_client(pkt_setblock pkt)
{
    if (!level_block_queue || !level_blocks) return; // !!!
    if (pkt.coord.x < 0 || pkt.coord.x >= level_prop->cells_x) return;
    if (pkt.coord.y < 0 || pkt.coord.y >= level_prop->cells_y) return;
    if (pkt.coord.z < 0 || pkt.coord.z >= level_prop->cells_z) return;

    uintptr_t index = World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z);

    block_t b = level_blocks[index];

    send_setblock_pkt(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
}

