
#include "mainmap.h"

void
open_main_level()
{
    stop_shared();
    char * m = main_level();

    if (!*m || (ini_settings->void_for_login && !user_authenticated)) {
	cmd_void(0,0);
    } else {
	char fixedname[NB_SLEN];
	fix_fname(fixedname, sizeof(fixedname), m);

	if (start_level(m, 0))
	    open_level_files(m, 0, 0, fixedname, 0);

	if (!level_prop) cmd_void(0, 0);

	if (level_prop) player_posn = level_prop->spawn;
	send_map_file(0);
    }
}

void
cmd_void(char * cmd, char * UNUSED(arg))
{
    char * voidname = "<void>";

    stop_shared();
    (void)start_level(voidname, -1);
    send_map_file(0);

    if (cmd) {
	printf_chat("@%s&S was sucked into the void.", player_list_name.c);
	printf_chat("&SYou jumped into the void");
    }
}
