
#include "permission.h"

int
perm_level_check(char * levelname, int expand_plus)
{
    if (levelname == 0) {
	if (expand_plus == 0) levelname = current_level_name;
	else {
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

    if (!client_trusted) {
	if (*userlevel)
	    printf_chat("&WPermission denied, your level name is '%s'", userlevel);
	else if (levelname == current_level_name)
	    printf_chat("&WYou do not have permission for level \"%s\"", levelname);
	else
	    printf_chat("&WPermission denied");
        return 0;
    }
    return 1;
}
