
#include "permission.h"

#if INTERFACE
inline static int
perm_is_admin()
{
    return my_user.user_group == 0 ||
	(client_trusted && !ini_settings->disallow_ip_admin);
}
#endif

int client_trusted = 0;

/* Does user have extended permissions for "levelname"
 */
int
perm_level_check(char * levelname, int expand_plus, int quiet)
{
    if (levelname == 0) {
	if (level_prop && level_blocks && expand_plus == 0) levelname = current_level_name;
	else {
	    if (!quiet)
		printf_chat("&WPermission denied on current level");
	    return 0;
	}
    }
    char userlevel[256] = "";
    if (server->allow_user_levels) {
	saprintf(userlevel, "%s+", user_id);
	if (expand_plus && strcmp(levelname, "+") == 0)
	    strcpy(levelname, userlevel);
	if (strcmp(levelname, userlevel) == 0)
	    return 1;
    }

    // TODO: Levels with list of users who can access.

    if (!perm_is_admin()) {
	if (!quiet) {
	    if (*userlevel)
		printf_chat("&WPermission denied, your level name is '%s'", userlevel);
	    else if (levelname == current_level_name)
		printf_chat("&WYou do not have permission for level \"%s\"", levelname);
	    else
		printf_chat("&WPermission denied");
	}
        return 0;
    }
    return 1;
}

int
perm_block_check(uint64_t est_block_count)
{
    if (command_limits.max_user_blocks == 0) return 1;
    if (est_block_count <= command_limits.max_user_blocks) return 1;
    if (perm_is_admin()) return 1;

    if (server->allow_user_levels) {
	char userlevel[256] = "";
	saprintf(userlevel, "%s+", user_id);
	if (strcmp(current_level_name, userlevel) == 0)
	    return 1;
    }

    return 0;
}
