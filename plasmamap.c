
#include "plasmamap.h"

void
gen_plasma_map(char * seed, int pre_zeroed)
{
    map_random_t rng[1];
    map_init_rng(rng, seed);

    uint16_t *heightmap = calloc(level_prop->cells_x*level_prop->cells_z, sizeof(*heightmap));

    gen_plasma_heightmap(rng, heightmap);

    int x, y, z;
    for(z=0; z<level_prop->cells_z; z++)
	for(x=0; x<level_prop->cells_x; x++)
	    level_blocks[World_Pack(x,0,z)] =
		heightmap[x+z*level_prop->cells_x] %
		    (Block_Black-Block_Red)+Block_Red;

    if (!pre_zeroed)
	for(y=1; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		    level_blocks[World_Pack(x,y,z)] = Block_Air;
}

LOCAL void
gen_plasma_heightmap(map_random_t *rng, uint16_t * heightmap)
{
    int cx = level_prop->cells_x/2;
    int cz = level_prop->cells_z/2;
    int rm = level_prop->cells_x>level_prop->cells_z?level_prop->cells_x:level_prop->cells_z;
    rm = rm/2 + 2;

    int h = 5000 + bounded_random_r(rng, 12);

    heightmap[cx+cz*level_prop->cells_x] = h;

    int first_e = 5 + bounded_random_r(rng, 3);
    for(int e = first_e; e>0; e--) {
	int s = (1<<(e-1));
	for(int r=s; r<rm; r+=s) {
	    for(int x=cx-r+s; x<=cx+r; x+=s) gen_plasma(rng, heightmap, x, cz-r, s);
	    for(int z=cz-r+s; z<=cz+r; z+=s) gen_plasma(rng, heightmap, cx+r, z, s);
	    for(int x=cx+r-s; x>=cx-r; x-=s) gen_plasma(rng, heightmap, x, cz+r, s);
	    for(int z=cz+r-s; z>=cz-r; z-=s) gen_plasma(rng, heightmap, cx-r, z, s);
	}
    }

    heightmap[cx+cz*level_prop->cells_x] = 0;
    gen_plasma(rng, heightmap, cx, cz, 1);
}

LOCAL void
gen_plasma(map_random_t *rng, uint16_t * heightmap, int tx, int tz, int s)
{
    if (tx < 0 || tx >= level_prop->cells_x) return;
    if (tz < 0 || tz >= level_prop->cells_z) return;
    int mx = level_prop->cells_x;

    int min_y = 1, max_y = 10000, avg=0, avcnt=0;
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
