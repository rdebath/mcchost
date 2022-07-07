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

typedef struct server_t server_t;
struct server_t {
    int magic;
    char software[NB_SLEN];
    char name[NB_SLEN];
    char motd[NB_SLEN];
    char secret[NB_SLEN];
    int private;
    int cpe_disabled;
    char main_level[NB_SLEN];
    time_t save_interval;
    time_t backup_interval;
    int max_players;
    int loaded_levels;
    time_t last_heartbeat;
    time_t last_backup;
    time_t last_unload;
    int last_heartbeat_port;
    int magic2;
};

typedef struct server_ini_t server_ini_t;
struct server_ini_t {
    int start_tcp_server;
    int tcp_port_no;
    int inetd_mode;
    int detach_tcp_server;
    int server_runonce;
    int enable_heartbeat_poll;
    char heartbeat_url[1024];
};
#endif

int line_ofd = -1;
int line_ifd = -1;

char user_id[NB_SLEN];	// This is ASCII not CP437 or UTF8
int user_authenticated = 0;
int user_logged_in = 0;
int server_id_op_flag = 1;
int inetd_mode = 0;
int start_cron_task = 0;

char program_name[512];

filelock_t system_lock[1] = {{.name = SYS_LOCK_NAME}};
volatile server_t *server = 0;

nbtstr_t client_software = {"(unknown)"};

char current_level_name[MAXLEVELNAMELEN+1];
char current_level_fname[MAXLEVELNAMELEN*4];

char heartbeat_url[1024] = "http://www.classicube.net/server/heartbeat/";
char logfile_pattern[1024] = "";
int server_runonce = 0;
int save_conf = 0;

server_ini_t ini_settings;

int cpe_enabled = 0;	// Set if this session is using CPE
int cpe_requested = 0;	// Set if cpe was requested, even if rejected.
int cpe_pending = 0;	// Currently running ExtInfo process.
int cpe_extn_remaining = 0;

block_t client_block_limit = Block_CP;

char * proc_args_mem = 0;
int    proc_args_len = 0;

