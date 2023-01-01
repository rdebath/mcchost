
#include "cmdreachdist.h"

#if INTERFACE
#define CMD_REACHDIST \
    {N"reachdistance", &cmd_reachdist,CMD_HELPARG}, {N"reach", &cmd_reachdist, .dup=1}
#endif

/*HELP reachdistance,reach H_CMD
&T/ReachDistance [dist]
Sets the how far away you can modify blocks.
  The default reach distance is 5.
Shortcuts: /Reach
*/

void
cmd_reachdist(char * UNUSED(cmd), char *arg)
{
    char * str1 = strtok(arg, " ");

    int rd = strtol(str1, 0, 0);
    if (rd > 1023) rd = 1023;
    if (rd < -1) rd = -1;
    printf_chat("Set your reach distance to %d blocks.", rd);

    read_ini_file_fields(&my_user);
    my_user.click_distance = rd * 32767 / 1023;
    write_current_user(3);

    send_clickdistance();
}
