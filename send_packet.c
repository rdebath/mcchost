#include <math.h>

#include "send_packet.h"

static inline int
fix_player_id(int * pplayer_id)
{
    if ((*pplayer_id < 0 || *pplayer_id > max_proto_player_id) && *pplayer_id != -1) return 0;

    if (*pplayer_id == 255) *pplayer_id = my_user_no;

    return 1;
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
    if (protocol_base_version >= 6)
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
send_lvlinit_pkt(int level_length)
{
    // extn_fastmap
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_LVLINIT;
    if (extn_fastmap)
	nb_int(&p, level_length);
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
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_SRVBLOCK;
    if (!cpe_requested) {
	x += classic_map_offset.x;
	y += classic_map_offset.y;
	z += classic_map_offset.z;
    }
    nb_short(&p, x);
    nb_short(&p, y);
    nb_short(&p, z);
    block = block_convert(block);
    nb_block_t(&p, block);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_spawn_pkt(int player_id, char * playername, xyzhv_t posn)
{
    if (!fix_player_id(&player_id)) return;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_SPAWN;
    *p++ = player_id;
    p += nb_string_write(p, playername);
    int x = posn.x;
    int y = posn.y;
    int z = posn.z;
    if (!cpe_requested) {
	x += classic_map_offset.x*32;
	y += classic_map_offset.y*32;
	z += classic_map_offset.z*32;
    }
    nb_entcoord(&p, x);
    nb_entcoord(&p, y+51);
    nb_entcoord(&p, z);
    *p++ = posn.h;
    *p++ = posn.v;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_posn_pkt(int player_id, xyzhv_t *oldpos, xyzhv_t posn)
{
    if (!fix_player_id(&player_id)) return;

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

    switch(todo) {
    default:
	*p++ = PKID_POSN;
	*p++ = player_id;
	int x = posn.x;
	int y = posn.y;
	int z = posn.z;
	if (!cpe_requested) {
	    x += classic_map_offset.x*32;
	    y += classic_map_offset.y*32;
	    z += classic_map_offset.z*32;
	}
	nb_entcoord(&p, x);
	if (player_id == -1)
	    nb_entcoord(&p, y+29);
	else
	    nb_entcoord(&p, y+51);
	nb_entcoord(&p, z);
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

    write_to_remote(packetbuf, p-packetbuf);
}

void
send_despawn_pkt(int player_id)
{
    if (!fix_player_id(&player_id)) return;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_DESPAWN;
    *p++ = player_id;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_message_pkt(int player_id, int type, char * message)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_MESSAGE;
    if (!extn_messagetypes) {
	if (type != 0 && type != 100) return;
	/* 0..127 prefix line with &f */
	/* 128..255 prefix line with &e */
	if (player_id >= -1 && player_id < 128)
	    *p++ = player_id;
	else
	    *p++ = 127;
    } else
	*p++ = type;
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
    if (!op_enabled) return;
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
    nb_block_t(&p, block_convert(blk));
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
send_textureurl_pkt(nbtstr_t * textureurl)
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
send_mapappear_pkt(nbtstr_t * textureurl, block_t side_blk, block_t edge_blk,
    int side_level, int cells_y, int cloudheight, int maxfog)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_MAPAPPEAR;
    p += nb_string_write(p, textureurl->c);
    nb_block_t(&p, side_blk);
    nb_block_t(&p, edge_blk);
    nb_short_dflt(&p, side_level, cells_y/2);
    if (extn_envmapappearance2) {
	nb_short_dflt(&p, cloudheight, cells_y+2);
	nb_short_dflt(&p, maxfog, 0);
    }
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

void
send_hackcontrol_pkt(int flags, int jump)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_HACKCTL;
    *p++ = !(flags&1); // Flying
    *p++ = !(flags&2); // NoClip
    *p++ = !(flags&4); // Speed
    *p++ = !(flags&8); // Spawn
    *p++ = !(flags&16); // TPV
    if (!(flags&0x20))  // Jump height
	nb_short(&p, -1);
    else
	nb_short(&p, jump);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_removeblockdef_pkt(block_t blkno)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_BLOCKUNDEF;
    nb_block_t(&p, blkno);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_defineblock_pkt(block_t blkno, blockdef_t *def)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    int use_v0 = 0;

    if (def->shape == 0) // Sprites are only available in PKID_BLOCKDEF
	use_v0 = 1;

    // ? if(side_text are same and minXZY == 0 and maxXZ == 16 && maxY in 2..16)

    if (use_v0)
	*p++ = PKID_BLOCKDEF;
    else
	*p++ = PKID_BLOCKDEF2;
    nb_block_t(&p, blkno);
    p += nb_string_write(p, def->name.c);
    *p++ = def->collide;
    {
	// Ugh: speed = 2 ** ((byteval-128)/64)
	uint8_t conv_speed = 128;
	double val = def->speed/1024.0;
	if (val < 0.2) val = 0.2;	// Min is 0.25
	if (val > 4) val = 4;		// Max is 3.95
	val = round(64 * log(val) / log(2) + 128);
	if (val >= 255) conv_speed = 255;
	else if (val <= 0) conv_speed = 0;
	else conv_speed = (uint8_t)val;
	*p++ = conv_speed;
    }

    // Textures Packet order: Top, Left, Right, Front, Back, Bottom
    nb_texid(&p, def->textures[0]);
    if (use_v0) {
	// Use "right" one ... Huh?
	nb_texid(&p, def->textures[2]);
    } else {
	nb_texid(&p, def->textures[1]);
	nb_texid(&p, def->textures[2]);
	nb_texid(&p, def->textures[3]);
	nb_texid(&p, def->textures[4]);
    }
    nb_texid(&p, def->textures[5]);
    *p++ = !!def->transmits_light;	// MCG make bool
    *p++ = def->walksound;
    *p++ = !!def->fullbright;		// MCG make bool
    if (use_v0) {
	*p++ = def->shape;
    } else {
	for(int i=0; i<6; i++)
	    *p++ = def->coords[i];
    }
    *p++ = def->draw;
    for(int i=0; i<4; i++)
	*p++ = def->fog[i];

    write_to_remote(packetbuf, p-packetbuf);
}

void
send_inventory_order_pkt(block_t order, block_t block)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_INVORDER;
    nb_block_t(&p, block);	// CC is reverse of documented order!
    nb_block_t(&p, order);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_sethotbar_pkt(int slot, int block)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_HOTBAR;
    block = block_convert(block);
    nb_block_t(&p, block);
    *p++ = slot;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_blockperm_pkt(block_t block, int placeok, int delok)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_BLOCKPERM;
    nb_block_t(&p, block); // NO Conversion!
    *p++ = placeok;
    *p++ = delok;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_addplayername_pkt(int player_id, char * playername, char * listname, char * groupname, int sortid)
{
    if (!fix_player_id(&player_id)) return;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_PLAYERNAME;
    if (player_id == -1 )
	nb_short(&p, 255);			// CC casts this to a uint8_t
    else if (player_id < 255)
	nb_short(&p, player_id);
    else
	nb_short(&p, player_id+1);
    p += nb_string_write(p, playername);	// Plain alpha only name for auto-complete
    p += nb_string_write(p, listname);		// Nickname including colours and title (append AFK)
    p += nb_string_write(p, groupname);		// Location or other group.
    *p++ = sortid;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_removeplayername_pkt(int player_id)
{
    if (!fix_player_id(&player_id)) return;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_RMPLAYER;
    if (player_id == -1 )
	nb_short(&p, 255);			// CC casts this to a uint8_t
    else if (player_id < 255)
	nb_short(&p, player_id);
    else
	nb_short(&p, player_id+1);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_addentity_pkt(int player_id, char * ingamename, char * skinname, xyzhv_t posn)
{
    if (!extn_extplayerlist)
	return send_spawn_pkt(player_id, ingamename, posn);

    if (!fix_player_id(&player_id)) return;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_ADDENT;
    *p++ = player_id;
    p += nb_string_write(p, ingamename);	// Hover name, including prefix colour
    p += nb_string_write(p, skinname);		// Skin, may be URL
    // if(!extn_extplayerlistv1) {
    nb_entcoord(&p, posn.x);
    if (player_id == -1)
	nb_entcoord(&p, posn.y+29);	// TODO: Is this right?
    else
	nb_entcoord(&p, posn.y+51);	// TODO: Is this right?
    nb_entcoord(&p, posn.z);
    *p++ = posn.h;
    *p++ = posn.v;
    // }
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_changemodel_pkt(int player_id, char * modelname)
{
    if (!extn_changemodel) return;
    if (!fix_player_id(&player_id)) return;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_CHANGEMDL;
    *p++ = player_id;
    p += nb_string_write(p, modelname);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_settextcolour_pkt(uint8_t code, int colour, uint8_t alpha)
{
    if (colour < 0 || colour > 0xFFFFFF) {
	alpha = 0xFF;
	colour = 0xFFFFFF;
    }

    int red = ((colour>>16)&0xFF);
    int green = ((colour>>8)&0xFF);
    int blue = ((colour)&0xFF);

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_TEXTCOLOUR;
    *p++ = red;
    *p++ = green;
    *p++ = blue;
    *p++ = alpha;
    *p++ = code;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_pingpong_pkt(int dir, int data)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_PINGPONG;
    *p++ = dir;
    nb_short(&p, data);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_setspawn_pkt(xyzhv_t posn)
{
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_SETSPAWN;
    nb_entcoord(&p, posn.x);
    nb_entcoord(&p, posn.y+51);
    nb_entcoord(&p, posn.z);
    *p++ = posn.h;
    *p++ = posn.v;
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_selection_cuboid_pkt(int id, struct selection_cuboid_t * cuboid)
{
    if (!cuboid || id < 0 || id > 127) return;
    if (!cuboid->defined || cuboid->colour < 0) {
	send_rm_selection_cuboid_pkt(id);
	return;
    }

    int red = ((cuboid->colour>>16)&0xFF);
    int green = ((cuboid->colour>>8)&0xFF);
    int blue = ((cuboid->colour)&0xFF);
    if (cuboid->colour < 0 || cuboid->colour > 0xFFFFFF)
	red = green = blue = 0;

    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_ADDCUBOID;
    *p++ = id;
    p += nb_string_write(p, cuboid->name.c);
    nb_short(&p, cuboid->start_x);
    nb_short(&p, cuboid->start_y);
    nb_short(&p, cuboid->start_z);
    nb_short(&p, cuboid->end_x);
    nb_short(&p, cuboid->end_y);
    nb_short(&p, cuboid->end_z);
    nb_short(&p, red);
    nb_short(&p, green);
    nb_short(&p, blue);
    nb_short(&p, cuboid->opacity);
    write_to_remote(packetbuf, p-packetbuf);
}

void
send_rm_selection_cuboid_pkt(int id)
{
    if (id < 0 || id > 127) return;
    uint8_t packetbuf[1024];
    uint8_t *p = packetbuf;
    *p++ = PKID_RMCUBOID;
    *p++ = id;
    write_to_remote(packetbuf, p-packetbuf);
}

#if INTERFACE
inline static void
nb_short(uint8_t **ptr, int v)
{
    uint8_t *p = *ptr;
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

inline static void
nb_short_clamp(uint8_t **ptr, int v)
{
    if (v>32767) v = 32767;
    if (v<-32768) v = -32768;
    uint8_t *p = *ptr;
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

inline static void
nb_short_dflt(uint8_t **ptr, int v, int def)
{
    if (v>32767 || v<-32768) v = def;
    uint8_t *p = *ptr;
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

inline static int
nb_string_write(uint8_t *pkt, char * str)
{
    if (!extn_alltext) { // Usually all of the fixes will be ok.
	int l;
	int lns = 0;
	for(l=0; l<MB_STRLEN; l++) {
	    if(str[l] == 0) break;
	    if(!extn_fullcp437 && (str[l]&0x80))
		pkt[l] = cp437_ascii[str[l] & 0x7f];
	    else
		pkt[l] = str[l];
	    if (pkt[l] != ' ') lns = l;
	}
	for(; l<MB_STRLEN; l++) pkt[l] = ' ';

	// CP437->ASCII translation creates spaces.
	while (lns > 0 && pkt[lns-1] == '&') {
	    pkt[lns] = ' '; pkt[lns-1] = ' ';
	    while(lns > 0 && pkt[lns] == ' ') lns--;
	}

	// Wipe extra colours.
	if (!extn_textcolours) {
	    for(l=0; l<MB_STRLEN; l++) {
		if (pkt[l] == '&') {
		    int ch = (uint8_t)pkt[l+1];
		    if (textcolour[ch].defined)
			pkt[l+1] = textcolour[ch].fallback;
		    if ((ch >= '0' && ch <= '9') ||
		        (ch >= 'a' && ch <= 'f') ||
		        (ch >= 'A' && ch <= 'F')) {
			l++;
		    } else {
			pkt[l] = '%';
			pkt[l+1] = ' ';
			l++;
		    }
		}
	    }
	}

	// Workaround problems with Emotes
	if (!extn_emotefix && pkt[lns] < ' ') {
	    if (lns < MB_STRLEN-1)
		pkt[lns+1] = '`';
	    else
		pkt[lns] = '*';
	}
    } else {
	int l;
	for(l=0; l<MB_STRLEN; l++) {
	    if(str[l] == 0) break;
	    pkt[l] = str[l];
	}
	for(; l<MB_STRLEN; l++)
	    pkt[l] = ' ';
    }

    return MB_STRLEN;
}

inline static void
nb_int(uint8_t **ptr, int v)
{
    uint8_t *p = *ptr;
    *p++ = (v>>24);
    *p++ = (v>>16);
    *p++ = (v>>8);
    *p++ = (v&0xFF);
    *ptr = p;
}

inline static void
nb_block_t(uint8_t **ptr, block_t block)
{
    if (!extn_extendblockno)
	*(*ptr)++ = (block < CPELIMITLO) ? block:0;
    else
        nb_short(ptr, block);
}

inline static void
nb_texid(uint8_t **ptr, int texid)
{
    if (!extn_extendtexno)
	*(*ptr)++ = texid;	// MOD256 by dfn
    else
        nb_short(ptr, texid);
}

inline static void
nb_entcoord(uint8_t **ptr, int vec)
{
    if (!extn_extentityposn)
        nb_short_clamp(ptr, vec);
    else
        nb_int(ptr, vec);
}
#endif
