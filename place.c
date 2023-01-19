
#include "place.h"

/*HELP place,pl H_CMD
&T/place b [x y z] [X Y Z]
Places the Block &Tb&S at your feet or at &T[x y z]&S
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
/*HELP abort,a H_CMD
&T/Abort
Turns off modes, toggles and pending operations.
Alias: &T/a
*/
/*HELP cuboid,z H_CMD
&T/cuboid [mode] [block]
Modes: solid/hollow/walls/wire
Draws a cuboid between two points
Alias: &T/z
*/
/*HELP about,b H_CMD
&T/about
Shows information about a block location
Alias: &T/b
*/

#if INTERFACE
#define CMD_PLACE  {N"place", &cmd_place}, {N"pl", &cmd_place, .dup=1}, \
                   {N"paint", &cmd_paint}, {N"p", &cmd_paint, .dup=1}, \
                   {N"mode", &cmd_mode}, \
                   {N"abort", &cmd_abort}, {N"a", &cmd_abort, .dup=1}, \
		   {N"mark", &cmd_mark}, {N"m", &cmd_mark, .dup=1}, \
		   {N"markall", &cmd_mark, .dup=1, .nodup=1}, \
		   {N"ma", &cmd_mark, .dup=1}, \
                   {N"about", &cmd_about}, {N"b", &cmd_about, .dup=1}, \
                   {N"cuboid", &cmd_cuboid}, \
		   {N"z", &cmd_cuboid, .dup=1}
#endif

int player_mode_paint = 0;
int player_mode_mode = -1;

xyzhv_t marks[3] = {0};
uint8_t player_mark_mode = 0;
char marking_for[NB_SLEN];
cmd_func_t mark_for_cmd = 0;
char * mark_cmd_cmd = 0;
char * mark_cmd_arg = 0;

int
can_place_block(block_t blk)
{
    if (level_prop->disallowchange || blk >= BLOCKMAX)
	return 0;
    if (server->cpe_disabled && (blk == Block_Grass || blk == Block_DoubleSlab || blk >= Block_CP))
        return 0;
    if ((level_prop->blockdef[blk].block_perm&1) != 0)
        return 0;
    return 1;
}

static inline int
can_delete_block(block_t blk)
{
    if ((level_prop->blockdef[blk].block_perm&2) != 0)
        return 0;
    return 1;
}

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
		if (args[i] == BLOCKNIL) {
		    printf_chat("&WUnknown block '%s'", p?p:"");
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
    if (level_prop->disallowchange) { printf_chat("&WThis level cannot be changed"); return; }
    if (!can_place_block(args[0])) { printf_chat("&WYou cannot place block %s", block_name(args[0])); return; }

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
    } else if(cnt == 4) {
	pkt.coord.x = args[1];
	pkt.coord.y = args[2];
	pkt.coord.z = args[3];
    } else {
	do_cuboid(args[0], 0, args[1], args[2], args[3], args[4], args[5], args[6]);
	return;
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
	printf_chat("&WToo many set-blocks received, suspected griefing.");
	return;
    }

    update_block(pkt);
    return;
}

void
cmd_abort(char * UNUSED(cmd), char * UNUSED(arg))
{
    printf_chat("&SToggles and pending actions cleared.");
    player_mode_mode = -1;
    player_mode_paint = 0;
    clear_pending_marks();
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

void
fetch_pending_marks(xyzhv_t smarks[3]) {
    memcpy(smarks, marks, sizeof(marks));
    memset(marks, 0, sizeof(marks));
    if (mark_cmd_cmd) free(mark_cmd_cmd);
    mark_cmd_cmd = 0;
    if (mark_cmd_arg) free(mark_cmd_arg);
    mark_cmd_arg = 0;
    mark_for_cmd = 0;
    *marking_for = 0;
    player_mark_mode = 0;
    show_marks_message();
    flush_to_remote();
}

void show_marks_message()
{
    send_block_permission();

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
	player_mode_paint = 0;
	clear_pending_marks();
	return;
    }

    if (!block) {
	player_mode_mode = -1;
	printf_chat("&SPlayer /mode command turned off");
    } else {
	player_mode_mode = -1;
	block_t b = block_id(block);
	if (b == BLOCKNIL) {
	    printf_chat("&WUnknown block '%s'", block?block:"");
	    return;
	}

	if (!can_place_block(b)) { printf_chat("&WYou cannot place block %s", block_name(b)); return; }

	player_mode_mode = b;
	printf_chat("&SPlayer /mode set to block &T%s", block_name(player_mode_mode));
    }
}

