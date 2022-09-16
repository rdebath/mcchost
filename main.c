#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/socket.h>

#include "main.h"

#if INTERFACE
#include <time.h>

#define SWNAME "MCCHost"

// These settings are shared between the servers, they cannot be
// different for different ports. But they get set immediatly they
// are changed by the user.
typedef struct server_t server_t;
struct server_t {
    int magic;
    char software[NB_SLEN];
    char name[NB_SLEN];
    char motd[MB_STRLEN*2+1];
    char main_level[NB_SLEN];

    char secret[NB_SLEN];
    time_t key_rotation;

    time_t save_interval;
    time_t backup_interval;

    int op_flag;
    int cpe_disabled;
    int private;

    int max_players;
    int loaded_levels;

    time_t last_heartbeat;
    time_t last_backup;
    time_t last_unload;
    int last_heartbeat_port;

    time_t afk_interval;
    time_t afk_kick_interval;

    int flag_log_commands;
    int flag_log_chat;

    int magic2;
};

// These settings are per port or per process settings, however,
// they can only be changed when the listener process restarts.
typedef struct server_ini_t server_ini_t;
struct server_ini_t {

    int tcp_port_no;
    int server_runonce;
    int start_tcp_server;
    int inetd_mode;
    int detach_tcp_server;

    int enable_heartbeat_poll;
    char heartbeat_url[1024];
    char user_id_suffix[NB_SLEN];
};
#endif

int line_ofd = -1;
int line_ifd = -1;

char user_id[NB_SLEN];	// This is ASCII not CP437 or UTF8
int user_authenticated = 0;
int user_logged_in = 0;
int inetd_mode = 0;
int start_heartbeat_task = 0;
int start_backup_task = 0;

char program_name[512];

filelock_t system_lock[1] = {{.name = SYS_LOCK_NAME}};
server_t *server = 0;

nbtstr_t client_software;

char current_level_name[MAXLEVELNAMELEN+1];
char current_level_fname[MAXLEVELNAMELEN*4];
int current_level_backup_id = 0;

char heartbeat_url[1024] = "http://www.classicube.net/server/heartbeat/";
char logfile_pattern[1024] = "";
int server_runonce = 0;
int save_conf = 0;

// Per server settings, not shared across instance
// Commandline overrides this but is NOT saved to server.ini
server_ini_t ini_settings = {0};

int op_enabled = 0;	// Op flag was set for this session
int cpe_enabled = 0;	// Set if this session is using CPE
int cpe_requested = 0;	// Set if cpe was requested, even if rejected.
int cpe_pending = 0;	// Currently running ExtInfo process.
int cpe_extn_remaining = 0;
int cpe_extn_advertised = 0;
int protocol_base_version = 7;

char * proc_args_mem = 0;
int    proc_args_len = 0;

int
main(int argc, char **argv)
{
    snprintf(program_name, sizeof(program_name), "%s", argv[0]);

    init_textcolours();

    process_args(argc, argv);

    proc_args_mem = argv[0];
    proc_args_len = argv[argc-1] + strlen(argv[argc-1]) - argv[0] + 1;

    if (!inetd_mode && !start_tcp_server && !save_conf
	&& !start_heartbeat_task && !start_backup_task
	&& (isatty(0) || isatty(1)))
	show_args_help();

    if (inetd_mode && start_tcp_server)
	show_args_help();

    if (*logfile_pattern)
	set_logfile(logfile_pattern, 0);

    if (save_conf) {
        ini_settings.start_tcp_server = start_tcp_server;
        ini_settings.tcp_port_no = tcp_port_no;
        ini_settings.inetd_mode = 0;
        ini_settings.detach_tcp_server = detach_tcp_server;
        ini_settings.enable_heartbeat_poll = enable_heartbeat_poll;
        ini_settings.server_runonce = server_runonce;
        strcpy(ini_settings.heartbeat_url, heartbeat_url);

	save_ini_file(system_ini_fields, SERVER_CONF_NAME);
	fprintf(stderr, "Configuration saved\n");
	exit(0);
    }

    lock_start(system_lock);

    {
	// Check main level lock
	char fixname[MAXLEVELNAMELEN*4];
	char sharename[256];
	fix_fname(fixname, sizeof(fixname), main_level());
	snprintf(sharename, sizeof(sharename), LEVEL_LOCK_NAME, fixname);
	level_lock->name = strdup(sharename);
	lock_start(level_lock);
	lock_stop(level_lock);
    }

    if (start_heartbeat_task || start_backup_task)
        run_timer_tasks();

    delete_session_id(0, 0, 0);

    if (start_tcp_server) {
	memset(proc_args_mem, 0, proc_args_len);
	snprintf(proc_args_mem, proc_args_len, "%s server", SWNAME);

	tcpserver();
    } else {
	line_ofd = 1; line_ifd = 0;
	if (inetd_mode) {
	    if (!log_to_stderr)
		logger_process();
	    check_inetd_connection();
	}
    }

    reinit_rand_gen();
    process_connection();
    run_request_loop();
    send_disconnect_message();
    stop_user();

    if (inetd_mode) {
	// Port got disconnected so we disconnect our end.
	close(line_ofd);
	if (line_ofd != line_ifd)
	    close(line_ifd);
	// Cleanup
	scan_and_save_levels(0);
    }

    return 0;
}

