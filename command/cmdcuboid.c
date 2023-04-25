
#include "cmdcuboid.h"

/*HELP cuboid,z H_CMD
&T/cuboid [mode] [block]
Modes: solid/hollow/walls/wire
Draws a cuboid between two points
Alias: &T/z, /box, /cw for /z wire, /ch for /z hollow, /Walls for /z walls, /hbox for /z hollow
*/

#if INTERFACE
#define CMD_CUBOID \
	{N"cuboid", &cmd_cuboid}, {N"z", &cmd_cuboid, .dup=1}, \
	{N"box", &cmd_cuboid, .dup=1}, \
	{N"cuboid-wire", &cmd_cuboid, .nodup=1, .dup=1}, \
	{N"cw", &cmd_cuboid, .dup=1}, \
	{N"cuboid-hollow", &cmd_cuboid, .nodup=1, .dup=1}, \
	{N"ch", &cmd_cuboid, .dup=1}, \
	{N"hbox", &cmd_cuboid, .dup=1}, \
	{N"cuboid-walls", &cmd_cuboid, .nodup=1, .dup=1}, \
	{N"walls", &cmd_cuboid, .dup=1}
#endif

char * cuboids[] = { "solid", "hollow", "walls", "wire", 0 };

void
cmd_cuboid(char * cmd, char * arg)
{
    if (!level_prop || level_prop->disallowchange) { printf_chat("&WLevel cannot be changed"); return; }

    char * p = strchr(arg, ' '), *c;
    if (!p) p = arg+strlen(arg);
    int ctype = 0;
    char * blkname = arg;
    if ((c=strchr(cmd, '-')) != 0) {
	for(int i = 0; cuboids[i]; i++) {
	    if (strcasecmp(cuboids[i], c+1) == 0)
		ctype = i;
	}
    } else {
	for(int i = 0; cuboids[i]; i++)
	    if (strncasecmp(cuboids[i], arg, p-arg) == 0) {
		ctype = i;
		blkname = p;
		break;
	    }
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

    if (complain_bad_block(b)) return;

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
