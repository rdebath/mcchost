#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include "createmap.h"

/*
 * This populates a default map file properties.

TODO:
    Default size from blocks file (move map_len_t right to end of file)
	--> only works if no blockdefs
 */

void
createmap(char * levelname)
{
    // Don't recreate a map that seems okay.
    if (level_prop->version_no == MAP_VERSION)
	if (level_prop->cells_x != 0 && level_prop->cells_y != 0 && level_prop->cells_z != 0)
	    return;

    // Save away the old size.
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

    init_map_null();

    level_prop->time_created = time(0),

    load_ini_file(level_ini_fields, MODEL_INI_NAME, 1, 0);

    patch_map_nulls(oldsize);

    open_blocks(levelname);

    map_len_t test_map;
    memcpy(&test_map, (void*)(level_blocks+level_prop->total_blocks),
	    sizeof(map_len_t));

    int blocks_valid = 1;
    if (test_map.magic_no != MAP_MAGIC) blocks_valid = 0;
    if (test_map.cells_x != level_prop->cells_x) blocks_valid = 0;
    if (test_map.cells_y != level_prop->cells_y) blocks_valid = 0;
    if (test_map.cells_z != level_prop->cells_z) blocks_valid = 0;

    if (!blocks_valid)
	init_flat_level();
}

void
init_map_null()
{
    *level_prop = (map_info_t){
	    .magic_no = MAP_MAGIC, .magic_no2 = MAP_MAGIC2,
	    .version_no = MAP_VERSION,
	    .cells_x = 0, .cells_y = 0, .cells_z = 0,
	    .total_blocks = 0,
	    .weather = 0, -1, -1, -1, -1, -1, -1,
	    .side_block = 7, 8, INT_MIN, -2,
	    .spawn = { INT_MIN, INT_MIN, INT_MIN },
	    .clouds_height = INT_MIN,
	    .clouds_speed = 256, 256, 128,
	    .click_distance = 160,
	};

    for(int i = 0; i<sizeof(level_prop->uuid); i++)
    {
#ifdef PCG32_INITIALIZER
        int by = pcg32_boundedrand(256);
#else
        int by = random() % 256;
#endif
	level_prop->uuid[i] = by;
    }
    // Not documented, but make it the bytes for a real UUID (big endian).
    level_prop->uuid[6] &= 0x0F;
    level_prop->uuid[6] |= 0x40;
    level_prop->uuid[10] &= 0x3F;
    level_prop->uuid[10] |= 0x80;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    memcpy(level_prop->blockdef, default_blocks, sizeof(default_blocks));
#pragma GCC diagnostic pop

    for (int i = 0; i<BLOCKMAX; i++) {
	level_prop->blockdef[i].inventory_order = i;
	level_prop->blockdef[i].fallback = i<CPELIMIT?i:Block_Orange;
    }

    for (int i = 0; i<16; i++)
	level_prop->blockdef[Block_CP+i].fallback = cpe_conversion[i];
}