void
process_connection()
{
    open_client_list();

    login();

    write_current_user(1);

    memset(proc_args_mem, 0, proc_args_len);
    snprintf(proc_args_mem, proc_args_len, "%s (%s)", SWNAME, user_id);

    user_logged_in = 1; // May not be "authenticated", but they exist.

    // If in classic mode, don't allow place of bedrock.
    op_enabled = (server->op_flag && cpe_requested);

    if (cpe_requested && !server->cpe_disabled) {
	send_ext_list();
	cpe_pending = 1;
    }

    if (!cpe_pending)
	complete_connection();

    cpe_enabled = (cpe_requested && !server->cpe_disabled);
}

void
complete_connection()
{
    if (extn_evilbastard)
	fatal("Server is incompatible with Evil bastard extension");

    send_server_id_pkt(server->name, server->motd, op_enabled);
    cpe_pending = 0;

    // List of users
    start_user();

    // Chat to users
    create_chat_queue();

    // Open level mmap files.
    char fixname[MAXLEVELNAMELEN*4];
    fix_fname(fixname, sizeof(fixname), main_level());
    start_level(main_level(), fixname, 0);
    open_level_files(main_level(), 0, fixname, 0);

    if (!level_prop) {
	start_level(main_level(), fixname, -1);
	if (level_prop) {
	    level_prop->readonly = 1;
	    level_prop->disallowchange = 0;
	}
    }

    send_textcolours();
    send_map_file();

    printf_chat("&SWelcome &7%s", user_id);
    printf_chat("@&a+ &7%s &Sconnected", user_id);

    read_only_message();

    cmd_help("welcome", 0);

    if (!level_prop)
	printf_chat("&WMain level failed to load, you are nowhere.");
}

void
read_only_message()
{
    if (!level_prop || !level_prop->readonly) return;
    if (level_prop->disallowchange && extn_inventory_order && extn_sethotbar) return;

    printf_chat("&WLoaded read only map, changes won't be saved.");
}

void
send_disconnect_message()
{
    printf_chat("@&c- &7%s &Sdisconnected", user_id);
    printlog("Connection dropped for %s", user_id);
}

void
fatal(char * emsg)
{
    static int in_fatal = 0;
    assert(!in_fatal);
    in_fatal = 1;

    fprintf(stderr, "FATAL(%d): %s\n", getpid(), emsg);

    if (level_chat_queue)
	printf_chat("@&W- &7%s &WCrashed: &S%s", user_id, emsg);

    if (line_ofd > 0)
	disconnect(1, emsg);

    exit(42);
}

void
logout(char * emsg)
{
    printf_chat("@&W- &7%s &S%s", user_id, emsg);
    disconnect(0, emsg);
}

LOCAL void
disconnect(int rv, char * emsg)
{
    if (line_ofd < 0) return;

    if (client_ipv4_port > 0 && *user_id)
	printlog("Disconnect %s, user '%s', %s", client_ipv4_str, user_id, emsg);
    else if (client_ipv4_port > 0)
	printlog("Disconnect %s, %s", client_ipv4_str, emsg);
    else if (*user_id)
	printlog("Disconnect user '%s', %s", user_id, emsg);
    else
	printlog("Disconnect %s", emsg);

    stop_user();
    send_discon_msg_pkt(emsg);
    flush_to_remote();
    shutdown(line_ofd, SHUT_RDWR);
    if (line_ofd != line_ifd)
	shutdown(line_ifd, SHUT_RDWR);

    if (inetd_mode && user_authenticated) {
	close(line_ofd);
	if (line_ofd != line_ifd)
	    close(line_ifd);
	// Cleanup
	scan_and_save_levels(0);
    }
    exit(rv);
}

