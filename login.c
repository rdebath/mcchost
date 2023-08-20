#include <fcntl.h>

#include "login.h"

/* Peek the login packet from the client and do processing for that */
/* The login_*() functions are below */
void
login()
{
    pkt_player_id player = {0};
    login_peek_packet(&player);

    saprintf(user_id, "%s%s", player.user_id, ini_settings->user_id_suffix);
    saprintf(player_list_name.c, "&f%s", user_id);

    login_check_protocol(player.protocol);
    login_check_userid(player.user_id);

    protocol_base_version = player.protocol;
    cpe_requested = player.cpe_flag;

    login_log_ident();

    // -------------------------------------------------------------------
    int mppass_is_null =
	(!*player.mppass
	|| strcmp("-", player.mppass) == 0
	|| strncmp("00000000000000000000000000000000", player.mppass, strlen(player.mppass)) == 0
	|| strcmp("(none)", player.mppass) == 0);

    // If it's from a trusted net authenticate based on that unless the user
    // is testing the mppass.
    if (client_trusted && mppass_is_null && !ini_settings->disallow_ip_verify) {
	// Trusted net and they haven't tried to enter an mppass.
	fprintf_logfile("IP bypass login successful for %s", user_id);
	user_authenticated = 1;
	return;
    }

    // -------------------------------------------------------------------
    // If the mppass is setup we must check it and only allow /pass if the
    // user has previously set it up. This is because we don't know which
    // users the registry has setup and we don't want to override those
    // which it has but we have never seen.
    int mppass_is_required = (!ini_settings->disable_salt_login &&
			    *server->secret != 0 && *server->secret != '-');

    if (mppass_is_required) {
	// Is the MPPass correct ?
	if (check_mppass(player.user_id, player.mppass)) {
	    fprintf_logfile("MPPass login successful for %s", user_id);
	    user_authenticated = 1;

#ifdef UCMD_PASS
	} else if (ini_settings->allow_pass_verify && has_passwd_hash()) {
	    // Allow passwd, but only if created when logged in.

	    // Maybe the given mppass is the passwd ?
	    if (do_pass(player.mppass, 1)) {
		fprintf_logfile("Password alt login successful for %s", user_id);
		user_authenticated = 1;
	    }
#endif

	} else {
	    if (strlen(player.mppass) != 32)
		disconnect_f(0, "Login failed! Please connect using the server list.");
	    disconnect_f(0, "Login failed! Close the game and refresh the server list.");
	}
    }

#ifdef UCMD_PASS
    if (!mppass_is_required) {
	// mppass_is_required == false ...

	// Is the mppass string actually the /pass passwd for this user.
	if (!user_authenticated && do_pass(player.mppass, 1)) {
	    fprintf_logfile("Password login successful for %s", user_id);
	    user_authenticated = 1;
	}

	// If both MPPass and passwd are turned off and a password hasn't been set
	// specifically for this user.
	if (!ini_settings->allow_pass_verify && !has_passwd_hash()) {
	    fprintf_logfile("Unverified login successful for %s", user_id);
	    user_authenticated = 1;
	}
    }

    if (!user_authenticated)
	fprintf_logfile("Using %s/pass login for %s", ini_settings->void_for_login?"void ":"", user_id);
#else
    if (!mppass_is_required) {
	fprintf_logfile("MPPass login turned off for %s", user_id);
	user_authenticated = 1;
    }
#endif
}

LOCAL void
login_peek_packet(pkt_player_id *player_p)
{
    char inbuf[4096] = {0};
    int insize = 0, rqsize = msglen[0];
    websocket = 0;

    // First logon packet is read decoded this, not the normal loop.
    // This prevents any processing of other packets and allows
    // us to give special responses and short timeouts.
    //
    // Also; the normal processing loop must now ignore packet zero.
    // (It does still log it though)
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
		    pkt_player_id player = {0};
		    convert_logon_packet(inbuf, &player);
		    strcpy(user_id, player.user_id);
		}
		disconnect_f(0, "Short logon packet received");
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
	    if (insize >= 2 && (inbuf[1] > 7 || inbuf[1] < 4)) {
		if (insize >= 66) {
		    pkt_player_id player = {0};
		    convert_logon_packet(inbuf, &player);
		    strcpy(user_id, player.user_id);
		}
		disconnect_f(0, "Unsupported protocol version %d", inbuf[1]);
	    }
	}
	if (insize >= 2 && inbuf[1] < 6)
	    rqsize = msglen[0] - 1;
	if (insize >= rqsize && inbuf[0] == 0 && inbuf[1] <= 7)
	    break;
    }
    fcntl(line_ifd, F_SETFL, 0);

    // Reset for requestloop; which will now read this packet and ignore it.
    if (websocket) websocket = 1;
    msglen[0] = rqsize; // Packetlen for proto 3..5

    convert_logon_packet(inbuf, player_p);
}

LOCAL void
login_check_protocol(int protocol)
{
    // The older protocols are mostly the same but with fewer valid blocks.
    // 7 -- classic 0.28 - 0.30	-- CPE only for this version.
    // 6 -- classic 0.0.20+	-- No setuser type packet.
    // 5 -- classic 0.0.19	-- No usertype field.
    // 3/4 -- classic 0.0.16+	-- Differences with teleport and spawn positions.
    // ? -- classic 0.0.15a	-- No protocol version in ident packet.
    switch (protocol) {
    case 7: break;
    case 6: client_block_limit = Block_Gold+1; break;
    case 5: client_block_limit = Block_Glass+1; break;
    case 4:
    case 3: client_block_limit = Block_Leaves+1; break;
    default: disconnect_f(0, "Unsupported protocol version %d", protocol);
    }
}

LOCAL void
login_check_userid(char * user)
{
    if (!user || *user == 0) disconnect_f(0, "Username must be entered");

    for(int i = 0; user[i]; i++)
	if (!isascii(user[i]) ||
	    (!isalnum(user[i]) && user[i] != '.' && user[i] != '_')) {
	    disconnect_f(0, "Invalid user name");
	}

    if (strlen(user) > 16)
	disconnect_f(0, "Usernames must be between 1 and 16 characters");
}

LOCAL void
login_log_ident()
{
    if (client_ipv4_port) {
	char proto[30] = "";
	if (protocol_base_version != 7)
	    sprintf(proto, " (protocol %d)", protocol_base_version);
	else if (!cpe_requested)
	    sprintf(proto, " (no cpe)");
	printlog("Logging in%s user%s '%s' from %s",
	    cpe_requested&&!server->cpe_disabled?"":" classic",
	    proto, user_id, client_ipv4_str);
    } else
	printlog("Logging in%s user '%s'",
	    cpe_requested&&!server->cpe_disabled?"":" classic", user_id);
}
