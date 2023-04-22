
#include "plainmap.h"

// Simple map generator.
// A heightmap is created by creating a square spiral from the centre of the
// map and limiting height changed to +/- one block per block.
// Spirals up to 32 blocks between points are used to make larger features.
// Depending on the height in each position it's classified as land or sand.
// Water is placed above the sand.
// Trees and flowers are planted on the land.
void
gen_plain_map(char * seed)
{
    map_random_t rng[1];
    map_init_rng(rng, seed);

    uint16_t *heightmap = calloc(level_prop->cells_x*level_prop->cells_z, sizeof(*heightmap));

    int flattened = 0, land_only = 0, flowerrate = 0, treerate = 0;

    {
	int std = bounded_random_r(rng, 6) != 1;
	int class = 1+bounded_random_r(rng, 3);

	if (!std) {
	    flattened = (class&1) != 0;
	    land_only = (class&2) != 0;
	}
    }

    {
	map_random_t rng2[1];
	seed_rng(rng, rng2);

	gen_plain_heightmap(rng2, heightmap, land_only, flattened);
    }

    flowerrate = 25 + 25 * bounded_random_r(rng, 15);
    treerate = 12 * (1<<bounded_random_r(rng, 6)) - 10;

    //printf_chat("Class %s, %s, Flowers 1:%d, trees 1:%d", flattened?"flattened":"hill", land_only?"land":"beach", flowerrate, treerate);

    int side_level = level_prop->side_level == INT_MIN?level_prop->cells_y/2:level_prop->side_level;
    int x, y, z;

    for(y=0; y<level_prop->cells_y; y++)
	for(z=0; z<level_prop->cells_z; z++)
	    for(x=0; x<level_prop->cells_x; x++)
	    if (y == 0 && side_level > y+3)
		level_blocks[World_Pack(x,y,z)] = Block_Stone;
	    else
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		if (y1>side_level-1) {
		    y1--; // First grass "above" water at water level.
		    if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y==y1)
			level_blocks[World_Pack(x,y,z)] = Block_Grass;
		    else if (y > y1-3)
			level_blocks[World_Pack(x,y,z)] = Block_Dirt;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Stone;

		} else {
		    int y2 = side_level-1;
		    if (y>y1 && y>y2)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_StillWater;
		    else if (y > y1-3 && y1<side_level-8)
			level_blocks[World_Pack(x,y,z)] = Block_Gravel;
		    else if (y > y1-3)
			level_blocks[World_Pack(x,y,z)] = Block_Sand;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Stone;

		}
	    }

    if (flowerrate > 3) {
	// Flowers are placed in positions that change when the
	// map size is different.
	map_random_t rng2[1];
	seed_rng(rng, rng2);

	for(z=0; z<level_prop->cells_z; z++)
	    for(x=0; x<level_prop->cells_x; x++)
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		if (y1 < side_level-land_only) continue;
		if (y1 >= level_prop->cells_y-1) continue;
		int r = bounded_random_r(rng2, flowerrate);
		if (r == 1)
		    level_blocks[World_Pack(x,y1,z)] = Block_Dandelion;
		else if (r == 2)
		    level_blocks[World_Pack(x,y1,z)] = Block_Rose;
		else if (r == 3 && bounded_random_r(rng2, 4) == 0)
		    level_blocks[World_Pack(x,y1,z)] = Block_Sapling;
	    }
    }

    // Merge side_level and side_level+1
    for(z=0; z<level_prop->cells_z; z++)
	for(x=0; x<level_prop->cells_x; x++)
	{
	    int y1 = heightmap[x+z*level_prop->cells_x];
	    if (y1>side_level-1 || land_only) {
		heightmap[x+z*level_prop->cells_x]--;
	    }
	}

    // No trees too close to spawn.
    x = level_prop->spawn.x/32; z = level_prop->spawn.z/32;
    for(int dx = -5; dx<6; dx++)
	for(int dz = -5; dz<6; dz++)
	{
	    int x2 = x+dx, z2 = z+dz;
	    if (x2 < 0 || x2 >= level_prop->cells_x) continue;
	    if (z2 < 0 || z2 >= level_prop->cells_z) continue;
	    heightmap[x2+z2*level_prop->cells_x] |= 0x8000;
	}

    spiral_tree_planting(rng, heightmap, treerate);
}