void
patch_map_nulls(xyzhv_t oldsize)
{
    if (level_prop->cells_x >= 16384 || level_prop->cells_y >= 16384 || level_prop->cells_z >= 16384 ||
        level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0 )
    {
	if (oldsize.valid == 1) {
	    level_prop->cells_x = oldsize.x;
	    level_prop->cells_y = oldsize.y;
	    level_prop->cells_z = oldsize.z;
	} else
	    level_prop->cells_y = (level_prop->cells_x = level_prop->cells_z = 128)/2;
    }

    level_prop->total_blocks = (int64_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;

    if (level_prop->side_level == INT_MIN)
	level_prop->side_level = level_prop->cells_y/2;

    if (level_prop->clouds_height == INT_MIN)
	level_prop->clouds_height = level_prop->cells_y+2;

    if (level_prop->spawn.x == INT_MIN)
	level_prop->spawn.x = level_prop->cells_x/2   *32+16;

    if (level_prop->spawn.y == INT_MIN)
	level_prop->spawn.y = level_prop->cells_y*3/4 *32+16;

    if (level_prop->spawn.z == INT_MIN)
	level_prop->spawn.z = level_prop->cells_z/2   *32+16;
}

void
init_flat_level()
{
    pcg32_random_t rng[1] = {PCG32_INITIALIZER};
    map_len_t test_map;
    int x, y, z, y1;

    if (strcasecmp(level_prop->theme, "pixel") == 0) {
	level_prop->side_level = 0;
	level_prop->seed[0] = 0;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    block_t px = Block_Air;
		    if (y==0) px = Block_Bedrock;
		    else if (x == 0 || z == 0) px = Block_White;
		    else if (x == level_prop->cells_x-1) px = Block_White;
		    else if (z == level_prop->cells_z-1) px = Block_White;
		    level_blocks[World_Pack(x,y,z)] = px;
		}
    } else if (strcasecmp(level_prop->theme, "empty") == 0) {
	level_prop->side_level = 1;
	level_prop->seed[0] = 0;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    block_t px = Block_Air;
		    if (y==0) px = Block_Bedrock;
		    level_blocks[World_Pack(x,y,z)] = px;
		}
    } else if (strcasecmp(level_prop->theme, "space") == 0) {
	int has_seed = !!level_prop->seed[0];
	pcg32_init_rng(rng, level_prop->theme, level_prop->seed);
	level_prop->side_level = 1;
	level_prop->edge_block = Block_Obsidian;
	level_prop->sky_colour = 0x000000;
	level_prop->cloud_colour = 0x000000;
	level_prop->fog_colour = 0x000000;
	level_prop->ambient_colour = 0x9B9B9B;
	level_prop->sunlight_colour = 0xFFFFFF;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    block_t px = Block_Air;
		    if (y==0) px = Block_Bedrock;
		    else if (y==1) px = Block_Obsidian;
		    else if (x == 0 || z == 0) px = Block_Obsidian;
		    else if (x == level_prop->cells_x-1) px = Block_Obsidian;
		    else if (y == level_prop->cells_y-1) px = Block_Obsidian;
		    else if (z == level_prop->cells_z-1) px = Block_Obsidian;
		    if (px == Block_Obsidian)
			if (pcg32_boundedrand_r(rng, 100) == 1)
			    px = Block_Iron;
		    level_blocks[World_Pack(x,y,z)] = px;
		}
	level_prop->dirty_save = !has_seed;
    } else if (strcasecmp(level_prop->theme, "rainbow") == 0) {
	int has_seed = !!level_prop->seed[0];
	pcg32_init_rng(rng, level_prop->theme, level_prop->seed);
	level_prop->side_level = 1;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    block_t px = Block_Air;
		    if (y==0) px = Block_White;
		    else if (x == 0 || z == 0) px = Block_White;
		    else if (x == level_prop->cells_x-1) px = Block_White;
		    else if (z == level_prop->cells_z-1) px = Block_White;
		    if (px == Block_White)
			px = pcg32_boundedrand_r(rng, Block_White-Block_Red)+Block_Red;
		    level_blocks[World_Pack(x,y,z)] = px;
		}
	level_prop->dirty_save = !has_seed;
    } else if (strcasecmp(level_prop->theme, "plain") == 0) {
	int has_seed = !!level_prop->seed[0];
	pcg32_init_rng(rng, level_prop->theme, level_prop->seed);
	gen_plain_map(rng);
	level_prop->dirty_save = !has_seed;

    } else {
	if (strcasecmp(level_prop->theme, "flat") == 0) {
	    if (level_prop->seed[0])
		level_prop->side_level = atoi(level_prop->seed);
	} else {
	    strcpy(level_prop->theme, "Flat");
	    level_prop->seed[0] = 0;
	}

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
    }

    test_map.magic_no = MAP_MAGIC;
    test_map.cells_x = level_prop->cells_x;
    test_map.cells_y = level_prop->cells_y;
    test_map.cells_z = level_prop->cells_z;
    memcpy((void*)(level_blocks+level_prop->total_blocks),
	    &test_map, sizeof(map_len_t));
}

