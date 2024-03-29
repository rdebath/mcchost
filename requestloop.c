
#include "requestloop.h"
#if _POSIX_VERSION >= 200112L
#include <sys/select.h>
#endif

static char *ttl_buf = 0;		/* Buffer for line output */
static int ttl_start = 0;
static int ttl_end = 0;
static int ttl_size = 0;

static char line_inp_buf[65536];
static char rcvdpkt[PKBUF];
int in_rcvd = 0;
static int ticks_with_pending_bytes = 0;

time_t last_ping = 0;
uint16_t last_ping_id = 0;
struct timeval last_ping_tv;
int last_ping_rcvd = 0;
int last_ping_ms = -1;
int max_ping_ms = -1;
int min_ping_ms = -1;
int avg_ping_ms = -1;
int idle_ticks = 0;

#if INTERFACE
#define TICK_INTERVAL	10000/*us*/
#define TICK_SECS(S) (S*1000000/TICK_INTERVAL)
#endif

time_t last_user_write = 0;

void
run_request_loop()
{
    int flg;
    time(&last_ping);
    while(!(flg = do_select()));
}

int
bytes_queued_to_send()
{
    return ttl_start != ttl_end;
}

void write_to_remote(uint8_t * str, int len)
{
    char wbuf[16];

    if (!extn_pingpong)
	time(&last_ping);

    if(server) { server->packets_sent++; server->bytes_sent += len; }

    while (ttl_end+len+sizeof(wbuf) >= ttl_size) { // Padding needed for websocket
	int nsz = ttl_size ? ttl_size*2 : 2048;
	char * nb = realloc(ttl_buf, nsz);
	if (nb == 0) fatal("Out of memory");
	ttl_size = nsz;
	ttl_buf = nb;
    }

    if (websocket) {
	// Adding a websocket packet for every one of ours to make it
	// easy for the remote to unwrap.
	int wbuflen = 0;
	websocket_header(wbuf, &wbuflen, len);
	for(int i = 0; i < wbuflen; i++)
	    ttl_buf[ttl_end++] = wbuf[i];
    }

    for(int i = 0; i < len; i++)
	ttl_buf[ttl_end++] = str[i];
}

void
flush_to_remote()
{
    if (line_ofd < 0) return;
    int tosend = ttl_end-ttl_start;
    int rv = write(line_ofd, ttl_buf+ttl_start, tosend);

    if( rv > 0 )
    {
	if( rv != ttl_end-ttl_start ) ttl_start += rv;
	else ttl_start = ttl_end = 0;
    } else if (rv < 0 && errno != EWOULDBLOCK) {
	if (errno == ECONNRESET && user_logged_in) {
	    // Common, just gone.
	    close(line_ofd); if(line_ofd!=line_ifd) close(line_ifd);
	    line_ifd = line_ofd = -1;
	    logout("Left the game.");
	}
	fatal("Error on write line_ofd");
    }
}