void
process_player_setblock(pkt_setblock pkt)
{
    if (!level_block_queue || !level_blocks || !user_authenticated)
	{ revert_client(pkt); return; }

    int do_revert = 0;

    if (!do_revert && player_lockout>0) {
	do_revert = 1;
	player_lockout = 50;
    }

    if (!do_revert) {
	uint64_t range = 0, r, ok_reach;
	r = (pkt.coord.x*32 - player_posn.x); range += r*r;
	r = (pkt.coord.y*32 - player_posn.y); range += r*r;
	r = (pkt.coord.z*32 - player_posn.z); range += r*r;
	ok_reach = level_prop->click_distance;
	if (my_user.click_distance > 0) ok_reach = my_user.click_distance;
	if (ok_reach == 0) do_revert = 1;
	else if (ok_reach > 0) {
	    if (ok_reach > 5*32) ok_reach += 4*32; else ok_reach += 2*32;
	    ok_reach *= ok_reach;
	    if (range >= ok_reach) {
		printf_chat("You can't build that far away.");
		do_revert = 1;
	    }
	}
    }

    if (pkt.coord.x < 0 || pkt.coord.x >= level_prop->cells_x) do_revert = 1;
    if (pkt.coord.y < 0 || pkt.coord.y >= level_prop->cells_y) do_revert = 1;
    if (pkt.coord.z < 0 || pkt.coord.z >= level_prop->cells_z) do_revert = 1;

    if (!do_revert && player_mark_mode) {
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

    // Classic mode can't place Grass.
    if (!cpe_enabled && pkt.block == Block_Grass)
	pkt.block = Block_Dirt;

    if (player_mode_mode >= 0 && pkt.mode) {
	pkt.block = player_mode_mode;
	pkt.mode = 2;
    }

    if (!do_revert) do_revert = !can_place_block(pkt.block);

    if (add_antispam_event(player_block_spam, server->block_spam_count, server->block_spam_interval, 0)) {
	my_user.kick_count++;
	logout("Kicked for suspected griefing.");
    }

    if (!level_block_queue || !level_blocks) return; // !!!

    if (do_revert) { revert_client(pkt); return; }

    block_t b = level_blocks[World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z)];
    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b)) {
	revert_client(pkt);
	return;
    }

    update_block(pkt);
}

void
revert_client(pkt_setblock pkt)
{
    block_t b;
    if (!level_prop || !level_block_queue || !level_blocks ||
        pkt.coord.x < 0 || pkt.coord.y < 0 || pkt.coord.z < 0 ||
	    pkt.coord.x >= level_prop->cells_x ||
	    pkt.coord.y >= level_prop->cells_y ||
	    pkt.coord.z >= level_prop->cells_z)
    {
	int y = pkt.coord.y;
	xyz_t mapsz;
	if (level_prop)
	    mapsz = (xyz_t){level_prop->cells_x, level_prop->cells_y, level_prop->cells_z};
	else mapsz = void_map_size;

	if (y<0)
	    b = Block_Bedrock;
	else if (y >= mapsz.y/2)
	    b = Block_Air;
	else if (y+2 >= mapsz.y/2 && (pkt.coord.x >= mapsz.x || pkt.coord.z >= mapsz.z))
	    b = Block_ActiveWater;
	else
	    b = Block_Bedrock;
    } else {
	uintptr_t index = World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z);
	b = level_blocks[index];
    }

    send_setblock_pkt(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
}