// Trivial map generator.
// A heightmap is created by creating a square spiral from the centre of the
// map and limiting height changed to +/- one block, with some smoothing.
// Depending on the height in each position it's classified as land or sand.
// Water is placed above the sand.
// Trees and flowers are planted on the land.
void
gen_plain_map(pcg32_random_t *rng)
{
    int x, y, z;
    uint16_t *heightmap = calloc(level_prop->cells_x*level_prop->cells_z, sizeof(*heightmap));
    gen_plain_heightmap(rng, heightmap);
    int sl = level_prop->cells_y;

    for(y=0; y<level_prop->cells_y; y++)
	for(z=0; z<level_prop->cells_z; z++)
	    for(x=0; x<level_prop->cells_x; x++)
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		if (y1 < sl) sl = y1;
		if (y1>level_prop->cells_y/2-1) {
		    if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y==y1)
			level_blocks[World_Pack(x,y,z)] = Block_Grass;
		    else if (y > y1-3)
			level_blocks[World_Pack(x,y,z)] = Block_Dirt;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Stone;
		} else {
		    int y2 = level_prop->cells_y/2-1;
		    if (y>y1 && y>y2)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_StillWater;
		    else if (y > y1-3)
			level_blocks[World_Pack(x,y,z)] = Block_Sand;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Stone;
		}
	    }

    // None too close to spawn.
    x = level_prop->spawn.x/32; z = level_prop->spawn.z/32;
    for(int dx = -5; dx<6; dx++)
	for(int dz = -5; dz<6; dz++)
	{
	    int x2 = x+dx, z2 = z+dz;
	    if (x2 < 0 || x2 >= level_prop->cells_x) continue;
	    if (z2 < 0 || z2 >= level_prop->cells_z) continue;
	    heightmap[x2+z2*level_prop->cells_x] |= 0x8000;
	}

    int flowerrate = 25 + 5 * pcg32_boundedrand_r(rng, 15);
    int treerate = 10 + 10 * pcg32_boundedrand_r(rng, 40);
    for(z=3; z<level_prop->cells_z-3; z++)
	for(x=3; x<level_prop->cells_x-3; x++)
	{
	    int y1 = heightmap[x+z*level_prop->cells_x];
	    if (y1 <= level_prop->cells_y/2-1) continue;
	    if (y1+7 > level_prop->cells_y ||
		    pcg32_boundedrand_r(rng, treerate) != 1) {
		y1 &= 0x7FFF;
		if (y1 >= level_prop->cells_y-1) continue;
		if (pcg32_boundedrand_r(rng, flowerrate) == 1)
		    level_blocks[World_Pack(x,y1+1,z)] = Block_Dandelion;
		else if (pcg32_boundedrand_r(rng, flowerrate) == 1)
		    level_blocks[World_Pack(x,y1+1,z)] = Block_Rose;
		continue;
	    }

	    int height = 3 + pcg32_boundedrand_r(rng, 4);
	    int y = y1+1;

	    level_blocks[World_Pack(x,y1,z)] = Block_Dirt;
	    for(int i=0; i<height; i++)
		level_blocks[World_Pack(x,y+i,z)] = Block_Log;

	    for (int dy = height - 2; dy <= height + 1; dy++) {
		if (y+dy >= level_prop->cells_y-1) continue;
		int extent = dy > height - 1 ? 1 : 2;
		for (int dz = -extent; dz <= extent; dz++)
                    for (int dx = -extent; dx <= extent; dx++)
		    {
			int xx = (x + dx), yy = (y + dy), zz = (z + dz);
			if (xx == x && zz == z && dy <= height) continue;

			if (abs(dx) == extent && abs(dz) == extent) {
			    if (dy > height) continue;
			    if (pcg32_boundedrand_r(rng, 2) == 0)
				level_blocks[World_Pack(xx,yy,zz)] = Block_Leaves;
			} else {
			    level_blocks[World_Pack(xx,yy,zz)] = Block_Leaves;
			}
		    }
	    }

	    // None too close.
	    for(int dx = -5; dx<6; dx++)
		for(int dz = -5; dz<6; dz++)
		{
		    int x2 = x+dx, z2 = z+dz;
		    if (x2 < 0 || x2 >= level_prop->cells_x) continue;
		    if (z2 < 0 || z2 >= level_prop->cells_z) continue;
		    heightmap[x2+z2*level_prop->cells_x] |= 0x8000;
		}
	}
}

