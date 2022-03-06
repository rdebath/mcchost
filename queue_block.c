#include <unistd.h>
#include <stdio.h>

#include "queue_block.h"

#if INTERFACE
typedef block_queue_t block_queue_t;
struct block_queue_t {
    uint32_t generation;	// uint so GCC doesn't fuck it up.
    int curr_offset;
    int queue_len;

    xyzb_t updates[1];
};
#endif

volatile block_queue_t *level_block_queue = 0;
intptr_t level_block_queue_len = 0;

static int last_id = -1;
static uint32_t last_generation;
static int reload_pending = 0;

void
update_block(pkt_setblock pkt)
{
    if (!level_block_queue || !level_blocks) return; // !!!
    if (pkt.coord.x < 0 || pkt.coord.x >= level_prop->cells_x) return;
    if (pkt.coord.y < 0 || pkt.coord.y >= level_prop->cells_y) return;
    if (pkt.coord.z < 0 || pkt.coord.z >= level_prop->cells_z) return;

    uintptr_t index = World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z);
    int b = pkt.mode?pkt.block:Block_Air;

    // No update needed.
    if (level_blocks[index] == b) return;

    level_blocks[index] = b;

    lock_shared();
    int id = level_block_queue->curr_offset++;
    if (level_block_queue->curr_offset >= level_block_queue->queue_len) {
	level_block_queue->curr_offset = 0;
	level_block_queue->generation ++;
    }
    level_block_queue->updates[id].x = pkt.coord.x;
    level_block_queue->updates[id].y = pkt.coord.y;
    level_block_queue->updates[id].z = pkt.coord.z;
    level_block_queue->updates[id].b = b;
    unlock_shared();
}

void
set_last_block_queue_id()
{
    if (!level_block_queue)
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
send_queued_blocks()
{
    int counter = 0;
    if (reload_pending && !bytes_queued_to_send()) {
	reload_pending = 0;
	usleep(500000);  // Half a second leeway to allow network to clear.
	send_map_file();
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
