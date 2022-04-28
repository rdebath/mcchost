#include <sys/types.h>

#include "createmap.h"

/*
 * This populates a default map file properties.
 */

/* TODO: Need static definitions for first 66 blocks.  */

void
createmap(char * levelname)
{
    if (level_prop->version_no == MAP_VERSION)
	if (level_prop->cells_x != 0 && level_prop->cells_y != 0 && level_prop->cells_z != 0)
	    return;

    send_message_pkt(0, "&eCreating new default level");

    uint64_t tv = level_prop->valid_blocks;
    if (level_prop->version_no != 0 && level_prop->version_no != MAP_VERSION)
	tv = 0;

    map_info_t def = {
	    .magic_no = MAP_MAGIC,
	    .version_no = MAP_VERSION,
	    .cells_x = 128, .cells_y = 64, .cells_z = 128,
	    .weather = 0, -1, -1, -1, -1, -1, 7, 8, -1, 2,
	    .spawn = { 64*32+16, 48*32, 64*32+16 }
	};

    *level_prop = def;

    if (tv != (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z)
	tv = 0;

    level_prop->valid_blocks = tv;

    for (int i = 1; i<BLOCKMAX; i++)
	level_prop->blockdef[i].blockslight = 1;

    // For Grass<->Dirt
    level_prop->blockdef[Block_Dirt].grass_block = Block_Grass;
    level_prop->blockdef[Block_Grass].dirt_block = Block_Dirt;

    // If Grass<->Dirt
    level_prop->blockdef[Block_Air].blockslight = 0;
    level_prop->blockdef[Block_Sapling].blockslight = 0;
    level_prop->blockdef[Block_Leaves].blockslight = 0;
    level_prop->blockdef[Block_Glass].blockslight = 0;
    level_prop->blockdef[Block_Dandelion].blockslight = 0;
    level_prop->blockdef[Block_Rose].blockslight = 0;
    level_prop->blockdef[Block_BrownMushroom].blockslight = 0;
    level_prop->blockdef[Block_RedMushroom].blockslight = 0;
    level_prop->blockdef[Block_Rope].blockslight = 0;
    level_prop->blockdef[Block_Fire].blockslight = 0;

    // Block stacking
    level_prop->blockdef[Block_Slab].stack_block = Block_DoubleSlab;
    level_prop->blockdef[Block_CobbleSlab].stack_block = Block_Cobble;

    // CPE fallbacks.
    level_prop->blockdef[Block_CobbleSlab].fallback = Block_Slab;
    level_prop->blockdef[Block_Rope].fallback = Block_BrownMushroom;
    level_prop->blockdef[Block_Sandstone].fallback = Block_Sand;
    level_prop->blockdef[Block_Snow].fallback = Block_Air;
    level_prop->blockdef[Block_Fire].fallback = Block_ActiveLava;
    level_prop->blockdef[Block_LightPink].fallback = Block_Pink;
    level_prop->blockdef[Block_ForestGreen].fallback = Block_Green;
    level_prop->blockdef[Block_Brown].fallback = Block_Dirt;
    level_prop->blockdef[Block_DeepBlue].fallback = Block_Blue;
    level_prop->blockdef[Block_Turquoise].fallback = Block_Cyan;
    level_prop->blockdef[Block_Ice].fallback = Block_Glass;
    level_prop->blockdef[Block_CeramicTile].fallback = Block_Iron;
    level_prop->blockdef[Block_Magma].fallback = Block_Obsidian;
    level_prop->blockdef[Block_Pillar].fallback = Block_White;
    level_prop->blockdef[Block_Crate].fallback = Block_Wood;
    level_prop->blockdef[Block_StoneBrick].fallback = Block_Stone;

    open_blocks(levelname);

    // If level not valid wipe to flat.
    if (level_prop->valid_blocks == 0) {
        int x, y, z, y1;
	// Calculation used for edge level in classic client.
        y1 = level_prop->cells_y/2-1;
        for(y=0; y<level_prop->cells_y; y++)
            for(z=0; z<level_prop->cells_z; z++)
                for(x=0; x<level_prop->cells_x; x++)
                {
                    if (y>y1)
                        level_blocks[World_Pack(x,y,z)] = 0;
                    else if (y==y1)
                        level_blocks[World_Pack(x,y,z)] = Block_Grass;
                    else
                        level_blocks[World_Pack(x,y,z)] = Block_Dirt;
                }
        level_prop->valid_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
    }

}

