
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
/*HELP about,b H_CMD
&T/about
Shows information about a block location
Alias: &T/b
*/

#if INTERFACE
#define UCMD_PLACE  {N"place", &cmd_place, CMD_HELPARG}, {N"pl", &cmd_place, CMD_ALIAS}, \
		   {N"paint", &cmd_paint}, {N"p", &cmd_paint, CMD_ALIAS}, \
		   {N"mode", &cmd_mode}, \
		   {N"abort", &cmd_abort}, {N"a", &cmd_abort, CMD_ALIAS}, \
		   {N"mark", &cmd_mark}, {N"m", &cmd_mark, CMD_ALIAS}, \
		   {N"markall", &cmd_mark, CMD_ALIAS, .nodup=1}, \
		   {N"ma", &cmd_mark, CMD_ALIAS}, \
		   {N"mark-all", &cmd_mark, CMD_ALIAS}, \
		   {N"about", &cmd_about}, {N"b", &cmd_about, CMD_ALIAS}
#endif

int player_mode_paint = 0;
int player_mode_mode = -1;
time_t player_block_message_time = 0;

xyzhv_t marks[3] = {0};
uint8_t player_mark_mode = 0;
char marking_for[NB_SLEN];
cmd_func_t mark_for_cmd = 0;
char * mark_cmd_cmd = 0;
char * mark_cmd_arg = 0;

int
can_place_block(block_t blk)
{
    if (!level_prop || level_prop->disallowchange || blk >= BLOCKMAX)
	return 0;
    if ((level_prop->blockdef[blk].block_perm&1) != 0) {
	if (perm_level_check(0, 0, 1))
	    return 1;
        return 0;
    }
    if (server->cpe_disabled) {
	if (blk == Block_Grass || blk == Block_DoubleSlab || blk >= Block_CP)
	    return 0;
	if (!server->op_flag && !perm_is_admin() &&
	    (blk == Block_Bedrock || blk == Block_ActiveWater || blk == Block_ActiveLava))
	    return 0;
    }
    return 1;
}

#if INTERFACE
inline static int
can_delete_block(block_t blk)
{
    if (level_prop->disallowchange)
	return 0;
    if ((level_prop->blockdef[blk].block_perm&2) != 0) {
	if (perm_level_check(0, 0, 1))
	    return 1;
        return 0;
    }
    if (server->cpe_disabled) {
	if (!server->op_flag && !perm_is_admin() && blk == Block_Bedrock)
	    return 0;
    }
    return 1;
}
#endif

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
    if (cnt != 1 && cnt != 4) {
	printf_chat("&WUsage: /place b [x y z]");
	return;
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

#if INTERFACE
#define fmt_spam_msg_w __attribute__ ((format (printf, 1, 2)))
#endif

void fmt_spam_msg_w
print_player_spam_msg(char * fmt, ...)
{
    time_t now = time(0);
    if (now <= player_block_message_time) return;

    char pbuf[16<<10];
    va_list ap;
    va_start(ap, fmt);
    int l = vsnprintf(pbuf, sizeof(pbuf), fmt, ap);
    if (l > sizeof(pbuf)) {
        strcpy(pbuf+sizeof(pbuf)-4, "...");
        l = sizeof(pbuf);
    }
    va_end(ap);

    player_block_message_time = now;
    printf_chat("%s", pbuf);
}

int
complain_bad_block(block_t blk)
{
    int bad_block = !can_place_block(blk);
    if (bad_block)
	print_player_spam_msg("&WYou cannot place \"%s\"", block_name(blk));
    return bad_block;
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
	if (marks[1].valid) {
	    printf_chat("(11)&fMark #2: &S(%d,%d,%d)", marks[1].x, marks[1].y, marks[1].z);
	    if (marks[2].valid) {
		printf_chat("(13)&fExtra Mark: &S(%d,%d,%d)", marks[2].x, marks[2].y, marks[2].z);
	    }
	} else
	    printf_chat("(11)&fMark #2: &S(Waiting)");
    }
}

void
cmd_mode(char * cmd, char * arg)
{
    char * block = strarg(arg);
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

	if (complain_bad_block(b)) return;

	player_mode_mode = b;
	printf_chat("&SPlayer /mode set to block &T%s", block_name(player_mode_mode));
    }
}

void
process_player_setblock(pkt_setblock pkt)
{
    if (!user_authenticated) {
	print_player_spam_msg("&SYou need to login first");
	revert_client(pkt);
	return;
    }
    if (!level_block_queue || !level_blocks || !user_authenticated) {
	print_player_spam_msg("&SLevel cannot be changed");
	revert_client(pkt);
	return;
    }

    int do_revert = 0;

    if (!do_revert && player_lockout>0) {
	do_revert = 1;
	player_lockout = 50;
	print_player_spam_msg("&SPlayer has changed level, stop placing blocks.");
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
		print_player_spam_msg("You can't build that far away.");
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

    if (!do_revert) do_revert = complain_bad_block(pkt.block);

    if (add_antispam_event(player_block_spam, server->block_spam_count, server->block_spam_interval, 0)) {
	if (server->block_spam_kick) {
	    my_user.kick_count++;
	    logout("Kicked for suspected griefing.");
	}
	int secs = 0;
        if (player_block_spam->banned_til)
            secs = player_block_spam->banned_til - time(0);

	if (secs > 10)
	    print_player_spam_msg("&WYou can't change blocks for %d seconds", secs);
	else
	    print_player_spam_msg("&WYou're changing blocks too fast, slow down.");
	do_revert = 1;
    }

    if (!level_block_queue || !level_blocks) return; // !!!

    if (do_revert) { revert_client(pkt); return; }

    block_t b = level_blocks[World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z)];
    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b)) {
	print_player_spam_msg("Cannot delete block %s", block_name(b));
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
	int x = pkt.coord.x;
	int y = pkt.coord.y;
	int z = pkt.coord.z;
	xyz_t mapsz;
	if (level_prop)
	    mapsz = (xyz_t){level_prop->cells_x, level_prop->cells_y, level_prop->cells_z};
	else mapsz = void_map_size;

	if (mapsz.y > 3 && (x == 0 || y <= 0 || z == 0 || x == mapsz.x-1 || y == mapsz.y-1 || z == mapsz.z-1))
	    b = Block_Bedrock;
	else if (y >= mapsz.y/2)
	    b = Block_Air;
	else if (y+2 >= mapsz.y/2 && (pkt.coord.x >= mapsz.x || pkt.coord.z >= mapsz.z || mapsz.y <= 3))
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
    if (strcasecmp(cmd, "markall") == 0) {
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
	    char * p = strarg(ar); ar = 0;
	    if (p == 0 || *p == '\0') break;
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
	printf_chat("&SUsage: &T/mark [x y z]&S");
	return;
    }

    if (has_offset[0]) args[0] += player_posn.x/32;
    if (has_offset[1]) args[1] += (player_posn.y-51)/32;
    if (has_offset[2]) args[2] += player_posn.z/32;

    int l = sizeof(marks)/sizeof(*marks);
    int v = 0;
    while(v<l && marks[v].valid) v++;
    if (v<l) {
	marks[v].valid = 1;
	marks[v].x = args[0];
	marks[v].y = args[1];
	marks[v].z = args[2];
	if (!extn_messagetypes)
	    printf_chat("&SMarked position (%d,%d,%d)", args[0], args[1], args[2]);
    } else {
	printf_chat("&WToo many marks -- cleared");
	clear_pending_marks();
    }

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
