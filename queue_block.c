#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "queue_block.h"

#if INTERFACE
typedef block_queue_t block_queue_t;
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;

    xyzb_t updates[1];
};
#endif

static int last_id = -1;
static uint32_t last_generation;
static int reload_pending = 0;

/* Note the pkt.mode has extra values
 * (2) Player in /paint mode, block needs update even if same.
 * (3) This is a place like command, client doesn't know yet.
 */
void
update_block(pkt_setblock pkt)
{
    if (!level_block_queue || !level_blocks) return; // !!!
    if (pkt.coord.x < 0 || pkt.coord.x >= level_prop->cells_x) return;
    if (pkt.coord.y < 0 || pkt.coord.y >= level_prop->cells_y) return;
    if (pkt.coord.z < 0 || pkt.coord.z >= level_prop->cells_z) return;

    uintptr_t index = World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z);
    block_t b = pkt.mode?pkt.block:Block_Air;
    if (b >= BLOCKMAX) b = BLOCKMAX-1;

    // No update needed.
    if (level_blocks[index] == b && pkt.mode != 2) return;

    // Physics Like updates ... these are trivial instant updates that allow
    // the classic client to enter blocks not in it's menu.

    // Stack blocks.
    if (pkt.coord.y > 0 && level_prop->blockdef[b].stack_block != 0)
    {
        block_t newblock = level_prop->blockdef[b].stack_block;
        uintptr_t ind2 = World_Pack(pkt.coord.x, pkt.coord.y-1, pkt.coord.z);
	if (b == level_blocks[ind2]) {
	    level_blocks[ind2] = newblock;
	    send_update(pkt.coord.x, pkt.coord.y-1, pkt.coord.z, newblock);
	    b = 0;
	    if (level_blocks[index] == b && pkt.mode != 3)
		send_setblock_pkt(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
	}
    }

    // Correct this Grass/Dirt block
    if (level_prop->blockdef[b].dirt_block != 0 || level_prop->blockdef[b].grass_block != 0)
    {
	block_t above = Block_Air;
	if (pkt.coord.y < level_prop->cells_y-1) {
	    uintptr_t ind2 = World_Pack(pkt.coord.x, pkt.coord.y+1, pkt.coord.z);
	    above = level_blocks[ind2];
	}
	if (above >= BLOCKMAX) above = BLOCKMAX-1;
	int transmits_light = level_prop->blockdef[above].transmits_light;
	if (!transmits_light && level_prop->blockdef[b].dirt_block != 0)
	    b = level_prop->blockdef[b].dirt_block;
	else if (transmits_light && level_prop->blockdef[b].grass_block != 0) {
	    b = grow_dirt_block(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
	}
    }

    // Fix Grass/Dirt block below
    if (pkt.coord.y > 0) {
	int transmits_light = level_prop->blockdef[b].transmits_light;
	uintptr_t ind2 = World_Pack(pkt.coord.x, pkt.coord.y-1, pkt.coord.z);
	block_t blk = level_blocks[ind2];
	if (blk >= BLOCKMAX) blk = BLOCKMAX-1;

	if (transmits_light && level_prop->blockdef[blk].grass_block != 0) {
	    int t = grow_dirt_block(pkt.coord.x, pkt.coord.y-1, pkt.coord.z, level_blocks[ind2]);
	    if (t != level_blocks[ind2]) {
		level_blocks[ind2] = t;
		send_update(pkt.coord.x, pkt.coord.y-1, pkt.coord.z, level_blocks[ind2]);
	    }
	}
	if (!transmits_light && level_prop->blockdef[blk].dirt_block != 0) {
	    level_blocks[ind2] = level_prop->blockdef[blk].dirt_block;
	    send_update(pkt.coord.x, pkt.coord.y-1, pkt.coord.z, level_blocks[ind2]);
	}
    }

    if (b != level_blocks[index]) {
	level_blocks[index] = b;
	send_update(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
    } else if (pkt.mode == 2) {
	send_setblock_pkt(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
    }
}

int
grow_dirt_block(int x, int y, int z, block_t blk)
{
    // Dirt gets changed into grass IF there is another grass block nearby.
    // This includes the grass block below that's about to change into dirt.
    int dx, dy, dz, found = 0;
    int grass = level_prop->blockdef[blk].grass_block;

    if (grass)
	for(dx=-1; dx<2 && !found; dx++) {
	    if (x+dx < 0 || x+dx>=level_prop->cells_x) continue;
	    for(dy=-1; dy<2 && !found; dy++) {
		if (y+dy < 0 || y+dy>=level_prop->cells_y) continue;
		for(dz=-1; dz<2 && !found; dz++)
		{
		    if (z+dz < 0 || z+dz>=level_prop->cells_z) continue;
		    if (dx==0 && dy==0 && dz==0) continue;
		    if (level_blocks[World_Pack(x+dx,y+dy,z+dz)] == grass)
			found = 1;
		}
	    }
	}

    if (found)
	return grass;
    else
	return blk;
}

void
unlocked_update(int x, int y, int z, int b)
{
    int id = level_block_queue->curr_offset;
    level_block_queue->updates[id].x = x;
    level_block_queue->updates[id].y = y;
    level_block_queue->updates[id].z = z;
    level_block_queue->updates[id].b = b;
    if (++level_block_queue->curr_offset >= level_block_queue->queue_len) {
	level_block_queue->curr_offset = 0;
	level_block_queue->generation ++;
    }
}

void
send_update(int x, int y, int z, int b)
{
    lock_shared();
    int id = level_block_queue->curr_offset;
    level_block_queue->updates[id].x = x;
    level_block_queue->updates[id].y = y;
    level_block_queue->updates[id].z = z;
    level_block_queue->updates[id].b = b;
    if (++level_block_queue->curr_offset >= level_block_queue->queue_len) {
	level_block_queue->curr_offset = 0;
	level_block_queue->generation ++;
    }
    unlock_shared();
}

void
set_last_block_queue_id()
{
    create_block_queue(level_name);
    lock_shared();
    last_id = level_block_queue->curr_offset;
    last_generation = level_block_queue->generation;
    unlock_shared();
}

void
wipe_last_block_queue_id()
{
    last_id = -1;
    last_generation = 0;
}

void
send_map_reload()
{
    reload_pending = 2;
}

void
send_queued_blocks()
{
    int counter = 0;
    if (reload_pending && !bytes_queued_to_send()) {
	if (reload_pending == 1)
	    usleep(500000);  // Half a second leeway to allow network to clear.
	reload_pending = 0;
	send_map_file();
	reset_player_list();
	return;
    }

    if (last_id < 0) return; // Should be "Map download in progress"

    for(;counter < 1024; counter++)
    {
	xyzb_t upd;
	lock_shared();
	if (last_generation != level_block_queue->generation)
	{
	    int isok = 0;
	    if (last_generation == level_block_queue->generation-1) {
		if (level_block_queue->curr_offset < last_id)
		    isok = 1;
	    }
	    if (!isok) {
		wipe_last_block_queue_id();
		reload_pending = 1;
		unlock_shared();
		return;
	    }
	}
	if (last_id == level_block_queue->curr_offset) {
	    // Nothing more to send.
	    unlock_shared();
	    return;
	}
	upd = level_block_queue->updates[last_id++];
	if (last_id == level_block_queue->queue_len) {
	    last_generation ++;
	    last_id = 0;
	}
	unlock_shared();

	send_setblock_pkt(upd.x, upd.y, upd.z, block_convert(upd.b));
    }
}
