#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "sendpacket.h"

static inline void
nb_short(uint8_t **ptr, int v)
{
    uint8_t *p = *ptr;
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

void
send_server_id_pkt(int ofd, char * servername, char * servermotd)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_IDENT;
    *p++ = 7; // Protocol version
    p += nb_string_write(p, servername);
    p += nb_string_write(p, servermotd);
    *p++ = 0; // Normal user.
    write(ofd, packetbuf, p-packetbuf);
}

void
send_ping_pkt(int ofd)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_PING;
    write(ofd, packetbuf, p-packetbuf);
}

void
send_lvlinit_pkt(int ofd)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_LVLINIT;
    write(ofd, packetbuf, p-packetbuf);
}

void
send_lvldata_pkt(int ofd, char *block, int len, int percent)
{
    uint8_t packetbuf[2048];
    uint8_t *p = packetbuf;
    int l = len>1024?1024:len;
    *p++ = PKID_LVLDATA;
    nb_short(&p, l);
    memcpy(p, block, l);
    if (l<1024) memset(p+l, 0, 1024-l);
    p += 1024;
    *p++ = percent;
    write(ofd, packetbuf, p-packetbuf);
}

void
send_lvldone_pkt(int ofd, int x, int y, int z)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_LVLDONE;
    nb_short(&p, x);
    nb_short(&p, y);
    nb_short(&p, z);
    write(ofd, packetbuf, p-packetbuf);
}

void
send_spawn_pkt(int ofd, int player_id, char * playername, xyzhv_t posn)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_SPAWN;
    *p++ = player_id;
    p += nb_string_write(p, playername);
    nb_short(&p, posn.x);
    nb_short(&p, posn.y);
    nb_short(&p, posn.z);
    *p++ = posn.h;
    *p++ = posn.v;
    write(ofd, packetbuf, p-packetbuf);
}

void
send_posn_pkt(int ofd, xyzhv_t *oldpos, xyzhv_t posn)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    int todo = -1;
    if (!oldpos || !oldpos->valid) {
	todo = 0;
    } else if (oldpos->x == posn.x && oldpos->y == posn.y && oldpos->y == posn.z) {
	if (oldpos->h == posn.h && oldpos->v == posn.v)
	    return;
	todo = 3;
    }
    // else is Delta (x,y,z) useful?

    if (oldpos) {*oldpos = posn; oldpos->valid = 1; }

    switch(todo) {
    default:
	*p++ = PKID_POSN0;
	nb_short(&p, posn.x);
	nb_short(&p, posn.y);
	nb_short(&p, posn.z);
	*p++ = posn.h;
	*p++ = posn.v;
	break;
    case 3:
	*p++ = PKID_POSN3;
	*p++ = posn.h;
	*p++ = posn.v;
	break;
    }

    write(ofd, packetbuf, p-packetbuf);
}

void
send_despawn_pkt(int ofd, int player_id)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_DESPAWN;
    *p++ = player_id;
    write(ofd, packetbuf, p-packetbuf);
}

void
send_message_pkt(int ofd, int player_id, char * message)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_MESSAGE;
    *p++ = player_id;
    p += nb_string_write(p, message);
    write(ofd, packetbuf, p-packetbuf);
}

void
send_discon_msg_pkt(int ofd, char * message)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_DISCON;
    p += nb_string_write(p, message);
    write(ofd, packetbuf, p-packetbuf);
}

void
send_op_pkt(int ofd, int opflg)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_OPER;
    *p++ = opflg?100:0;
    write(ofd, packetbuf, p-packetbuf);
}

LOCAL int
nb_string_write(uint8_t *pkt, char * str)
{
    int l;
    for(l=0; l<MB_STRLEN; l++) {
	if(str[l] == 0) break;
	pkt[l] = str[l];
    }
    for(; l<MB_STRLEN; l++)
	pkt[l] = ' ';

    return MB_STRLEN;
}

#if INTERFACE
#define PKID_IDENT      0x00
#define PKID_PING       0x01
#define PKID_LVLINIT    0x02
#define PKID_LVLDATA    0x03
#define PKID_LVLDONE    0x04
#define PKID_SETBLOCK   0x05
#define PKID_SRVBLOCK   0x06
#define PKID_SPAWN      0x07
#define PKID_POSN0      0x08
#define PKID_POSN1      0x09
#define PKID_POSN2      0x0A
#define PKID_POSN3      0x0B
#define PKID_DESPAWN    0x0C
#define PKID_MESSAGE    0x0D
#define PKID_DISCON     0x0E
#define PKID_OPER       0x0F
#endif
