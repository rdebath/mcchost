#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#if _POSIX_VERSION >= 200112L
#include <sys/select.h>
#endif

#include "requestloop.h"

int64_t bytes_sent;
static char *ttl_buf = 0;		/* Buffer for line output */
static int ttl_start = 0;
static int ttl_end = 0;
static int ttl_size = 0;

static char line_inp_buf[65536];
static char rcvdpkt[PKBUF];
int in_rcvd = 0;
static int ticks_with_pending_bytes = 0;

time_t last_ping = 0;

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

void write_to_remote(char * str, int len)
{
    time(&last_ping);

    bytes_sent += len;

    for(int i = 0; i < len; i++) {
	if( ttl_end >= ttl_size ) {
	    int nsz = ttl_size ? ttl_size*2 : 2048;
	    char * nb = realloc(ttl_buf, nsz);
	    if (nb == 0) fatal("Out of memory");
	    ttl_size = nsz;
	    ttl_buf = nb;
	}
	ttl_buf[ttl_end++] = str[i];
    }
}

void
flush_to_remote()
{
    int tosend = ttl_end-ttl_start;
    int rv = write(line_ofd, ttl_buf+ttl_start, tosend);

    if( rv > 0 )
    {
	if( rv != ttl_end-ttl_start ) ttl_start += rv;
	else ttl_start = ttl_end = 0;
    } else if (rv < 0)
	fatal("write line_fd syscall");
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

    tv.tv_sec = 0; tv.tv_usec = 10000;
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

#if 0
    if( FD_ISSET(tty_ifd, &rfds) ) {
	{
	    char txbuf[2048];
	    rv = read(tty_ifd, txbuf, sizeof(txbuf));
	    if ( rv > 0 )
		received_from_tty(txbuf, rv);
	    else if (rv < 0)
		fatal("read tty_ifd syscall");
	    else if (rv == 0)
		return -1;
	}
    }
#endif

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
	if( rv > 0 )
	    remote_received(line_inp_buf, rv);
	else if (rv < 0) {
	    perror("Error reading line_fd");
	    return -1;
	} else if (rv == 0)
	    return -1;
    }

    return 0;
}


void
on_select_timeout()
{
    time_t now;
    int tc = cpe_pending?60:5; // Not while CPE pending

    // If a packet has not been received in full expect the rest soon.
    if (in_rcvd>0 && ++ticks_with_pending_bytes > 1500) // Should be 15 seconds
	fatal("Broken packet received -- protocol failure");

    time(&now);
    int secs = ((now-last_ping) & 0xFF);
    if (secs > tc) {
	if (cpe_pending)
	    fatal("CPE Protocol negotiation failure");

	// Send keepalive.
	send_ping_pkt();
	time(&last_ping);
    }
    if (cpe_pending) return;

    check_user();
    check_metadata_update();
    send_queued_chats(0);
    send_queued_blocks();
    if (my_user.dirty)
	write_current_user(0);
}

void
remote_received(char *str, int len)
{
    int i, cmd, clen;
    time(&last_ping);

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
		fatal("Unknown packet type received");
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
sanitise_nbstring(char *buf, char *str)
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
	    else if (buf[i] & 0x80)
		buf[i] = cp437_ascii[buf[i] & 0x7F];
    }
    char * p = buf+MB_STRLEN;
    while (p>buf && (p[-1] == ' ' || p[-1] == 0)) *(--p) = 0;
}

void
process_client_message(int cmd, char * pktbuf)
{
    // During the initial startup ignore (almost) everything.
    if (cpe_pending) {
	if (cmd != PKID_EXTINFO && cmd != PKID_EXTENTRY && cmd != PKID_CUSTBLOCK)
	    return;
    }
    if (!cpe_enabled && cmd > PKID_OPER)
	logout("Unexpected packet type received"); //return;

    switch(cmd)
    {
    case PKID_SETBLOCK:
	{
	    char * p = pktbuf+1;
	    pkt_setblock pkt;
	    pkt.coord.x = IntBE16(p); p+=2;
	    pkt.coord.y = IntBE16(p); p+=2;
	    pkt.coord.z = IntBE16(p); p+=2;
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
	    }
	    pkt.pos.h = *p++;
	    pkt.pos.v = *p++;
	    pkt.pos.valid = 1;

	    // They lie! Position /reload is -22, position of feet is -51
	    pkt.pos.y -= 51;

	    update_player_pos(pkt);
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
		sanitise_nbstring(pkt.message, p);
	    // p+=64;
	    process_chat_message(pkt.message_type, pkt.message);
	    update_player_move_time();
	}
	break;

    case PKID_EXTINFO:
	{
	    char * p = pktbuf+1;
	    int count;
	    // Perhaps we want to sanitise this string after all the entries
	    // have been received ... possibly including FullCP437.
	    sanitise_nbstring(client_software.c, p); p+=64;
	    count = IntBE16(p); p+=2;
	    if (cpe_pending)
		cpe_extn_remaining = count;
	    else
		cpe_extn_remaining = 0;
	}
	break;
    case PKID_EXTENTRY:
	{
	    char * p = pktbuf+1;
	    pkt_extentry pkt;
	    sanitise_nbstring(pkt.extname, p); p+=64;
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
	    if (cpe_pending == 3)
		complete_connection();
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
    sanitise_nbstring(pkt->user_id, p); p+=64;
    sanitise_nbstring(pkt->mppass, p); p+=64;
    int flg = (pkt->protocol == 6 || pkt->protocol == 7) ? *p++ : 0;
    pkt->cpe_flag = pkt->protocol == 7 && flg == 0x42;
}