int
do_select()
{
    fd_set rfds, wfds, efds;
    struct timeval tv;
    int rv;
    int max_fd = 10;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    FD_SET(line_ifd, &efds);		/* Always want to know about problems */
    FD_SET(line_ofd, &efds);		/* Always want to know about problems */
    FD_SET(line_ifd, &rfds);

    if( ttl_start != ttl_end ) FD_SET(line_ofd, &wfds);

    tv.tv_sec = 0; tv.tv_usec = TICK_INTERVAL;
    rv = select(max_fd+1, &rfds, &wfds, &efds, &tv);
    if (rv < 0 && errno == EINTR) {
	return 0;
    }

    if (rv < 0)
	fatal("select syscall");

    // Done on a timed basis
    if (!rv) on_select_timeout();

    if( FD_ISSET(line_ifd, &efds) )
	return -1;
    if( FD_ISSET(line_ofd, &efds) )
	return -1;

    if( FD_ISSET(line_ofd, &wfds) )
    {
	int tosend = ttl_end-ttl_start;
	if (tosend > 65536) tosend = 65536;
	rv = write(line_ofd, ttl_buf+ttl_start, tosend);
	if( rv > 0 )
	{
	    if( rv != ttl_end-ttl_start ) ttl_start += rv;
	    else {
		ttl_start = ttl_end = 0;

		/* Once the map is sent we probably don't need the memory */
		if (ttl_size > 65536) {
		    free(ttl_buf);
		    ttl_buf = 0;
		    ttl_size = 0;
		}
	    }
	} else if (rv < 0)
	    fatal("write line_fd syscall");
    }

    if( FD_ISSET(line_ifd, &rfds) )
    {
	rv = read(line_ifd, line_inp_buf, sizeof(line_inp_buf));
	if( rv > 0 ) {
	    if (websocket) {
		// I really don't care about websocket packets,
		// an unmasked exabyte packet is perfectly fine.
		if (websocket_translate(line_inp_buf, &rv) < 0)
		    return -1;
	    }
	    remote_received(line_inp_buf, rv);
	} else if (rv < 0) {
	    if (errno == ECONNRESET && user_logged_in)
		return -1;
	    perror("Error reading line_ifd");
	    return -1;
	} else if (rv == 0)
	    return -1;
    }

    return 0;
}


void
on_select_timeout()
{
    // 0.30 leaves just over 50ms between position updates.
    // Classicube is around 60ms.
    if (++idle_ticks > TICK_SECS(60) && !cpe_pending)
	logout("disconnected");
    else if (idle_ticks == TICK_SECS(10)) {
	if (!cpe_requested && player_posn.valid) {
	    // If there's gravity there a TP up will get a response.
	    xyzhv_t tmp_pos = player_posn;
	    tmp_pos.y+=23;
	    send_posn_pkt(-1, 0, tmp_pos);
	}
    } else if (idle_ticks == TICK_SECS(15))
	printlog("User %s: connection issue -- long idle", user_id);

    update_player_packet_idle();

    time_t now;
    int tc = cpe_pending?60:5; // Not while CPE pending

    // If a packet has not been received in full expect the rest soon.
    if (in_rcvd>0 && ++ticks_with_pending_bytes > TICK_SECS(15))
	fatal("Broken packet received -- protocol failure");

    if (player_lockout>0) player_lockout--;

    check_waitpid();

    time(&now);
    int secs = ((now-last_ping) & 0xFF);
    if (secs > tc) {
	if (cpe_pending) {
	    fprintf_logfile("User %s CPE failure (%d,%d) with %s",
		user_id, cpe_extn_advertised, cpe_extn_remaining, client_software.c);

	    logout("Connection failure, CPE logon not completed within a minute.");
	}

	// Send keepalive.
	if (!extn_pingpong)
	    send_ping_pkt();
	else {
	    if (last_ping_rcvd == 0) last_ping_ms = -1;
	    last_ping_rcvd = 0;
	    send_pingpong_pkt(1, ++last_ping_id);
	    gettimeofday(&last_ping_tv, 0);
	    flush_to_remote();
	}
	time(&last_ping);
    }
    if (cpe_pending) return;

    check_user_summon();
    check_other_users();
    check_this_user();
    check_metadata_update();
    process_queued_cmds();
    send_queued_chats(0);
    send_queued_blocks();

    secs = ((now-last_user_write) & 0xFF);
    if (secs >= 45) {
	write_current_user(0);
	last_user_write = now;
    }

    run_delay_task();
}

