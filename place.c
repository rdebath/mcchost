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
    pkt.mode = (pkt.heldblock != Block_Air);
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

	pkt.heldblock = args[0];
	pkt.mode = (pkt.heldblock != Block_Air);
	pkt.block = pkt.mode?pkt.heldblock:Block_Air;
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    pkt.coord.x = x;
		    pkt.coord.y = y;
		    pkt.coord.z = z;
		    update_block(pkt);
		}
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
    if (player_mode_paint) {
	pkt.block = pkt.heldblock;
	pkt.mode = 2;
    }

    update_block(pkt);
}
