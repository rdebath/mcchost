#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>

#include "args.h"

char ** program_args = 0;

void
process_args(int argc, char **argv)
{
    program_args = calloc(argc+8, sizeof(*program_args));
    int bc = 1, plen = strlen(argv[0]);

    getprogram(argv[0]);

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
			strncpy(server.name, argv[ar+1], sizeof(server.name)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-motd") == 0) {
			strncpy(server.motd, argv[ar+1], sizeof(server.motd)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-salt") == 0 || strcmp(argv[ar], "-secret") == 0) {
			ar++; addarg++;
			strncpy(server.secret, argv[ar], sizeof(server.secret)-1);
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
		    server.private = 1;
		    break;
		}

		if (strcmp(argv[ar], "-public") == 0) {
		    server.private = 0;
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

		if (strcmp(argv[ar], "-log-stderr") == 0) {
		    log_to_stderr = 1;
		    addarg = 1;
		    break;
		}

		if (strcmp(argv[ar], "-nocpe") == 0) { server.cpe_disabled = 1; break; }

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
	    load_ini_file(system_ini_fields, SERVER_CONF_NAME, 1, 0);
    }

    // Pad the program args so we get some space after a restart.
    do {
	program_args[bc++] = strdup("-no-detach");
	plen += 11;
    } while(plen < 32);

    struct timeval now;
    gettimeofday(&now, 0);
    srandom(now.tv_sec ^ (now.tv_usec*4294U));

    if (enable_heartbeat_poll && server.secret[0] == 0) {
	// A pretty trivial semi-random code, maybe 20-30bits of randomness.
	static char base62[] =
	    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	for(int i=0; i<16; i++)
	    server.secret[i] = base62[((unsigned)random())%62];
	server.secret[16] = 0;
	fprintf(stderr, "Generated server secret %s\n", server.secret);
    }
}

LOCAL void
getprogram(char * argv0)
{
    // Is argv0 absolute or a $PATH lookup?
    if (argv0[0] == '/' || strchr(argv0, '/') == 0) {
	program_args[0] = strdup(argv0);
	return;
    }

    // For relative paths try something different.
    char buf[PATH_MAX*2];
    int l = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;

    // /proc/self/exe gives something runnable.
    if (l > 0 && access(buf, X_OK) == 0) {
	program_args[0] = strdup(buf);
	return;
    }

    // Last try, just use the exe name
    char * p = strrchr(argv0, '/');
    program_args[0] = p?strdup(p+1):strdup("");
}
