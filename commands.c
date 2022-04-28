#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

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
	send_message_pkt(0, "&e/quit, /rq, /crash, /reload, /place, /save");
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

    if (strcasecmp(cmd, "crash") == 0 || strcasecmp(cmd, "servercrash") == 0) {
	char * crash_type = strtok(0,"");
	assert(!crash_type || strcmp(crash_type, "666"));
	logout("Server crash! Error code 42");
    }

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
	if (cnt != 1 && cnt != 4 && cnt != 7) {
	    send_message_pkt(0, "&cUsage: /place b [x y z] [X Y Z]");
	    return;
	}

	pkt_setblock pkt;
	pkt.heldblock = args[0];
	pkt.mode = (pkt.heldblock != Block_Air);
	pkt.block = pkt.mode?pkt.heldblock:Block_Air;
	if (cnt == 1) {
	    pkt.coord.x = player_posn.x/32;	// check range [0..cells_x)
	    pkt.coord.y = (player_posn.y+16)/32;// Add half a block for slabs
	    pkt.coord.z = player_posn.z/32;
	    update_block(pkt);
	} else if(cnt == 4) {
	    pkt.coord.x = args[1];
	    pkt.coord.y = args[2];
	    pkt.coord.z = args[3];
	    update_block(pkt);
	} else {
	    int i, x, y, z;
	    int max[3] = { level_prop->cells_x-1, level_prop->cells_y-1, level_prop->cells_z-1};
	    for(i=0; i<3; i++) {
		if (args[i+1]<0 && args[i+4]<0) return; // Off the map.
		if (args[i+1]>max[i] && args[i+4]>max[i]) return; // Off the map.
	    }
	    for(i=0; i<6; i++) { // Crop to map.
		if (args[i+1]<0) args[i+1] = 0;
		if (args[i+1]>max[i%3]) args[i+1] = max[i%3];
	    }

	    pkt.heldblock = args[0];
	    pkt.mode = (pkt.heldblock != Block_Air);
	    pkt.block = pkt.mode?pkt.heldblock:Block_Air;
	    for(y=args[2]; y<=args[5]; y++)
		for(x=args[1]; x<=args[4]; x++)
		    for(z=args[3]; z<=args[6]; z++)
		    {
			pkt.coord.x = x;
			pkt.coord.y = y;
			pkt.coord.z = z;
			update_block(pkt);
		    }
	}
	return;
    }

    if (strcasecmp(cmd, "save") == 0)
    {
	char * arg = strtok(0, "");
	if (!client_ipv4_localhost)
	    send_message_pkt(0, "&cUsage: /save [Auth] filename");
	else if (!arg || !*arg)
	    send_message_pkt(0, "&cUsage: /save filename");
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