int
main(int argc, char **argv)
{
    snprintf(program_name, sizeof(program_name), "%s", argv[0]);

    process_args(argc, argv);

    proc_args_mem = argv[0];
    proc_args_len = argv[argc-1] + strlen(argv[argc-1]) - argv[0] + 1;

    if (!inetd_mode && !start_tcp_server && !save_conf && !start_cron_task
	&& (isatty(0) || isatty(1)))
	show_args_help();

    if (inetd_mode && start_tcp_server)
	show_args_help();

    if (*logfile_pattern)
	set_logfile(logfile_pattern, 0);

    if (save_conf) {
        ini_settings.start_tcp_server = start_tcp_server;
        ini_settings.tcp_port_no = tcp_port_no;
        ini_settings.inetd_mode = inetd_mode;
        ini_settings.detach_tcp_server = detach_tcp_server;
        ini_settings.enable_heartbeat_poll = enable_heartbeat_poll;
        ini_settings.server_runonce = server_runonce;
        strcpy(ini_settings.heartbeat_url, heartbeat_url);

	save_ini_file(system_ini_fields, SERVER_CONF_NAME);
	fprintf(stderr, "Configuration saved\n");
	exit(0);
    }

    init_dirs();

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

    if (start_cron_task)
        run_timer_tasks();

    delete_session_id(0, 0, 0);

    if (start_tcp_server) {
	memset(proc_args_mem, 0, proc_args_len);
	snprintf(proc_args_mem, proc_args_len, "%s server", server->software);

	tcpserver();
    } else {
	line_ofd = 1; line_ifd = 0;
	if (inetd_mode) {
	    if (!log_to_stderr)
		logger_process();
	    check_inetd_connection();
	}
    }

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
    snprintf(proc_args_mem, proc_args_len, "%s (%s)", server->software, user_id);

    user_logged_in = 1; // May not be "authenticated", but they exist.

    // If in classic mode, don't allow place of bedrock.
    if (!cpe_requested) server_id_op_flag = 0;

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

    send_server_id_pkt(server->name, server->motd, server_id_op_flag);
    cpe_pending = 0;

    // List of users
    start_user();

    // Chat to users
    create_chat_queue();

    // Open level mmap files.
    char fixname[MAXLEVELNAMELEN*4];
    fix_fname(fixname, sizeof(fixname), main_level());
    start_level(main_level(), fixname);
    open_level_files(main_level(), fixname, 0);

    if (!level_prop) fatal("Unable to load initial map file -- sorry");

    send_map_file();

    printf_chat("&SWelcome &7%s", user_id);
    printf_chat("@&a+ &7%s &Sconnected", user_id);

    read_only_message();

    cmd_help("welcome", 0);
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
login()
{
    char inbuf[256] = {0};
    int insize = 0, rqsize = msglen[0];
    pkt_player_id player = {0};

    // First logon packet is read using this, not the normal loop.
    // This prevents any processing of other packets and allows
    // us to give special responses and short timeouts.
    // Also; the normal processing loop can't do a login (again).
    time_t startup = time(0);
    fcntl(line_ifd, F_SETFL, (int)O_NONBLOCK);
    int sleeps = 0;
    while(insize < rqsize)
    {
	int cc = recv(line_ifd, inbuf, sizeof(inbuf), MSG_PEEK);
	if (cc>0) insize = cc;
	if (cc<=0) {
	    if (errno != EAGAIN && errno != EINTR)
		teapot(inbuf, insize);
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

	if (insize >= 20 || sleeps > 5) {
	    // Special quick exits for bad callers.
	    if (insize >= 1 && inbuf[0] != 0)
		teapot(inbuf, insize);
	    if (insize >= 2 && inbuf[1] < 3)
		teapot(inbuf, insize);
	    if (insize >= 2 && inbuf[1] != 7) {
		if (insize >= 66) {
		    convert_logon_packet(inbuf, &player);
		    strcpy(user_id, player.user_id);
		}
		disconnect(0, "Only protocol version seven is supported");
	    }
	}
	if (insize >= 2 && inbuf[1] < 6)
	    rqsize = msglen[0] - 1;
    }
    fcntl(line_ifd, F_SETFL, 0);

    convert_logon_packet(inbuf, &player);
    strcpy(user_id, player.user_id);

    if (player.protocol != 7)
	disconnect(0, "Only protocol version 7 is supported");

    for(int i = 0; user_id[i]; i++)
	if (!isascii(user_id[i]) ||
	    (!isalnum(user_id[i]) && user_id[i] != '.' && user_id[i] != '_')) {
	    disconnect(0, "Invalid user name");
	}

    if (client_ipv4_port)
	printlog("Logging in user '%s' from %s", user_id, client_ipv4_str);
    else
	printlog("Logging in user '%s'", user_id);

    if (*server->secret != 0 && *server->secret != '-') {
	if (strlen(player.mppass) != 32 && !client_trusted)
	    disconnect(0, "Login failed! Mppass required");
    }

    if (*user_id == 0) disconnect(0, "Username must be entered");
    if (strlen(user_id) > 16)
	disconnect(0, "Usernames must be between 1 and 16 characters");

    cpe_requested = player.cpe_flag;

    if (client_trusted && (!*player.mppass || strcmp("(none)", player.mppass) == 0))
	user_authenticated = 1;
    else if (*server->secret != 0 && *server->secret != '-') {
	// NB: Not vulnerable to length extension attacks due to previous
	// checks on user_id character set and length. (and NULs not allowed)
	MD5_CTX mdContext;
	MD5Init (&mdContext);
	unsigned char * s = (unsigned char *)server->secret;
	MD5Update (&mdContext, s, strlen(s));
	s = (unsigned char *)user_id;
	MD5Update (&mdContext, s, strlen(s));
	MD5Final (&mdContext);

	char hashbuf[NB_SLEN];
	for (int i = 0; i < 16; i++)
	    sprintf(hashbuf+i*2, "%02x", mdContext.digest[i]);

	if (strcasecmp(hashbuf, player.mppass) != 0)
	    disconnect(0, "Login failed! Close the game and sign in again.");

	user_authenticated = 1;
    }
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
teapot(uint8_t * buf, int len)
{
    int dump_it = 1;
    int rv = 4;
    int send_new_logout = 0;
    int send_tls_fail = 0;

    if (client_ipv4_port) {
	int fm = 1;
	if (len == 0)
	    rv = dump_it = 0;
	else if (len > 0 && len < 5) {
	    char hbuf[32] = "";
	    for(int i=0; i<len; i++)
		sprintf(hbuf+i*6, ", 0x%02x", buf[i]);
	    printlog("Received byte%s %s from %s",
		len>1?"s":"", hbuf+2, client_ipv4_str);
	    fm = rv = dump_it = 0;
	} else if (buf[0] == 0x16 && buf[1] == 0x03 && buf[1] >= 1 && buf[1] <= 3) { 
	    printlog("Received a TLS Client hello packet from %s",
		client_ipv4_str);
	    fm = rv = dump_it = 0;
	    send_tls_fail = 1;
	} else if (len > 12 && buf[1] == 0 && buf[0] >= 12 && buf[0] < len && buf[0] < 32 &&
	    buf[buf[0]-2] == ((tcp_port_no>>8)&0xFF) && buf[buf[0]-1] == (tcp_port_no&0xFF)) {
	    // This looks like a current protocol packet.
	    printlog("Failed connect from %s, %s", client_ipv4_str,
		"Using newer Minecraft protocol.");
	    fm = rv = dump_it = 0;
	    send_new_logout = 1;
	}
	if (fm)
	    printlog("Failed connect from %s, %s", client_ipv4_str,
		len ? "invalid client hello": "no data.");
    }
    else if (len <= 0)
	printlog("Nothing received from remote");

    if (dump_it) {
	for(int i = 0; i<len; i++)
	    hex_logfile(buf[i]);
	hex_logfile(EOF);
    }

    if (line_ofd > 0)
    {
	if (send_new_logout) {
	    // This should be a new protocol disconnect message.
	    char msg[] =  {
		     70,   0,  68, '{', '"', 't', 'e', 'x', 't', '"',
		    ':', ' ', '"', 'O', 'n', 'l', 'y', ' ', 'c', 'l',
		    'a', 's', 's', 'i', 'c', ' ', 'p', 'r', 'o', 't',
		    'o', 'c', 'o', 'l', ' ', 's', 'e', 'v', 'e', 'n',
		    ' ', 'i', 's', ' ', 's', 'u', 'p', 'p', 'o', 'r',
		    't', 'e', 'd', '"', ',', ' ', '"', 'c', 'o', 'l',
		    'o', 'r', '"', ':', ' ', '"', 'r', 'e', 'd', '"',
		    '}', 1, 0, 0 };
	    write_to_remote(msg, sizeof(msg)-1);
	} else if (send_tls_fail) {
	    char msg[] = { 0x15, 0x03, 0x01, 0x00, 0x02, 0x02, 80 };
	    write_to_remote(msg, sizeof(msg));
	} else {
	    char msg[] = "418 I'm a teapot\n";
	    write_to_remote(msg, sizeof(msg)-1);
	}

	flush_to_remote();
	shutdown(line_ofd, SHUT_RDWR);
	if (line_ofd != line_ifd)
	    shutdown(line_ifd, SHUT_RDWR);
    }
    exit(rv);
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
    fprintf(f, "  -dir X     Change to directory before opening data files.\n\n");

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
    fprintf(f, "  -nocpe     Don't accept a CPE request.\n");

    fprintf(f, "  -cron      Run periodic tasks, heartbeat and backups\n");
    fprintf(f, "  -runonce   Accept one connection without forking, for debugging.\n");
    fprintf(f, "  -logstderr Logs to stderr, not log files.\n");
    fprintf(f, "  -saveconf  Save current system conf and exit\n");

    exit(1);
}

char *
main_level()
{
    static char main_level_cpy[NB_SLEN];
    volatile char *s = server->main_level;
    char * d = main_level_cpy;
    while ( STFU(*d++ = *s++) );
    return main_level_cpy;
}
