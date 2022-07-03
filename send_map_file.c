#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <zlib.h>

#include "send_map_file.h"

#if INTERFACE
#define block_convert(_bl) ((_bl)<client_block_limit?_bl:f_block_convert(_bl))
#endif

// CPE defined translations.
block_t cpe_conversion[] = {
    0x2C, 0x27, 0x0C, 0x00, 0x0A, 0x21, 0x19, 0x03,
    0x1d, 0x1c, 0x14, 0x2a, 0x31, 0x24, 0x05, 0x01
};

static uint32_t metadata_generation = 0;
static block_t max_defined_block = 0;

int reset_hotbar_on_mapload = 0;
int client_blockperm_state = 3;
int client_inventory_custom = 0;

// Convert from Map block numbers to ones the client will understand.
block_t
f_block_convert(block_t in)
{
    if (in < client_block_limit) {
#if 0
	// NB: If CustomBlock isn't enabled but Blockdef is should the CPE
	// fallbacks be enabled? I think Blockdef should be considered an
	// alias of CustomBlock V2, at least here.

	if (!customblock_enabled && in >= Block_CP && in < Block_CPE) {
	    if (!level_prop->blockdef[in].defined)
		in = cpe_conversion[in-Block_CP];
	}
#endif
	return in;
    }
    if (in >= BLOCKMAX) in = BLOCKMAX-1;

    if (level_prop->blockdef[in].defined
	&& level_prop->blockdef[in].fallback < Block_CPE)
	in = level_prop->blockdef[in].fallback;

    if (in >= client_block_limit && in >= Block_CP && in < Block_CPE)
	in = cpe_conversion[in-Block_CP];

    return in >= client_block_limit ? Block_Bedrock : in;
}

