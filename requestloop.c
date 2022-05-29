#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "requestloop.h"
#include "inline.h"

int64_t bytes_sent;
static char *ttl_buf = 0;		/* Buffer for line output */
static int ttl_start = 0;
static int ttl_end = 0;
static int ttl_size = 0;

static char line_inp_buf[65536];
static char rcvdpkt[PKBUF];
int in_rcvd = 0;

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
	else if (rv < 0)
	    fatal("read line_fd syscall");
	else if (rv == 0)
	    return -1;
    }

    return 0;
}


void
on_select_timeout()
{
    time_t now;
    int tc = 5;
    if (cpe_pending) tc = 60;
    time(&now);
    int secs = ((now-last_ping) & 0xFF);
    if (secs > tc) {
	// Send keepalive.
	send_ping_pkt();
	time(&last_ping);
    }
    if (cpe_pending) return;

    check_user();
    check_metadata_update();
    send_queued_chats();
    send_queued_blocks();
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
	    process_client_message(cmd, rcvdpkt);
	    in_rcvd = 0;
	}
    }
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
		if (extn_extendblockno) {
		    pkt.held_block = IntBE16(p); p+=2;
		} else
		    pkt.held_block = (uint8_t)(*p++);
	    } else
		pkt.player_id = (uint8_t)(*p++);
	    pkt.pos.x = IntBE16(p); p+=2;
	    pkt.pos.y = IntBE16(p); p+=2;
	    pkt.pos.z = IntBE16(p); p+=2;
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
	    pkt.msg_flag = *p++;
	    if (pkt.msg_flag == 1 && extn_longermessages) {
		char * b = pkt.message;
		memcpy(b, p, MB_STRLEN);
		for(int i = 0; i<MB_STRLEN; i++) if (b[i] == 0) b[i] = ' ';
		b[MB_STRLEN] = 0;
	    } else {
		cpy_nbstring(pkt.message, p); p+=64;
	    }
	    process_chat_message(pkt.msg_flag, pkt.message);
	}
	break;

    case PKID_EXTINFO:
	{
	    char * p = pktbuf+1;
	    int count;
	    cpy_nbstring(client_software.c, p); p+=64;
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
	    cpy_nbstring(pkt.extname, p); p+=64;
	    pkt.version = IntBE32(p); p+=4;
	    process_extentry(&pkt);
	}
	break;
    case PKID_CUSTBLOCK:
	{
	    char * p = pktbuf+1;
	    int version = *p++;
	    if (version > 0 && max_blockno_to_send < 65)
		max_blockno_to_send = 65;

	    cpe_pending |= 2;
	    if (cpe_pending == 3)
		complete_connection();
	}
	break;

    }
}
