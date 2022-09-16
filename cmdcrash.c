#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <signal.h>

#include "cmdcrash.h"

/*HELP crash H_CMD
&T/crash
Crash the server, default is a fatal() error.
&T/crash 666&S Assertion failure
&T/crash 606&S Segmentation violation
&T/crash 696&S kill-9
&T/crash 616&S exit(EXIT_FAILURE)
&T/crash 646&S Client crash for Java 0.30
*/
#if INTERFACE
#define CMD_ZCRASH \
    {N"crash", &cmd_crash }, {N"servercrash", &cmd_quit, .dup=1}
#endif

int *crash_ptr = 0;

void
cmd_crash(char * UNUSED(cmd), char * arg)
{
    close_userdb();

    char * crash_type = arg;
    if (!crash_type) return cmd_help(0, "crash");
    assert(strcmp(crash_type, "666"));

    if (crash_type && strcmp(crash_type, "696") == 0)
	kill(getpid(), SIGKILL);
    else if (strcmp(crash_type, "616") == 0)
	exit(EXIT_FAILURE);
    else if (strcmp(crash_type, "606") == 0)
	printf_chat("Value should fail %d", *crash_ptr);
    else if (strcmp(crash_type, "646") == 0) {
	if (cpe_enabled)
	    return printf_chat("You have CPE enabled, 646 should be fixed.");

	uint8_t packetbuf[128];
	uint8_t *p = packetbuf;
	char txtbuf[64] = "&cCrasher\370\003&0";
	*p++ = PKID_MESSAGE;
	*p++ = 255;
	memcpy(p, txtbuf, sizeof(txtbuf));
	p += sizeof(txtbuf);
	write_to_remote(packetbuf, p-packetbuf);
	flush_to_remote();

	printf_chat("If you see this it didn't crash");

    } else if (crash_type) {
	char cbuf[1024];
	snprintf(cbuf, sizeof(cbuf), "Server crash! Error code %s", crash_type);
	fatal(cbuf);
    }
}
