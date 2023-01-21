
#include "queue_block.h"

#if INTERFACE

#define MIN_QUEUE 250
#define STD_QUEUE 2048
typedef block_queue_t block_queue_t;
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    uint32_t curr_offset;
    uint32_t queue_len;
    uint32_t last_queue_len;

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
    block_t old_block = level_blocks[index];
    if (old_block == b && pkt.mode != 2) return;

    // User counters
    if (b == 0) my_user.blocks_deleted++; else my_user.blocks_placed++;
    my_user.dirty = 1;

    // Physics Like updates ... these are trivial instant updates that allow
    // the classic 0.30 client to enter blocks not in it's menu.
    block_t new_below = (block_t)-1;

    // Stack blocks.
    if (pkt.coord.y > 0 && level_prop->blockdef[b].stack_block != 0 && old_block == Block_Air)
    {
        block_t newblock = level_prop->blockdef[b].stack_block;
        uintptr_t ind2 = World_Pack(pkt.coord.x, pkt.coord.y-1, pkt.coord.z);
	if (b == level_blocks[ind2]) {
	    new_below = newblock;
	    b = 0;
	    pkt.mode = 2;
	}
    }

    block_t above = Block_Air;
    if (pkt.coord.y < level_prop->cells_y-1) {
	uintptr_t indup = World_Pack(pkt.coord.x, pkt.coord.y+1, pkt.coord.z);
	above = level_blocks[indup];
    }
    if (above >= BLOCKMAX) above = BLOCKMAX-1;

    // Correct this Grass/Dirt block
    if (level_prop->blockdef[b].dirt_block != 0 || level_prop->blockdef[b].grass_block != 0)
    {
	int transmits_light = level_prop->blockdef[above].transmits_light;
	if (!transmits_light && level_prop->blockdef[b].dirt_block != 0 &&
		level_prop->blockdef[b].dirt_block != old_block)
	    b = level_prop->blockdef[b].dirt_block;
	else if (transmits_light && level_prop->blockdef[b].grass_block != 0 &&
		level_prop->blockdef[b].grass_block != old_block) {
	    b = grow_dirt_block(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
	}
    }

    // Fix Grass/Dirt block below
    uintptr_t ind2 = 0;
    block_t blkbelow = Block_Bedrock;
    if (pkt.coord.y > 0) {
	int transmits_light = level_prop->blockdef[b].transmits_light;
	ind2 = World_Pack(pkt.coord.x, pkt.coord.y-1, pkt.coord.z);
	blkbelow = level_blocks[ind2];
	if (blkbelow >= BLOCKMAX) blkbelow = BLOCKMAX-1;

	if (transmits_light && level_prop->blockdef[blkbelow].grass_block != 0) {
	    int t = grow_dirt_block(pkt.coord.x, pkt.coord.y-1, pkt.coord.z, level_blocks[ind2]);
	    if (t != level_blocks[ind2])
		new_below = t;
	}
	if (!transmits_light && level_prop->blockdef[blkbelow].dirt_block != 0)
	    new_below = level_prop->blockdef[blkbelow].dirt_block;
    }

    // Make sure the updates are in the right order and try to minimise the
    // number of updates sent.
    if (b != (block_t)-1) {
	if (b != level_blocks[index]) {
	    level_blocks[index] = b;
	    send_update(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
	} else if (pkt.mode == 2) {
	    send_setblock_pkt(pkt.coord.x, pkt.coord.y, pkt.coord.z, b);
	}
    }
    if (new_below != (block_t)-1 && pkt.coord.y > 0) {
	if (new_below != level_blocks[ind2]) {
	    level_blocks[ind2] = new_below;
	    send_update(pkt.coord.x, pkt.coord.y-1, pkt.coord.z, new_below);
	    blkbelow = new_below;
	}
    }

    // Now sand and gravel
    if (pkt.coord.y > 0) {
	if (level_prop->blockdef[b].fastfall_flag && blkbelow == 0)
	    check_for_sand(pkt.coord.x, pkt.coord.y, pkt.coord.z);
    }
    if (above != Block_Air && level_prop->blockdef[above].fastfall_flag)
	check_for_sand(pkt.coord.x, pkt.coord.y+1, pkt.coord.z);
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
check_for_sand(int x, int y, int z)
{
    if (x < 0 || x >= level_prop->cells_x) return;
    if (y < 0 || y >= level_prop->cells_y) return;
    if (z < 0 || z >= level_prop->cells_z) return;

    int min_y = y;
    while(min_y>0)
    {
	uintptr_t index = World_Pack(x, min_y, z);
	block_t blk = level_blocks[index];
	if (blk >= BLOCKMAX) blk = BLOCKMAX-1;
	if (blk != Block_Air && !level_prop->blockdef[blk].fastfall_flag) break;
	min_y--;
    }

    int sav_y = -1;
    block_t sav_b = Block_Air;

    int new_y = min_y, old_y;
    for(old_y = min_y; old_y < level_prop->cells_y; old_y++) {
	for(;;new_y++) {
	    if (new_y >= level_prop->cells_y) goto break_break;
	    uintptr_t index = World_Pack(x, new_y, z);
	    block_t blk = level_blocks[index];
	    if (blk >= BLOCKMAX) blk = BLOCKMAX-1;
	    if (blk == Block_Air) break;
	}
	if (old_y < new_y) old_y = new_y;
	block_t to_move = Block_Air;
	for(;;old_y++) {
	    if (old_y >= level_prop->cells_y) goto break_break;
	    uintptr_t index = World_Pack(x, old_y, z);
	    block_t blk = level_blocks[index];
	    if (blk >= BLOCKMAX) blk = BLOCKMAX-1;
	    to_move = blk;
	    if (blk != Block_Air) break;
	}

	if (!level_prop->blockdef[to_move].fastfall_flag) break;

	if (sav_y == new_y) {
	    level_blocks[World_Pack(x, sav_y, z)] = sav_b;
	    sav_y = -1;
	}
	if (sav_y >= 0 && sav_y < level_prop->cells_y) {
	    send_update(x, sav_y, z, Block_Air);
	    sav_y = -1;
	}

	if (level_blocks[World_Pack(x, new_y, z)] != to_move) {
	    level_blocks[World_Pack(x, new_y, z)] = to_move;
	    send_update(x, new_y, z, to_move);
	}

	sav_y = old_y;
	sav_b = level_blocks[World_Pack(x, sav_y, z)];
	level_blocks[World_Pack(x, old_y, z)] = Block_Air;
    }

break_break:;
    if (sav_y >= 0 && sav_y < level_prop->cells_y) {
	send_update(x, sav_y, z, Block_Air);
    }
}

void
prelocked_update(int x, int y, int z, int b)
{
    level_prop->dirty_save = 1;
    level_prop->last_modified = time(0);
    uint32_t id = level_block_queue->curr_offset;
    level_block_queue->updates[id].x = x;
    level_block_queue->updates[id].y = y;
    level_block_queue->updates[id].z = z;
    level_block_queue->updates[id].b = b;
    if (++level_block_queue->curr_offset >= level_block_queue->queue_len) {
	level_block_queue->curr_offset = 0;
	level_block_queue->generation ++;
	level_block_queue->last_queue_len = level_block_queue->queue_len;
    }
}

void
send_update(int x, int y, int z, int b)
{
    lock_fn(level_lock);
    check_block_queue(0);
    level_prop->dirty_save = 1;
    level_prop->last_modified = time(0);
    uint32_t id = level_block_queue->curr_offset;
    level_block_queue->updates[id].x = x;
    level_block_queue->updates[id].y = y;
    level_block_queue->updates[id].z = z;
    level_block_queue->updates[id].b = b;
    if (++level_block_queue->curr_offset >= level_block_queue->queue_len) {
	level_block_queue->curr_offset = 0;
	level_block_queue->generation ++;
	level_block_queue->last_queue_len = level_block_queue->queue_len;
    }
    unlock_fn(level_lock);
}

void
set_last_block_queue_id()
{
    if (!level_block_queue) return;

    lock_fn(level_lock);
    check_block_queue(0);
    last_id = level_block_queue->curr_offset;
    last_generation = level_block_queue->generation;
    unlock_fn(level_lock);
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
    reload_pending = 1;
}

void
send_queued_blocks()
{
    if (reload_pending) {
	if (bytes_queued_to_send()) return;

	// reload_pending contains centiseconds till reload, but we adjust.
	if (reload_pending > 1) {
	    reload_pending--;
	    if (reload_pending > 6 && last_id == level_block_queue->curr_offset)
		reload_pending -= 5;
	    last_id = level_block_queue->curr_offset;
	    return;
	}

	reload_pending = 0;
	if (perm_is_admin()) {	// Message for admin only
	    if (check_block_queue(1))
		printf_chat("Reset block queue and reload");
	    else
		printf_chat("Reloading");
	}
	send_map_file();
	return;
    }

    if (last_id < 0) return;
    if (!level_block_queue) return;

    check_block_queue(1);

    for(int counter = 0;counter < 1024; counter++)
    {
	xyzb_t upd;
	lock_fn(level_lock);
	if (last_generation != level_block_queue->generation)
	{
	    int isok = 0;
	    if (last_generation == level_block_queue->generation-1 &&
		level_block_queue->queue_len == level_block_queue->last_queue_len) {
		if (level_block_queue->curr_offset < last_id)
		    isok = 1;
	    }
	    if (!isok) {
#if 0
		printlog("Queue overflow triggered reload "
			 "Gen=%d->%d, id=%d->%d, qlen=%d->%d",
			last_generation, level_block_queue->generation,
			last_id, level_block_queue->curr_offset,
			level_block_queue->last_queue_len,
			level_block_queue->queue_len);
#endif
		wipe_last_block_queue_id();
		reload_pending = 100;
		unlock_fn(level_lock);
		return;
	    }
	}
	if (last_id == level_block_queue->curr_offset) {
	    // Nothing more to send.
	    unlock_fn(level_lock);
	    return;
	}
	upd = level_block_queue->updates[last_id++];
	if (last_id == level_block_queue->queue_len) {
	    last_generation ++;
	    last_id = 0;
	}
	unlock_fn(level_lock);

	send_setblock_pkt(upd.x, upd.y, upd.z, upd.b);
    }
}
