#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "sendpacket.h"

static inline void
nb_short(uint8_t **ptr, int v)
{
    uint8_t *p = *ptr;
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

#if 0
static void write_to_remote(int fd, uint8_t * buf, int buflen)
{
    int rv = write(fd, buf, buflen);
    if (rv < 0)
	perror("Write error");
    if (rv == 0)
	fatal("End of file on packet pipe");
    if (rv != buflen)
	fatal("Packet write failed full write");
}
#endif

void
send_server_id_pkt(char * servername, char * servermotd, int user_type)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_IDENT;
    *p++ = 7; // Protocol version
    p += nb_string_write(p, servername);
    p += nb_string_write(p, servermotd);
    *p++ = user_type?100:0; // Is operator?
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_ping_pkt()
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_PING;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_lvlinit_pkt()
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_LVLINIT;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_lvldata_pkt(char *block, int len, int percent)
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
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_lvldone_pkt(int x, int y, int z)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_LVLDONE;
    nb_short(&p, x);
    nb_short(&p, y);
    nb_short(&p, z);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_setblock_pkt(int x, int y, int z, int block)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_SRVBLOCK;
    nb_short(&p, x);
    nb_short(&p, y);
    nb_short(&p, z);
    *p++ = block_convert(block);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_spawn_pkt(int player_id, char * playername, xyzhv_t posn)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_SPAWN;
    *p++ = player_id;
    p += nb_string_write(p, playername);
    nb_short(&p, posn.x);
    nb_short(&p, posn.y+29);
    nb_short(&p, posn.z);
    *p++ = posn.h;
    *p++ = posn.v;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_posn_pkt(int player_id, xyzhv_t *oldpos, xyzhv_t posn)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    int todo = 0;
    if (!oldpos || !oldpos->valid) {
	todo = 0;
    } else if (oldpos->x == posn.x && oldpos->y == posn.y && oldpos->z == posn.z) {
	if (oldpos->h == posn.h && oldpos->v == posn.v)
	    return;
	todo = 3;
    } else {
	// Is Delta (x,y,z) useful?
	int diff_ok = 1;
	if (oldpos->x - posn.x > 127 || oldpos->x - posn.x < -127) diff_ok = 0;
	if (oldpos->y - posn.y > 127 || oldpos->y - posn.y < -127) diff_ok = 0;
	if (oldpos->z - posn.z > 127 || oldpos->z - posn.z < -127) diff_ok = 0;
	if (diff_ok) {
	    if (oldpos->h == posn.h && oldpos->v == posn.v)
		todo = 2;
	    else
		todo = 1;
	}
    }

// #define PKID_POSN       0x08 /* 0x08 */ 2+6+2, 	Pabs+O
// #define PKID_POSN1      0x09 /* 0x09 */ 7,		PRel+O
// #define PKID_POSN2      0x0A /* 0x0a */ 5,		PRel
// #define PKID_POSN3      0x0B /* 0x0b */ 4,		Or

    switch(todo) {
    default:
	*p++ = PKID_POSN;
	*p++ = player_id;
	nb_short(&p, posn.x);
	nb_short(&p, posn.y+29);
	nb_short(&p, posn.z);
	*p++ = posn.h;
	*p++ = posn.v;
	break;
    case 1:
	*p++ = PKID_POSN1;
	*p++ = player_id;
	*p++ = posn.x - oldpos->x;
	*p++ = posn.y - oldpos->y;
	*p++ = posn.z - oldpos->z;
	*p++ = posn.h;
	*p++ = posn.v;
	break;
    case 2:
	*p++ = PKID_POSN2;
	*p++ = player_id;
	*p++ = posn.x - oldpos->x;
	*p++ = posn.y - oldpos->y;
	*p++ = posn.z - oldpos->z;
	break;
    case 3:
	*p++ = PKID_POSN3;
	*p++ = player_id;
	*p++ = posn.h;
	*p++ = posn.v;
	break;
    }

    if (oldpos) {*oldpos = posn; oldpos->valid = 1; }

    write_to_remote(packetbuf, p-packetbuf);
}

void
send_despawn_pkt(int player_id)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_DESPAWN;
    *p++ = player_id;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_message_pkt(int id, char * message)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_MESSAGE;
    /* 0..127 prefix line with &f */
    /* 128..255 prefix line with &e */
    *p++ = id;
    p += nb_string_write(p, message);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_discon_msg_pkt(char * message)
{
    // If the message begins with "Kicked " or "Banned " CC will not reconnect.
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_DISCON;
    p += nb_string_write(p, message);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_op_pkt(int opflg)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_OPER;
    *p++ = opflg?100:0;
    write_to_remote(packetbuf, p-packetbuf);
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
