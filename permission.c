
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
    // Nobody owns the void.
    if (levelname == 0) {
	if (level_prop && level_blocks && expand_plus == 0) levelname = current_level_name;
	else {
	    if (!quiet)
		printf_chat("&WPermission denied on current level");
	    return 0;
	}
    }

    // Flag for all users are level owner on all levels
    if (server->all_levels_owned)
	return 1;

    // Your level by name
    char userlevel[256] = "";
    if (server->allow_user_levels) {
	saprintf(userlevel, "%s+", user_id);
	if (expand_plus && strcmp(levelname, "+") == 0)
	    strcpy(levelname, userlevel);
	if (strcmp(levelname, userlevel) == 0)
	    return 1;
    }

    // Admin is all levels owner too
    if (perm_is_admin())
	return 1;

    // Everyone is op for this level
    if (level_prop && level_prop->other_user_op && strcmp(levelname, current_level_name) == 0)
	return 1;

    // Explicit OP list for this level
    // Problem; this is a short fixed array, not unbounded.
    if (level_prop && level_prop->op_user_list[0] && *user_id && strcmp(levelname, current_level_name) == 0) {
	if (in_strlist(level_prop->op_user_list, user_id))
	    return 1;
    }

    // Okay, not an owner or surrogate.
    if (!quiet) {
	if (levelname && strcmp(levelname, "+") == 0)
	    printf_chat("&WPersonal levels are not enabled");
	else if (levelname && *levelname)
	    printf_chat("&WYou do not have permission for level \"%s\"", levelname);
	else
	    printf_chat("&WPermission denied for that level.");
    }
    return 0;
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
