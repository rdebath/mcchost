
#include <zlib.h>

#include "send_map_file.h"

#if INTERFACE
#define block_convert(_bl) ((_bl)<level_block_limit?_bl:f_block_convert(_bl))
#endif

// CPE defined translations.
block_t cpe_conversion[] = {
    0x2C, 0x27, 0x0C, 0x00, 0x0A, 0x21, 0x19, 0x03,
    0x1d, 0x1c, 0x14, 0x2a, 0x31, 0x24, 0x05, 0x01
};

// Random translations for protocols 4,5 and 6
block_t proto_conversion[] = {
    // Available for any proto
    // Block_Air, Block_Stone, Block_Grass, Block_Dirt, Block_Cobble,
    // Block_Wood, Block_Sapling, Block_Bedrock, Block_ActiveWater,
    // Block_StillWater, Block_ActiveLava, Block_StillLava, Block_Sand,
    // Block_Gravel, Block_GoldOre, Block_IronOre, Block_CoalOre,
    // Block_Log, Block_Leaves,

    // Proto 4
    Block_GoldOre, Block_Leaves,

    // Proto 5
    Block_Stone, Block_Stone, Block_Stone, Block_Stone, Block_Stone,
    Block_Stone, Block_Stone, Block_Stone, Block_Stone, Block_Stone,
    Block_Stone, Block_Stone, Block_Stone, Block_Stone, Block_Stone,
    Block_Stone, Block_Sapling, Block_Sapling, Block_Sapling,
    Block_Sapling, Block_Sponge,

    // Proto 6
    Block_Stone, Block_Grey, Block_Grey, Block_Red,
    Block_Red, Block_Wood, Block_Cobble, Block_Black
};

static uint32_t metadata_generation = 0;
static uint32_t blockdef_generation = 0;
static uint32_t edgedef_generation = 0;
static block_t max_defined_block = 0;

int reset_hotbar_on_mapload = 0;
int client_inventory_custom = 0;
int classic_limit_blocks = 0;		// Used "reload classic" to see in classic mode.

static uint8_t tex16_def[BLOCKMAX];
xyz_t void_map_size = {1,1,1};
xyz_t classic_map_offset = {0,0,0};

static nbtstr_t last_send_texture;

// Convert from Map block numbers to ones the client will understand.
block_t
f_block_convert(block_t in)
{
    if (in < level_block_limit) return in;
    if (in >= BLOCKMAX) return Block_Bedrock;

    if (level_prop->blockdef[in].defined) {
	int tries = 0;

	do {
	    int fallback = 0;
	    if (classic_limit_blocks && in >= Block_CP) fallback = 1;
	    else if (in >= client_block_limit) fallback = 1;
	    else if (!extn_blockdefn) fallback = 1;
	    else if (!extn_extendtexno && tex16_def[in]) fallback = 2;
	    if (!fallback) break;
	    block_t r = level_prop->blockdef[in].fallback;
	    if (r == in || r >= BLOCKMAX) {
		if (in >= Block_CPE) in = Block_Bedrock;
		break;
	    }
	    in = r;
	    if (in < client_block_limit && fallback<2 && !classic_limit_blocks) break;
	    tries++;
	} while(tries < 4 && level_prop->blockdef[in].defined);
    }

    if (classic_limit_blocks && in >= Block_CP && in < Block_CPE)
	in = cpe_conversion[in-Block_CP];

    if (!customblock_enabled && in >= Block_CP && in < Block_CPE) {
	if (!extn_blockdefn || !level_prop->blockdef[in].defined)
	    in = cpe_conversion[in-Block_CP];
    }

    if (in < client_block_limit) return in;
    if (in >= Block_Sponge && in <= Block_Obsidian)
	in = proto_conversion[in-Block_Sponge];
    if (in < client_block_limit) return in;
    if (in >= Block_Sponge && in <= Block_Obsidian)
	in = proto_conversion[in-Block_Sponge];
    if (in < client_block_limit) return in;
    return Block_Bedrock;
}

