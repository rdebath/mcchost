
#include "cmdignore.h"

#if INTERFACE
#define UCMD_IGNORE \
    {N"ignore", &cmd_ignore, CMD_HELPARG}
#endif

/*HELP ignore H_CMD
&T/Ignore [name]
All chat from the player with [name] is ignored.
Using the same [name] again will unignore.
&T/Ignore -list
List players being ignored.
*/

void
cmd_ignore(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg_rest();

    if (strcasecmp("list", str1) == 0 || strcasecmp("-list", str1) == 0) {
	if (my_user.ignore_list && *my_user.ignore_list) {
	    char * s = strdup(my_user.ignore_list);
	    for(char * p = s; *p; p++)
		if (*p == ',') *p = ' ';

	    printf_chat("&WCurrently ignoring the following players: &S%s", s);
	    free(s);
	} else
	    printf_chat("&WCurrently ignoring nobody");
	return;
    }

    if (str1 || str2) {
	char user_name[128];
	int uid = find_player(str1, user_name, sizeof(user_name), 1, 0);
	if (uid < 0) return;

	read_ini_file_fields(&my_user);

	if (in_strlist(my_user.ignore_list, user_name)) {
	    printf_chat("&SNo longer ignoring %s", user_name);
	    del_strlist(my_user.ignore_list, user_name);
	    if (!*my_user.ignore_list) {
		free(my_user.ignore_list);
		my_user.ignore_list = 0;
	    }
	} else {
	    char * os = my_user.ignore_list;
	    int l = (os?strlen(os):0) + strlen(user_name) + 2;
	    char * ns = malloc(l);
	    if (ns) {
		strcpy(ns, user_name);
		if (my_user.ignore_list && *my_user.ignore_list) {
		    strcat(ns, ",");
		    strcat(ns, my_user.ignore_list);
		}
		if (my_user.ignore_list) free(my_user.ignore_list);
		my_user.ignore_list = ns;

		printf_chat("&WNow ignoring %s", user_name);
	    }

	}
	write_current_user(3);
	return;
    }

}