void
send_map_file()
{
    if (!level_prop || !level_blocks) {
#if 1
	char empty_zlib[] = {0xe3, 0x64, 0x00, 0x00};
	char empty_gzip[] = {
	    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x03, 0x63, 0x60, 0x60, 0x60, 0xe2, 0x64,
	    0x00, 0x00, 0x84, 0xce, 0x84, 0x63, 0x06, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	send_lvlinit_pkt(2);
	if (extn_fastmap)
	    send_lvldata_pkt(empty_zlib, 4, 0);
	else
	    send_lvldata_pkt(empty_gzip, 0x1a, 0);
	send_lvldone_pkt(1, 2, 1);
	if (extn_envcolours) {
	    for(int i = 0; i<6; i++)
		send_envsetcolour_pkt(i, 0);
	}
	player_posn.x = player_posn.z = 16; player_posn.y = 256;
	player_posn.v = player_posn.h = 0;
	send_spawn_pkt(255, user_id, player_posn);
#endif
	printf_chat("&WCannot send level data, file not mapped");
	return;
    }

    uintptr_t level_len = (uintptr_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;

    // if(extn_instantmotd) ...

    // Send_system_ident()
    // Send_hack_control()

    send_lvlinit_pkt(level_len);

    send_block_definitions();
    send_inventory_order();

    set_last_block_queue_id(); // Send updates from now.
    send_block_array();

    send_lvldone_pkt(level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);

    // Send_system_ident() // Level motd
    // Send_hack_control()

    send_metadata();
    reset_player_list();
}

void
check_metadata_update()
{
    if (!level_prop || !level_blocks) return;
    if (metadata_generation == level_prop->metadata_generation) return;

    send_metadata();
}

void
send_metadata()
{
    if (!level_prop || !level_blocks) return;

    metadata_generation = level_prop->metadata_generation;
    send_env_colours();
    send_map_property();
    send_weather();
    send_textureurl();
    send_block_permission();
    // send_players()
    // send_entities()
    // send_spawn()
    send_clickdistance();
}

void
send_block_array()
{
    int blocks_buffered = 0;
    uintptr_t level_len = (uintptr_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
    block_t conv_blk[BLOCKMAX];

    for(block_t b = 0; b<BLOCKMAX; b++)
	conv_blk[b] = block_convert(b);

    uintptr_t blocks_sent = 0;
    int need_himap = 0;

    for(int part = 0; part<2; part++)
    {
	unsigned char blockbuffer[65536];

	if (!extn_fastmap) {
	    blockbuffer[0] = (level_len>>24);
	    blockbuffer[1] = (level_len>>16);
	    blockbuffer[2] = (level_len>>8);
	    blockbuffer[3] = (level_len&0xFF);
	}

	int zrv = 0;
	z_stream strm = {0};
	strm.zalloc = Z_NULL;
	strm.zfree  = Z_NULL;
	strm.opaque = Z_NULL;
	if (extn_fastmap)
	    zrv = deflateInit2 (&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				 -MAX_WBITS, 8,
				 Z_DEFAULT_STRATEGY);
	else
	    zrv = deflateInit2 (&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
				 MAX_WBITS | 16, 8,
				 Z_DEFAULT_STRATEGY);
	if (zrv != Z_OK) {
	    char buf[256];
	    sprintf(buf, "ZLib:deflateInit2() failed RV=%d.\n", zrv);
	    fatal(buf);
	}

	strm.next_in = blockbuffer;
	strm.avail_in = extn_fastmap?0:4;

	uintptr_t level_blocks_used = 0;
	unsigned char zblockbuffer[1024];
	int percent = part;

	strm.avail_out = sizeof(zblockbuffer);
	strm.next_out = zblockbuffer;

	do {
	    if (strm.avail_in == 0 && level_blocks_used < level_len) {
		// copy some bytes from level_blocks
		strm.next_in = blockbuffer;
		strm.avail_in = 0;

		for(int i=0; i<sizeof(blockbuffer) && level_blocks_used < level_len; i++)
		{
		    block_t b = level_blocks[level_blocks_used];
		    if (b<BLOCKMAX)
			b = conv_blk[b];
		    else
			b = block_convert(level_blocks[level_blocks_used]);

		    if (b>255 && extn_extendblockno) need_himap = 1;
		    if (part == 0)
			blockbuffer[i] = b;
		    else if (b<768)
			blockbuffer[i] = (b>>8);
		    else
			blockbuffer[i] = 0;

		    level_blocks_used += 1;
		    strm.avail_in += 1;
		}
	    }

	    zrv = deflate(&strm, strm.avail_in != 0?Z_NO_FLUSH:Z_FINISH);
	    if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR) {
		char buf[256];
		sprintf(buf, "ZLib:deflate() failed RV=%d.\n", zrv);
		fatal(buf);
	    }

	    if (strm.avail_out == 0 || (zrv == Z_STREAM_END && sizeof(zblockbuffer) != strm.avail_out)) {
		send_lvldata_pkt(zblockbuffer, sizeof(zblockbuffer)-strm.avail_out, percent);
		blocks_sent += 1;
		strm.avail_out = sizeof(zblockbuffer);
		strm.next_out = zblockbuffer;
		if (!extn_extendblockno)
		    percent = (int64_t)level_blocks_used * 100 / level_len;

		blocks_buffered ++;
		if (blocks_buffered > 64)
		    flush_to_remote();
	    }

	} while(zrv != Z_STREAM_END);

	deflateEnd(&strm);

	if (!need_himap || !extn_extendblockno) break;
    }

    // Record for next time.
    level_prop->last_map_download_size = blocks_sent * 1028 + 8;
}

void
send_env_colours()
{
    if (!extn_envcolours) return;

    send_envsetcolour_pkt(0, level_prop->sky_colour);
    send_envsetcolour_pkt(1, level_prop->cloud_colour);
    send_envsetcolour_pkt(2, level_prop->fog_colour);
    send_envsetcolour_pkt(3, level_prop->ambient_colour);
    send_envsetcolour_pkt(4, level_prop->sunlight_colour);
    send_envsetcolour_pkt(5, level_prop->skybox_colour);
}

void
send_textureurl()
{
    if (!extn_envmapaspect) return;

    send_textureurl_pkt(&level_prop->texname);
}

void
send_map_property()
{
    if (!extn_envmapaspect) return;

    send_setmapproperty_pkt(0, level_prop->side_block);
    send_setmapproperty_pkt(1, level_prop->edge_block);
    send_setmapproperty_pkt(2, level_prop->side_level);
    send_setmapproperty_pkt(3, level_prop->clouds_height);
    send_setmapproperty_pkt(4, level_prop->max_fog);
    send_setmapproperty_pkt(5, level_prop->clouds_speed);
    send_setmapproperty_pkt(6, level_prop->weather_speed);
    send_setmapproperty_pkt(7, level_prop->weather_fade);
    send_setmapproperty_pkt(8, level_prop->exp_fog);
    send_setmapproperty_pkt(9, level_prop->side_offset);
    send_setmapproperty_pkt(10, level_prop->skybox_hor_speed);
    send_setmapproperty_pkt(11, level_prop->skybox_ver_speed);
}

void
send_weather()
{
    if (!extn_weathertype) return;

    send_weather_pkt(level_prop->weather);
}

void
send_clickdistance()
{
    if (!extn_clickdistance) return;

    send_clickdistance_pkt(
	    level_prop->click_distance > 0
	    ? level_prop->click_distance
	    : 160);
}

void
send_block_definitions()
{
    if (!extn_blockdefn || !extn_blockdefnext) return;

    block_t newmax = 0;
    for(block_t b = 0; b<client_block_limit; b++)
    {
	if (!level_prop->blockdef[b].defined) {
	    if (b<max_defined_block)
		send_removeblockdef_pkt(b);
	} else {
	    newmax = b+1;
	    blockdef_t def = level_prop->blockdef[b];
	    send_defineblock_pkt(b, &def);
	}
    }
    max_defined_block = newmax;
}

void
send_inventory_order()
{
    if (!extn_inventory_order) return;

    if (level_prop->readonly && level_prop->disallowchange) {
	for(int inv = 0; inv < client_block_limit; inv++)
	    send_inventory_order_pkt(inv, 0);
	client_inventory_custom = 1;
	if (extn_sethotbar) {
	    reset_hotbar_on_mapload = 1;
	    for(int id = 0; id<9; id++)
		send_sethotbar_pkt(id, Block_Air);
	}
	return;
    }

    if (reset_hotbar_on_mapload) {
	reset_hotbar_on_mapload = 0;
	if (extn_sethotbar) {
	    static block_t tbl[] = {
		Block_Stone, Block_Cobble, Block_Brick, Block_Dirt, Block_Wood,
		Block_Log, Block_Leaves, Block_Glass, Block_Slab, 0
	    };
	    for(int id = 0; id<9; id++)
		send_sethotbar_pkt(id, tbl[id]);
	}
    }

    int inv_block[BLOCKMAX] = {0};
    block_t b;
    int nondefault = 0;

    for(b=1; b < client_block_limit && b<BLOCKMAX; b++) {
	int inv = b;
	if (!level_prop->blockdef[b].defined) {
	    if (b >= Block_CPE) continue;
	} else
	    inv = level_prop->blockdef[b].inventory_order;

	if (inv == (block_t)-1) inv = b;
	if (inv <= 0 || inv >= client_block_limit) {
	    send_inventory_order_pkt(0, b);
	    nondefault = 1;
	    continue;
	}
	inv_block[inv] = b;
	if (inv != b) nondefault = 1;
    }

    if (!nondefault && !client_inventory_custom) return;

    send_inventory_order_pkt(0, 0); // Hmmm.

    for(int inv = 0; inv < client_block_limit; inv++)
	if (inv_block[inv] != 0)
	    send_inventory_order_pkt(inv, inv_block[inv]);

    client_inventory_custom = nondefault;
}

void
send_block_permission()
{
    if (!extn_block_permission) return;
    int pok = 1, dok = 1;
    block_t b;

    if (level_prop->disallowchange)
	pok = dok = 0;

    if (client_blockperm_state == (pok<<1)+dok) return;

    // send_blockperm_pkt(0, 1, 1); // Block 0 action is undefined.

    for(b=1; b< client_block_limit; b++)
	send_blockperm_pkt(b, pok, dok);

    client_blockperm_state = (pok<<1)+dok;
}
