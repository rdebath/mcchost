#include <fcntl.h>
#include <sys/stat.h>

#include "createmap.h"

/*
 * This populates a default map file properties.
 */

void
createmap(char * levelname)
{
    // Don't recreate a map that seems okay.
    if (level_prop->version_no == TY_LVERSION)
	if (level_prop->cells_x != 0 && level_prop->cells_y != 0 && level_prop->cells_z != 0)
	    return;

    // Save away the old size.
    xyzhv_t oldsize = {0};
    if (level_prop->magic_no == TY_MAGIC) {
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

    level_prop->time_created = time(0);

    if (!oldsize.valid)
	read_blockfile_size(levelname, &oldsize);

    patch_map_nulls(oldsize);

    open_blocks(levelname);

    init_block_file(0, level_blocks_zeroed);
}

void
init_map_null()
{
    *level_prop = (map_info_t){
	    .magic_no = TY_MAGIC, .magic_no2 = TY_MAGIC2,
	    .version_no = TY_LVERSION,
	    .cells_x = 0, .cells_y = 0, .cells_z = 0,
	    .total_blocks = 0,
	    .weather = 0, -1, -1, -1, -1, -1, -1,
	    .side_block = 7, 8, INT_MIN, -2,
	    .spawn = { INT_MIN, INT_MIN, INT_MIN },
	    .clouds_height = INT_MIN,
	    .clouds_speed = 256, 256, 128,
	    .click_distance = 160,
	    .hacks_jump = -1,
	};

    init_rand_gen(); // If required

    for(int i = 0; i<sizeof(level_prop->uuid); i++)
    {
        int by = bounded_random(256);
	level_prop->uuid[i] = by;
    }
    // Not documented, but make it the bytes for a real UUID (big endian).
    level_prop->uuid[6] &= 0x0F;
    level_prop->uuid[6] |= 0x40;
    level_prop->uuid[10] &= 0x3F;
    level_prop->uuid[10] |= 0x80;

    memcpy(level_prop->blockdef, default_blocks, sizeof(default_blocks));

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
    if (level_prop->cells_x > 65535 || level_prop->cells_y > 65535 || level_prop->cells_z > 65535 ||
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

    if (level_prop->spawn.x == INT_MIN)
	level_prop->spawn.x = level_prop->cells_x/2   *32+16;

    if (level_prop->spawn.y == INT_MIN)
	level_prop->spawn.y = level_prop->cells_y*3/4 *32+16;

    if (level_prop->spawn.z == INT_MIN)
	level_prop->spawn.z = level_prop->cells_z/2   *32+16;
}

void
init_block_file(uint64_t fallback_seed, int pre_zeroed)
{
    map_len_t test_map;
    memcpy(&test_map, level_blocks_len_t, sizeof(map_len_t));

    int blocks_valid = 1;
    if (test_map.magic_no != TY_MAGIC) blocks_valid = 0;
    if (test_map.cells_x != level_prop->cells_x) blocks_valid = 0;
    if (test_map.cells_y != level_prop->cells_y) blocks_valid = 0;
    if (test_map.cells_z != level_prop->cells_z) blocks_valid = 0;

    if (blocks_valid)
	level_prop->dirty_save = 1;
    else {
	test_map.magic_no = TY_MAGIC;
	test_map.cells_x = level_prop->cells_x;
	test_map.cells_y = level_prop->cells_y;
	test_map.cells_z = level_prop->cells_z;
	test_map.blksz = sizeof(block_t);
	memcpy(level_blocks_len_t, &test_map, sizeof(map_len_t));

	init_level_blocks(fallback_seed, pre_zeroed);
    }
}

// These flags are not strictly required, but probably a good idea
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/* Check the file
 */
void
read_blockfile_size(char * levelname, xyzhv_t * oldsize)
{
    char sharename[256];
    sprintf(sharename, LEVEL_BLOCKS_NAME, levelname);

    int sz = 1;
    {
        int p = sysconf(_SC_PAGESIZE);
        sz += p - 1;
        sz -= sz % p;
    }

    struct stat st;
    if (stat(sharename, &st) < 0 || (st.st_mode & S_IFMT) == 0) return; // Nope
    if (st.st_size < sz) return; // Too small

    int fd = open(sharename, O_RDONLY|O_NOFOLLOW|O_CLOEXEC, 0600);
    if (fd < 0) return; // Okay, ... weird.

    lseek(fd, -sizeof(map_len_t), SEEK_END);

    union {
	char buf[sizeof(map_len_t)];
	map_len_t size;
    } b;

    int cc = read(fd, b.buf, sizeof(b));
    close(fd);
    if (cc <= 0) return;

    if (b.size.magic_no == TY_MAGIC) {
	unsigned x = b.size.cells_x;
	unsigned y = b.size.cells_y;
	unsigned z = b.size.cells_z;
	if (x>65535||y>65535||z>65535||x==0||y==0||z==0|| (int64_t)x*y*z > INT_MAX)
	    return;

	oldsize->x = x;
	oldsize->y = y;
	oldsize->z = z;
	oldsize->valid = 1;
    }
}

void
init_level_blocks(uint64_t fallback_seed, int pre_zeroed)
{
    int x, y, z, y1, quiet = 0;
    struct timeval start;
    gettimeofday(&start, 0);

    if (strcasecmp(level_prop->theme, "pixel") == 0) {
	// Pixel: White sides and bedrock bottom layer
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
	// Empty: Bedrock bottom layer
	level_prop->side_level = 1;
	level_prop->seed[0] = 0;
	for(y=0; y<level_prop->cells_y; y++) {
	    if (y == 1 && pre_zeroed) break;
	    block_t px = Block_Air;
	    if (y==0) px = Block_Bedrock;
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		    level_blocks[World_Pack(x,y,z)] = px;
	}

    } else if (strcasecmp(level_prop->theme, "air") == 0) {
	// Air: Entire map is air
	level_prop->seed[0] = 0;
	if (!pre_zeroed)
	    for(y=0; y<level_prop->cells_y; y++)
		for(z=0; z<level_prop->cells_z; z++)
		    for(x=0; x<level_prop->cells_x; x++)
			level_blocks[World_Pack(x,y,z)] = Block_Air;

    } else if (strcasecmp(level_prop->theme, "rainbow") == 0 ||
               strcasecmp(level_prop->theme, "bw") == 0) {
	// Rainbow: Sides and bottom layer are random choice of colour blocks.
	int f = (strcasecmp(level_prop->theme, "bw") == 0);
	int min_blk = f?Block_Black:Block_Red;
	int mod_r = f?2:(Block_Grey + 1 - min_blk);
	int mul_r = f?2:1;
	level_prop->dirty_save |=
	    populate_map_seed(level_prop->seed, fallback_seed);
	map_random_t rng[1];
	map_init_rng(rng, level_prop->seed);
	level_prop->side_level = 1;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    if (    y == 0 || y == level_prop->cells_y-1 ||
			    x == 0 || x == level_prop->cells_x-1 ||
			    z == 0 || z == level_prop->cells_z-1) {
			// Includes Grey and Black?
			level_blocks[World_Pack(x,y,z)] =
			    bounded_random_r(rng, mod_r)*mul_r+min_blk;
		    } else {
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    }
		}

    } else if (strcasecmp(level_prop->theme, "space") == 0) {
	// Space: Bottom layer bedrock, 2nd layer, sides and top layer are
	// obsidian except for 1:100 which is Iron
	level_prop->dirty_save |=
	    populate_map_seed(level_prop->seed, fallback_seed);
	map_random_t rng[1];
	map_init_rng(rng, level_prop->seed);
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
		    else if (y==1 || y == level_prop->cells_y-1 ||
			     x == 0 || x == level_prop->cells_x-1 ||
			     z == 0 || z == level_prop->cells_z-1) {
			if (bounded_random_r(rng, 100) == 1)
			    px = Block_Iron;
			else
			    px = Block_Obsidian;
		    }
		    level_blocks[World_Pack(x,y,z)] = px;
		}

    } else if (strcasecmp(level_prop->theme, "plain") == 0) {
	level_prop->dirty_save |=
	    populate_map_seed(level_prop->seed, fallback_seed);
	gen_plain_flat_map(level_prop->seed);

    } else if (strcasecmp(level_prop->theme, "general") == 0) {
	level_prop->dirty_save |=
	    populate_map_seed(level_prop->seed, fallback_seed);
	gen_plain_map(level_prop->seed);

    } else if (strcasecmp(level_prop->theme, "plasma") == 0) {
	level_prop->dirty_save |=
	    populate_map_seed(level_prop->seed, fallback_seed);
	gen_plasma_map(level_prop->seed, pre_zeroed);
	level_prop->side_level = 1;

    } else {
	// Unknown map style is generated as unseeded flat.
	if (strcasecmp(level_prop->theme, "flat") == 0) {
	    if (level_prop->seed[0])
		level_prop->side_level = atoi(level_prop->seed);
	    level_prop->edge_block = 2;
	} else {
	    strcpy(level_prop->theme, "Flat");
	    level_prop->seed[0] = 0;
	    quiet = 1;
	}

	// Flat: Edge level is grass, everything below is dirt.
	y1 = level_prop->side_level;
	if (y1 == INT_MIN) y1 = level_prop->cells_y/2;
	y1--;
	for(y=0; y<level_prop->cells_y; y++) {
	    block_t b = Block_Air;
	    if (y>y1) {
		if (pre_zeroed) break;
	    } else if (y==y1)
		b = Block_Grass;
	    else
		b = Block_Dirt;
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		    level_blocks[World_Pack(x,y,z)] = b;
	}
    }

    // This assumes we always want to overwrite the ASCII format cw file.
    // If this line is removed we only do it for random seeds.
    level_prop->dirty_save = 1;

    saprintf(level_prop->software, "%s %s", SWNAME, Version);

    if (!quiet) {
	struct timeval now;
	gettimeofday(&now, 0);
	printlog("Map gen (%d,%d,%d) theme=%s%s%s, time %s",
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
	    level_prop->theme, level_prop->seed[0]?", seed=":"", level_prop->seed,
	    conv_ms_a((now.tv_sec-start.tv_sec)*1000.0+(now.tv_usec-start.tv_usec)/1000.0));
    }
}

char *
conv_ms_a(double ms)
{
    static char prtbuf[32];
    if (ms > 1000.0)
	snprintf(prtbuf, sizeof(prtbuf), "%.3fs", ms/1000.0);
    else
	snprintf(prtbuf, sizeof(prtbuf), "%.2fms", ms);
    return prtbuf;
}
