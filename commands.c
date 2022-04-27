#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "commands.h"

// TODO:
// Commands.
// Command expansion/aliases
//      Which way? /cuboid-walls <--> /cuboid walls
//      /os map <--> /map

// Check strtok to one that can use quotes too. 2nd arg is "use rest" ?

void
run_command(char * msg)
{
    char buf[256];

    if (msg[1] == 0) return; //TODO: Repeat command.

    char * cmd = strtok(msg+1, " ");
    if (cmd == 0) return;

    if (strcasecmp(cmd, "help") == 0) {
	send_message_pkt(0, "&cHelp command: TODO ...");
	// /faq -> /help faq...
	// /news -> /help news...
	// /view -> /help view...
	return;
    }

    // Some trivial commands.
    if (strcasecmp(cmd, "quit") == 0) {
	char * arg = strtok(0, "");
	if (arg) {
	    snprintf(buf, sizeof(buf), "Left the game: %s", arg);
	    logout(buf);
	} else
	    logout("Left the game.");
    }
    if (strcasecmp(cmd, "rq") == 0) logout("RAGEQUIT!!");

    if (strcasecmp(cmd, "hax") == 0 || strcasecmp(cmd, "hacks") == 0)
	kicked("Your IP has been backtraced + reported to FBI Cyber Crimes Unit.");

    if (strcasecmp(cmd, "crash") == 0 || strcasecmp(cmd, "servercrash") == 0)
	strtok(0,"") ? abort(): logout("Server crash! Error code 42");

    if (strcasecmp(cmd, "reload") == 0)
    {
	char * arg = strtok(0, " ");
	if (arg == 0)
	    send_map_reload();
	else if (strcasecmp(arg, "all") == 0)
	    level_block_queue->generation += 2;
	else
	    send_message_pkt(0, "&cUsage: /reload [all]");
	return;
    }

    if (strcasecmp(cmd, "place") == 0 || strcasecmp(cmd, "pl") == 0)
    {
	int args[8] = {0};
	int cnt = 0;
	for(int i = 0; i<8; i++) {
	    char * p = strtok(0, " ");
	    if (p == 0) break;
	    // if (i == 0) check block name; else
	    // check for relative x/y/z posn
	    args[i] = atoi(p);
	    cnt = i+1;
	}
	if (cnt != 1 && cnt != 4) {
	    send_message_pkt(0, "&cUsage: /place b [x y z] [X Y Z]");
	    return;
	}

	pkt_setblock pkt;
	if (cnt == 1) {
	    pkt.coord.x = player_posn.x/32;	// check range [0..cells_x)
	    pkt.coord.y = player_posn.y/32-1;
	    pkt.coord.z = player_posn.z/32;
	} else {
	    pkt.coord.x = args[1];
	    pkt.coord.y = args[2];
	    pkt.coord.z = args[3];
	}
	pkt.heldblock = args[0];
	pkt.mode = (pkt.heldblock != Block_Air);
	pkt.block = pkt.mode?pkt.heldblock:Block_Air;
	update_block(pkt);
	return;
    }

    if (strcasecmp(cmd, "save") == 0)
    {
	char * arg = strtok(0, "");
	if (!arg || !*arg)
	    send_message_pkt(0, "&cUsage: /reload [all]");
	else {
	    save_map_to_file(arg);
	    send_message_pkt(0, "&eFile saved");
	}
	return;
    }

    snprintf(buf, sizeof(buf), "&eUnknown command \"%.20s\".", cmd);
    send_message_pkt(0, buf);
    return;

}
