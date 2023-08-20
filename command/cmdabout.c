
#include "cmdabout.h"

#if INTERFACE
#define UCMD_ABOUT {N"about", &cmd_about}, {N"b", &cmd_about, CMD_ALIAS}
#endif

/*HELP about,b H_CMD
&T/about
Shows information about a block location
Alias: &T/b
*/

void
cmd_about(char * cmd, char * arg)
{
    if (!marks[0].valid) {
	if (!extn_messagetypes)
	    printf_chat("&SPlace or break a blocks to show information");
	player_mark_mode = 1;
	request_pending_marks("Selecting location for block info", cmd_about, cmd, arg);
	return;
    }

    if (!level_prop ||
	marks[0].x < 0 || marks[0].x >= level_prop->cells_x ||
        marks[0].y < 0 || marks[0].y >= level_prop->cells_y ||
        marks[0].z < 0 || marks[0].z >= level_prop->cells_z) {
	printf_chat("&SLocation outside of map area");
	clear_pending_marks();
	return;
    }

    if (!level_prop || !level_block_queue || !level_blocks) return;

    int x = marks[0].x, y = marks[0].y, z = marks[0].z;
    clear_pending_marks();

    uintptr_t index = World_Pack(x, y, z);
    block_t b = level_blocks[index];

    printf_chat("&SBlock (%d, %d, %d): %d = %s.", x, y, z, b, block_name(b));
}
