#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "requestloop.h"

int64_t bytes_sent;
static char *ttl_buf = 0;		/* Buffer for line output */
static int ttl_start = 0;
static int ttl_end = 0;
static int ttl_size = 0;

static char rcvdpkt[PKBUF];
int in_rcvd = 0;

int pending_marks = 0;

void
run_request_loop()
{
    int flg;
    while(!(flg = do_select()));
}

int
bytes_queued_to_send()
{
    return ttl_start != ttl_end;
}

void write_to_remote(char * str, int len)
{
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
	    else ttl_start = ttl_end = 0;
	} else if (rv < 0)
	    fatal("write line_fd syscall");
    }

    if( FD_ISSET(line_ifd, &rfds) )
    {
	char txbuf[65536];
	rv = read(line_ifd, txbuf, sizeof(txbuf));
	if( rv > 0 )
	    remote_received(txbuf, rv);
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
    send_queued_chats();
    send_queued_blocks();
    pending_marks = 0;
}

void
remote_received(char *str, int len)
{
    int i, cmd, clen;

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
	if (msglen[rcvdpkt[0] & 0xFF] <= in_rcvd) {
	    process_client_message(rcvdpkt[0] & 0xFF, rcvdpkt);
	    in_rcvd = 0;
	}
    }
}

void
process_client_message(int cmd, char * pktbuf)
{
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
	    pkt.heldblock = *p++;

	    pkt.block = pkt.mode?pkt.heldblock:Block_Air;

	    update_block(pkt);
	    if (++pending_marks > 999)
		send_queued_blocks();
	}
	break;
    case PKID_MESSAGE:
	{
	    char * p = pktbuf+1;
	    pkt_message pkt;
	    pkt.msg_flag = *p++;
	    cpy_nbstring(pkt.message, p); p+=64;
	    update_chat(pkt);
	}
	break;
    }
}
