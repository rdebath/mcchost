#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>

#include "main.h"

int line_ofd = -1;
int line_ifd = -1;

char user_id[NB_SLEN];	// This is ASCII not CP437 or UTF8
int user_authenticated = 0;
int user_logged_in = 0;
int inetd_mode = 0;
int start_heartbeat_task = 0;
int start_backup_task = 0;

nbtstr_t client_software;

char current_level_name[MAXLEVELNAMELEN+1];
int current_level_backup_id = 0;

char logfile_pattern[1024] = "";

int op_enabled = 0;	// The set-op packet can be sent
int cpe_enabled = 0;	// Set if this session is using CPE
int cpe_requested = 0;	// Set if cpe was requested, even if rejected.
int cpe_pending = 0;	// Currently running ExtInfo process.
int cpe_extn_remaining = 0;
int cpe_extn_advertised = 0;
int protocol_base_version = 7;
time_t login_time = 0;

int
main(int argc, char **argv)
{
    check_stdio_fd();

    saprintf(program_name, "%s", argv[0]);

    init_textcolours();

    proctitle_init(argc, argv);
    process_args(argc, argv);

    set_logfile(logfile_pattern, 0);

    lock_start(system_lock);

    if (start_heartbeat_task || start_backup_task)
        run_timer_tasks();

    delete_session_id(0, 0, 0);

    if (start_tcp_server) {
	proctitle("%s server", SWNAME);

	tcpserver();
    } else {
	line_ofd = 1; line_ifd = 0;
	if (inetd_mode) {
	    if (!log_to_stderr)
		logger_process();
	    check_inetd_connection();
	}
    }

    lock_restart(system_lock);

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
	scan_and_save_levels(1);
    }

    return 0;
}

void
process_connection()
{
    open_client_list();

    login();

    write_current_user(1);

    if (my_user.banned) {
	if (my_user.ban_message[0])
	    disconnect(0, my_user.ban_message);
	else
	    disconnect(0, "Banned on this server");
    }

    if (ini_settings->admin_only_login) {
	if (!perm_is_admin())
	    disconnect(0, "Login rejected! Only administrators may connect");
	if (!ini_settings->void_for_login && !user_authenticated)
	    disconnect(0, "Login rejected, unverified admin may only see void map");
    }

    proctitle("%s (%s)", SWNAME, user_id);

    user_logged_in = 1; // May not be "authenticated", but they exist.

    // If in classic mode, don't allow place of bedrock.
    // But also Classic v7 protocol for c0.28_01 does not support op packet.
    op_enabled = (server->op_flag && protocol_base_version == 7 && cpe_requested);

    if (cpe_requested)
	send_cpe_ext_list();

    if (!cpe_pending)
	complete_connection();
}

void
complete_connection()
{
    if (extn_evilbastard)
	fatal("Server is incompatible with Evil bastard extension");

    send_server_id_pkt(server->name, server->motd, server->op_flag);
    cpe_pending = 0;

    // List of users
    start_user();
    update_player_look();

    // Chat to users
    create_chat_queue();

    // Chat to users
    create_cmd_queue();

    // Permissions.
    load_ini_file(cmdset_ini_fields, CMDSET_CONF_NAME, 1, 0);
    init_cmdset_perms();

    open_main_level();

#ifdef UCMD_TEXTCOLOUR
    send_textcolours();
#endif

    if (my_user.logon_count > 1)
	printf_chat("&SWelcome back %s&S! You've been here %d times!", player_list_name.c, (int)my_user.logon_count);
    else
	printf_chat("&SWelcome %s", player_list_name.c);
    printf_chat("@&a+ %s &Sconnected", player_list_name.c);

    read_only_message();

#ifdef UCMD_HELP
    cmd_help("welcome", 0);
#endif

#ifdef UCMD_PASS
    if (!user_authenticated)
	do_pass(0, 0);
    else
#endif
    if (!level_prop)
	printf_chat("&WMain level failed to load, you are nowhere.");
}

void
read_only_message()
{
    if (!level_prop || !level_prop->readonly) return;
    if (level_prop->map_load_failure) {
	printf_chat("&WMap set to read only as load was incomplete");
	return;
    }
    if (level_prop->disallowchange && extn_inventory_order && extn_sethotbar) return;

    printf_chat("&WLoaded read only map, changes won't be saved.");
}

void
send_disconnect_message()
{
    printf_chat("@&c- %s &Sdisconnected", player_list_name.c);
    printlog("Connection dropped for %s", user_id);
}

void
fatal(char * emsg)
{
    static int in_fatal = 0;
    assert(!in_fatal);
    in_fatal = 1;

    fprintf(stderr, "FATAL(%d): %s\n", getpid(), emsg);

    if (shared_chat_queue)
	printf_chat("@&W- %s &WCrashed: &S%s", player_list_name.c, emsg);

    if (line_ofd > 0)
	disconnect(1, emsg);

    exit(42);
}

void
crashed(char * emsg)
{
    printf_chat("@&W- %s &WCrashed: &S%s", player_list_name.c, emsg);
    disconnect(0, emsg);
}

void
logout(char * emsg)
{
    printf_chat("@&W- %s &S%s", player_list_name.c, emsg);
    disconnect(0, emsg);
}

void
kicked(char * emsg)
{
    printf_chat("@&W- %s &Skicked %s", player_list_name.c, emsg);
    char kbuf[256], *s, *d;
    saprintf(kbuf, "Kicked %s", emsg);
    for(s=d=kbuf; *s; s++)
	if (*s != '&') *d++ = *s;
	else if (s[1]) s++;
    *d = 0;
    my_user.kick_count++;
    disconnect(0, kbuf);
}

