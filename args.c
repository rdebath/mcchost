
#include "args.h"

#if INTERFACE

#define SWNAME "MCCHost"

// These settings are shared between the servers, they cannot be
// different for different ports. But they get set immediatly they
// are changed by the user.
typedef struct server_t server_t;
struct server_t {
    int64_t magic;
    char software[NB_SLEN];
    char name[NB_SLEN];
    char motd[MB_STRLEN*2+1];
    char main_level[NB_SLEN];

    char secret[NB_SLEN];
    time_t key_rotation;

    time_t save_interval;
    time_t backup_interval;
    uint8_t no_unload_main;
    uint8_t no_save_inuse;
    uint8_t use_utc_zone;

    int op_flag;
    int cpe_disabled;
    int disable_web_server;
    int check_web_client_ip;

    int max_players;
    int loaded_levels;
    int connected_sessions;
    int pinned_levels;

    time_t last_heartbeat;
    time_t last_backup;
    time_t last_unload;
    int last_heartbeat_port;

    time_t afk_interval;
    time_t afk_kick_interval;

    uint8_t flag_log_commands;
    uint8_t flag_log_place_commands;
    uint8_t flag_log_chat;

    uint8_t allow_user_levels;
    uint8_t all_levels_owned;

    int player_update_ms;

    int block_spam_count;
    int block_spam_interval;
    int block_spam_kick;

    int cmd_spam_count;
    int cmd_spam_interval;
    int cmd_spam_ban;

    int chat_spam_count;
    int chat_spam_interval;
    int chat_spam_ban;

    int ip_connect_delay;

    struct server_ini_t shared_ini_settings;
    int magic2;
    int version;
};

// These settings are per port or per process settings, however,
// they can only be changed when the listener process restarts.
typedef struct server_ini_t server_ini_t;
struct server_ini_t {
    int use_port_specific_file;

    int tcp_port_no;
    int server_runonce;
    int start_tcp_server;
    int inetd_mode;
    int detach_tcp_server;

    int enable_heartbeat_poll;
    char heartbeat_url[1024];
    char user_id_suffix[NB_SLEN];
    int use_http_post;
    int omit_software_version;
    int private;

    int disallow_ip_admin;
    int disallow_ip_verify;
    int allow_pass_verify;
    int admin_only_login;
    int void_for_login;
    int no_map_padding;

    int trusted_localnet;
    char localnet_cidr[64];
};
#endif

// Shared settings, stored in memory shared between all processes
server_t *server = 0;
// Per server settings, not shared across instance
// Commandline overrides this
server_ini_t server_ini_settings = {0};
server_ini_t * ini_settings = &server_ini_settings;

// GBL for above shared ram
filelock_t system_lock[1] = {{.name = SYS_LOCK_NAME}};

// Copied from ini_settings
int start_tcp_server = 0;
int detach_tcp_server = 0;
int log_to_stderr = 0;
int tcp_port_no = 25565;
int enable_heartbeat_poll = 1;
int server_runonce = 0;

int save_conf = 0;
char program_name[512];
char ** program_args = 0;
int proc_self_exe_ok = 0;
char * proc_self_exe = 0;

#ifndef __linux__
time_t proc_start_time = 0;
#endif

