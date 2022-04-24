#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

void
run_command(char * msg)
{
    char buf[256];

    if (msg[1] == 0) return; //TODO: Repeat command.

    // TODO: Commands.
    if (strcasecmp(msg, "/help") == 0) {
	send_message_pkt(0, "Help command: TODO ...");
	// /faq -> /help faq...
	// /news -> /help news...
	// /view -> /help view...
	return;
    }

    // Some trivial commands.
    if (strcasecmp(msg, "/quit") == 0) logout("Left the game.");
    if (strcasecmp(msg, "/rq") == 0) logout("RAGEQUIT!!");
    if (strcasecmp(msg, "/hax") == 0 || strcasecmp(msg, "/hacks") == 0)
	kicked("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

    if (strcasecmp(msg, "/crash") == 0)
	abort(), logout("Server crash! Error code 42");
    if (strcasecmp(msg, "/reload") == 0)
    {
	send_map_reload();
	//level_block_queue->generation += 2;
	return;
    }

    strcpy(buf, "&eUnknown command \"");
    sscanf(msg+1, "%s", buf+strlen(buf));
    buf[60] = 0;
    strcat(buf, "\".");
    send_message_pkt(0, buf);
    return;

}
