#include <stdlib.h>
#include <string.h>

#include "cmdreplace.h"

/*HELP replace,r H_CMD
&T/replace [block] [block2].. [new]
Replaces [block] with [new] between two points.
  If more than one [block] is given, they are all replaced.
  If only [block] is given, replaces with your held block.
Aliases: /r
*/

/*HELP replaceall,ra H_CMD
&T/replace [block] [block2].. [new]
Replaces [block] with [new] for the map.
  If more than one [block] is given, they are all replaced.
  If only [block] is given, replaces with your held block.
Aliases: /ra
*/

#if INTERFACE
#define CMD_REPLACE \
    {N"replace", &cmd_replace}, {N"r", &cmd_replace, .dup=1}, \
    {N"replaceall", &cmd_replaceall}, {N"ra", &cmd_replaceall, .dup=1}
#endif

void
cmd_replace(char * cmd, char * arg)
{
    if (level_prop->disallowchange) { printf_chat("&WLevel cannot be changed"); return; }
    if (!arg || !*arg) {
	printf_chat("You need at least one block to replace.");
	return;
    }

    if (!marks[0].valid || !marks[1].valid) {
	if (!marks[0].valid) {
	    if (!extn_heldblock) {
		printf_chat("&SPlace or break two blocks to determine edges of %s replacement.", arg);
	    } else if (!extn_messagetypes)
		printf_chat("&SPlace or break two blocks to determine edges.");
	}
	player_mark_mode = 2;
	if (marks[0].valid) player_mark_mode--;
	request_pending_marks("Selecting region for Replace", cmd_replace, cmd, arg);
	return;
    }

    block_t bllist[256], rpl;
    int blcount = 0;
    for(char * ar = strtok(arg, " ");ar;ar=strtok(0," ")) {
	block_t b = block_id(ar);
	if (b == BLOCKNIL) {
	    printf_chat("&WUnknown block '%s'", ar);
	    return;
	}
	bllist[blcount++] = b;
    }
    if (blcount == 1)
	rpl = block_id(0); // Held block
    else
	rpl = bllist[--blcount];

    replace_blocks(rpl, bllist, blcount,
	marks[0].x, marks[0].y, marks[0].z,
	marks[1].x, marks[1].y, marks[1].z);

    clear_pending_marks();
}

void
cmd_replaceall(char * UNUSED(cmd), char * arg)
{
    if (level_prop->disallowchange) { printf_chat("&WLevel cannot be changed"); return; }
    if (!arg || !*arg) {
	printf_chat("You need at least one block to replace.");
	return;
    }

    block_t bllist[256], rpl;
    int blcount = 0;
    for(char * ar = strtok(arg, " ");ar;ar=strtok(0," ")) {
	block_t b = block_id(ar);
	if (b == BLOCKNIL) {
	    printf_chat("&WUnknown block '%s'", ar);
	    return;
	}
	bllist[blcount++] = b;
    }
    if (blcount == 1)
	rpl = block_id(0); // Held block
    else
	rpl = bllist[--blcount];

    replace_blocks(rpl, bllist, blcount,
	0, 0, 0,
	level_prop->cells_x-1, level_prop->cells_y-1, level_prop->cells_z-1);
}

void
replace_blocks(block_t rpl, block_t*bllist, int blcount, int x0, int y0, int z0, int x1, int y1, int z1)
{
    int args[7] = {0, x0, y0, z0, x1, y1, z1};

    if (clamp_cuboid(args) == 0 || blcount < 1)
	return;

    if (rpl >= BLOCKMAX) rpl = BLOCKMAX-1;

    // See Cuboid for lock/update notes.

    if (blcount == 1) {
	// Single block, simple and should be faster.
	block_t b = bllist[0];
	lock_fn(level_lock);
	my_user.dirty = 1;
	int x, y, z, placecount = 0;
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    uintptr_t index = World_Pack(x, y, z);
		    if (level_blocks[index] != b) continue;
		    placecount++;
		    level_blocks[index] = rpl;
		    prelocked_update(x, y, z, rpl);
		}
	my_user.blocks_drawn += placecount;
	unlock_fn(level_lock);
    } else {
	lock_fn(level_lock);
	my_user.dirty = 1;
	int x, y, z, placecount = 0;
	for(y=args[2]; y<=args[5]; y++)
	    for(x=args[1]; x<=args[4]; x++)
		for(z=args[3]; z<=args[6]; z++)
		{
		    uintptr_t index = World_Pack(x, y, z);
		    int found = 0;
		    for(int i=0; i<blcount && !found; i++) {
			if (level_blocks[index] == bllist[i])
			    found = 1;
		    }
		    if (!found) continue;
		    placecount++;
		    level_blocks[index] = rpl;
		    prelocked_update(x, y, z, rpl);
		}
	my_user.blocks_drawn += placecount;
	unlock_fn(level_lock);
    }

}
