
#include "cmdtime.h"

#if INTERFACE
#define CMD_TIME \
    {N"time", &cmd_time}, {N"ti", &cmd_time, .dup=1}
#endif

/*HELP time,ti H_CMD
&T/Time [timezone]
Shows the server time
Shortcuts: /ti
*/

void
cmd_time(char * UNUSED(cmd), char *arg)
{
    int machine_time = 0;
    if (arg)
	snprintf(my_user.timezone, sizeof(my_user.timezone), "%s", arg);
    else if (!*my_user.timezone)
	if (strcmp(my_user.last_ip, "127.0.0.1") == 0)
	    machine_time = 1;

    if (!machine_time) {
	if (!*my_user.timezone && !arg && *my_user.last_ip)
	    detect_timezone();

	if (*my_user.timezone) setenv("TZ", my_user.timezone, 1); else setenv("TZ", "UTC+00:00", 1);
	tzset();
    }

    char tmbuf[64];

    time_t t[1];
    struct tm tm[1];
    time(t);
    localtime_r(t, tm);
    strftime(tmbuf, sizeof(tmbuf), "%F %T %z", tm);

    if (!machine_time) {
	unsetenv("TZ");
	tzset();
    }

    printf_chat("Current server time is %s", tmbuf);
}

void
detect_timezone()
{
    if (strcmp(my_user.last_ip, "127.0.0.1") == 0) return;
    if (server->use_utc_zone) {
	sprintf(my_user.timezone, "UTC+00:00");
	return;
    }

    // See also: http://ip-api.com/json/8.8.8.8
    char buf[256];
    sprintf(buf, "curl -s -f -m 2 https://ipapi.co/%s/utc_offset", my_user.last_ip);
    FILE * fd = popen(buf, "r");
    if (fd) {
	*buf = 0;
	int cc = fread(buf, 1, sizeof(buf)-1, fd);
	pclose(fd);
	buf[cc] = 0;
	if ((buf[0] == '+' || buf[0] == '-') && strlen(buf) >= 5) {
	    if (buf[0] == '+') buf[0] = '-'; else buf[0] = '+';
	    sprintf(my_user.timezone, "UTC%c%.2s:%.2s", buf[0], buf+1, buf+3);
	}
    }
}
