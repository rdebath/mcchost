
#include "cmdreachdist.h"

#if INTERFACE
#define CMD_REACHDIST \
    {N"reachdistance", &cmd_reachdist}, {N"reach", &cmd_reachdist, .dup=1}
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
    char * str1 = strtok(arg, " ");

    if (!str1)
	return cmd_help(0, cmd);

    int rd = strtol(str1, 0, 0);
    if (rd > 1023) rd = 1023;
    if (rd < -1) rd = -1;
    printf_chat("Set your reach distance to %d blocks.", rd);

    my_user.click_distance = rd * 32767 / 1023;
    my_user.dirty = 1;
    send_clickdistance();
}
