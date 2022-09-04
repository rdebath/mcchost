#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include "plainmap.h"

// Simple map generator.
// A heightmap is created by creating a square spiral from the centre of the
// map and limiting height changed to +/- one block per block.
// Spirals up to 32 blocks between points are used to make larger features.
// Depending on the height in each position it's classified as land or sand.
// Water is placed above the sand.
// Trees and flowers are planted on the land. (no clumps)
void
gen_plain_map(map_random_t *rng)
{
    uint16_t *heightmap = calloc(level_prop->cells_x*level_prop->cells_z, sizeof(*heightmap));

    int land_only = bounded_random_r(rng, 4) == 1;
    gen_plain_heightmap(rng, heightmap, land_only);

    int flowerrate = 25 + 25 * bounded_random_r(rng, 15);

    int x, y, z;
    for(y=0; y<level_prop->cells_y; y++)
	for(z=0; z<level_prop->cells_z; z++)
	    for(x=0; x<level_prop->cells_x; x++)
	    if (y == 0)
		level_blocks[World_Pack(x,y,z)] = Block_Stone;
	    else
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
		if (y1>level_prop->side_level-1 || land_only) {
		    if (land_only && y1<level_prop->side_level-1)
			y1 = level_prop->side_level-1;
		    if (y==y1+1) {
			if (bounded_random_r(rng, flowerrate) == 1)
			    level_blocks[World_Pack(x,y,z)] = Block_Dandelion;
			else if (bounded_random_r(rng, flowerrate) == 1)
			    level_blocks[World_Pack(x,y,z)] = Block_Rose;
			else
			    level_blocks[World_Pack(x,y,z)] = Block_Air;
		    } else if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y==y1)
			level_blocks[World_Pack(x,y,z)] = Block_Grass;
		    else if (y > y1-3)
			level_blocks[World_Pack(x,y,z)] = Block_Dirt;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Stone;

		} else if (y1<level_prop->side_level-8) {
		    int y2 = level_prop->side_level-1;
		    if (y>y1 && y>y2)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_StillWater;
		    else if (y > y1-3)
			level_blocks[World_Pack(x,y,z)] = Block_Gravel;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Stone;

		} else {
		    int y2 = level_prop->side_level-1;
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

    int treerate = 5 + 25 * bounded_random_r(rng, 30);
    for(z=3; z<level_prop->cells_z-3; z++)
	for(x=3; x<level_prop->cells_x-3; x++)
	{
	    int y1 = heightmap[x+z*level_prop->cells_x];
	    if (y1 <= level_prop->side_level) continue;
	    if (y1+7 > level_prop->cells_y ||
		    bounded_random_r(rng, treerate) != 1) {
		continue;
	    }

	    int height = 3 + bounded_random_r(rng, 4);
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
			    if (bounded_random_r(rng, 2) == 0)
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
gen_plain_heightmap(map_random_t *srng, uint16_t * heightmap, int land_only)
{
    int cx = level_prop->cells_x/2;
    int cz = level_prop->cells_z/2;
    int rm = level_prop->cells_x>level_prop->cells_z?level_prop->cells_x:level_prop->cells_z;
    rm = rm/2 + 2;

    int min_y = land_only?level_prop->side_level-4:1;
    int h = level_prop->side_level-1;
    if (!land_only) h += 1+bounded_random_r(srng, 8);
    if (h >= level_prop->cells_y) h = level_prop->side_level;
    if (h >= level_prop->cells_y) h = level_prop->cells_y/2;

    heightmap[cx+cz*level_prop->cells_x] = h;
    level_prop->spawn.y = (h+3) *32+16;

    for(int e = bounded_random_r(srng, 6) + 2; e>0; e--) {
	map_random_t rng[1];
	seed_rng(srng, rng);
	int s = (1<<(e-1));
	for(int r=s; r<rm; r+=s) {
	    for(int x=cx-r; x<cx+r; x+=s) gen_plain(rng, heightmap, x, cz-r, s, min_y);
	    for(int z=cz-r; z<cz+r; z+=s) gen_plain(rng, heightmap, cx+r, z, s, min_y);
	    for(int x=cx+r; x>cx-r; x-=s) gen_plain(rng, heightmap, x, cz+r, s, min_y);
	    for(int z=cz+r; z>cz-r; z-=s) gen_plain(rng, heightmap, cx-r, z, s, min_y);
	}
    }

    {
	map_random_t rng[1];
	seed_rng(srng, rng);
	heightmap[cx+cz*level_prop->cells_x] = 0;
	gen_plain(rng, heightmap, cx, cz, 1, min_y);
    }
}

LOCAL void
gen_plain(map_random_t *rng, uint16_t * heightmap, int tx, int tz, int s, int mmin_y)
{
    if (tx < 0 || tx >= level_prop->cells_x) return;
    if (tz < 0 || tz >= level_prop->cells_z) return;
    int mx = level_prop->cells_x;

    int min_y = mmin_y, max_y = level_prop->cells_y-1, avg=0, avcnt=0;
    for(int dx = -s; dx<2*s; dx++)
	for(int dz = -s; dz<2*s; dz++)
	{
	    int x = tx+dx, z = tz+dz;
	    if (dx == 0 && dz == 0) continue;
	    if (x < 0 || x >= level_prop->cells_x) continue;
	    if (z < 0 || z >= level_prop->cells_z) continue;
	    int h = heightmap[x+z*mx];
	    if (h == 0) continue;
	    if (min_y < h-s) min_y = h-s;
	    if (max_y > h+s) max_y = h+s;
	    avg += h; avcnt++;
	}

    if (s >= 32) {
	max_y = max_y + (max_y-min_y);
	min_y = min_y - (max_y-min_y);
	if (max_y >= level_prop->cells_y) max_y = level_prop->cells_y-1;
	if (min_y < 1) min_y = 1;
    }

    if (min_y > max_y && avcnt) {
	avg += avcnt/2; avg /= avcnt;
	heightmap[tx+tz*mx] = avg;
	return;
    }
    if (min_y > max_y) return;
    if (min_y >= max_y) {
	heightmap[tx+tz*mx] = min_y;
    } else if (s>1) {
	heightmap[tx+tz*mx] = min_y + bounded_random_r(rng, max_y-min_y+1);
    } else {
	avg += avcnt/2; avg /= avcnt;
	heightmap[tx+tz*mx] = avg;
    }
}
