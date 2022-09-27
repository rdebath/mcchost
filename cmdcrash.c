#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <signal.h>

#include "cmdcrash.h"

// I expect some people won't like that this does _real_ crashes,
// so if you -DFAKECRASH it becomes just another exit command.
// Except the 646 one, which crashes the _client_.
//
// If you don't like that either, delete this file.

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
#define CMD_CRASH \
    {N"crash", &cmd_crash }, {N"servercrash", &cmd_quit, .dup=1}
#endif

int *crash_ptr = 0;

void
cmd_crash(char * UNUSED(cmd), char * arg)
{
    close_userdb();

    char * crash_type = arg;
    if (!crash_type) return cmd_help(0, "crash");

    if (strcmp(crash_type, "646") == 0) {
	if (cpe_enabled)
	    return printf_chat("You have CPE enabled, 646 is fixed.");

	uint8_t packetbuf[128];
	uint8_t *p = packetbuf;
	char txtbuf[64] = "&cCrasher\370\003&0";
	*p++ = PKID_MESSAGE;
	*p++ = 255;
	memcpy(p, txtbuf, sizeof(txtbuf));
	p += sizeof(txtbuf);
	write_to_remote(packetbuf, p-packetbuf);
	flush_to_remote();

	return printf_chat("If you see this it didn't crash");
    }

#ifdef FAKECRASH
    char * emsg = 0;
    if (strcmp(crash_type, "666") == 0) {
	emsg = "kicked by signal Aborted (6) (core dumped)";
    } else if (strcmp(crash_type, "606") == 0) {
	emsg = "kicked by signal Segmentation fault (11) (core dumped)";
    } else if (strcmp(crash_type, "696") == 0) {
	emsg = "kicked by signal Killed (9)";
    } else if (strcmp(crash_type, "616") == 0) {
	emsg = "kicked by panic with exit status 1";
    }
    if (emsg) {
	printf_chat("@&W- &7%s &W%s", user_id, emsg);
	stop_user();
	exit(0);
    }
    if (crash_type) {
	char cbuf[1024];
	saprintf(cbuf, "Server crash! Error code %s", crash_type);
	crashed(cbuf);
    }
#else
    assert(strcmp(crash_type, "666"));

    if (crash_type && strcmp(crash_type, "696") == 0)
	kill(getpid(), SIGKILL);
    else if (strcmp(crash_type, "616") == 0)
	exit(EXIT_FAILURE);
    else if (strcmp(crash_type, "606") == 0)
	printf_chat("Value should fail %d", *crash_ptr);
    else

    else if (crash_type) {
	char cbuf[1024];
	saprintf(cbuf, "Server crash! Error code %s", crash_type);
	fatal(cbuf);
    }
#endif
}
