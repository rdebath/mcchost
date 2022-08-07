#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <limits.h>

#include "args.h"

char ** program_args = 0;
int proc_self_exe_ok = 0;
char * proc_self_exe = 0;

void
process_args(int argc, char **argv)
{
    program_args = calloc(argc+8, sizeof(*program_args));
    int bc = 1, plen = strlen(argv[0]);
    int port_no = -1;

    getprogram(argv[0]);

    for(int pass2 = 0; pass2<2; pass2++)
    {
	// On !pass2 dump most of the arg values into this structure.
	//
	// During the first pass the -dir, -port and -inetd_mode flags
	// are significant for finding the correct config files to load.

	server_t tmpserver = {0};
	if (!pass2)
	    server = &tmpserver;

	for(int ar = 1; ar<argc; ar++) {
	    if (argv[ar][0] != '-') {
		if (!pass2)
		    fprintf(stderr, "Skipping argument '%s'\n", argv[ar]);
		continue;
	    }

	    int addarg = 1;
	    do {
		if (ar+1 < argc) {
		    if (strcmp(argv[ar], "-name") == 0) {
			char cbuf[NB_SLEN*4];
			strncpy(cbuf, argv[ar+1], sizeof(cbuf)-1);
			int l = strlen(cbuf)+1;
			convert_to_cp437(cbuf, &l);
			strncpy(server->name, cbuf, sizeof(server->name)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-motd") == 0) {
			strncpy(server->motd, argv[ar+1], sizeof(server->motd)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-salt") == 0 || strcmp(argv[ar], "-secret") == 0) {
			ar++; addarg++;
			strncpy(server->secret, argv[ar], sizeof(server->secret)-1);
			// Try to hide the argument used as salt from ps(1)
			if (pass2)
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
			port_no = tcp_port_no = atoi(argv[ar+1]);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-log") == 0) {
			strncpy(logfile_pattern, argv[ar+1], sizeof(logfile_pattern)-1);
			ar++; addarg++;
			break;
		    }

		    if (strcmp(argv[ar], "-dir") == 0) {
			if (!pass2)
			    if(chdir(argv[ar+1]) < 0) {
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
		    start_tcp_server = 0;
		    enable_heartbeat_poll = 0;
		    break;
		}

		if (strcmp(argv[ar], "-pipe") == 0) {
		    inetd_mode = 1;
		    start_tcp_server = 0;
		    enable_heartbeat_poll = 0;
		    client_trusted = 1;
		    break;
		}

		if (strcmp(argv[ar], "-tcp") == 0) {
		    inetd_mode = 0;
		    start_tcp_server = 1;
		    enable_heartbeat_poll = 0;
		    break;
		}

		if (strcmp(argv[ar], "-net") == 0) {
		    inetd_mode = 0;
		    start_tcp_server = 1;
		    enable_heartbeat_poll = 1;
		    break;
		}

		if (strcmp(argv[ar], "-cron") == 0) {
		    start_heartbeat_task = 1;
		    start_backup_task = 1;
		    break;
		}

		if (strcmp(argv[ar], "-register") == 0) {
		    start_heartbeat_task = 1;
		    enable_heartbeat_poll = 1;
		    break;
		}

		if (strcmp(argv[ar], "-cleanup") == 0) {
		    start_backup_task = 1;
		    break;
		}

		if (strcmp(argv[ar], "-private") == 0) {
		    server->private = 1;
		    break;
		}

		if (strcmp(argv[ar], "-public") == 0) {
		    server->private = 0;
		    break;
		}

		if (strcmp(argv[ar], "-runonce") == 0) {
		    server_runonce = 1;
		    log_to_stderr = 1;
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

		if (strcmp(argv[ar], "-logstderr") == 0) {
		    log_to_stderr = 1;
		    break;
		}

		if (strcmp(argv[ar], "-nocpe") == 0) {
		    server->cpe_disabled = 1;
		    break;
		}

		fprintf(stderr, "Invalid argument '%s'\n", argv[ar]);
		exit(1);
	    } while(0);

	    // Copy all the args for restart.
	    if (!pass2 && addarg) {
		if (addarg == 2) {
		    program_args[bc++] = strdup(argv[ar-1]);
		    plen += strlen(argv[ar-1])+1;
		}
		program_args[bc++] = strdup(argv[ar]);
		plen += strlen(argv[ar])+1;
	    }
	}

	if (!pass2) {
	    // First pass is done so now we are in the right dir we read in
	    // the defaults and file configs.
	    server = 0;
	    init_dirs();
	    open_system_conf();

	    if (server->magic != MAP_MAGIC || server->magic2 != MAP_MAGIC2)
		*server = (server_t){
		    .magic = MAP_MAGIC, .magic2 = MAP_MAGIC2,
		    .software = SWNAME,
		    .name = SWNAME " Server",
		    .main_level = "main",
		    .save_interval = 15*60,
		    .backup_interval = 24*3600,
		    .key_rotation = 6*3600,
		    .max_players = 255,
		    .flag_log_commands = 1,
		    .flag_log_chat = 1,
		    .afk_interval = 10*60,
		    .afk_kick_interval = 45*60,
		};

	    load_ini_file(system_ini_fields, SERVER_CONF_NAME, 1, 0);

	    // If set, use server.ini over program default.
	    if (ini_settings.tcp_port_no > 0 && ini_settings.tcp_port_no <= 65535)
		tcp_port_no = ini_settings.tcp_port_no;

	    // Argument overrides even on first pass.
	    if (port_no > 0 && port_no <= 65535)
		tcp_port_no = port_no;

	    // If in inetd_mode read the port number we are using from the socket.
	    if (inetd_mode) {
		int p = read_sock_port_no(0);
		if (p>0) tcp_port_no = p;
	    }

	    if (tcp_port_no > 0 && tcp_port_no <= 65535 && !save_conf) {
		char buf[256];
		snprintf(buf, sizeof(buf), SERVER_CONF_PORT, tcp_port_no);
		load_ini_file(system_ini_fields, buf, 1, 0);

		// Don't let this override!
		ini_settings.tcp_port_no = tcp_port_no;
	    }

	    // These will be overridden on second pass.
	    start_tcp_server = ini_settings.start_tcp_server;
	    tcp_port_no = ini_settings.tcp_port_no;
	    detach_tcp_server = ini_settings.detach_tcp_server;
	    enable_heartbeat_poll = ini_settings.enable_heartbeat_poll;
	    server_runonce = ini_settings.server_runonce;
	    strcpy(heartbeat_url, ini_settings.heartbeat_url);
	    // inetd_mode = ini_settings.inetd_mode;
	}
    }

    // Pad the program args so we get some space after a restart.
    // Also we must turn off -detach to keep the same pid.
    do {
	program_args[bc++] = strdup("-no-detach");
	plen += 11;
    } while(plen < 32);

    if (enable_heartbeat_poll && server->secret[0] == 0)
	generate_secret();
}

LOCAL void
getprogram(char * argv0)
{
    // See if /proc/self/exe looks ok.
    char buf[PATH_MAX] = "";
    int l = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (l > 0) buf[l] = 0;

    // /proc/self/exe gives something runnable.
    if (l > 0 && access(buf, X_OK) == 0) {
	// Yup
	proc_self_exe_ok = 1;
	proc_self_exe = strdup(buf);
    }

    // Is argv0 absolute? Then use it for restart.
    if (argv0[0] == '/') {
	program_args[0] = strdup(argv0);
	return;
    }

    // Otherwise use /proc/self/exe if it's okay.
    if (proc_self_exe_ok) {
	// Save it away because we use it for restart after it's been recreated.
	program_args[0] = strdup(buf);
	return;
    }

    // Note we don't want use use this last one because execvp() will not
    // return if the executable is broken. (It's passed to /bin/sh instead)

    // Last try, assume the PATH will get fixed sometime.
    char * p = strrchr(argv0, '/');
    program_args[0] = p?strdup(p+1):strdup("");
}