void
send_map_file(int reload)
{
    if (extn_instantmotd)
	send_system_ident();

    if (!level_prop || !level_blocks) {

	send_void_map();
	fprintf_logfile("Level \"%s\" no mmap so void map sent.", current_level_name);
	return;
    }

    uintptr_t level_len = (uintptr_t)level_prop->cells_x * level_prop->cells_y * level_prop->cells_z;
    if (level_len > 0x80000000U) level_len = 0x80000000U;

    send_hack_control();

    if (level_prop->texname.c[0]) {
	// If textures are same keep for load screen
	if (strcmp(last_send_texture.c, level_prop->texname.c) != 0)
#ifdef DOUBLE_DOWNLOAD
	    send_textureurl(); // Texture for New map on load screen.
#else
	    reset_textureurl(); // Normal dirt texture on load screen.
#endif
    } else if (level_prop->texname.c[0] != 0)
	reset_textureurl(); // Normal dirt texture on load screen.

    send_lvlinit_pkt(level_len);

    send_block_definitions();
    send_inventory_order();

    set_last_block_queue_id(); // Send updates from now.
    if (ini_settings->no_map_padding || (cpe_requested || extn_fastmap ||
	    ((level_prop->cells_x | level_prop->cells_y | level_prop->cells_z) & 31) == 0))
	send_block_array(level_len);
    else
	send_padded_block_array();

    send_metadata();

    if (!reload && level_prop->announce_motd && level_prop->motd[0]) {
	char *p, announce[MB_STRLEN*2+1];
	strcpy(announce, level_prop->motd);
	// Usual to try to hide -hax headings.
	if ((p = strstr(announce, "&0")) != 0) *p = 0;
	else if ((p = strstr(announce, "-hax")) != 0) *p = 0;
	printf_chat("~(100)%s", announce);
    }

    reset_player_list();
}

void
send_void_map()
{
    if (extn_hackcontrol)
	send_hackcontrol_pkt(0xFF, -1);

    char empty_zlib[] = {0xe3, 0x64, 0x00, 0x00};
    char empty_gzip[] = {
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x63, 0x60, 0x60, 0x60, 0xe2, 0x64,
	0x00, 0x00, 0x84, 0xce, 0x84, 0x63, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char empty_classic[] = {
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xed, 0xd3,
	0xb1, 0x11, 0x00, 0x20, 0x10, 0xc3, 0xb0, 0x74, 0xac, 0xc5, 0xfe, 0x53,
	0x31, 0x01, 0xd4, 0xdc, 0x47, 0x6a, 0x5d, 0x3b, 0xd9, 0x59, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x7c, 0x79, 0xd2, 0x75, 0xfd, 0xe7,
	0xee, 0x7f, 0x5d, 0xef, 0xed, 0xfe, 0xd7, 0xf5, 0xde, 0xee, 0x7f, 0x5d,
	0xef, 0xed, 0xfe, 0xd7, 0xf5, 0xde, 0xee, 0x7f, 0x5d, 0xef, 0xed, 0xfe,
	0xd7, 0xf5, 0xde, 0x0e, 0x70, 0x71, 0x00, 0xe3, 0xce, 0x1f, 0xb6, 0x04,
	0x40, 0x00, 0x00
    };
    if (!cpe_requested) {
	send_lvlinit_pkt(32*16*32);
	// Beware, 0.30 can't do a 1x2x1 map, so give them 32x16x32
	// In fact the real map should always be a multiple of 16x16x16 of
	// that size or larger
	send_lvldata_pkt(empty_classic, 0x57, 0);
	send_lvldone_pkt(32, 16, 32);
	void_map_size = (xyz_t){32,16,32};

	player_posn.x = player_posn.z = 16*32; player_posn.y = 320;
	player_posn.v = player_posn.h = 0; player_posn.valid = 1;
    } else {
	send_lvlinit_pkt(2);
	if (extn_fastmap)
	    send_lvldata_pkt(empty_zlib, 4, 0);
	else
	    send_lvldata_pkt(empty_gzip, 0x1a, 0);
	send_lvldone_pkt(1, 2, 1);
	void_map_size = (xyz_t){1,2,1};
	if (extn_envcolours) {
	    for(int i = 0; i<6; i++)
		send_envsetcolour_pkt(i, 0);
	}
	player_posn.x = player_posn.z = 16; player_posn.y = 256;
	player_posn.v = player_posn.h = 0; player_posn.valid = 1;

	send_block_definitions(); // Clear any previous.
	send_inventory_order();
    }

    reset_player_list();
}

void
check_metadata_update()
{
    if (!level_prop || !level_blocks) return;
    if (metadata_generation == level_prop->metadata_generation &&
        blockdef_generation == level_prop->blockdef_generation) return;

    if(extn_instantmotd)
	send_system_ident();

    if (blockdef_generation != level_prop->blockdef_generation) {
	send_block_definitions(); // Causes a screen redraw
	send_inventory_order();
    }

    send_metadata();
}

void
send_metadata()
{
    if (!level_prop || !level_blocks) return;

    metadata_generation = level_prop->metadata_generation;
    send_hack_control();
    send_env_colours();
    send_map_appearance();
    if (edgedef_generation != level_prop->edgedef_generation)
	reset_edge_property();
    edgedef_generation = level_prop->edgedef_generation;
    send_map_property();
    send_cuboids();
    send_weather();
    send_textureurl();
    send_block_permission();
    // send_players()
    // send_entities()
    // send_spawn()
    send_clickdistance();
}

void
send_block_array(uintptr_t level_len)
{
    int blocks_buffered = 0;
    block_t conv_blk[BLOCKMAX];

    struct timeval sync_time;
    gettimeofday(&sync_time, 0);

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
	if (zrv != Z_OK)
	    fatal_f("ZLib:deflateInit2() failed RV=%d.\n", zrv);

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
			b = block_convert(b);

		    if (b>255) need_himap = 1;
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
	    if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR)
		fatal_f("ZLib:deflate() failed RV=%d.\n", zrv);

	    if (strm.avail_out == 0 || (zrv == Z_STREAM_END && sizeof(zblockbuffer) != strm.avail_out)) {
		send_lvldata_pkt(zblockbuffer, sizeof(zblockbuffer)-strm.avail_out, percent);
		blocks_sent += 1;
		strm.avail_out = sizeof(zblockbuffer);
		strm.next_out = zblockbuffer;
		if (!extn_extendblockno)
		    percent = (int64_t)level_blocks_used * 100 / level_len;

		blocks_buffered ++;
		struct timeval now;
		gettimeofday(&now, 0);
		double ms = (now.tv_sec-sync_time.tv_sec)*1000.0+(now.tv_usec-sync_time.tv_usec)/1000.0;
		if (blocks_buffered > 256 || ms > 1000.0) {
		    sync_time = now;
		    flush_to_remote();
		    blocks_buffered = 0;
		}
	    }

	} while(zrv != Z_STREAM_END);

	deflateEnd(&strm);

	if (!need_himap || !extn_extendblockno) break;
    }

    // Record for next time.
    level_prop->last_map_download_size = blocks_sent * 1028 + 8;

    send_lvldone_pkt(level_prop->cells_x, level_prop->cells_y, level_prop->cells_z);
}