void
process_args(int argc, char **argv)
{
    program_args = calloc(argc+8, sizeof(*program_args));
    int bc = 1, plen = strlen(argv[0]);
    int port_no = -1;
    int found_dir_arg = 0;
#ifndef __linux__
    int found_process_start = 0;
#endif

    getprogram(argv[0]);

    for(int pass2 = 0; pass2<2; pass2++)
    {
	// During the first pass the -dir, -port and -inetd mode flags
	// are significant for finding the correct config files to load.
	// For that pass dump arg values into this structure.
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
		if (strcmp(argv[ar], "-help") == 0 ||
		    strcmp(argv[ar], "-h") == 0 ||
		    strcmp(argv[ar], "--help") == 0) {
		    show_args_help();
		    exit(0);
		}

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
			strncpy(ini_settings->heartbeat_url, argv[ar+1], sizeof(ini_settings->heartbeat_url)-1);
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
			found_dir_arg = 1;
			break;
		    }

		    if (strcmp(argv[ar], "-process-start-time") == 0) {
			ar++; addarg++;
			// Can find process start time directly on Linux.
#ifdef __linux__
			addarg = 0;
#else
			proc_start_time = strtoimax(argv[ar+1], 0, 0);
			found_process_start = 1;
#endif
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
		    ini_settings->private = 1;
		    break;
		}

		if (strcmp(argv[ar], "-public") == 0) {
		    ini_settings->private = 0;
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

		if (strcmp(argv[ar], "-cwd") == 0) {
		    if (found_dir_arg) addarg = 0;
		    found_dir_arg = 1; // Disable move to ~/.mcchost
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
	    if (!found_dir_arg)
		find_dirs();

	    // Missing sever.ini means we can check this early.
	    if (access(SERVER_CONF_NAME, F_OK) != 0)
		check_something_todo();

	    init_dirs();

	    server = 0;
	    open_system_conf();

	    if (server->magic != TY_MAGIC || server->magic2 != TY_MAGIC2 || server->version != TY_VERSION)
		*server = (server_t){
		    .magic = TY_MAGIC, .magic2 = TY_MAGIC2, .version = TY_VERSION,
		    .software = SWNAME,
		    .name = SWNAME " Server",
		    .main_level = "main",
		    .save_interval = 15*60,
		    .backup_interval = 24*3600,
		    .key_rotation = 6*3600,
		    .max_players = 128,
		    .flag_log_commands = 1,
		    .flag_log_place_commands = 1,
		    .flag_log_chat = 1,
		    .afk_interval = 10*60,
		    .afk_kick_interval = 45*60,
		    .allow_user_levels = 1,
		    .player_update_ms = 100,
		    .block_spam_count = 250,
		    .block_spam_interval = 5,
		    .cmd_spam_count = 25,
		    .cmd_spam_interval = 1,
		    .cmd_spam_ban = 30,
		    .chat_spam_count = 8,
		    .chat_spam_interval = 5,
		    .chat_spam_ban = 10,
		    .ip_connect_delay = 5,
		};

	    load_ini_file(system_ini_fields, SERVER_CONF_NAME, 1, 0);
	    server_ini_settings = server->shared_ini_settings;
	    ini_settings = &server->shared_ini_settings;

	    // If set, use server.ini over program default.
	    if (server->shared_ini_settings.tcp_port_no > 0 &&
		    server->shared_ini_settings.tcp_port_no <= 65535)
		tcp_port_no = server->shared_ini_settings.tcp_port_no;

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
		saprintf(buf, SERVER_CONF_PORT, tcp_port_no);
		if (access(buf, F_OK) == 0) {
		    load_ini_file(system_x_ini_fields, buf, 1, 0);
		    server_ini_settings.use_port_specific_file = 1;
		    ini_settings = &server_ini_settings;
		}

		// Don't let this override!
		ini_settings->tcp_port_no = tcp_port_no;
	    }

	    // These will be overridden on second pass.
	    start_tcp_server = ini_settings->start_tcp_server;
	    tcp_port_no = ini_settings->tcp_port_no;
	    detach_tcp_server = ini_settings->detach_tcp_server;
	    enable_heartbeat_poll = ini_settings->enable_heartbeat_poll;

	    // Only reasonable from command line
	    // inetd_mode = ini_settings->inetd_mode;
	    // server_runonce = ini_settings->server_runonce;
	}
    }

    check_something_todo();

    if (enable_heartbeat_poll && server->secret[0] == 0)
	generate_secret();

    if (save_conf) {
	char pwd[PATH_MAX];
	save_system_ini_file();
	char * p = getcwd(pwd, sizeof(pwd));
	fprintf(stderr, "Configuration saved to '%s/"SERVER_CONF_NAME"'\n", p?p:"");
	exit(0);
    }

    // The -dir arg isn't added, make sure we don't go anywhere.
    if (!found_dir_arg) {
	program_args[bc++] = strdup("-cwd");
	plen += 5;
    }

#ifndef __linux__
    if (!found_process_start) {
	if (!proc_start_time) proc_start_time = time(0);
	program_args[bc++] = strdup("-process-start-time");
	plen += 30;
	char buf[sizeof(intmax_t)*3+3];
	sprintf(buf, "%jd", (intmax_t)proc_start_time);
	program_args[bc++] = strdup(buf);
    }
#endif

    // Pad the program args so we get some space after a restart.
    // Also we must turn off -detach to keep the same pid.
    do {
	program_args[bc++] = strdup("-no-detach");
	plen += 11;
    } while(plen < 32);
}

LOCAL void
check_something_todo()
{
    // Check we're supposed to do something.
    if (!inetd_mode && !start_tcp_server && !save_conf
	&& !start_heartbeat_task && !start_backup_task
	&& (isatty(0) || isatty(1)))
	show_args_help();

    if (inetd_mode && start_tcp_server)
	show_args_help();
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
    program_args[0] = p?strdup(p+1):strdup(argv0);
}

void
save_system_ini_file()
{
    ini_settings->start_tcp_server = start_tcp_server;
    ini_settings->tcp_port_no = tcp_port_no;
    ini_settings->enable_heartbeat_poll = enable_heartbeat_poll;
    ini_settings->server_runonce = server_runonce;

    // Only change these if we are specifically saving the config.
    if (save_conf) {
	// This will be overridded when the server is restarted.
	ini_settings->detach_tcp_server = detach_tcp_server;
    }

    // Only reasonable from command line
    // inetd_mode = ini_settings->inetd_mode;
    // server_runonce = ini_settings->server_runonce;

    save_ini_file(system_ini_fields, SERVER_CONF_NAME);

    if (ini_settings->use_port_specific_file) {
	char buf[256];
	saprintf(buf, SERVER_CONF_PORT, tcp_port_no);

	save_ini_file(system_x_ini_fields, buf);
    }
}