LOCAL void
gen_plain_heightmap(map_random_t *srng, uint16_t * heightmap, int land_only, int flattened)
{
    int cx = level_prop->cells_x/2;
    int cz = level_prop->cells_z/2;
    int rm = level_prop->cells_x>level_prop->cells_z?level_prop->cells_x:level_prop->cells_z;
    rm = rm/2 + 2;

    int side_level = level_prop->side_level == INT_MIN?level_prop->cells_y/2:level_prop->side_level;
    int h = side_level;
    if (!flattened && !land_only && level_prop->cells_y>15)
	h += bounded_random_r(srng, 8);
    if (h >= level_prop->cells_y) h = side_level;
    if (h >= level_prop->cells_y) h = level_prop->cells_y/2;
    level_prop->spawn.y = (h+3) *32+16;

    heightmap[cx+cz*level_prop->cells_x] = 0x7FFF;

    int first_e, s_div;
    first_e = 2 + bounded_random_r(srng, 6);
    if (first_e>8) first_e = 8;
    s_div = 1 + bounded_random_r(srng, 8);

    for(int e = first_e; e>0; e--) {

	// Use a derived RNG so that different size maps keep similar centres.
	map_random_t rng[1];
	seed_rng(srng, rng);

	int s = (1<<(e-1));
	//if (s>level_prop->cells_y) continue;
	for(int r=s; r<rm; r+=s) {
	    for(int x=cx-r+s; x<=cx+r; x+=s) gen_plain(rng, heightmap, x, cz-r, s, s_div);
	    for(int z=cz-r+s; z<=cz+r; z+=s) gen_plain(rng, heightmap, cx+r, z, s, s_div);
	    for(int x=cx+r-s; x>=cx-r; x-=s) gen_plain(rng, heightmap, x, cz+r, s, s_div);
	    for(int z=cz+r-s; z>=cz-r; z-=s) gen_plain(rng, heightmap, cx-r, z, s, s_div);
	}
    }

    heightmap[cx+cz*level_prop->cells_x] = 0;
    gen_plain(srng, heightmap, cx, cz, 1, s_div);

    if (flattened) {
	int min_y = 0x7FFF, max_y = -0x7FFF;
	for(int z=0; z<level_prop->cells_z; z++)
	    for(int x=0; x<level_prop->cells_x; x++)
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		y1 -= 0x7FFF;
		if (y1 < min_y) min_y = y1;
		if (y1 > max_y) max_y = y1;
	    }
	for(int z=0; z<level_prop->cells_z; z++)
	    for(int x=0; x<level_prop->cells_x; x++)
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		y1 -= 0x7FFF;
		if ((max_y-min_y)/8 != 0)
		    y1 /= (max_y-min_y)/8;
		y1 += 0x7FFF;
		heightmap[x+z*level_prop->cells_x] = y1;
	    }
    }

    if (land_only)
	for(int z=0; z<level_prop->cells_z; z++)
	    for(int x=0; x<level_prop->cells_x; x++)
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		y1 -= 0x7FFF;
		y1 += h;
		if (y1<side_level)
		    y1 = heightmap[x+z*level_prop->cells_x] =
			(side_level)*2 - y1;
		y1 -= h;
		y1 += 0x7FFF;
		heightmap[x+z*level_prop->cells_x] = y1;
	    }

    int limit = 0;
#if 1
    {
	int min_h = 0xFFFF, max_h = 0;
	for(int z=0; z<level_prop->cells_z; z++)
	    for(int x=0; x<level_prop->cells_x; x++)
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		y1 -= 0x7FFF;
		y1 += h;
		if (y1>max_h) max_h = y1;
		if (y1<min_h) min_h = y1;
	    }

	if (max_h >= level_prop->cells_y) limit = max_h;
    }
#endif

    for(int z=0; z<level_prop->cells_z; z++)
	for(int x=0; x<level_prop->cells_x; x++)
	{
	    int y1 = heightmap[x+z*level_prop->cells_x];
	    y1 -= 0x7FFF;
	    y1 += h;
	    if (y1 < 0) y1 = 0;
	    if (limit && y1 > h) y1 = ((y1-h)*(level_prop->cells_y-h)/(limit-h))+h;
	    if (y1 >= level_prop->cells_y) y1 = level_prop->cells_y-1;
	    if (level_prop->cells_y > 16 && y1 >= level_prop->cells_y-8)
		y1 = level_prop->cells_y-8;
	    heightmap[x+z*level_prop->cells_x] = y1;
	}
}