void
cmd_mark(char * cmd, char * arg)
{
    int args[3] = {0};
    int has_offset[3] = {0};
    int cnt = 0;
    char * ar = arg?arg:"";
    if (strcasecmp(cmd, "markall") == 0 || strcasecmp(ar, "all") == 0) {
	char buf[] = "0";
	cmd_mark("m", buf);
	ar = 0; cnt = 3;
	if (level_prop) {
	    args[0] = level_prop->cells_x-1;
	    args[1] = level_prop->cells_y-1;
	    args[2] = level_prop->cells_z-1;
	}
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
request_pending_marks(char * why, cmd_func_t cmd_fn, char * cmd, char * arg)
{
    mark_for_cmd = cmd_fn;
    if (mark_cmd_cmd) free(mark_cmd_cmd);
    mark_cmd_cmd = 0;
    if (mark_cmd_arg) free(mark_cmd_arg);
    mark_cmd_arg = 0;
    if (cmd) mark_cmd_cmd = strdup(cmd);
    if (arg) mark_cmd_arg = strdup(arg);
    strncpy(marking_for, why, sizeof(marking_for)-1);
    show_marks_message();
}

int
clamp_cuboid(int * args, uint64_t * block_count)
{
    int i;
    int max[3] = { level_prop->cells_x-1, level_prop->cells_y-1, level_prop->cells_z-1};

    // Swap if inverted.
    for(i=0; i<3; i++) {
	if (args[i+1] > args[i+4]) {
	    int t = args[i+1];
	    args[i+1] = args[i+4];
	    args[i+4] = t;
	}
    }
    // Off the map ?
    for(i=0; i<3; i++) {
	if (args[i+1]<0 && args[i+4]<0) return 0;
	if (args[i+1]>max[i] && args[i+4]>max[i]) return 0;
    }
    // Crop to map.
    for(i=0; i<6; i++) {
	if (args[i+1]<0) args[i+1] = 0;
	if (args[i+1]>max[i%3]) args[i+1] = max[i%3];
    }
    for(i=0; i<3; i++) {
	if (args[i+4] < args[i+1])
	    {int t=args[i+1]; args[i+1]=args[i+4]; args[i+4]=t; }
    }
    if (block_count) {
	uint64_t c = 1;
	for(i=0; i<3; i++)
	    c *= (args[i+4]-args[i+1]+1);
	*block_count = c;
    }
    return 1;
}

void
cmd_about(char * cmd, char * arg)
{
    if (!marks[0].valid) {
	if (!extn_messagetypes)
	    printf_chat("&SPlace or break a blocks to show information");
	player_mark_mode = 1;
	request_pending_marks("Selecting location for block info", cmd_about, cmd, arg);
	return;
    }

    if (!level_prop ||
	marks[0].x < 0 || marks[0].x >= level_prop->cells_x ||
        marks[0].y < 0 || marks[0].y >= level_prop->cells_y ||
        marks[0].z < 0 || marks[0].z >= level_prop->cells_z) {
	printf_chat("&SLocation outside of map area");
	clear_pending_marks();
	return;
    }

    if (!level_prop || !level_block_queue || !level_blocks) return;

    int x = marks[0].x, y = marks[0].y, z = marks[0].z;
    clear_pending_marks();

    uintptr_t index = World_Pack(x, y, z);
    block_t b = level_blocks[index];

    printf_chat("&SBlock (%d, %d, %d): %d = %s.", x, y, z, b, block_name(b));
}

char * cuboids[] = { "solid", "hollow", "walls", "wire", 0 };

void
cmd_cuboid(char * cmd, char * arg)
{
    if (!level_prop || level_prop->disallowchange) { printf_chat("&WLevel cannot be changed"); return; }

    char * p = strchr(arg, ' ');
    if (!p) p = arg+strlen(arg);
    int ctype = 0;
    char * blkname = arg;
    for(int i = 0; cuboids[i]; i++)
	if (strncasecmp(cuboids[i], arg, p-arg) == 0) {
	    ctype = i;
	    blkname = p;
	    break;
	}
    while(*blkname == ' ') blkname++;

    block_t b = block_id(blkname);
    if (b == BLOCKNIL) {
	printf_chat("&WUnknown block '%s'", blkname);
	return;
    }

    if (!marks[0].valid || !marks[1].valid) {
	if (!marks[0].valid) {
	    if (!extn_heldblock) {
		printf_chat("&SPlace or break two blocks to determine edges of %s cuboid.",
		    block_name(block_id(arg)));
	    } else if (!extn_messagetypes)
		printf_chat("&SPlace or break two blocks to determine edges.");
	}
	player_mark_mode = 2;
	if (marks[0].valid) player_mark_mode--;
	request_pending_marks("Selecting region for Cuboid", cmd_cuboid, cmd, arg);
	return;
    }

    if (!can_place_block(b)) {
	printf_chat("&WYou cannot place block %s", block_name(b));
	return;
    }

    //TODO: Other cuboids.
    xyzhv_t smarks[3];
    fetch_pending_marks(smarks);

    do_cuboid(b, ctype,
	smarks[0].x, smarks[0].y, smarks[0].z,
	smarks[1].x, smarks[1].y, smarks[1].z);
}

void
do_cuboid(block_t b, int ctype, int x0, int y0, int z0, int x1, int y1, int z1)
{
    int args[7] = {0, x0, y0, z0, x1, y1, z1};

    int64_t blk_count;
    if (clamp_cuboid(args, &blk_count) == 0) return;
    if (ctype != 0) {
	int x = args[4]-args[1]+1,
	    y = args[5]-args[2]+1,
	    z = args[6]-args[3]+1;
	switch(ctype) {
	case 1: // Hollow
	    if (x>2 && y>2 && z>2)
		blk_count -= (int64_t)(x-2)*(y-2)*(z-2);
	    else
		ctype = 0;
	    break;
	case 2: // Walls
	    if (x>2 && z>2)
		blk_count -= (int64_t)(x-2)*y*(z-2);
	    else
		ctype = 0;
	    break;
	case 3: // Wire
	    blk_count = x*4+y+4+z*4-8;
	    break;
	}
    }
    if (!perm_block_check(blk_count)) {
	printf_chat("&WToo many blocks %jd>%jd", (intmax_t)blk_count, (intmax_t)command_limits.max_user_blocks);
	return;
    }

    // Cuboid does not "adjust" blocks so call
    // send_update(), not update_block().
    //
    // Lock may be very slow, so update all the blocks
    // in one run using unlocked_update().
    //
    // Too large and this will trigger a /reload as it'll
    // run too fast so buzzing the lock isn't needed
    //
    // NB: Slab processing is wrong for multi-layer cuboid.
    // Grass/dirt processing might be reasonable to convert grass->dirt
    // but dirt->grass depends on "nearby" grass so the "correct" flow
    // is not well defined.
    int x, y, z, placecount = 0;
    if (b >= BLOCKMAX) b = BLOCKMAX-1;
    my_user.dirty = 1;
    lock_fn(level_lock);
    check_block_queue(0);

    switch(ctype)
    {
    case 0: // Full cuboid
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    uintptr_t index = World_Pack(x, y, z);
		    if (level_blocks[index] == b) continue;
		    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b))
			continue;
		    placecount++;
		    level_blocks[index] = b;
		    prelocked_update(x, y, z, b);
		}
	break;
    case 1: // Hollow and
    case 2: // Walls styles
	for(y=args[2]; y<=args[5]; y++)
	{
	    int fy = (y==args[2] || y==args[5]) && ctype == 1;
	    for(x=args[1]; x<=args[4]; x++)
	    {
		int fx = (x==args[1] || x==args[4]);
		for(z=args[3]; z<=args[6]; z++)
		{
		    int fz = (z==args[3] || z==args[6]);
		    if (!fy && !fx && !fz && z != args[6]) {
			z = args[6]-1;
			continue;
		    }

		    uintptr_t index = World_Pack(x, y, z);
		    if (level_blocks[index] == b) continue;
		    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b))
			continue;
		    placecount++;
		    level_blocks[index] = b;
		    prelocked_update(x, y, z, b);
		}
	    }
	}
	break;
    case 3: // Just the edges (wire)
	for(y=args[2]; y<=args[5]; y++)
	{
	    int fy = (y==args[2] || y==args[5]);
	    for(x=args[1]; x<=args[4]; x++)
	    {
		int fx = (x==args[1] || x==args[4]);
		if (fx+fy < 1 && x != args[4]) {
		    x = args[4]-1;
		    continue;
		}
		for(z=args[3]; z<=args[6]; z++)
		{
		    int fz = (z==args[3] || z==args[6]);
		    if (fx+fy+fz < 2 && z != args[6]) {
			z = args[6]-1;
			continue;
		    }

		    uintptr_t index = World_Pack(x, y, z);
		    if (level_blocks[index] == b) continue;
		    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b))
			continue;
		    placecount++;
		    level_blocks[index] = b;
		    prelocked_update(x, y, z, b);
		}
	    }
	}
	break;
    }
    my_user.blocks_drawn += placecount;
    unlock_fn(level_lock);
}
