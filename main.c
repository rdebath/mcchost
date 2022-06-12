#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <sys/socket.h>

#include "main.h"

#define LIBINLINE
#include "inline.h"

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
    int magic2;
};
#endif

int line_ofd = -1;
int line_ifd = -1;

char user_id[NB_SLEN];
int user_authenticated = 0;
int server_id_op_flag = 1;
int inetd_mode = 0;

char program_name[512];

#if 0
server_t server[1] =
{
    (server_t){
	.software = "MCCHost",
	.name = "MCCHost Server",
	.main_level = "main",
	.save_interval = 300,
	.backup_interval = 86400,
    }
};
#endif

volatile server_t *server = 0;

nbtstr_t client_software = {"(unknown)"};

char current_level_name[MAXLEVELNAMELEN+1];
char current_level_fname[MAXLEVELNAMELEN*4];

char heartbeat_url[1024] = "http://www.classicube.net/server/heartbeat/";
char logfile_pattern[1024] = "";
int server_runonce = 0;
int save_conf = 0;

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

    if (!inetd_mode && !start_tcp_server && (isatty(0) || isatty(1)))
	show_args_help();

    if (*logfile_pattern)
	set_logfile(logfile_pattern, 0);

    if (save_conf) {
	save_ini_file(system_ini_fields, SERVER_CONF_NAME);
	fprintf(stderr, "Configuration saved\n");
	exit(0);
    }

    init_dirs();

    delete_session_id(0, 0, 0);

    if (start_tcp_server) {
	memset(proc_args_mem, 0, proc_args_len);
	snprintf(proc_args_mem, proc_args_len, "%s port %d", server->software, tcp_port_no);

	tcpserver();
    } else {
	line_ofd = 1; line_ifd = 0;
    }

    process_connection();
    run_request_loop();
    send_disconnect_message();
    stop_user();
    return 0;
}

void
process_connection()
{
    open_client_list();

    login();

    memset(proc_args_mem, 0, proc_args_len);
    snprintf(proc_args_mem, proc_args_len, "%s (%s)", server->software, user_id);

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
    open_level_files(fixname, 0);
    if (level_prop)
	create_block_queue(fixname);

    if (!level_prop)
	fatal("Unable to load initial map file -- sorry");

    send_map_file();
    send_spawn_pkt(255, user_id, level_prop->spawn);

    printf_chat("&SWelcome &7%s", user_id);
    printf_chat("@&a+ &7%s &Sconnected", user_id);

    read_only_message();

    cmd_help("welcome", 0);
}

void
read_only_message()
{
    if (level_prop->readonly) {
	if (level_prop->allowchange)
	    printf_chat("&WLoaded read only map, any changes you make will be discarded when the level unloads");
	else
	    printf_chat("&WLoaded read only map");
    }
}

void
send_disconnect_message()
{
    printf_chat("@&c- &7%s &Sdisconnected", user_id);
    fprintf_logfile("Connection dropped for %s", user_id);
}

void
login()
{
    char inbuf[1024];
    int insize = 0, inptr = 0, rqsize = msglen[0];

    // We should get 
    time_t startup = time(0);
    fcntl(line_ifd, F_SETFL, (int)O_NONBLOCK);
    while(insize-inptr < rqsize)
    {
	int cc = read(line_ifd, inbuf+insize, rqsize-insize);
	if (cc>0) insize += cc;
	if (cc<=0) {
	    if (errno != EAGAIN && errno != EINTR)
		teapot(inbuf+inptr, insize);
	    time_t now = time(0);
	    if (now-startup > 4) {
		if (insize >= 2 && inbuf[inptr+1] >= 3 && inbuf[inptr+1] <= 7) {
		    if (insize >= 66 && inbuf[inptr+1] > 0 && inbuf[inptr+1] < 7)
			cpy_nbstring(user_id, inbuf+2);
		    quiet_drop("Short logon packet received");
		} else
		    teapot(inbuf+inptr, insize);
	    } else
		usleep(50000);
	}

	if (insize >= 1 && inbuf[inptr] != 0) // Special exit for weird caller.
	    teapot(inbuf+inptr, insize);
	if (insize >= 2 && inbuf[inptr+1] < 3)
	    teapot(inbuf+inptr, insize);
	if (insize >= 2 && inbuf[inptr+1] < 6)
	    rqsize = msglen[0] - 1;
    }
    fcntl(line_ifd, F_SETFL, 0);

    char mppass[NB_SLEN];
    cpy_nbstring(mppass, inbuf+64+2);
    cpy_nbstring(user_id, inbuf+2);

    if (inbuf[inptr+1] != 7)
	quiet_drop("Only protocol version seven is supported");

    for(int i = 0; user_id[i]; i++)
	if (!isascii(user_id[i]) ||
	    (!isalnum(user_id[i]) && user_id[i] != '.' && user_id[i] != '_')) {
	    quiet_drop("Invalid user name");
	}

    if (client_ipv4_port)
	fprintf_logfile("Logging in user '%s' from %s:%d",
	    user_id, client_ipv4_str, client_ipv4_port);
    else
	fprintf_logfile("Logging in user '%s'", user_id);

    if (*server->secret != 0 && *server->secret != '-') {
	if (strlen(mppass) != 32 && !client_ipv4_localhost)
	    quiet_drop("Login failed!");
    }

    if (*user_id == 0) quiet_drop("Username must be entered");
    if (strlen(user_id) > 16)
	quiet_drop("Usernames must be between 1 and 16 characters");

    cpe_requested = (inbuf[inptr+1] == 7 && inbuf[inptr+128+2] == 0x42);

    if (*mppass == 0 && client_ipv4_localhost)
	user_authenticated = 1;
    else if (*server->secret != 0 && *server->secret != '-') {
	char hashbuf[NB_SLEN*2];
	strcpy(hashbuf, IGNORE_VOLATILE_CHARP(server->secret));
	strcat(hashbuf, user_id);

	MD5_CTX mdContext;
	unsigned int len = strlen (hashbuf);
	MD5Init (&mdContext);
	MD5Update (&mdContext, (unsigned char *)hashbuf, len);
	MD5Final (&mdContext);

	for (int i = 0; i < 16; i++)
	    sprintf(hashbuf+i*2, "%02x", mdContext.digest[i]);

	if (strcasecmp(hashbuf, mppass) != 0)
	    quiet_drop("Login failed! Close the game and sign in again.");

	user_authenticated = 1;
    }
}

