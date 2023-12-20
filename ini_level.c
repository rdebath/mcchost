#include <zlib.h>

#include "ini_level.h"

/* ASCII mode for CW files is basically an ini file as saved by /isave
 * But in addition it has the ability to run /place and plain /cuboid
 * commands to populate a simple variation on flat or pixel.
 *
 * NB: Using lots of pl/z commands for a complex level will be much
 *     larger than just using a real CW file.
 *
 * Lines starting with "/pl", "/place", "/z", "/cuboid", "/m" and "/mark" are
 * known commands all other commands are ignored.
 */
int
try_asciimode(gzFile ifd, char * levelfile, char * ini_filename, char * ini_filename2, uint64_t fallback_seed)
{
    int quiet = 0;
    struct timeval start;
    gettimeofday(&start, 0);

    ini_state_t st = {.quiet = 0, .filename = levelfile};
    int blocks_opened = 0;

    printlog("Trying to load \"%s\" as an ini file", ini_filename);

    init_map_null();

    char ibuf[BUFSIZ];
    while(gzgets(ifd, ibuf, sizeof(ibuf))) {
	char * p = ibuf;
	while (*p == ' ' || *p == '\t') p++;
	if (*p == '/' ) {
	    if (!blocks_opened) {
		blocks_opened = 1;
		xyzhv_t oldsize = {0};
		patch_map_nulls(oldsize);

		if (open_blocks(levelfile) < 0)
		    return 0;
	    }
	    st.no_unsafe = 1;
	    apply_ini_command(p+1);
	    continue;
	}
        if (load_ini_line(&st, level_ini_fields, ibuf) != 0) {
	    level_prop->version_no = level_prop->magic_no = 0;
	    return 0;
	}
    }

    if (!blocks_opened) {
	xyzhv_t oldsize = {0};
	read_blockfile_size(levelfile, &oldsize);

	if (!oldsize.valid && ini_filename2 != 0 && *ini_filename2 &&
	    (level_prop->cells_x == 0 || level_prop->cells_y == 0 || level_prop->cells_z == 0)) {

	    if (access(ini_filename2, F_OK) == 0) {
		printlog("Trying old theme from map ini file.");
		map_info_t tgt[1] = {0};
		level_ini_tgt = tgt;
		load_ini_file(mcc_level_ini_fields, ini_filename2, 1, 0);
		level_ini_tgt = 0;
		if (tgt->cells_x > 0 && tgt->cells_y && tgt->cells_z) {
		    level_prop->cells_x = tgt->cells_x;
		    level_prop->cells_y = tgt->cells_y;
		    level_prop->cells_z = tgt->cells_z;

		    memcpy(level_prop->theme, tgt->theme, sizeof(level_prop->theme));
		    memcpy(level_prop->seed, tgt->seed, sizeof(level_prop->seed));
		    level_prop->texname = tgt->texname;
		}
	    }
	}

	patch_map_nulls(oldsize);

	if (open_blocks(levelfile) < 0)
	    return 0;

	init_block_file(fallback_seed, level_blocks_zeroed);
	quiet = 1;
    }

    if (!quiet) {
        struct timeval now;
        gettimeofday(&now, 0);
        printlog("INI load (%d,%d,%d) time %s",
            level_prop->cells_x, level_prop->cells_y, level_prop->cells_z,
            conv_ms_a((now.tv_sec-start.tv_sec)*1000.0+(now.tv_usec-start.tv_usec)/1000.0));
    }
    return 1;
}

static int cub_state = 0;
static int blk, mark[3];

LOCAL void
apply_ini_command(char * buf)
{
    char * cmd = strtok(buf, " ");
    if (cmd == 0) return;
    if (strcasecmp(cmd, "place") == 0 || strcasecmp(cmd, "pl") == 0) {
	int b = block_id(strtok(0, " "));
	int v = 0, a[3];
	for(int i = 0; i<3; i++) {
	    char * s = strtok(0, " ");
	    if (s) v = atoi(s);
	    a[i] = v;
	}
	if (a[0] < 0 || a[1] < 0 || a[2] < 0) return;
	if (a[0] >= level_prop->cells_x) return;
	if (a[1] >= level_prop->cells_y) return;
	if (a[2] >= level_prop->cells_z) return;
	if (b < 0 || b >= BLOCKMAX) b = 1;
	level_blocks[World_Pack(a[0],a[1],a[2])] = b;
	return;
    }
    if (strcasecmp(cmd, "cuboid") == 0 || strcasecmp(cmd, "z") == 0) {
	cub_state = 1;
	blk = block_id(strtok(0, ""));
	if (blk < 0 || blk >= BLOCKMAX) blk = 1;
	return;
    }
    if (strcasecmp(cmd, "mark") == 0 || strcasecmp(cmd, "m") == 0) {
	int v = 0, a[3];
	for(int i = 0; i<3; i++) {
	    char * s = strtok(0, " ");
	    if (s) v = atoi(s);
	    a[i] = v;
	}
	if (cub_state == 1) {
	    for(int i = 0; i<3; i++)
		mark[i] = a[i];
	    cub_state = 2;
	    return;
	}
	if (cub_state != 2) return;
	cub_state = 0;

	// Crop to map
	int m[3] = {level_prop->cells_x, level_prop->cells_y, level_prop->cells_z};
	for(int i = 0; i<3; i++) {
	    if (a[i] < mark[i]) { int t=mark[i]; mark[i]=a[i]; a[i]=t;}
	    if (a[i] < 0 && mark[i] < 0) return; // Off map
	    if (a[i] >= m[i] && mark[i] >= m[i]) return;
	    if (a[i] < 0) a[i] = 0;
	    if (mark[i] < 0) mark[i] = 0;
	    if (a[i] >= m[i]) a[i] = m[i]-1;
	    if (mark[i] >= m[i]) mark[i] = m[i]-1;
	}

	for(int y=mark[1]; y<a[1]+1; y++)
	    for(int z=mark[2]; z<a[2]+1; z++)
		for(int x=mark[0]; x<a[0]+1; x++)
		    level_blocks[World_Pack(x,y,z)] = blk;
    }
}
