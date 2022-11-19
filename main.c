#include <fcntl.h>
#include <errno.h>
#include <assert.h>

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
char current_level_fname[MAXLEVELNAMELEN*4];
int current_level_backup_id = 0;

char logfile_pattern[1024] = "";

int op_enabled = 0;	// The set-op packet can be sent
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
    saprintf(program_name, "%s", argv[0]);

    init_textcolours();

    process_args(argc, argv);

    proc_args_mem = argv[0];
    proc_args_len = argv[argc-1] + strlen(argv[argc-1]) - argv[0] + 1;

    if (*logfile_pattern)
	set_logfile(logfile_pattern, 0);

    lock_start(system_lock);

    {
	// Check main level lock
	char fixname[MAXLEVELNAMELEN*4];
	char sharename[256];
	fix_fname(fixname, sizeof(fixname), main_level());
	saprintf(sharename, LEVEL_LOCK_NAME, fixname);
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

    if (my_user.user_perm < 0) {
	if (my_user.ban_message[0])
	    disconnect(0, my_user.ban_message);
	else
	    disconnect(0, "Banned on this server");
    }

    memset(proc_args_mem, 0, proc_args_len);
    snprintf(proc_args_mem, proc_args_len, "%s (%s)", SWNAME, user_id);

    user_logged_in = 1; // May not be "authenticated", but they exist.

    // If in classic mode, don't allow place of bedrock.
    // But also Classic v7 protocol for c0.28_01 does not support op packet.
    op_enabled = (server->op_flag && protocol_base_version == 7 && cpe_requested);

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

    send_server_id_pkt(server->name, server->motd, server->op_flag);
    cpe_pending = 0;

    // List of users
    start_user();
    update_player_look();

    // Chat to users
    create_chat_queue();

    // Chat to users
    create_cmd_queue();

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
    if (level_prop)
	level_prop->no_unload = server->no_unload_main;

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

    if (shared_chat_queue)
	printf_chat("@&W- &7%s &WCrashed: &S%s", user_id, emsg);

    if (line_ofd > 0)
	disconnect(1, emsg);

    exit(42);
}

void
crashed(char * emsg)
{
    printf_chat("@&W- &7%s &WCrashed: &S%s", user_id, emsg);
    disconnect(0, emsg);
}

void
logout(char * emsg)
{
    printf_chat("@&W- &7%s &S%s", user_id, emsg);
    disconnect(0, emsg);
}

void
kicked(char * emsg)
{
    printf_chat("@&W- &7%s &Skicked %s", user_id, emsg);
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
    printf_chat("@&W- &7%s &Sbanned %s", user_id, emsg);
    char kbuf[256], *s, *d;
    saprintf(kbuf, "Banned %s", emsg);
    for(s=d=kbuf; *s; s++)
	if (*s != '&') *d++ = *s;
	else if (s[1]) s++;
    *d = 0;
    my_user.kick_count++;
    my_user.dirty = 1;
    my_user.user_perm = -1;
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
	    scan_and_save_levels(0);
	} else
	    socket_shutdown(0);
    }
    exit(rv);
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
#ifdef MAXHTTPDOWNLOAD
		} else if(http_download(inbuf, insize)) {
		    exit(0);
#endif
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
    saprintf(user_id, "%s%s", player.user_id, ini_settings->user_id_suffix);
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

    if (client_ipv4_port) {
	char proto[30] = "";
	if (protocol_base_version != 7)
	    sprintf(proto, " (protocol %d)", player.protocol);
	printlog("Logging in%s user%s '%s' from %s",
	    cpe_requested&&!server->cpe_disabled?"":" classic",
	    proto, user_id, client_ipv4_str);
    } else
	printlog("Logging in%s user '%s'",
	    cpe_requested&&!server->cpe_disabled?"":" classic", user_id);

    if (*server->secret != 0 && *server->secret != '-') {
	if (!client_trusted && !ini_settings->allow_pass_verify &&
		strlen(player.mppass) != 32 &&
		strcmp(player.mppass, server->secret) != 0)
	    disconnect(0, "Login failed! Mppass required");
    }

    if (*user_id == 0) disconnect(0, "Username must be entered");
    if (strlen(user_id) > 16)
	disconnect(0, "Usernames must be between 1 and 16 characters");

    if (client_trusted && !ini_settings->disallow_ip_verify && (!*player.mppass
			|| strcmp("-", player.mppass) == 0
			|| strncmp("00000000000000000000000000000000", player.mppass, strlen(player.mppass)) == 0
			|| strcmp("(none)", player.mppass) == 0))
    {
	// Trusted net and they haven't tried to enter an mppass.
	user_authenticated = 1;

    } else if (*server->secret != 0 && *server->secret != '-') {
	// There's an attempted mppass or we require one.
	if (check_mppass(player.user_id, player.mppass) == 0) {
	    if (!ini_settings->allow_pass_verify)
		disconnect(0, "Login failed! Close the game and refresh the server list.");
	    else
		printlog("User %s failed with mppass \"%s\", try /pass", user_id, player.mppass);
	} else
	    user_authenticated = 1;
    }
}