void
fatal(char * emsg)
{
    static int in_fatal = 0;
    assert(!in_fatal);
    in_fatal = 1;

    if (level_chat_queue)
	printf_chat("@&W- &7%s &WCrashed: &S%s", user_id, emsg);
    disconnect(1, emsg);
}

void
logout(char * emsg)
{
    printf_chat("@&W- &7%s &S%s", user_id, emsg);
    disconnect(0, emsg);
}

LOCAL void
quiet_drop(char * emsg)
{
    for(int i = 0; user_id[i]; i++)
	if (user_id[i] <= ' ' || user_id[i] == '\'' || user_id[i] > '~')
	    user_id[i] = '*';
    disconnect(0, emsg);
}

LOCAL void
teapot(uint8_t * buf, int len)
{
    if (line_ofd < 0) return;

    if (client_ipv4_port)
	fprintf_logfile("Failed connect %s:%d, %s",
	    client_ipv4_str, client_ipv4_port,
	    len ? "invalid client hello": "no data.");

    for(int i = 0; i<len; i++)
	hex_logfile(buf[i]);
    hex_logfile(EOF);

    char msg[] = "418 I'm a teapot\n";
    write_to_remote(msg, sizeof(msg)-1);

    flush_to_remote();
    shutdown(line_ofd, SHUT_RDWR);
    if (line_ofd != line_ifd)
	shutdown(line_ifd, SHUT_RDWR);
    exit(4);
}

LOCAL void
disconnect(int rv, char * emsg)
{
    if (line_ofd < 0) return;

    if (client_ipv4_port && *user_id)
	fprintf_logfile("Disconnect %s:%d, user '%s', %s",
	    client_ipv4_str, client_ipv4_port, user_id, emsg);
    else if (client_ipv4_port)
	fprintf_logfile("Disconnect %s:%d, %s",
	    client_ipv4_str, client_ipv4_port, emsg);
    else if (*user_id)
	fprintf_logfile("Disconnect user '%s', %s", user_id, emsg);
    else
	fprintf_logfile("Disconnect %s", emsg);

    stop_user();
    send_discon_msg_pkt(emsg);
    flush_to_remote();
    shutdown(line_ofd, SHUT_RDWR);
    if (line_ofd != line_ifd)
	shutdown(line_ifd, SHUT_RDWR);
    exit(rv);
}

void
show_args_help()
{
    fprintf(stderr, "Usage: %s [-inetd|-tcp|-net] ...\n", program_name);
    fprintf(stderr, "  -dir X     Change to directory before opening data files.\n\n");

    fprintf(stderr, "  -tcp       Open a tcp socket for listening, no default secret.\n");
    fprintf(stderr, "  -net       Open a socket, register the secret at:\n%16s%s\n", "", heartbeat_url);
    fprintf(stderr, "  -port X    Set port number listened to.\n\n");
    fprintf(stderr, "  -inetd     Assume socket is stdin/out from inetd or similar\n");
    fprintf(stderr, "  -detach    Background self, eg if starting from command line.\n");
    fprintf(stderr, "  -no-detach Do not background; starting from init, systemd, docker.\n");

    fprintf(stderr, "  -private -public\n");
    fprintf(stderr, "             When registering set privacy state.\n");

    fprintf(stderr, "  -name X    Set server name\n");
    fprintf(stderr, "  -motd X    Set server motd\n");
    fprintf(stderr, "  -secret X\n");
    fprintf(stderr, "  -salt X    Set server salt/secret\n");
    fprintf(stderr, "  -heartbeat http://host.xz/path\n");
    fprintf(stderr, "             Change hearbeat url\n");
    fprintf(stderr, "  -runonce   Accept one connection without forking, for debugging.\n");
    fprintf(stderr, "  -nocpe     Don't accept a CPE request.\n");
    fprintf(stderr, "  -saveconf  Save current system conf and exit\n");

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