void
send_padded_block_array()
{
    int blocks_buffered = 0;
    int scells_x = level_prop->cells_x;
    int scells_y = level_prop->cells_y;
    int scells_z = level_prop->cells_z;

    if (scells_x&15) scells_x = ((scells_x+15) & -16);
    if (scells_y&15) scells_y = ((scells_y+15) & -16);
    if (scells_z&15) scells_z = ((scells_z+15) & -16);
    if (scells_x < 32) scells_x = 32;
    if (scells_z < 32) scells_z = 32;

    uintptr_t slevel_len = (uintptr_t)scells_x * scells_y * scells_z;
    block_t conv_blk[BLOCKMAX];

    classic_map_offset.x = (scells_x - level_prop->cells_x) / 2;
    classic_map_offset.y = (scells_y - level_prop->cells_y) / 2;
    classic_map_offset.z = (scells_z - level_prop->cells_z) / 2;

    for(block_t b = 0; b<BLOCKMAX; b++)
	conv_blk[b] = block_convert(b);

    uintptr_t blocks_sent = 0;
    unsigned char blockbuffer[65536];

    struct timeval sync_time;
    gettimeofday(&sync_time, 0);

    if (!extn_fastmap) {
	blockbuffer[0] = (slevel_len>>24);
	blockbuffer[1] = (slevel_len>>16);
	blockbuffer[2] = (slevel_len>>8);
	blockbuffer[3] = (slevel_len&0xFF);
    }

    int zrv = 0;
    z_stream strm = {0};
    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;
    zrv = deflateInit2 (&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
			 MAX_WBITS | 16, 8,
			 Z_DEFAULT_STRATEGY);
    if (zrv != Z_OK)
	fatal_f("ZLib:deflateInit2() failed RV=%d.\n", zrv);

    strm.next_in = blockbuffer;
    strm.avail_in = extn_fastmap?0:4;

    uintptr_t level_blocks_used = 0;
    unsigned char zblockbuffer[1024];
    int percent = 0;

    strm.avail_out = sizeof(zblockbuffer);
    strm.next_out = zblockbuffer;

    int x= -classic_map_offset.x;
    int y= -classic_map_offset.y;
    int z= -classic_map_offset.z;
    do {
	if (strm.avail_in == 0 && level_blocks_used < slevel_len) {
	    // copy some bytes from level_blocks
	    strm.next_in = blockbuffer;
	    strm.avail_in = 0;

	    for(int i=0; i<sizeof(blockbuffer) && level_blocks_used < slevel_len; i++)
	    {
		block_t b = Block_Bedrock;

		if (x>=0 && y>=0 && z>=0 &&
		    x<level_prop->cells_x && y<level_prop->cells_y && z<level_prop->cells_z) 
		    b = level_blocks[World_Pack(x,y,z)];
		else {
		    if (y<0)
			b = Block_Bedrock;
		    else if (y >= (level_prop->cells_y+1)/2)
			b = Block_Air;
		    else if (y+2 >= (level_prop->cells_y+1)/2)
			b = Block_ActiveWater;
		}

		if (b<BLOCKMAX)
		    b = conv_blk[b];
		else
		    b = block_convert(b);

		blockbuffer[i] = b;

		level_blocks_used += 1;
		x++;
		if (x == scells_x-classic_map_offset.x) {
		    x= -classic_map_offset.x; z++;
		    if (z == scells_z-classic_map_offset.z)
		    {z=-classic_map_offset.z; y++;}
		}
		strm.avail_in += 1;
	    }
	}

	zrv = deflate(&strm, strm.avail_in != 0?Z_NO_FLUSH:Z_FINISH);
	if (zrv != Z_OK && zrv != Z_STREAM_END && zrv != Z_BUF_ERROR)
	    fatal_f("ZLib:deflate() failed RV=%d.\n", zrv);

	if (strm.avail_out == 0 || (zrv == Z_STREAM_END && sizeof(zblockbuffer) != strm.avail_out)) {
	    send_lvldata_pkt(zblockbuffer, sizeof(zblockbuffer)-strm.avail_out, percent);
	    blocks_sent += 1;
	    strm.avail_out = sizeof(zblockbuffer);
	    strm.next_out = zblockbuffer;

	    percent = (int64_t)level_blocks_used * 100 / slevel_len;

	    blocks_buffered ++;
	    struct timeval now;
	    gettimeofday(&now, 0);
	    double ms = (now.tv_sec-sync_time.tv_sec)*1000.0+(now.tv_usec-sync_time.tv_usec)/1000.0;
	    if (blocks_buffered > 64 || ms > 1000.0) {
		sync_time = now;
		flush_to_remote();
		blocks_buffered = 0;
	    }
	}

    } while(zrv != Z_STREAM_END);

    deflateEnd(&strm);

    // Record for next time.
    level_prop->last_map_download_size = blocks_sent * 1028 + 8;

    send_lvldone_pkt(scells_x, scells_y, scells_z);
}