void
show_args_help()
{
    FILE * f = stderr;
    fprintf(f, "Usage: %s [-inetd|-tcp|-net] ...\n", program_name);
    fprintf(f, "  -dir X     Change to directory before opening data files.\n");
    fprintf(f, "  -cwd       Use the current directory for data.\n");
    fprintf(f, "             (The default is ~/.mcchost.)\n\n");

    fprintf(f, "  -tcp       Open a tcp socket for listening, no default secret.\n");
    fprintf(f, "  -net       Open a socket, register the secret at:\n%16s%s\n", "", heartbeat_url);
    fprintf(f, "  -port X    Set port number listened to.\n\n");
    fprintf(f, "  -inetd     Assume socket is stdin/out from inetd or similar\n");
    fprintf(f, "  -detach    Background self, eg if starting from command line.\n");
    fprintf(f, "  -no-detach Do not background; starting from init, systemd, docker.\n");

    fprintf(f, "  -private -public\n");
    fprintf(f, "             When registering set privacy state.\n");

    fprintf(f, "  -name X    Set server name\n");
    fprintf(f, "  -motd X    Set server motd\n");
    fprintf(f, "  -secret X\n");
    fprintf(f, "  -salt X    Set server salt/secret\n");
    fprintf(f, "  -heartbeat http://host.xz/path\n");
    fprintf(f, "             Change hearbeat url\n");
    fprintf(f, "  -nocpe     Don't accept a CPE request.\n\n");

    fprintf(f, "  -cron      Run periodic tasks, heartbeat and backups then exit.\n");
    fprintf(f, "  -register  Register at heartbeat and exit. Run every 30 seconds.\n");
    fprintf(f, "  -cleanup   Do saves and backups. Run every 12 minutes or hours.\n");
    fprintf(f, "  -runonce   Accept one connection without forking, for debugging.\n");
    fprintf(f, "  -logstderr Logs to stderr, not log files.\n");
    fprintf(f, "  -saveconf  Save current system conf then exit\n");

    exit(1);
}

char *
main_level()
{
    static char main_level_cpy[NB_SLEN];
    char *s = server->main_level;
    char * d = main_level_cpy;
    while ( STFU(*d++ = *s++) );
    return main_level_cpy;
}