LOCAL void
gen_plain(map_random_t *rng, uint16_t * heightmap, int tx, int tz, int s, int s_div)
{
    if (tx < 0 || tx >= level_prop->cells_x) return;
    if (tz < 0 || tz >= level_prop->cells_z) return;
    int mx = level_prop->cells_x;

    int min_y = 1, max_y = 0xFFFF, avg=0, avcnt=0;
    for(int dx = -s; dx<2*s; dx++)
	for(int dz = -s; dz<2*s; dz++)
	{
	    int x = tx+dx, z = tz+dz;
	    if (dx == 0 && dz == 0) continue;
	    if (x < 0 || x >= level_prop->cells_x) continue;
	    if (z < 0 || z >= level_prop->cells_z) continue;
	    int h = heightmap[x+z*mx];
	    if (h == 0) continue;
	    if (!s_div) {
		if (min_y < h-1) min_y = h-1;
		if (max_y > h+1) max_y = h+1;
	    } else {
		if (min_y < h-s) min_y = h-1-s/s_div;
		if (max_y > h+s) max_y = h+1+s/s_div;
	    }
	    avg += h; avcnt++;
	}

#if 0
    // This adds "spikes" that will drag up/down the local area before
    // being removed by the finer passes.
    if (s >= 32) {
	max_y = max_y + (max_y-min_y);
	min_y = min_y - (max_y-min_y);
	//if (max_y >= level_prop->cells_y) max_y = level_prop->cells_y-1;
	if (max_y >= 0xFFFF) max_y = 0xFFFF;
	if (min_y < 1) min_y = 1;
    }
#endif

    if (min_y > max_y && avcnt) {
	avg += avcnt/2; avg /= avcnt;
	heightmap[tx+tz*mx] = avg;
	return;
    }
    if (min_y > max_y) return; // Leave a hole rather than div by zero.

    if (min_y >= max_y) {
	heightmap[tx+tz*mx] = min_y;
    } else if (s>1 || avcnt == 0) {
	heightmap[tx+tz*mx] = min_y + bounded_random_r(rng, max_y-min_y+1);
    } else {
	avg += avcnt/2; avg /= avcnt;
	heightmap[tx+tz*mx] = avg;
    }
}

#if 0
LOCAL void
linear_tree_planting(map_random_t *rng, uint16_t * heightmap, int treerate)
{
    for(int z=3; z+3<level_prop->cells_z; z++)
    {
	for(int x=3; x+3<level_prop->cells_x; x++)
	{
	    if (x>=level_prop->cells_x || z>=level_prop->cells_z) continue;

	    int y1 = heightmap[x+z*level_prop->cells_x];
	    if (y1 <= side_level) continue;
	    if (treerate == 0 ||
		    y1+7 > level_prop->cells_y ||
		    bounded_random_r(rng, treerate) != 1) {
		continue;
	    }

	    plant_tree(rng, x, y1, z);

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
}
#endif

LOCAL void
spiral_tree_planting(map_random_t *rng, uint16_t * heightmap, int treerate)
{
    int s = 1;
    int cx = level_prop->cells_x/2;
    int cz = level_prop->cells_z/2;
    int rm = level_prop->cells_x>level_prop->cells_z?level_prop->cells_x:level_prop->cells_z;
    rm = rm/2 + 1;

    try_plant_tree(rng, heightmap, treerate, cx, cz);
    for(int r=s; r<rm; r+=s) {
        for(int x=cx-r+s; x<=cx+r; x+=s) try_plant_tree(rng, heightmap, treerate, x, cz-r);
        for(int z=cz-r+s; z<=cz+r; z+=s) try_plant_tree(rng, heightmap, treerate, cx+r, z);
        for(int x=cx+r-s; x>=cx-r; x-=s) try_plant_tree(rng, heightmap, treerate, x, cz+r);
        for(int z=cz+r-s; z>=cz-r; z-=s) try_plant_tree(rng, heightmap, treerate, cx-r, z);
    }
}

LOCAL void
try_plant_tree(map_random_t *rng, uint16_t * heightmap, int treerate, int x, int z)
{
    if (x-3 < 0 || x+3 >= level_prop->cells_x ||
	z-3 < 0 || z+3 >= level_prop->cells_z) return;

    int side_level = level_prop->side_level == INT_MIN?level_prop->cells_y/2:level_prop->side_level;
    int y1 = heightmap[x+z*level_prop->cells_x];
    if (y1 <= side_level) return;
    if (treerate == 0 || y1 > 0x8000 || bounded_random_r(rng, treerate) != 1)
	return;

    map_random_t rng2[1];
    seed_rng(rng, rng2);
    if (y1+7 <= level_prop->cells_y)
	plant_tree(rng2, x, y1, z);

    // None too close.
    for(int dx = -4; dx<5; dx++)
	for(int dz = -4; dz<5; dz++)
	{
	    int x2 = x+dx, z2 = z+dz;
	    if (x2 < 0 || x2 >= level_prop->cells_x) continue;
	    if (z2 < 0 || z2 >= level_prop->cells_z) continue;
	    heightmap[x2+z2*level_prop->cells_x] |= 0x8000;
	}
}

LOCAL void
plant_tree(map_random_t *rng, int x, int y1, int z)
{
    int height = 3 + bounded_random_r(rng, 4);
    int y = y1+1;

    level_blocks[World_Pack(x,y1,z)] = Block_Dirt;
    for(int i=0; i<height; i++)
	level_blocks[World_Pack(x,y+i,z)] = Block_Log;

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
		    if (bounded_random_r(rng, 2) == 0)
			level_blocks[World_Pack(xx,yy,zz)] = Block_Leaves;
		} else {
		    level_blocks[World_Pack(xx,yy,zz)] = Block_Leaves;
		}
	    }
    }
}
