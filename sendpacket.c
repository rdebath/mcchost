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

static inline void
nb_short_clamp(uint8_t **ptr, int v)
{
    if (v>32767) v = 32767;
    if (v<-32768) v = -32768;
    uint8_t *p = *ptr;
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

static inline int
nb_string_write(uint8_t *pkt, volatile char * str)
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

static inline void
nb_int(uint8_t **ptr, int v)
{
    uint8_t *p = *ptr;
    *p++ = (v>>24);
    *p++ = (v>>16);
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

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
    // extn_fastmap
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_LVLINIT;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_lvldata_pkt(char *block, int len, int percent)
{
    // extn_extendblocks ?
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
    // extn_extendblocks
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
    nb_short_clamp(&p, posn.x);
    nb_short_clamp(&p, posn.y+51);
    nb_short_clamp(&p, posn.z);
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
	nb_short_clamp(&p, posn.x);
	if (player_id == 255)
	    nb_short_clamp(&p, posn.y+29);
	else
	    nb_short_clamp(&p, posn.y+51);
	nb_short_clamp(&p, posn.z);
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

void
send_extinfo_pkt(char * appname, int num_extn)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_EXTINFO;
    p += nb_string_write(p, appname);
    nb_short(&p, num_extn);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_extentry_pkt(char * extension, int version)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_EXTENTRY;
    p += nb_string_write(p, extension);
    nb_int(&p, version);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_clickdistance_pkt(int dist)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_CLICKDIST;
    nb_short(&p, dist);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_customblocks_pkt()
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_CUSTBLOCK;
    *p++ = 1;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_holdthis_pkt(block_t blk, int lock)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_HELDBLOCK;
    *p++ = block_convert(blk);
    *p++ = !!lock;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_envsetcolour_pkt(int id, int rgb)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    int r, g, b;
    if (rgb < 0 || rgb > 0xFFFFFF) r = g = b = -1;
    else {
	r = (rgb>>16) & 0xFF;
	g = (rgb>>8 ) & 0xFF;
	b = (rgb    ) & 0xFF;
    }

    *p++ = PKID_MAPCOLOUR;
    *p++ = id;
    nb_short(&p, r);
    nb_short(&p, g);
    nb_short(&p, b);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_textureurl_pkt(volatile nbtstr_t * textureurl)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_TEXURL;
    p += nb_string_write(p, textureurl->c);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_setmapproperty_pkt(int prop_id, int prop_value)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_MAPPROP;
    *p++ = prop_id;
    nb_int(&p, prop_value);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_weather_pkt(int weather_id)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_WEATHER;
    *p++ = weather_id;
    write_to_remote(packetbuf, p-packetbuf);
}

