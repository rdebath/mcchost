#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"

/*
 * TODO:
 * +) tcpserver
 * +) fetch url -- libcurl?
 * +) load/save level to *.cw
 * +) CPE
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

	    if (strcmp(argv[ar], "-salt") == 0) {
		ar++;
		strncpy(server_salt, argv[ar], sizeof(server_salt)-1);
		// Hide the argument used as salt from ps(1)
		for(char * p = argv[ar]; *p; p++) *p = 'X';
		continue;
	    }

	    if (strcmp(argv[ar], "-port") == 0) {
		tcp_port_no = atoi(argv[ar+1]);
		ar++;
		continue;
	    }

	    if (strcmp(argv[ar], "-log") == 0) {
		open_logfile(argv[ar+1], 0);
		ar++;
		continue;
	    }
	}

	if (strcmp(argv[ar], "-cpe") == 0) {
	    max_blockno_to_send = 255;
	    enable_cp437 = 1;
	    continue;
	}

	if (strcmp(argv[ar], "-tcp") == 0) {
	    start_tcp_server = 1;
	    continue;
	}
    }
}
