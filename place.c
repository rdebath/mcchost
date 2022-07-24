#include <stdlib.h>
#include <string.h>

#include "place.h"

int player_mode_paint = 0;
int player_mode_mode = -1;

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
/*HELP mode H_CMD
&T/mode [BlockNo]
Always place &TBlockNo&S rather than your held block.
Useful for hidden block types.
*/
/*HELP mark H_CMD
&T/Mark <x y z>&S - Places a marker for selections, e.g for /z
Use ~ before a coordinate to mark relative to current position
If no coordinates are given, marks at where you are standing
If only x coordinate is given, it is used for y and z too
*/

#if INTERFACE
#define CMD_PLACE  {N"place", &cmd_place}, {N"pl", &cmd_place, .dup=1}, \
                   {N"paint", &cmd_paint}, {N"p", &cmd_paint, .dup=1}, \
                   {N"mode", &cmd_mode}, \
                   {N"abort", &cmd_mode, .dup=1}, {N"a", &cmd_mode, .dup=1}, \
		   {N"mark", &cmd_mark}, {N"m", &cmd_mark, .dup=1}, \
		   {N"ma", &cmd_mark, .dup=1, .nodup=1}, \
                   {N"cuboid", &cmd_cuboid}, {N"z", &cmd_cuboid, .dup=1}

#endif

xyzhv_t marks[3] = {0};
uint8_t player_mark_mode = 0;
char marking_for[NB_SLEN];
cmd_func_t mark_for_cmd = 0;
char * mark_cmd_cmd = 0;
char * mark_cmd_arg = 0;

void
cmd_place(char * cmd, char * arg)
{
    int args[8] = {0};
    int cnt = 0;
    char * ar = arg;
    if (ar)
	for(int i = 0; i<8; i++) {
	    char * p = strtok(ar, " "); ar = 0;
	    if (p == 0) break;
	    if (i == 0) {
		args[i] = block_id(p);
		if (args[i] == BLOCKMAX) {
		    printf_chat("&WUnknown block '%s'", p);
		    return;
		}
	    } else
		args[i] = atoi(p);
	    cnt = i+1;
	}
    if (cnt != 1 && cnt != 4 && cnt != 7) {
	if (cnt == 0) return cmd_help("", cmd);
	printf_chat("&WUsage: /place b [x y z] [X Y Z]");
	return;
    }

    if (!level_block_queue || !level_blocks) return;

    // NB: Place is treated just like the client setblock, including any
    // Grass/Dirt/Slab conversions. The Cuboid call is NOT treated in the
    // same way.
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
	plain_cuboid(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    }
    return;
}

void
cmd_paint(char * UNUSED(cmd), char * UNUSED(arg))
{
    player_mode_paint = !player_mode_paint;

    printf_chat("&SPainting mode: &T%s.", player_mode_paint?"ON":"OFF");
}

void
clear_pending_marks() {
    memset(marks, 0, sizeof(marks));
    if (mark_cmd_cmd) free(mark_cmd_cmd);
    mark_cmd_cmd = 0;
    if (mark_cmd_arg) free(mark_cmd_arg);
    mark_cmd_arg = 0;
    mark_for_cmd = 0;
    *marking_for = 0;
    player_mark_mode = 0;
    show_marks_message();
}

void show_marks_message()
{
    if (!extn_messagetypes) return;

    printf_chat("(13)&f%s", marking_for);
    if (!*marking_for && !marks[0].valid) {
	printf_chat("(12)");
	printf_chat("(11)");
	return;
    }

    if (marks[0].valid)
	printf_chat("(12)&fMark #1: &S(%d,%d,%d)", marks[0].x, marks[0].y, marks[0].z);
    else
	printf_chat("(12)&fMark #1: &S(Waiting)");

    if (marks[0].valid || player_mark_mode >= 2) {
	if (marks[1].valid)
	    printf_chat("(11)&fMark #2: &S(%d,%d,%d)", marks[1].x, marks[1].y, marks[1].z);
	else
	    printf_chat("(11)&fMark #2: &S(Waiting)");
    }
}