void
gen_plain_heightmap(pcg32_random_t *rng, uint16_t * heightmap)
{

    int cx = level_prop->cells_x/2;
    int cz = level_prop->cells_z/2;
    int rm = level_prop->cells_x>level_prop->cells_z?level_prop->cells_x:level_prop->cells_z;
    rm = rm/2 + 2;

    heightmap[cx+cz*level_prop->cells_x] = level_prop->cells_y/2;

    for(int s = 2; s>0; s--)
	for(int r=s; r<rm; r+=s) {
	    for(int x=cx-r; x<cx+r; x+=s) gen_plain(rng, heightmap, x, cz-r, s);
	    for(int z=cz-r; z<cz+r; z+=s) gen_plain(rng, heightmap, cx+r, z, s);
	    for(int x=cx+r; x>cx-r; x-=s) gen_plain(rng, heightmap, x, cz+r, s);
	    for(int z=cz+r; z>cz-r; z-=s) gen_plain(rng, heightmap, cx-r, z, s);
	}

    heightmap[cx+cz*level_prop->cells_x] = 0;
    gen_plain(rng, heightmap, cx, cz, 1);
}

LOCAL void
gen_plain(pcg32_random_t *rng, uint16_t * heightmap, int tx, int tz, int s)
{
    if (tx < 0 || tx >= level_prop->cells_x) return;
    if (tz < 0 || tz >= level_prop->cells_z) return;

    int min_y = 0, max_y = level_prop->cells_y-1, avg=0, avcnt=0;
    for(int dx = -s; dx<2*s; dx++)
	for(int dz = -s; dz<2*s; dz++)
	{
	    int x = tx+dx, z = tz+dz;
	    if (x < 0 || x >= level_prop->cells_x) continue;
	    if (z < 0 || z >= level_prop->cells_z) continue;
	    int h = heightmap[x+z*level_prop->cells_x];
	    if (h == 0) continue;
	    if (min_y < h-1) min_y = h-1;
	    if (max_y > h+1) max_y = h+1;
	    avg += h; avcnt++;
	}

    if (min_y > max_y && avcnt) {
	avg += avcnt/2; avg /= avcnt;
	heightmap[tx+tz*level_prop->cells_x] = avg;
	return;
    }
    if (min_y > max_y) return;
    if (min_y >= max_y) {
	heightmap[tx+tz*level_prop->cells_x] = min_y;
    } else if (s>1) {
#if 0
	int ex = level_prop->cells_x / 6;
	int ez = level_prop->cells_z / 6;
	if (tx < ex || tx+ex > level_prop->cells_x ||
	    tz < ez || tz+ez > level_prop->cells_z)
	    if (max_y > min_y+1) max_y = min_y+1;
#endif

	heightmap[tx+tz*level_prop->cells_x] = min_y + pcg32_boundedrand_r(rng, max_y-min_y+1);
    } else {
	avg += avcnt/2; avg /= avcnt;
	heightmap[tx+tz*level_prop->cells_x] = avg;
    }
}
