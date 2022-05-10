#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <limits.h>

#include "createmap.h"

/*
 * This populates a default map file properties.
 *
TODO:
    File source order -- Only one
	Uncompressed props file -- on open map.
	map/levelname.cw
	map/levelname.ini
	model.cw
	model.ini

    For *.ini allow existing block file.

    Default size from blocks file (move map_len_t right to end of file)

    Use INT_MIN to flag post calculated fields; if not reset do the calculation.

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
	    .weather = 0, -1, -1, -1, -1, -1, -1,
	    .side_block = 7, 8, -1, -2,
	    .spawn = { 64*32+16, 48*32, 64*32+16, .valid = 1 }
	};

    {
	char buf2[256];
        snprintf(buf2, sizeof(buf2), "map/%.200s.ini", levelname);
        load_ini_file(level_ini_fields, buf2, 1);
    }

    if (oldsize.valid) {
	level_prop->cells_x = oldsize.x;
	level_prop->cells_y = oldsize.y;
	level_prop->cells_z = oldsize.z;
	level_prop->spawn.valid = 0;
    }

    if (!level_prop->spawn.valid)
    {
	// Calculate normal spawn posn
	level_prop->spawn.x = level_prop->cells_x/2;
	level_prop->spawn.y = level_prop->cells_y*3/4;
	level_prop->spawn.z = level_prop->cells_z/2;
	level_prop->spawn.valid = 1;
    }

    // Calculation used for edge level in classic client.
    level_prop->side_level = level_prop->cells_y/2;
    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    memcpy(level_prop->blockdef, default_blocks, sizeof(default_blocks));
#pragma GCC diagnostic pop

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

        y1 = level_prop->side_level-1;
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

void
init_map_from_size(xyz_t size)
{
    *level_prop = (map_info_t){
	    .magic_no = MAP_MAGIC,
	    .version_no = MAP_VERSION,
	    .cells_x = size.x, .cells_y = size.y, .cells_z = size.z,
	    .total_blocks = (int64_t)size.x*size.y*size.z,
	    .weather = 0, -1, -1, -1, -1, -1, -1,
	    .side_block = 7, 8, size.y/2, -2,
	    .spawn = { size.x*32+16, size.y*3/4*32, size.z*32+16 },
	    .clouds_height = size.y+2,
	    .clouds_speed = 256, 256, 128,
	    .skybox_hor_speed = 1024, 1024,
	    .click_distance = -1
	};

}

void
patch_map_nulls()
{
    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;

    if (level_prop->side_level == INT_MIN)
	level_prop->side_level = level_prop->cells_y/2;

    if (level_prop->clouds_height == INT_MIN)
	level_prop->clouds_height = level_prop->cells_y+2;

    if (level_prop->spawn.x == INT_MIN)
	level_prop->spawn.x = level_prop->cells_x/2;

    if (level_prop->spawn.y == INT_MIN)
	level_prop->spawn.y = level_prop->cells_y*3/4;

    if (level_prop->spawn.z == INT_MIN)
	level_prop->spawn.z = level_prop->cells_z/2;
}