void
remote_received(char *str, int len)
{
    int i, cmd, clen;
    if (!extn_pingpong)
	time(&last_ping);

    idle_ticks = 0;
    update_player_packet_idle();

    if (!in_rcvd) {
	cmd = (*str & 0xFF);
	clen = msglen[cmd];
	if (clen == len) {
	    ticks_with_pending_bytes = 0;
	    process_client_message(cmd, str);
	    return;
	}
    }

    for(i = 0; i < len; i++) {
	rcvdpkt[in_rcvd++] = str[i];
	int cmd = rcvdpkt[0] & 0xFF;
	if (msglen[cmd] <= in_rcvd) {
	    if (msglen[cmd] == 0)
		fatal_f("Unknown packet type 0x%02x received by server", cmd);
	    ticks_with_pending_bytes = 0;
	    process_client_message(cmd, rcvdpkt);
	    in_rcvd = 0;
	}
    }
}

// This is for any string received from the client.
//
// NULs in the strings are not defined, however, the string received from
// the client may have NULs in any location, embedded ones are converted
// to 0xFF or 0x20 depending on extn_fullcp437, trailing ones are treated
// as trailing spaces.
//
// In addition the client should only send CP437 after it has been
// negotiated using CPE. Control characters (AKA Emoji) are more of
// an issue as the classic client could use those, with "quirks".
static inline void
sanitise_nbstring(char *buf, char *str, int printable)
{
    int l = 0;
    memcpy(buf, str, MB_STRLEN);
    buf[MB_STRLEN] = 0;
    for(int i = 0; i<MB_STRLEN; i++)
	if (buf[i] != ' ' && buf[i] != 0) l = i+1;
    if (extn_fullcp437) {
	for(int i = 0; i<l; i++) if (buf[i] == 0) buf[i] = 0xFF;
    } else {
	for(int i = 0; i<l; i++)
	    if (buf[i] == 0)
		buf[i] = ' ';
	    else if (printable && ((buf[i] > 0 && buf[i] < ' ') || buf[i] == '\177'))
		buf[i] = '*';
	    else if (buf[i] & 0x80)
		buf[i] = cp437_ascii[buf[i] & 0x7F];
    }
    char * p = buf+MB_STRLEN;
    while (p>buf && (p[-1] == ' ' || p[-1] == 0)) *(--p) = 0;
}

