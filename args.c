#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include "args.h"

/*
 * TODO:
 * +) scan for -dir first, then load defaults, then rescan command line.
 *
 * +) load/save level to *.cw and use for backups, restores and "unload".
 *    -- Add locking so one load at a time ?
 * +) /commands -- list help subjects with H_CMD.
 * +) Multiple Levels. (/newlvl, /goto, /main, /levels)
 * +) User history records.
 * +) Block history records.
 * +) /edlin for editing text files and virtual text files (blockdefs)
 *
 * +) Config file ~/.mcchost.ini
 *
 * +) Maybe embed commands: gzip, curl and gdb
 * +) Maybe exec($0, ...) on accept()
 * +) NAME: (*) MCCHost
 *
 *
 * +) /afk command (and auto)
 * +) /mode command (Grass, bedrock, water etc)
 * +) /info command
 * +) /about command
 * +) /tp command
 * +) /fly command
 */

void
process_args(int argc, char **argv)
{
    for(int ar = 1; ar<argc; ar++) {
	if (argv[ar][0] != '-') {
	    fprintf(stderr, "Skipping argument '%s'\n", argv[ar]);
	    continue;
	}

	if (ar+1 < argc) {
	    if (strcmp(argv[ar], "-name") == 0) {
		strncpy(server_name, argv[ar+1], sizeof(server_name)-1);
		ar++;
		continue;
	    }

	    if (strcmp(argv[ar], "-motd") == 0) {
		strncpy(server_motd, argv[ar+1], sizeof(server_motd)-1);
		ar++;
		continue;
	    }

	    if (strcmp(argv[ar], "-salt") == 0 || strcmp(argv[ar], "-secret") == 0) {
		ar++;
		strncpy(server_secret, argv[ar], sizeof(server_secret)-1);
		// Try to hide the argument used as salt from ps(1)
		for(char * p = argv[ar]; *p; p++) *p = 'X';
		continue;
	    }

	    if (strcmp(argv[ar], "-heartbeat") == 0) {
		strncpy(heartbeat_url, argv[ar+1], sizeof(heartbeat_url)-1);
		ar++;
		enable_heartbeat_poll = 1;
		continue;
	    }

	    if (strcmp(argv[ar], "-port") == 0) {
		tcp_port_no = atoi(argv[ar+1]);
		ar++;
		continue;
	    }

	    if (strcmp(argv[ar], "-log") == 0) {
		set_logfile(strdup(argv[ar+1]));
		ar++;
		continue;
	    }

	    if (strcmp(argv[ar], "-dir") == 0) {
		if(chdir(strdup(argv[ar+1])) < 0) {
		    perror(argv[ar+1]);
		    exit(1);
		}
		ar++;
		continue;
	    }
	}

	if (strcmp(argv[ar], "-saveconf") == 0) {
	    save_conf = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-inetd") == 0) {
	    inetd_mode = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-tcp") == 0) {
	    start_tcp_server = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-net") == 0) {
	    start_tcp_server = 1;
	    enable_heartbeat_poll = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-private") == 0) {
	    server_private = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-runonce") == 0) {
	    server_runonce = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-nocpe") == 0) { cpe_disabled = 1; continue; }

	fprintf(stderr, "Invalid argument '%s'\n", argv[ar]);
	exit(1);
    }

    if (enable_heartbeat_poll && server_secret[0] == 0) {
	// A pretty trivial semi-random code.
	struct timeval now;
	static char base62[] =
	    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	gettimeofday(&now, 0);
	srandom(now.tv_sec ^ (now.tv_usec*1000));
	for(int i=0; i<16; i++) {
	    server_secret[i] = base62[((unsigned)random())%62];
	}
	server_secret[16] = 0;
	fprintf(stderr, "Generated server secret %s\n", server_secret);
    }
}
