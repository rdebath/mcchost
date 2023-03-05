
#include "cmdfixgrass.h"

/*HELP fixgrass,fg H_CMD
&T/fixgrass
Checks cuboid between two points and swaps dirt with grass forms depending
on light passing block above.
Aliases: /fg
*/

#if INTERFACE
#define CMD_FIXGRASS \
    {N"fixgrass", &cmd_fixgrass, .map=1}, {N"fg", &cmd_fixgrass, .dup=1}
#endif

void
cmd_fixgrass(char * cmd, char * arg)
{
    if (level_prop->disallowchange) { printf_chat("&WLevel cannot be changed"); return; }

    if (!marks[0].valid || !marks[1].valid) {
	if (!marks[0].valid && !extn_messagetypes)
		printf_chat("&SPlace or break two blocks to determine edges.");
	player_mark_mode = 2;
	if (marks[0].valid) player_mark_mode--;
	request_pending_marks("Selecting region for fixgrass", cmd_fixgrass, cmd, arg);
	return;
    }

    xyzhv_t smarks[3];
    fetch_pending_marks(smarks);

    fixgrass_blocks(smarks[0].x, smarks[0].y, smarks[0].z,
		    smarks[1].x, smarks[1].y, smarks[1].z);
}

void
fixgrass_blocks(int x0, int y0, int z0, int x1, int y1, int z1)
{
    int args[7] = {0, x0, y0, z0, x1, y1, z1};

    int64_t blk_count;
    if (clamp_cuboid(args, &blk_count) == 0)
	return;
    if (!perm_block_check(blk_count)) {
        printf_chat("&WToo many blocks %jd>%jd", (intmax_t)blk_count, (intmax_t)command_limits.max_user_blocks);
        return;
    }

    // See Cuboid for lock/update notes.

    lock_fn(level_lock);
    check_block_queue(0);
    my_user.dirty = 1;
    int x, y, z, placecount = 0;
    for(y=args[2]; y<=args[5]; y++)
	for(x=args[1]; x<=args[4]; x++)
	    for(z=args[3]; z<=args[6]; z++)
	    {
		uintptr_t index = World_Pack(x, y, z);
		block_t b = level_blocks[index];

		if (	level_prop->blockdef[b].dirt_block == 0 &&
			level_prop->blockdef[b].grass_block == 0)
		    continue;

		block_t above = Block_Air;
		if (y < level_prop->cells_y-1) {
		    uintptr_t indup = World_Pack(x, y+1, z);
		    above = level_blocks[indup];
		    if (above >= BLOCKMAX) above = BLOCKMAX-1;
		}
		int transmits_light = level_prop->blockdef[above].transmits_light;

		if (!transmits_light && level_prop->blockdef[b].dirt_block != 0 &&
			level_prop->blockdef[b].dirt_block != b)
		    b = level_prop->blockdef[b].dirt_block;
		else if (transmits_light && level_prop->blockdef[b].grass_block != 0 &&
			level_prop->blockdef[b].grass_block != b) {
		    b = level_prop->blockdef[b].grass_block;
		}

		if (level_blocks[index] == b) continue;
		placecount++;
		level_blocks[index] = b;
		prelocked_update(x, y, z, b);
	    }
    my_user.blocks_drawn += placecount;
    unlock_fn(level_lock);
}