void
login()
{
    char inbuf[4096] = {0};
    int insize = 0, rqsize = msglen[0];
    pkt_player_id player = {0};
    websocket = 0;

    // First logon packet is read using this, not the normal loop.
    // This prevents any processing of other packets and allows
    // us to give special responses and short timeouts.
    //
    // Also; the normal processing loop doesn't have the code for a re-login.
    time_t startup = time(0);
    fcntl(line_ifd, F_SETFL, (int)O_NONBLOCK);
    int sleeps = 0;
    while(insize < sizeof(inbuf))
    {
	int cc = recv(line_ifd, inbuf, sizeof(inbuf), MSG_PEEK);
	if (cc>0) insize = cc;
	if (cc<=0) {
	    if (errno != EAGAIN && errno != EINTR) {
		line_ifd = -1; line_ofd = -1; // Probably unusable.
		// NB: no close before exit
		teapot(inbuf, insize);
	    }
	}

	if (websocket) {
	    websocket = 1; // We are restarting translation.
	    websocket_translate(inbuf, &insize);
	}

	time_t now = time(0);
	if (now-startup > 4) {
	    if (insize >= 2 && inbuf[1] >= 3 && inbuf[1] <= 7) {
		if (insize >= 66) {
		    convert_logon_packet(inbuf, &player);
		    strcpy(user_id, player.user_id);
		}
		disconnect(0, "Short logon packet received");
	    } else
		teapot(inbuf, insize);
	} else {
	    msleep(50);
	    sleeps++;
	}

	if (insize > 14 && !websocket &&
		(memcmp(inbuf, "GET ", 4) == 0 ||
		 memcmp(inbuf, "HEAD ", 5) == 0 ||
		 memcmp(inbuf, "PRI * HTTP/2.0", 14) == 0)) {
	    // HTTP GET requests, including websocket.
	    char * eoh;
	    if (insize == sizeof(inbuf))
		teapot(inbuf, insize);
	    else if ((eoh = strstr(inbuf, "\r\n\r\n")) != 0) {
		// This looks like a complete HTTP request.
		*eoh = 0;
		if (websocket_startup(inbuf, insize)) {
		    websocket = 1;
		    if (read(line_ifd, inbuf, eoh-inbuf+4) != eoh-inbuf+4) // NOM! Header.
			fatal("Buffered socket read() failed");
		    insize = 0; // Clean
		} else {
		    *eoh = '\r';
		    teapot(inbuf, insize);
		}
	    }
	} else if (insize >= 16 || sleeps > 5) {
	    // Special quick exits for bad callers.
	    if (insize >= 1 && inbuf[0] != 0)
		teapot(inbuf, insize);
	    if (insize >= 2 && (inbuf[1] < 3 || inbuf[1] > 9))
		teapot(inbuf, insize);
	    if (insize >= 2 && (inbuf[1] > 7 || inbuf[1] < 5)) {
		if (insize >= 66) {
		    convert_logon_packet(inbuf, &player);
		    strcpy(user_id, player.user_id);
		}
		disconnect(0, "Unsupported protocol version");
	    }
	}
	if (insize >= 2 && inbuf[1] < 6)
	    rqsize = msglen[0] - 1;
	if (insize >= rqsize && inbuf[0] == 0 && inbuf[1] <= 7)
	    break;
    }
    fcntl(line_ifd, F_SETFL, 0);

    if (websocket) websocket = 1; // Reset for requestloop.

    convert_logon_packet(inbuf, &player);
    snprintf(user_id, sizeof(user_id), "%s%s", player.user_id, ini_settings.user_id_suffix);
    protocol_base_version = player.protocol;

    if (player.protocol > 7 || player.protocol < 5)
	disconnect(0, "Unsupported protocol version");

    // The older protocols are mostly the same but with fewer valid blocks.
    // 7 -- classic 0.28 - 0.30	-- CPE only for this version.
    // 6 -- classic 0.0.20+	-- No setuser type packet.
    // 5 -- classic 0.0.19	-- No usertype field.
    // 3/4 -- classic 0.0.16+	-- Problems with teleport and spawn positions.
    // ? -- classic 0.0.15a	-- No protocol version in ident packet.
    if (player.protocol < 7)
	switch (player.protocol) {
	case 6: client_block_limit = Block_Gold+1; break;
	case 5: client_block_limit = Block_Glass+1; msglen[0]--; break;
	default: client_block_limit = Block_Leaves+1; msglen[0]--; break;
	}

    for(int i = 0; player.user_id[i]; i++)
	if (!isascii(player.user_id[i]) ||
	    (!isalnum(player.user_id[i]) && player.user_id[i] != '.' && player.user_id[i] != '_')) {
	    disconnect(0, "Invalid user name");
	}

    cpe_requested = player.cpe_flag;

    if (client_ipv4_port)
	printlog("Logging in%s user '%s' from %s",
	    cpe_requested&&!server->cpe_disabled?"":" classic", user_id,
	    client_ipv4_str);
    else
	printlog("Logging in%s user '%s'",
	    cpe_requested&&!server->cpe_disabled?"":" classic", user_id);

    if (*server->secret != 0 && *server->secret != '-') {
	if (strlen(player.mppass) != 32 && !client_trusted)
	    disconnect(0, "Login failed! Mppass required");
    }

    if (*user_id == 0) disconnect(0, "Username must be entered");
    if (strlen(user_id) > 16)
	disconnect(0, "Usernames must be between 1 and 16 characters");

    if (client_trusted && (!*player.mppass
			|| strcmp("-", player.mppass) == 0
			|| strncmp("00000000000000000000000000000000", player.mppass, strlen(player.mppass)) == 0
			|| strcmp("(none)", player.mppass) == 0))
	// Trusted net and they haven't tried to enter an mppass.
	user_authenticated = 1;
    else if (*server->secret != 0 && *server->secret != '-') {
	// There's an attempted mppass or we require one.
	if (check_mppass(player.user_id, player.mppass) == 0) {
	    // printlog("User %s failed with mppass %s", user_id, player.mppass);
	    disconnect(0, "Login failed! Close the game and refresh the server list.");
	}

	user_authenticated = 1;
    }
}