void
cmd_mode(char * cmd, char * arg)
{
    char * block = strtok(arg, " ");
    if (cmd && (strcasecmp(cmd, "abort") == 0 || strcasecmp(cmd, "a") == 0)) {
	printf_chat("&SToggles and pending actions cleared.");
	player_mode_mode = -1;
	clear_pending_marks();
	return;
    }

    if (!block) {
	player_mode_mode = -1;
	printf_chat("&SPlayer /mode command turned off");
    } else {
	player_mode_mode = -1;
	block_t b = block_id(block);
	if (b >= BLOCKMAX) {
	    printf_chat("&WUnknown block '%s'", block);
	    return;
	}

	// Classic mode, don't let them place blocks they can't remove.
	if (!server_id_op_flag && !cpe_requested) {
	    if (b==Block_Bedrock) b=Block_Magma;
	    if (b==Block_ActiveLava) b=Block_StillLava;
	    if (b==Block_ActiveWater) b=Block_StillWater;
	}

	player_mode_mode = b;
	printf_chat("&SPlayer /mode set to block &T%s", block_name(player_mode_mode));
    }
}

void
process_player_setblock(pkt_setblock pkt)
{
    if (!level_block_queue || !level_blocks) return; // !!!
    if (level_prop->disallowchange) {
	revert_client(pkt);
	return;
    }

    if (player_mark_mode) {
	int l = sizeof(marks)/sizeof(*marks);
	int v = 0;
	while(v<l && marks[v].valid) v++;
	if (v<l) {
	    marks[v].valid = 1;
	    marks[v].x = pkt.coord.x;
	    marks[v].y = pkt.coord.y;
	    marks[v].z = pkt.coord.z;
	    if (!extn_messagetypes)
		printf_chat("&SMarked position (%d,%d,%d)",
		    pkt.coord.x, pkt.coord.y, pkt.coord.z);
	    player_mark_mode--;
	} else
	    player_mark_mode = 0;

	revert_client(pkt);
	show_marks_message();

	if (player_mark_mode == 0) {
	    if (mark_for_cmd) {
		(*mark_for_cmd)(mark_cmd_cmd, mark_cmd_arg);
		if (!player_mark_mode)
		    clear_pending_marks();
	    }
	}
	return;
    }

    if (player_mode_paint) {
	pkt.block = pkt.heldblock;
	pkt.mode = 2;
    }

    if (player_mode_mode >= 0 && pkt.mode) {
	pkt.block = player_mode_mode;
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

void
cmd_mark(char * cmd, char * arg)
{
    int args[3] = {0};
    int has_offset[3] = {0};
    int cnt = 0;
    char * ar = arg;
    if (strcasecmp(cmd, "ma") == 0 || strcasecmp(ar, "all") == 0) {
	char buf[] = "0";
	cmd_mark("m", buf);
	ar = 0; cnt = 3;
	args[0] = level_prop->cells_x-1;
	args[1] = level_prop->cells_y-1;
	args[2] = level_prop->cells_z-1;
    }
    if (ar)
	for(int i = 0; i<3; i++) {
	    char * p = strtok(ar, " "); ar = 0;
	    if (p == 0) break;
	    if (p[0] == '~') {has_offset[i] = 1; p++;}
	    args[i] = atoi(p);
	    cnt = i+1;
	}

    if (cnt == 0) { cnt = 1; args[0] = 0; has_offset[0] = 1; }
    if (cnt == 1) {
	cnt = 3;
	args[1] = args[2] = args[0];
	has_offset[1] = has_offset[2] = has_offset[0];
    }

    if (cnt != 3) {
	if (cnt == 0) {
	    if (marks[0].valid) {
		if (player_mark_mode) {
		    int l = sizeof(marks)/sizeof(*marks);
		    for(int i = 0; i<l; i++)
			if (marks[i].valid)
			    player_mark_mode++;
		}
		memset(marks, 0, sizeof(marks));
		if (player_mark_mode)
		    printf_chat("&SMarks cleared, please reenter.");
		else
		    printf_chat("&SMarks cleared");
		show_marks_message();
		return;
	    }
	}
	printf_chat("&SUsage: &T/mark [x y z]&S");
	return;
    }

    if (has_offset[0]) args[0] += player_posn.x/32;
    if (has_offset[1]) args[1] += (player_posn.y-51)/32;
    if (has_offset[2]) args[2] += player_posn.z/32;

    int l = sizeof(marks)/sizeof(*marks);
    int v = 0;
    while(v<l && marks[v].valid) v++;
    if (v>=l) { // Too many? Discard oldest
	printf_chat("&SNote: removed mark (%d,%d,%d)", marks[0].x, marks[0].y, marks[0].z);
	for (int i=1; i<l; i++)
	    marks[i-1] = marks[i];
	v--;
    }
    if (v<l) {
	marks[v].valid = 1;
	marks[v].x = args[0];
	marks[v].y = args[1];
	marks[v].z = args[2];
	if (!extn_messagetypes)
	    printf_chat("&SMarked position (%d,%d,%d)", args[0], args[1], args[2]);
    } else
	printf_chat("&WToo many marks");

    show_marks_message();
    if (player_mark_mode) {
	player_mark_mode--;
	if (player_mark_mode == 0) {
	    if (mark_for_cmd) {
		(*mark_for_cmd)(mark_cmd_cmd, mark_cmd_arg);
		if (!player_mark_mode)
		    clear_pending_marks();
	    }
	}
    }
}

void
request_pending_marks(char * why, char * cmd, char * arg)
{
    mark_for_cmd = cmd_cuboid;
    if (mark_cmd_cmd) free(mark_cmd_cmd);
    mark_cmd_cmd = 0;
    if (mark_cmd_arg) free(mark_cmd_arg);
    mark_cmd_arg = 0;
    if (cmd) mark_cmd_cmd = strdup(cmd);
    if (arg) mark_cmd_arg = strdup(arg);
    strncpy(marking_for, why, sizeof(marking_for)-1);
    show_marks_message();
}

void
cmd_cuboid(char * cmd, char * arg)
{
    if (!marks[0].valid || !marks[1].valid) {
	if (!marks[0].valid) {
	    if (!extn_heldblock) {
		block_t b = arg?block_id(arg): player_held_block;
		if (b == (block_t)-1) b = 1;
		printf_chat("&SPlace or break two blocks to determine edges of %s cuboid.", block_name(b));
	    } else if (!extn_messagetypes)
		printf_chat("&SPlace or break two blocks to determine edges.");
	}
	player_mark_mode = 2;
	if (marks[0].valid) player_mark_mode--;
	request_pending_marks("Selecting region for Cuboid", cmd, arg);
	return;
    }

    block_t b = arg?block_id(arg): player_held_block;
    if (b == (block_t)-1) b = 1;

    //TODO: Other cuboids.

    plain_cuboid(b,
	marks[0].x, marks[0].y, marks[0].z,
	marks[1].x, marks[1].y, marks[1].z);

    clear_pending_marks();
}

void
plain_cuboid(block_t b, int x0, int y0, int z0, int x1, int y1, int z1)
{
    int i;
    int max[3] = { level_prop->cells_x-1, level_prop->cells_y-1, level_prop->cells_z-1};
    int args[7] = {0, x0, y0, z0, x1, y1, z1};

    // Swap if inverted.
    for(i=0; i<3; i++) {
	if (args[i+1] > args[i+4]) {
	    int t = args[i+1];
	    args[i+1] = args[i+4];
	    args[i+4] = t;
	}
    }
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
    lock_fn(level_lock);
    my_user.dirty = 1;
    int x, y, z, placecount = 0;
    if (b >= BLOCKMAX) b = BLOCKMAX-1;
    for(y=args[2]; y<=args[5]; y++)
	for(x=args[1]; x<=args[4]; x++)
	    for(z=args[3]; z<=args[6]; z++)
	    {
		uintptr_t index = World_Pack(x, y, z);
		if (level_blocks[index] == b) continue;
		placecount++;
		level_blocks[index] = b;
		prelocked_update(x, y, z, b);
	    }
    if (b) my_user.blocks_drawn += placecount;
    else my_user.blocks_deleted += placecount;
    unlock_fn(level_lock);

    // NB: Slab processing is wrong for multi-layer cuboid.
    // Grass/dirt processing might be reasonable to convert grass->dirt
    // but dirt->grass depends on "nearby" grass so the "correct" flow
    // is not well defined.
}
