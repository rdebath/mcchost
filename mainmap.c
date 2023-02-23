
#include "mainmap.h"

void
open_main_level()
{
    stop_shared();

    if (ini_settings->void_for_login && !user_authenticated) {
	cmd_void(0,0);
    } else {
	char fixedname[NB_SLEN];
	fix_fname(fixedname, sizeof(fixedname), main_level());

	if (start_level(main_level(), fixedname, 0))
	    open_level_files(main_level(), 0, 0, fixedname, 0);

	if (!level_prop) cmd_void(0, 0);

	if (level_prop) player_posn = level_prop->spawn;
	send_map_file();
    }
}

void
cmd_void(char * cmd, char * UNUSED(arg))
{
    char * voidname = "<void>";

    char fixedname[NB_SLEN];
    fix_fname(fixedname, sizeof(fixedname), voidname);
    stop_shared();
    (void)start_level(voidname, fixedname, -1);
    send_map_file();

    if (cmd) {
	printf_chat("@%s&S was sucked into the void.", player_list_name.c);
	printf_chat("&SYou jumped into the void");
    }
}
