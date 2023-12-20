
#include "cmdreachdist.h"

#if INTERFACE
#define UCMD_REACHDIST \
    {N"reachdistance", &cmd_reachdist,CMD_HELPARG}, {N"reach", &cmd_reachdist, CMD_ALIAS}
#endif

/*HELP reachdistance,reach H_CMD
&T/ReachDistance [dist]
Sets the how far away you can modify blocks.
  The default reach distance is 5.
Shortcuts: /Reach
*/

void
cmd_reachdist(char * cmd, char *arg)
{
    if (!cpe_support(cmd, extn_clickdistance)) return;

    char * str1 = strarg(arg);

    read_ini_file_fields(&my_user);

    int rd = strtoi(str1, 0, 0);

    if (rd < 0) {
	my_user.click_distance = -1;
	printf_chat("Set your reach distance to map default.");
    } else {
	if (rd > 1023) rd = 1023;
	printf_chat("Set your reach distance to %d blocks.", rd);

	my_user.click_distance = rd * 32767 / 1023;
    }

    write_current_user(3);
    send_clickdistance();
}
