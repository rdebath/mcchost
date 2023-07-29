
#include "cmdtree.h"

#if INTERFACE
#define UCMD_TREE	{N"tree", &cmd_tree}
#endif

void
cmd_tree(char * cmd, char * arg)
{
    if (level_prop->disallowchange) { printf_chat("&WThis level cannot be changed"); return; }
    if (complain_bad_block(Block_Log)) return;
    if (complain_bad_block(Block_Leaves)) return;

    if (!marks[0].valid) {
	if (!extn_messagetypes)
	    printf_chat("&SPlace or break a blocks to plant tree");
	player_mark_mode = 1;
	request_pending_marks("Selecting location for tree", cmd_tree, cmd, arg);
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

    plant_tree(x,y,z);
}

void
plant_tree(int x, int y, int z)
{
    int height = 3 + bounded_random(4);

    for(int i=0; i<=height; i++)
	check_place_block(Block_Log, x,y+i,z);

    for (int dy = height - 2; dy <= height + 1; dy++) {
	if (y+dy >= level_prop->cells_y) continue;
	int extent = dy > height - 1 ? 1 : 2;
	for (int dz = -extent; dz <= extent; dz++)
	    for (int dx = -extent; dx <= extent; dx++)
	    {
		int xx = (x + dx), yy = (y + dy), zz = (z + dz);
		if (xx == x && zz == z && dy <= height) continue;

		if (abs(dx) == extent && abs(dz) == extent) {
		    if (dy > height) continue;
		    if (bounded_random(2) == 0)
			check_place_block(Block_Leaves, xx,yy,zz);
		} else {
		    check_place_block(Block_Leaves, xx,yy,zz);
		}
	    }
    }
}

void
check_place_block(block_t blk, int x, int y, int z)
{
    pkt_setblock pkt;
    pkt.heldblock = blk;
    pkt.mode = 3;
    pkt.block = pkt.mode?pkt.heldblock:Block_Air;
    pkt.coord.x = x;
    pkt.coord.y = y;
    pkt.coord.z = z;

    if (pkt.coord.x < 0 || pkt.coord.x >= level_prop->cells_x) return;
    if (pkt.coord.y < 0 || pkt.coord.y >= level_prop->cells_y) return;
    if (pkt.coord.z < 0 || pkt.coord.z >= level_prop->cells_z) return;

    block_t b = level_blocks[World_Pack(pkt.coord.x, pkt.coord.y, pkt.coord.z)];
    if (b != Block_Air && b < BLOCKMAX && !can_delete_block(b)) {
        printf_chat("&WYou cannot change the block at location (%d,%d,%d)",
            pkt.coord.x, pkt.coord.y, pkt.coord.z);
        return;
    }

    update_block(pkt);
}