struct preset { char * name; int fog, sky, clouds, sun, shadow; }
    presets[] = {
        { "Normal",   0xFFFFFF, 0x99CCFF, 0xFFFFFF, 0xFFFFFF, 0x9B9B9B },
        { "Cartoon",  0x00FFFF, 0x1E90FF, 0x00BFFF, 0xF5DEB3, 0xF4A460 },
        { "Noir",     0x000000, 0x1F1F1F, 0x000000, 0x696969, 0x1F1F1F },
        { "Watery",   0x5F9EA0, 0x008080, 0x008B8B, 0xE0FFFF, 0x008B8B },
        { "Misty",    0xBBBBBB, 0x657694, 0x9494A5, 0x696984, 0x4E4E69 },
        { "Gloomy",   0x6A80A5, 0x405875, 0x405875, 0x444466, 0x3B3B59 },
        { "Cloudy",   0xAFAFAF, 0x8E8E8E, 0x8E8E8E, 0x9B9B9B, 0x8C8C8C },
        { "Sunset",   0xFFA322, 0x836668, 0x9A6551, 0x7F6C60, 0x46444C },
        { "Dusk",     0xD36538, 0x836668, 0x836668, 0x525163, 0x30304B },
        { "Midnight", 0x131947, 0x070A23, 0x1E223A, 0x181828, 0x0F0F19 },
        {0}
    };

