#include <sys/types.h>
#include <signal.h>

#include "cmdrestart.h"

#if INTERFACE
#define CMD_RESTART {N"restart", &cmd_restart, CMD_PERM_ADMIN}
#endif

/*HELP restart H_CMD
&T/restart
Restarts the server listener process.
*/

void
cmd_restart(char * UNUSED(cmd), char * UNUSED(arg))
{
    if (alarm_handler_pid) {
	if (kill(alarm_handler_pid, SIGHUP) < 0) {
	    perror("kill(alarm_handler,SIGHUP)");
	    printf_chat("&WCannot signal listener process");
	} else
	    printf_chat("&SListener process restart triggered");
    } else if (inetd_mode)
	printf_chat("&SNo listener process exists for this session");
    else
	printf_chat("&WListener process not found");
}
