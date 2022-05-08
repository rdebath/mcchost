#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "createmap.h"

/*
 * This populates a default map file properties.
 */

void
createmap(char * levelname)
{
    if (level_prop->version_no == MAP_VERSION)
	if (level_prop->cells_x != 0 && level_prop->cells_y != 0 && level_prop->cells_z != 0)
	    return;

    xyzhv_t oldsize = {0};
    if (level_prop->magic_no == MAP_MAGIC) {
	if (level_prop->cells_x != 0 && level_prop->cells_y != 0 && level_prop->cells_z != 0)
	    if (level_prop->total_blocks == (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z)
	    {
		oldsize.x = level_prop->cells_x;
		oldsize.y = level_prop->cells_y;
		oldsize.z = level_prop->cells_z;
		oldsize.valid = 1;
	    }
    }

    *level_prop = (map_info_t){
	    .magic_no = MAP_MAGIC,
	    .version_no = MAP_VERSION,
	    .cells_x = 128, .cells_y = 64, .cells_z = 128,
	    .weather = 0, -1, -1, -1, -1, -1, 7, 8, -1, 2,
	    .spawn = { 64*32+16, 48*32, 64*32+16 }
	};

    {
	char buf2[256];
        snprintf(buf2, sizeof(buf2), "map/%.200s.ini", levelname);
        load_ini_file(level_ini_fields, buf2, 0);
    }

    if (oldsize.valid) {
	level_prop->cells_x = oldsize.x;
	level_prop->cells_y = oldsize.y;
	level_prop->cells_z = oldsize.z;

	// Calculate normal spawn posn (max out at 1023)
	level_prop->spawn.x = level_prop->cells_x/2;
	level_prop->spawn.y = level_prop->cells_y*3/4;
	level_prop->spawn.z = level_prop->cells_z/2;
	if (level_prop->spawn.x >= 1024) level_prop->spawn.x = 1023;
	if (level_prop->spawn.y >= 1024) level_prop->spawn.y = 1023;
	if (level_prop->spawn.z >= 1024) level_prop->spawn.z = 1023;
	level_prop->spawn.x = level_prop->spawn.x *32+16;
	level_prop->spawn.y = level_prop->spawn.y *32;
	level_prop->spawn.z = level_prop->spawn.z *32+16;
    }

    // Calculation used for edge level in classic client.
    level_prop->side_level = level_prop->cells_y/2-1;
    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    memcpy(level_prop->blockdef, default_blocks, sizeof(default_blocks));
#pragma GCC diagnostic pop

    // For Testing; later these will NOT be defined.
    for (int i = 0; i<66; i++)
	level_prop->blockdef[i].defined = 1;

    for (int i = 0; i<BLOCKMAX; i++)
	level_prop->blockdef[i].inventory_order = i;

    open_blocks(levelname);

    map_len_t test_map;
    memcpy(&test_map, (void*)(level_blocks+level_prop->total_blocks),
	    sizeof(map_len_t));

    int blocks_valid = 1;
    if (test_map.magic_no != MAP_MAGIC) blocks_valid = 0;
    if (test_map.cells_x != level_prop->cells_x) blocks_valid = 0;
    if (test_map.cells_y != level_prop->cells_y) blocks_valid = 0;
    if (test_map.cells_z != level_prop->cells_z) blocks_valid = 0;

    // If level not valid wipe to flat.
    if (!blocks_valid) {
        int x, y, z, y1;

        y1 = level_prop->side_level;
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

	test_map.magic_no = MAP_MAGIC;
	test_map.cells_x = level_prop->cells_x;
	test_map.cells_y = level_prop->cells_y;
	test_map.cells_z = level_prop->cells_z;
	memcpy((void*)(level_blocks+level_prop->total_blocks),
		&test_map, sizeof(map_len_t));
    }
}

