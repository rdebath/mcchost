
#include "cmdresizelvl.h"

/*HELP resizelvl H_CMD
&T/resizelvl ...
*/

#if INTERFACE
#define UCMD_RESIZELVL \
    {N"resizelvl", &cmd_resizelvl, CMD_PERM_LEVEL, CMD_HELPARG}
#endif

pid_t level_resize_pid = 0;

void
cmd_resizelvl(char * UNUSED(cmd), char * arg)
{
    if (!level_prop || level_prop->disallowchange || level_prop->readonly ||
	    current_level_backup_id != 0)
	return printf_chat("&WLevel won't be saved so it cannot be resized");

    char * sx = strarg(arg);
    char * sy = strarg(0);
    char * sz = strarg(0);
    char * confstr = strarg(0);

    int x=0,y=0,z=0;
    char *e;

    x = strtol(sx, &e, 10);
    if (*e || x<1 || x>MAPDIMMAX) { printf_chat("&WValue %s is not a valid dimension", sx); return; }

    y = strtol(sy, &e, 10);
    if (*e || y<1 || y>MAPDIMMAX) { printf_chat("&WValue %s is not a valid dimension", sy); return; }

    z = strtol(sz, &e, 10);
    if (*e || z<1 || z>MAPDIMMAX) { printf_chat("&WValue %s is not a valid dimension", sz); return; }

    if ((int64_t)x*y*z > INT_MAX) {
	printf_chat("&WNew map dimensions are too large to use, %jd cells",
	    (intmax_t)x*y*z);
	return;
    }

    if (x < level_prop->cells_x || y < level_prop->cells_y || z < level_prop->cells_z) {
	if (!confstr || strcasecmp(confstr, "confirm") != 0) {
	    printf_chat("&WNew level dimensions are smaller than the current dimensions, you will lose blocks.");
	    printf_chat("Type &T/ResizeLvl %d %d %d confirm&S if you're sure.", x, y, z);
	    return;
	}
    }

    resize_current_level(x, y, z);
}

int
resize_current_level(int new_x, int new_y, int new_z)
{
    if (level_processor_pid || current_level_backup_id != 0) return -1;
    if ((level_processor_pid = fork()) != 0) {
	if (level_processor_pid<0) {
	    level_processor_pid = 0;
	    perror("fork()");
	    return -1;
	}
	return 1;
    }

    if (line_ofd >= 0) close(line_ofd);
    if (line_ifd >= 0 && line_ofd != line_ifd) close(line_ifd);
    line_ifd = line_ofd = -1;

    // 1) Create a new blocks file with new size.
    if (open_tmp_blocks(shdat.level_fixed_name, new_x, new_y, new_z) < 0)
	exit(0);

    map_block_t * level_tmpblocks = shdat.dat[SHMID_TMPBLOCKS].ptr;
    int64_t total_blocks = (int64_t)new_x*new_y*new_z;

    // This shouldn't be needed ...
    if (!shdat.dat[SHMID_TMPBLOCKS].zeroed)
	memset(level_tmpblocks, 0, total_blocks*sizeof(map_block_t));

    // Populate the magic
    map_len_t test_map = {0};
    test_map.magic_no = TY_MAGIC;
    test_map.cells_x = new_x;
    test_map.cells_y = new_y;
    test_map.cells_z = new_z;
    test_map.blksz = sizeof(map_block_t);
    memcpy(level_blocks_len_t, &test_map, sizeof(map_len_t));

    // 2) Copy common level blocks
    copyblocks(
	level_tmpblocks, new_x, new_y, new_z, level_blocks,
	level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);

    // 3) Rename file.
    char sharename_tmp[256];
    sprintf(sharename_tmp, LEVEL_TMPBLOCKS_NAME, shdat.level_fixed_name);
    char sharename_blk[256];
    sprintf(sharename_blk, LEVEL_BLOCKS_NAME, shdat.level_fixed_name);
    if (rename(sharename_tmp, sharename_blk) < 0) {
	perror("rename(sharename_tmp, sharename_blk)");
	printf_chat("&WResize rename failed, aborting");
	exit(1);
    }

    // Mark the props as resized. (WARN: Others may see this!)
    lock_fn(level_lock);
    level_prop->cells_x = new_x;
    level_prop->cells_y = new_y;
    level_prop->cells_z = new_z;
    level_prop->total_blocks = total_blocks;
    level_prop->dirty_save = 1;
    level_prop->last_modified = time(0);
    if (level_block_queue)
	level_block_queue->generation += 2;
    unlock_fn(level_lock);

    // Now force a reload for everyone.
    char * lvlname = strdup(current_level_name);
    stop_shared();
    *current_level_name = 0;
    current_level_backup_id = -1;
    summon_list_t * summon_list = calloc(MAX_USER, sizeof(summon_list_t));
    wait_for_forced_unload(lvlname, summon_list);
    if (summon_list[0].active)
	return_users_to_level(lvlname, summon_list);
    exit(0);
}

#define World_Pack_old(x, y, z) (((y) * (uintptr_t)old_z + (z)) * old_x + (x))
#define World_Pack_new(x, y, z) (((y) * (uintptr_t)new_z + (z)) * new_x + (x))

void
copyblocks(
    map_block_t *new_blocks, int new_x, int new_y, int new_z,
    map_block_t *old_blocks, int old_x, int old_y, int old_z)
{
    int high_x = new_x>old_x?old_x:new_x;
    int high_y = new_y>old_y?old_y:new_y;
    int high_z = new_z>old_z?old_z:new_z;

    int x, y, z;

    for(y=0; y<high_y; y++)
    {
	for(z=0; z<high_z; z++)
	{
	    for(x=0; x<high_x; x++)
	    {
		new_blocks[World_Pack_new(x,y,z)] = old_blocks[World_Pack_old(x,y,z)];
	    }
	}
    }
}