void
banned(char * emsg)
{
    printf_chat("@&W- %s &Sbanned %s", player_list_name.c, emsg);
    char kbuf[256], *s, *d;
    saprintf(kbuf, "Banned %s", emsg);
    for(s=d=kbuf; *s; s++)
	if (*s != '&') *d++ = *s;
	else if (s[1]) s++;
    *d = 0;
    my_user.kick_count++;
    my_user.dirty = 1;
    my_user.banned = 1;
    strncpy(my_user.ban_message, kbuf, MB_STRLEN);
    my_user.ini_dirty = 1;
    disconnect(0, kbuf);
}

LOCAL void
disconnect(int rv, char * emsg)
{
    if (client_ipv4_port > 0 && *user_id)
	printlog("Disconnect %s, user '%s', %s", client_ipv4_str, user_id, emsg);
    else if (client_ipv4_port > 0)
	printlog("Disconnect %s, %s", client_ipv4_str, emsg);
    else if (*user_id)
	printlog("Disconnect user '%s', %s", user_id, emsg);
    else
	printlog("Disconnect %s", emsg);

    stop_user();

    if (line_ofd > 0) {
	send_discon_msg_pkt(emsg);
	flush_to_remote();

	if (inetd_mode && user_authenticated) {
	    socket_shutdown(1);
	    scan_and_save_levels(1);
	} else
	    socket_shutdown(0);
    }
    exit(rv);
}

#if INTERFACE
#define fmt_logout_w __attribute__ ((format (printf, 1, 2)))
#endif

void fmt_logout_w
logout_f(char * fmt, ...)
{
    char pbuf[16<<10];
    va_list ap;
    va_start(ap, fmt);
    int l = vsnprintf(pbuf, sizeof(pbuf), fmt, ap);
    if (l > sizeof(pbuf)) {
        strcpy(pbuf+sizeof(pbuf)-4, "...");
        l = sizeof(pbuf);
    }
    va_end(ap);

    logout(pbuf);
}

#if INTERFACE
#define fmt_fatal_w __attribute__ ((format (printf, 1, 2)))
#endif

void fmt_fatal_w
fatal_f(char * fmt, ...)
{
    char pbuf[16<<10];
    va_list ap;
    va_start(ap, fmt);
    int l = vsnprintf(pbuf, sizeof(pbuf), fmt, ap);
    if (l > sizeof(pbuf)) {
        strcpy(pbuf+sizeof(pbuf)-4, "...");
        l = sizeof(pbuf);
    }
    va_end(ap);

    fatal(pbuf);
}

#if INTERFACE
#define fmt_disc_w __attribute__ ((format (printf, 2, 3)))
#endif

void fmt_disc_w
disconnect_f(int rv, char * fmt, ...)
{
    char pbuf[16<<10];
    va_list ap;
    va_start(ap, fmt);
    int l = vsnprintf(pbuf, sizeof(pbuf), fmt, ap);
    if (l > sizeof(pbuf)) {
        strcpy(pbuf+sizeof(pbuf)-4, "...");
        l = sizeof(pbuf);
    }
    va_end(ap);

    disconnect(rv, pbuf);
}

void
socket_shutdown(int do_close)
{
    if (line_ifd < 0 && line_ofd < 0) return;
    if (line_ifd >= 0 && line_ofd >= 0) {
	if (shutdown(line_ofd, SHUT_WR) < 0)
	    perror("shutdown(line_ofd,SHUT_WR)");
	else if (line_ifd >= 0) {
	    fcntl(line_ifd, F_SETFL, (int)O_NONBLOCK);
	    char buf[4096];
	    int cc, max_loop = 120;
	    while((cc = read(line_ifd, buf, sizeof(buf))) != 0) {
		if (cc>0) continue;
		if (errno != EWOULDBLOCK) {
		    if (errno != ECONNRESET)
			perror("Read after shutdown");
		    break;
		}
		if (--max_loop <= 0) break;
		msleep(500);
	    }
	    fcntl(line_ifd, F_SETFL, 0);
	}
    }

    if (do_close) {
	close(line_ofd);
	if (line_ofd != line_ifd)
	    close(line_ifd);
	line_ofd = line_ifd = -1;
    }
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
    fprintf(f, "  -net       Open a socket, register the secret at:\n%16s%s\n",
	    "", "https://www.classicube.net");
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
    fprintf(f, "  -user U,G  Use user \"U\" and group \"G\"\n");

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
init_textcolours() {
    for (int i = 0; i<256; i++) {
	textcolour[i].defined = 0;
	textcolour[i].name.c[0] = 0;
	textcolour[i].fallback = 'f';
	textcolour[i].colour = -1;
	textcolour[i].a = 255;
    }

    set_alias('H', 'e'); // HelpDescriptionColor
    set_alias('I', '5'); // IRCColor
    set_alias('S', 'e'); // DefaultColor
    set_alias('T', 'a'); // HelpSyntaxColor
    set_alias('W', 'c'); // WarningErrorColor

    for(int i = 'A'; i<='F'; i++)
	set_alias(i, i-'A'+'a');
}

void
set_alias(int from, int to)
{
    if (from > 255 || from < 0) return;
    textcolour[from].name.c[0] = 0;
    textcolour[from].defined = 1;
    textcolour[from].fallback = to;
    textcolour[from].colour = -1;
    textcolour[from].a = -1;
}