void
send_env_colours()
{
    if (!extn_envcolours) return;

    if (!classic_limit_blocks) {
	send_envsetcolour_pkt(0, level_prop->sky_colour);
	send_envsetcolour_pkt(1, level_prop->cloud_colour);
	send_envsetcolour_pkt(2, level_prop->fog_colour);
	send_envsetcolour_pkt(3, level_prop->ambient_colour);
	send_envsetcolour_pkt(4, level_prop->sunlight_colour);
	send_envsetcolour_pkt(5, level_prop->skybox_colour);
    } else {
	send_envsetcolour_pkt(0, presets[0].sky);
	send_envsetcolour_pkt(1, presets[0].clouds);
	send_envsetcolour_pkt(2, presets[0].fog);
	send_envsetcolour_pkt(3, presets[0].shadow);
	send_envsetcolour_pkt(4, presets[0].sun);
	send_envsetcolour_pkt(5, 0xFFFFFF);
    }
}

void
send_system_ident()
{
    if (level_prop && level_prop->motd[0]) {
	// Nasty hack for longer MOTD string.
	if (strlen(level_prop->motd) > MB_STRLEN)
	    send_server_id_pkt(level_prop->motd, level_prop->motd+MB_STRLEN, server->op_flag);
	else
	    send_server_id_pkt(server->name, level_prop->motd, server->op_flag);
    } else
	send_server_id_pkt(server->name, server->motd, server->op_flag);
}

void
send_textureurl()
{
    if (!extn_envmapaspect) return;
    // We define the textures based on block definitions.
    // If client doesn't have blockdefs they can't properly use our textures.
    // TODO: If no blockdefs skip this check ?
    if (!extn_blockdefn) return;

    send_textureurl_pkt(&level_prop->texname);
    last_send_texture = level_prop->texname;
}

void
reset_textureurl()
{
    if (!extn_envmapaspect || !extn_blockdefn) return;

    nbtstr_t nil = {0};
    send_textureurl_pkt(&nil);
}

void
reset_edge_property()
{
    if (!extn_envmapaspect) return;
    send_setmapproperty_pkt(0, 0);
    send_setmapproperty_pkt(1, 0);
}

void
send_map_property()
{
    if (!extn_envmapaspect) return;

    if (!classic_limit_blocks) {
	int side_level = level_prop->side_level;
	if (side_level == INT_MIN) side_level = level_prop->cells_y/2;
	int clouds_height = level_prop->clouds_height;
	if (clouds_height == INT_MIN) clouds_height = level_prop->cells_y+2;
	int side_offset = level_prop->side_offset;
	if (side_offset == INT_MIN) side_offset = -2;

	send_setmapproperty_pkt(0, level_prop->side_block);
	send_setmapproperty_pkt(1, level_prop->edge_block);
	send_setmapproperty_pkt(2, side_level);
	send_setmapproperty_pkt(3, clouds_height);
	send_setmapproperty_pkt(4, level_prop->max_fog);
	send_setmapproperty_pkt(5, level_prop->clouds_speed);
	send_setmapproperty_pkt(6, level_prop->weather_speed);
	send_setmapproperty_pkt(7, level_prop->weather_fade);
	send_setmapproperty_pkt(8, level_prop->exp_fog);
	send_setmapproperty_pkt(9, side_offset);
	send_setmapproperty_pkt(10, level_prop->skybox_hor_speed);
	send_setmapproperty_pkt(11, level_prop->skybox_ver_speed);
    } else {
	send_setmapproperty_pkt(0, Block_Bedrock);
	send_setmapproperty_pkt(1, Block_ActiveWater);
	send_setmapproperty_pkt(2, level_prop->cells_y/2);
	send_setmapproperty_pkt(3, level_prop->cells_y+2);
	send_setmapproperty_pkt(4, 0);
	send_setmapproperty_pkt(5, 256);
	send_setmapproperty_pkt(6, 256);
	send_setmapproperty_pkt(7, 128);
	send_setmapproperty_pkt(8, 0);
	send_setmapproperty_pkt(9, -2);
	send_setmapproperty_pkt(10, 0);
	send_setmapproperty_pkt(11, 0);
    }
}

void
send_map_appearance()
{
    if (extn_envmapaspect) return; // Not needed then.
    if (extn_extendblockno) return; // NOPE!!
    if (!extn_envmapappearance) return;

    send_mapappear_pkt(&level_prop->texname,
	level_prop->side_block,
	level_prop->edge_block,
	level_prop->side_level,
	level_prop->cells_y,
	level_prop->clouds_height,
	level_prop->max_fog);
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

    int click_distance = my_user.click_distance;

    if (click_distance <= 0)
	click_distance =
	    level_prop->click_distance >= 0
	    ? level_prop->click_distance
	    : 160;

    send_clickdistance_pkt(click_distance);
}