void
process_client_message(int cmd, char * pktbuf)
{
    if(server) { server->packets_rcvd++; server->bytes_rcvd += msglen[cmd]; }

    // During the initial startup ignore (almost) everything.
    if (cpe_pending) {
	if (cmd != PKID_EXTINFO && cmd != PKID_EXTENTRY && cmd != PKID_CUSTBLOCK)
	    return;
    }
    if (!cpe_enabled && cmd > PKID_OPER)
	logout_f("Unexpected packet type received: 0x%02x", cmd);

#if 0
    if (!cpe_enabled) {
	fprintf_logfile("Packet %d (process_client_message)", cmd);
	for(int i = 0; i<msglen[cmd]; i++) {
	    hex_logfile(pktbuf[i] & 0xFF);
	}
	hex_logfile(EOF);
    }
#endif

    switch(cmd)
    {
    case PKID_SETBLOCK:
	{
	    char * p = pktbuf+1;
	    pkt_setblock pkt;
	    // Assume this is a UIntBE16 as -ve is unreasonable
	    pkt.coord.x = UIntBE16(p); p+=2;
	    pkt.coord.y = UIntBE16(p); p+=2;
	    pkt.coord.z = UIntBE16(p); p+=2;
	    if (!cpe_requested) {
		pkt.coord.x -= classic_map_offset.x;
		pkt.coord.y -= classic_map_offset.y;
		pkt.coord.z -= classic_map_offset.z;
	    }
	    pkt.mode = *p++;
	    if (extn_extendblockno) {
		pkt.heldblock = IntBE16(p); p+=2;
	    } else
		pkt.heldblock = *(uint8_t*)p++;

	    pkt.block = pkt.mode?pkt.heldblock:Block_Air;

	    process_player_setblock(pkt);
	    update_player_move_time();

	    if (bytes_queued_to_send() > 65536) {
		send_queued_blocks();
		flush_to_remote();
	    }
	}
	break;

    case PKID_POSN:
	{
	    char * p = pktbuf+1;
	    pkt_player_posn pkt;
	    if (extn_heldblock) {
		pkt.player_id = 255;
		if (extn_extendblockno) {
		    pkt.held_block = IntBE16(p); p+=2;
		} else
		    pkt.held_block = (uint8_t)(*p++);
	    } else {
		pkt.player_id = (uint8_t)(*p++);
		pkt.held_block = -1;
	    }
	    if (extn_extentityposn) {
		pkt.pos.x = IntBE32(p); p+=4;
		pkt.pos.y = IntBE32(p); p+=4;
		pkt.pos.z = IntBE32(p); p+=4;
	    } else {
		pkt.pos.x = IntBE16(p); p+=2;
		pkt.pos.y = IntBE16(p); p+=2;
		pkt.pos.z = IntBE16(p); p+=2;
		if (!cpe_requested) {
		    pkt.pos.x -= classic_map_offset.x*32;
		    pkt.pos.y -= classic_map_offset.y*32;
		    pkt.pos.z -= classic_map_offset.z*32;
		}
	    }
	    pkt.pos.h = *p++;
	    pkt.pos.v = *p++;
	    pkt.pos.valid = 1;

	    // They lie! Position /reload is -22, position of feet is -51
	    pkt.pos.y -= 51;

	    update_player_pos(pkt);

	    if (!level_prop && !cpe_requested && !extn_extentityposn) {
		if (void_map_size.x > 3 && void_map_size.y > 3 && void_map_size.z > 3) {
		    // TP if outside void map
		    if (pkt.pos.x < 32 || pkt.pos.x >= void_map_size.x*32-32 ||
			pkt.pos.y < 16 || pkt.pos.y >= void_map_size.y*32-48 ||
			pkt.pos.z < 32 || pkt.pos.z >= void_map_size.z*32-32) {

			xyzhv_t spawn = {
			    void_map_size.x*16+16,
			    void_map_size.y*16+51,
			    void_map_size.z*16+16,
			    .valid = 1
			};

			send_posn_pkt(-1, &player_posn, spawn);
			player_posn = spawn;
		    }
		}
	    }
	}
	break;

    case PKID_MESSAGE:
	{
	    char * p = pktbuf+1;
	    pkt_message pkt;
	    if (extn_longermessages) {
		pkt.message_type = *p++;
		pkt.player_id = 0xFF;
	    } else {
		pkt.message_type = 0;
		pkt.player_id = *p++;
	    }
	    if (pkt.message_type == 1) {
		// Continuation messages are 64 bytes, *by definition*
		// So replace all NULs by spaces rather than using
		// the sanitise_nbstring() function.
		// NB: CP437 is allowed rather than checked as the client
		// probably negotiated it anyway.

		char * b = pkt.message;
		memcpy(b, p, MB_STRLEN);
		for(int i = 0; i<MB_STRLEN; i++)
		    if (b[i] == 0) b[i] = extn_fullcp437?0xFF:' ';
		b[MB_STRLEN] = 0;
	    } else
		sanitise_nbstring(pkt.message, p, 0);
	    // p+=64;
	    process_chat_message(pkt.message_type, pkt.message);
	}
	break;

    case PKID_IDENT:
	// MUST ignore this packet.
	break;

    case PKID_EXTINFO:
	{
	    char * p = pktbuf+1;
	    int count;
	    // Perhaps we want to sanitise this string after all the entries
	    // have been received ... possibly including FullCP437.
	    sanitise_nbstring(client_software.c, p, 1); p+=64;
	    count = IntBE16(p); p+=2;
	    if (cpe_pending) {
		cpe_extn_remaining = count;
		cpe_extn_advertised = cpe_extn_remaining;

		if (cpe_extn_advertised <= 0) {
		    fprintf_logfile("Note: Client for %s advertised %d extensions",
			user_id, cpe_extn_advertised);
		    pkt_extentry pkt = {0};
		    process_extentry(&pkt);
		}

	    } else
		cpe_extn_remaining = 0;
	}
	break;

    case PKID_EXTENTRY:
	{
	    char * p = pktbuf+1;
	    pkt_extentry pkt;
	    sanitise_nbstring(pkt.extname, p, 1); p+=64;
	    pkt.version = IntBE32(p); p+=4;
	    process_extentry(&pkt);
	}
	break;

    case PKID_CUSTBLOCK:
	{
	    char * p = pktbuf+1;
	    int version = *p++;
	    if (version > 0) {
		customblock_enabled = 1;
		if (client_block_limit < Block_CPE)
		    client_block_limit = Block_CPE;
	    }

	    cpe_pending |= 2;
	    if (cpe_pending == 3) {
		complete_connection();
		if (extn_pingpong) last_ping = time(0) - 60;
	    }
	}
	break;

    case PKID_PLAYERCLK:
	if (user_authenticated) // Maybe move to process_* function.
	{
	    char * p = pktbuf+1;
	    pkt_playerclick pkt;

	    pkt.button = *p++;
	    pkt.action = *p++;
	    pkt.h = IntBE16(p); p+=2;
	    pkt.v = IntBE16(p); p+=2;
	    pkt.entity = *p++;
	    pkt.block_x = IntBE16(p); p+=2;	// Only within click range.
	    pkt.block_y = IntBE16(p); p+=2;
	    pkt.block_z = IntBE16(p); p+=2;
	    pkt.face = *p++;			// CC: 255 => missed.

	    // TODO: Something?
	    (void)pkt;
#if 0
	    fprintf(stderr, "%slick of button %d, at (%d,%d), Hit ent (%d), block (%d,%d,%d)[%d]\n",
		pkt.action?"Unc":"C",
		pkt.button,
		pkt.h, pkt.v,
		pkt.entity,
		pkt.block_x, pkt.block_y, pkt.block_z, pkt.face);
#endif
	}
	break;

    case PKID_PINGPONG:
	if (extn_pingpong)
	{
	    char * p = pktbuf+1;
	    uint8_t dir;
	    int data;
	    dir = *p++;
	    data = UIntBE16(p); p+=2;
	    if (dir == 0) {
		send_pingpong_pkt(dir, data);
		flush_to_remote();
	    } else if (data == last_ping_id) {
		// Update stats ...
		last_ping_rcvd = 1;
		struct timeval tv;
		gettimeofday(&tv, 0);
		last_ping_ms =
		    (tv.tv_sec-last_ping_tv.tv_sec)*1000 +
		    (tv.tv_usec-last_ping_tv.tv_usec)/1000;

		if (min_ping_ms == -1 || min_ping_ms > last_ping_ms)
		    min_ping_ms = last_ping_ms;
		if (max_ping_ms == -1 || max_ping_ms < last_ping_ms)
		    max_ping_ms = last_ping_ms;

		if (avg_ping_ms == -1) avg_ping_ms = last_ping_ms;
		else avg_ping_ms = (avg_ping_ms + last_ping_ms)/2;
	    }
	}
	break;

    case PKID_PLUGINMSG:
	{
	    char * p = pktbuf+1;
	    pkt_message pkt;
	    pkt.message_type = *p++;
	    pkt.player_id = 0xFF;
	    sanitise_nbstring(pkt.message, p, 0);
	    fprintf_logfile("Plugin msg ch%d \"%s\"", pkt.message_type, pkt.message);
	}
	break;
    }
}

void
convert_logon_packet(char * pktbuf, pkt_player_id * pkt)
{
    char * p = pktbuf+1;
    pkt->protocol = *p++;
    // These will always be sanitised to ASCII
    sanitise_nbstring(pkt->user_id, p, 1); p+=64;
    sanitise_nbstring(pkt->mppass, p, 1); p+=64;
    int flg = (pkt->protocol == 6 || pkt->protocol == 7) ? *p++ : 0;
    pkt->cpe_flag = pkt->protocol == 7 && flg == 0x42;
}
