#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include "args.h"

/*
 * TODO:
 * +) Comment preserving ini file save.
 *
 * +) User prefix/suffix for multiple heartbeat servers.
 *
 * +) load/save level to *.cw and use for backups, restores and "unload".
 *    -- Add locking so one load at a time ?
 * +) /load level ini file from curl pipe ?
 *
 * +) Multiple Levels. (/newlvl, /goto, /main, /levels)
 * +) Block/User history records.
 * +) /edlin for editing text files and virtual text files (blockdefs)
 *
 * +) Maybe embed called commands: gzip, curl and gdb(stacktrace)
 * +) Maybe exec($0, ...) on accept()
 * +) NAME: (*) MCCHost
 *
 * +) Backup while physics running --> Copylvl then freeze physics.
 *    Copylvl can more easily do patchups.
 *
 * +) /spawn command
 * +) /afk command (and auto)
 * +) /mode command (Grass, bedrock, water etc)
 * +) /info command
 * +) /about command
 * +) /tp command
 * +) /fly command
 */

char ** program_args = 0;

void
process_args(int argc, char **argv)
{
    program_args = calloc(argc+8, sizeof(*program_args));
    int bc = 0, plen = strlen(argv[0]);

    program_args[bc++] = strdup(argv[0]);
    if (argv[0][0] != '/' && strchr(argv[0], '/') != 0)
	disable_restart = 1;
	// TODO: Save /proc/self/exe ?

    for(int pass = 0; pass<2; pass++)
    {
	for(int ar = 1; ar<argc; ar++) {
	    if (argv[ar][0] != '-') {
		if (pass == 0)
		    fprintf(stderr, "Skipping argument '%s'\n", argv[ar]);
		continue;
	    }

	    int addarg = 1;
	    do {
		if (ar+1 < argc) {
		    if (strcmp(argv[ar], "-name") == 0) {
			strncpy(server_name, argv[ar+1], sizeof(server_name)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-motd") == 0) {
			strncpy(server_motd, argv[ar+1], sizeof(server_motd)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-salt") == 0 || strcmp(argv[ar], "-secret") == 0) {
			ar++; addarg++;
			strncpy(server_secret, argv[ar], sizeof(server_secret)-1);
			// Try to hide the argument used as salt from ps(1)
			for(char * p = argv[ar]; *p; p++) *p = 'X';
			break;
		    }

		    if (strcmp(argv[ar], "-heartbeat") == 0) {
			strncpy(heartbeat_url, argv[ar+1], sizeof(heartbeat_url)-1);
			ar++; addarg++;
			enable_heartbeat_poll = 1;
			break;
		    }

		    if (strcmp(argv[ar], "-port") == 0) {
			tcp_port_no = atoi(argv[ar+1]);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-log") == 0) {
			strncpy(logfile_pattern, argv[ar+1], sizeof(logfile_pattern)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-dir") == 0) {
			if (pass == 0)
			    if(chdir(strdup(argv[ar+1])) < 0) {
				perror(argv[ar+1]);
				exit(1);
			    }
			ar++;
			addarg = 0;
			break;
		    }
		}

		if (strcmp(argv[ar], "-saveconf") == 0) {
		    save_conf = 1;
		    addarg = 0;
		    break;
		}

		if (strcmp(argv[ar], "-inetd") == 0) {
		    inetd_mode = 1;
		    break;
		}

		if (strcmp(argv[ar], "-tcp") == 0) {
		    start_tcp_server = 1;
		    break;
		}

		if (strcmp(argv[ar], "-net") == 0) {
		    start_tcp_server = 1;
		    enable_heartbeat_poll = 1;
		    break;
		}

		if (strcmp(argv[ar], "-private") == 0) {
		    server_private = 1;
		    break;
		}

		if (strcmp(argv[ar], "-public") == 0) {
		    server_private = 0;
		    break;
		}

		if (strcmp(argv[ar], "-runonce") == 0) {
		    server_runonce = 1;
		    break;
		}

		if (strcmp(argv[ar], "-detach") == 0) {
		    detach_tcp_server = 1;
		    addarg = 0;
		    break;
		}

		if (strcmp(argv[ar], "-no-detach") == 0) {
		    detach_tcp_server = 0;
		    addarg = 0;
		    break;
		}

		if (strcmp(argv[ar], "-nocpe") == 0) { cpe_disabled = 1; break; }

		fprintf(stderr, "Invalid argument '%s'\n", argv[ar]);
		exit(1);
	    } while(0);

	    if (pass == 0 && addarg) {
		if (addarg == 2) {
		    program_args[bc++] = strdup(argv[ar-1]);
		    plen += strlen(argv[ar-1])+1;
		}
		program_args[bc++] = strdup(argv[ar]);
		plen += strlen(argv[ar])+1;
	    }
	}

	if (pass == 0)
	    load_ini_file(system_ini_fields, SERVER_CONF_NAME, 1);
    }

    do {
	program_args[bc++] = strdup("-no-detach");
	plen += 11;
    } while(plen < 32);

    if (disable_restart)
	fprintf(stderr, "WARNING: Restart disabled due to relative path\n");

    if (enable_heartbeat_poll && server_secret[0] == 0) {
	// A pretty trivial semi-random code, maybe 20bits of randomness.
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