void
send_block_definitions()
{
    if (level_prop)
	blockdef_generation = level_prop->blockdef_generation;

    if (!extn_blockdefn || !customblock_enabled) {
	level_block_limit = client_block_limit;
	if (client_block_limit > Block_CP && !customblock_enabled)
	    level_block_limit = Block_CP;
	for(block_t b = 0; b<level_block_limit; b++)
	    if (level_prop && level_prop->blockdef[b].defined) {
		level_block_limit = b;
		break;
	    }

	if (!extn_blockdefn)
	    return;
    }
    if (!extn_extendtexno) {
	for(block_t b = 0; b<client_block_limit; b++) {
	    tex16_def[b] = 0;
	    if (!level_prop || !level_prop->blockdef[b].defined) continue;
	    for(int t = 0; t<6; t++)
		if (level_prop->blockdef[b].textures[t] > 255)
		    tex16_def[b] = 1;
	    if (tex16_def[b] && b<level_block_limit)
		level_block_limit = b;
	}
    }

    block_t newmax = 0;
    for(block_t b = 0; b<client_block_limit; b++)
    {
	if (!level_prop && b == Block_Bedrock) {
	    newmax = b+1;
	    blockdef_t def = default_blocks[Block_Bedrock];
	    def.collide = 0;
	    send_defineblock_pkt(b, &def);
	} else if (!level_prop && b == Block_StillWater) {
	    newmax = b+1;
	    blockdef_t def = default_blocks[Block_StillWater];
	    def.fog[0] = 0; def.fog[1] = 0; def.fog[2] = 0; def.fog[3] = 0;
	    send_defineblock_pkt(b, &def);
	} else if (!level_prop || !level_prop->blockdef[b].defined) {
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

    if (player_on_new_level && level_prop && level_prop->reset_hotbar)
	reset_hotbar_on_mapload = 1;

    if (!level_prop || (level_prop->readonly && level_prop->disallowchange)) {
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

    if (reset_hotbar_on_mapload)
	reset_hotbar();

    int inv_block[BLOCKMAX] = {0};
    block_t b;
    int nondefault = 0;

    for(b=1; b < client_block_limit && b<BLOCKMAX; b++) {
	int inv = b;
	if (!level_prop->blockdef[b].defined) {
	    if (b >= Block_CPE) continue;
	    if (!customblock_enabled && b >= Block_CP)
		inv = 0;
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
reset_hotbar()
{
    reset_hotbar_on_mapload = 0;
    if (!extn_sethotbar) return;

    static block_t tbl[] = {
	Block_Stone, Block_Cobble, Block_Brick, Block_Dirt, Block_Wood,
	Block_Log, Block_Leaves, Block_Glass, Block_Slab, 0
    };
    for(int id = 0; id<9; id++)
	send_sethotbar_pkt(id, tbl[id]);
}

void
send_block_permission()
{
    if (!level_prop) return;

    if (!extn_block_permission) {
	send_op_pkt(
	    level_prop->disallowchange == 0 &&
	    level_prop->blockdef[Block_Bedrock].block_perm == 0 &&
	    level_prop->blockdef[Block_ActiveWater].block_perm == 0 &&
	    level_prop->blockdef[Block_ActiveLava].block_perm == 0);
	return;
    }

    int pok = 1, dok = 1;
    block_t b;

    if (player_mark_mode)
	;
    else if (level_prop->disallowchange)
	pok = dok = 0;

    // send_blockperm_pkt(0, 1, 1); // Block 0 action is undefined.

    for(b=1; b< client_block_limit; b++) {
	int pl_ok = pok;
	if (!player_mark_mode)
	    pl_ok &= !(level_prop->blockdef[b].block_perm&1);

	int dl_ok = dok;
	if (!player_mark_mode)
	    dl_ok &= !(level_prop->blockdef[b].block_perm&2);

	send_blockperm_pkt(b, pl_ok, dl_ok);
    }
}

void
send_hack_control()
{
    if (!extn_hackcontrol) return;
    send_hackcontrol_pkt(level_prop->hacks_flags, level_prop->hacks_jump);
}

void
send_cuboids()
{
    if (!extn_selectioncuboid || !level_prop) return;

    for(int i = 0; i<MAX_CUBES; i++)
    {
	if (!level_prop->cuboid[i].defined) continue;
	send_selection_cuboid_pkt(i, level_prop->cuboid+i);
    }
}
