#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

#include "plainmap-flat.h"

// Simple map generator.
// A heightmap is created by creating a square spiral from the centre of the
// map and limiting height changed to +/- one block per block.
// Depending on the height in each position it's classified as land or sand.
// Water is placed above the sand.
// Trees and flowers are planted on the land. (no clumps)
void
gen_plain_map_old(map_random_t *rng, int UNUSED(flg))
{
    uint16_t *heightmap = calloc(level_prop->cells_x*level_prop->cells_z, sizeof(*heightmap));

    gen_plain_heightmap(rng, heightmap);

    int x, y, z;
    for(y=0; y<level_prop->cells_y; y++)
	for(z=0; z<level_prop->cells_z; z++)
	    for(x=0; x<level_prop->cells_x; x++)
	    if (y == 0 && level_prop->side_level > y+3)
		level_blocks[World_Pack(x,y,z)] = Block_Stone;
	    else
	    {
		int y1 = heightmap[x+z*level_prop->cells_x];
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

    int flowerrate = 25 + 5 * bounded_random_r(rng, 15);
    int treerate = 10 + 10 * bounded_random_r(rng, 40);
    for(z=3; z<level_prop->cells_z-3; z++)
	for(x=3; x<level_prop->cells_x-3; x++)
	{
	    int y1 = heightmap[x+z*level_prop->cells_x];
	    if (y1 <= level_prop->cells_y/2-1) continue;
	    if (y1+7 > level_prop->cells_y ||
		    bounded_random_r(rng, treerate) != 1) {
		y1 &= 0x7FFF;
		if (y1 >= level_prop->cells_y-1) continue;
		if (bounded_random_r(rng, flowerrate) == 1)
		    level_blocks[World_Pack(x,y1+1,z)] = Block_Dandelion;
		else if (bounded_random_r(rng, flowerrate) == 1)
		    level_blocks[World_Pack(x,y1+1,z)] = Block_Rose;
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

LOCAL void
gen_plain_heightmap(map_random_t *rng, uint16_t * heightmap)
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
gen_plain(map_random_t *rng, uint16_t * heightmap, int tx, int tz, int s)
{
    if (tx < 0 || tx >= level_prop->cells_x) return;
    if (tz < 0 || tz >= level_prop->cells_z) return;
    int mx = level_prop->cells_x;

    int min_y = 0, max_y = level_prop->cells_y-1, avg=0, avcnt=0;
    for(int dx = -s; dx<2*s; dx++)
	for(int dz = -s; dz<2*s; dz++)
	{
	    int x = tx+dx, z = tz+dz;

	    if (x < 0 || x >= level_prop->cells_x) continue;
	    if (z < 0 || z >= level_prop->cells_z) continue;
	    int h = heightmap[x+z*mx];
	    if (h == 0) continue;
	    if (min_y < h-1) min_y = h-1;
	    if (max_y > h+1) max_y = h+1;
	    avg += h; avcnt++;
	}

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
