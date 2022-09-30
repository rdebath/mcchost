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

    level_prop->time_created = time(0);

    if (!oldsize.valid)
	read_blockfile_size(levelname, &oldsize);

    patch_map_nulls(oldsize);

    open_blocks(levelname);

    init_block_file(0);
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
	    .hacks_jump = -1,
	};

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
init_block_file(uint64_t fallback_seed)
{
    map_len_t test_map;
    memcpy(&test_map, (void*)(level_blocks+level_prop->total_blocks),
	    sizeof(map_len_t));

    int blocks_valid = 1;
    if (test_map.magic_no != MAP_MAGIC) blocks_valid = 0;
    if (test_map.cells_x != level_prop->cells_x) blocks_valid = 0;
    if (test_map.cells_y != level_prop->cells_y) blocks_valid = 0;
    if (test_map.cells_z != level_prop->cells_z) blocks_valid = 0;

    if (blocks_valid)
	level_prop->dirty_save = 1;
    else {
	test_map.magic_no = MAP_MAGIC;
	test_map.cells_x = level_prop->cells_x;
	test_map.cells_y = level_prop->cells_y;
	test_map.cells_z = level_prop->cells_z;
	memcpy((void*)(level_blocks+level_prop->total_blocks),
		&test_map, sizeof(map_len_t));

	init_level_blocks(fallback_seed);
    }
}

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

    lseek(fd, -sz, SEEK_END);
    char buf[sz];
    int cc = read(fd, buf, sizeof(buf));
    close(fd);
    if (cc <= 0) return;

    // Clunky search for the size of a map near the end of the block
    // file. Seems to work.
    union {
	char buf[sizeof(map_len_t)];
	map_len_t size;
    } b;
    for(int i=0; i<sz-sizeof(map_len_t); i++) {
	memcpy(b.buf, buf+i, sizeof(map_len_t));
	if (b.size.magic_no == MAP_MAGIC) {
	    unsigned x = b.size.cells_x;
	    unsigned y = b.size.cells_y;
	    unsigned z = b.size.cells_z;
	    if (x>16384||y>16384||z>16384|| (int64_t)x*y*z > INT_MAX)
		continue;

	    oldsize->x = x;
	    oldsize->y = y;
	    oldsize->z = z;
	    oldsize->valid = 1;
	    return;
	}
    }
}

void
init_level_blocks(uint64_t fallback_seed)
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
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    block_t px = Block_Air;
		    if (y==0) px = Block_Bedrock;
		    level_blocks[World_Pack(x,y,z)] = px;
		}

    } else if (strcasecmp(level_prop->theme, "air") == 0) {
	// Air: Entire map is air
	level_prop->seed[0] = 0;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		    level_blocks[World_Pack(x,y,z)] = Block_Air;

    } else if (strcasecmp(level_prop->theme, "rainbow") == 0) {
	// Rainbow: Sides and bottom layer are random choice of colour blocks.
	int has_seed = !!level_prop->seed[0];
	uint32_t seed = 1;
	if (has_seed) seed = strtol(level_prop->seed, 0, 0);
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
			    lehmer_pm(seed)%(Block_White-Block_Red)+Block_Red;
		    } else {
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    }
		}

    } else if (strcasecmp(level_prop->theme, "space") == 0) {
	// Space: Bottom layer bedrock, 2nd layer, sides and top layer are
	// obsidian except for 1:100 which is Iron
	int has_seed = !!level_prop->seed[0];
	uint32_t seed = 1;
	if (has_seed) seed = strtol(level_prop->seed, 0, 0);
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
			if (lehmer_pm(seed)%100 == 1)
			    px = Block_Iron;
			else
			    px = Block_Obsidian;
		    }
		    level_blocks[World_Pack(x,y,z)] = px;
		}

    } else if (strcasecmp(level_prop->theme, "plain") == 0) {
	if (!level_prop->seed[0] && fallback_seed)
	    sprintf(level_prop->seed, "%jd", (uintmax_t)fallback_seed);
	int has_seed = !!level_prop->seed[0];
	gen_plain_flat_map(level_prop->seed);
	level_prop->dirty_save = !has_seed;

    } else if (strcasecmp(level_prop->theme, "general") == 0) {
	if (!level_prop->seed[0] && fallback_seed)
	    sprintf(level_prop->seed, "%jd", (uintmax_t)fallback_seed);
	int has_seed = !!level_prop->seed[0];
	gen_plain_map(level_prop->seed);
	level_prop->dirty_save = !has_seed;

    } else if (strcasecmp(level_prop->theme, "plasma") == 0) {
	if (!level_prop->seed[0] && fallback_seed)
	    sprintf(level_prop->seed, "%jd", (uintmax_t)fallback_seed);
	int has_seed = !!level_prop->seed[0];
	gen_rainbow_map(level_prop->seed);
	level_prop->dirty_save = !has_seed;
	level_prop->side_level = 1;

    } else {
	// Unknown map style is generated as flat.
	if (strcasecmp(level_prop->theme, "flat") == 0) {
	    if (level_prop->seed[0])
		level_prop->side_level = atoi(level_prop->seed);
	} else {
	    strcpy(level_prop->theme, "Flat");
	    level_prop->seed[0] = 0;
	}
	quiet = 1;

	// Flat: Edge level is grass, everything below is dirt.
	y1 = level_prop->side_level-1;
	for(y=0; y<level_prop->cells_y; y++)
	    for(z=0; z<level_prop->cells_z; z++)
		for(x=0; x<level_prop->cells_x; x++)
		{
		    if (y>y1)
			level_blocks[World_Pack(x,y,z)] = Block_Air;
		    else if (y==y1)
			level_blocks[World_Pack(x,y,z)] = Block_Grass;
		    else
			level_blocks[World_Pack(x,y,z)] = Block_Dirt;
		}
    }

    if (!quiet) {
	struct timeval now;
	gettimeofday(&now, 0);
	printlog("Map gen (%d,%d,%d) theme=%s%s%s, time %.2fms",
	    level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
	    level_prop->theme, level_prop->seed[0]?", seed=":"", level_prop->seed,
	    (now.tv_sec-start.tv_sec)*1000.0+(now.tv_usec-start.tv_usec)/1000.0);
    }
}
