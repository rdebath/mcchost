
#include "cpe_login.h"

#if INTERFACE
typedef struct ext_list_t ext_list_t;
struct ext_list_t {
    char * name;
    int version;
    int *enabled_flag;
    int enabled;
    int disabled;  // Don't advertise this extension.
    int nolate;    // Fatal error to enable this after startup.
};
#endif

static int extn_blockdefinitions = 0;
static int extn_blockdefinitionsext = 0;

#define N .name= /*STFU*/
static struct ext_list_t extensions[] = {
    { N"EnvMapAppearance",    1, &extn_envmapappearance },	//Old
    { N"ClickDistance",       1, &extn_clickdistance },
    { N"CustomBlocks",        1, &extn_customblocks },
    { N"HeldBlock",           1, &extn_heldblock },
    { N"TextHotKey",          1, &extn_texthotkey },		//ServerConf
    { N"ExtPlayerList",       2, &extn_extplayerlist },		//V1 is Broken
    { N"EnvColors",           1, &extn_envcolours },
    { N"SelectionCuboid",     1, &extn_selectioncuboid },	//MapConf
    { N"BlockPermissions",    1, &extn_block_permission },	//UserMapConf
    { N"ChangeModel",         1, &extn_changemodel },		//UserBotConf
    { N"EnvMapAppearance",    2, &extn_envmapappearance2 },	//Old
    { N"EnvWeatherType",      1, &extn_weathertype },
    { N"HackControl",         1, &extn_hackcontrol },
    { N"EmoteFix",            1, &extn_emotefix },
    { N"MessageTypes",        1, &extn_messagetypes },
    { N"LongerMessages",      1, &extn_longermessages },
    { N"FullCP437",           1, &extn_fullcp437 },
    { N"BlockDefinitions",    1, &extn_blockdefinitions },
    { N"BlockDefinitionsExt", 2, &extn_blockdefinitionsext },
    { N"TextColors",          1, &extn_textcolours },		//ServerConf
    { N"BulkBlockUpdate",     1, .disabled=1 },
    { N"EnvMapAspect",        1, &extn_envmapaspect },
    { N"PlayerClick",         1, },
    { N"EntityProperty",      1, .disabled=1 },
    { N"ExtEntityPositions",  1, &extn_extentityposn, .nolate=1 },
    { N"TwoWayPing",          1, &extn_pingpong },
    { N"InventoryOrder",      1, &extn_inventory_order },
    { N"InstantMOTD",         1, &extn_instantmotd },

    { N"ExtendedTextures",    1, &extn_extendtexno, .nolate=1 },
    { N"ExtendedBlocks",      1, &extn_extendblockno, .nolate=1 },
    { N"FastMap",             1, &extn_fastmap, .nolate=1 },

    { N"SetHotbar",           1, &extn_sethotbar },
    { N"SetSpawnpoint",       1, &extn_setspawnpoint },

    { N"VelocityControl",     1, .disabled=1 },
    { N"CustomParticles",     1, .disabled=1 },
    { N"CustomModels",        2, .disabled=1 },
    { N"PluginMessages",      1, .disabled=1 },

    { N"EvilBastard" ,        1, &extn_evilbastard },
    {0}
};
#undef N

char *classicube[] = {
    "ClickDistance", "CustomBlocks", "HeldBlock", "EmoteFix",
    "TextHotKey", "ExtPlayerList", "EnvColors", "SelectionCuboid",
    "BlockPermissions", "ChangeModel", "EnvMapAppearance",
    "EnvWeatherType", "MessageTypes", "HackControl", "PlayerClick",
    "FullCP437", "LongerMessages", "BlockDefinitions",
    "BlockDefinitionsExt", "BulkBlockUpdate", "TextColors",
    "EnvMapAspect", "EntityProperty", "ExtEntityPositions",
    "TwoWayPing", "InventoryOrder", "InstantMOTD", "FastMap", "SetHotbar",
    "SetSpawnpoint", "VelocityControl", "CustomParticles", "CustomModels",
    "PluginMessages",

    0,
};

// Always last two for CC
char *classicube_lasttwo[] = { "ExtendedTextures", "ExtendedBlocks", 0};
int classicube_match_len = 0;
int classicube_lastmatch = 0;

int extn_blockdefn = 0;
int extn_clickdistance = 0;
int extn_customblocks = 0;
int extn_envcolours = 0;

int extn_block_permission = 0;
int extn_changemodel = 0;
int extn_emotefix = 0;
int extn_envmapappearance = 0;
int extn_envmapappearance2 = 0;
int extn_envmapaspect = 0;
int extn_evilbastard = 0;
int extn_extendblockno = 0;
int extn_extendtexno = 0;
int extn_extentityposn = 0;
int extn_extplayerlist = 0;
int extn_fastmap = 0; // CC Crashes with 10bit gzip maps. pre 1.3.3
int extn_fullcp437 = 0;
int extn_hackcontrol = 0;
int extn_heldblock = 0;
int extn_instantmotd = 0;
int extn_inventory_order = 0;
int extn_longermessages = 0;
int extn_messagetypes = 0;
int extn_pingpong = 0;
int extn_selectioncuboid = 0;
int extn_sethotbar = 0;
int extn_setspawnpoint = 0;
int extn_textcolours = 0;
int extn_texthotkey = 0;
int extn_weathertype = 0;
int extn_alltext = 0;

int customblock_pkt_sent = 0;
int customblock_enabled = 0;

block_t client_block_limit = Block_CP;	// Unsafe to send to client.
block_t level_block_limit = 0;		// We need to remap this or higher.

void
send_ext_list()
{
    int i, count = 0;
    for(i = 0; extensions[i].version; i++)
	if (!extensions[i].disabled)
	    count++;

    send_extinfo_pkt(server->software, count);

    for(i = 0; extensions[i].version; i++)
	if (!extensions[i].disabled)
	    send_extentry_pkt(extensions[i].name, extensions[i].version);
}

void
process_extentry(pkt_extentry * pkt)
{
    int enabled_extension = 0;

    if (classicube[classicube_match_len] &&
	strcmp(classicube[classicube_match_len], pkt->extname) == 0)
	classicube_match_len++;
    else if (cpe_extn_remaining > 0 && cpe_extn_remaining <= 2) {
	if (strcmp(classicube_lasttwo[2-cpe_extn_remaining], pkt->extname) == 0)
	    classicube_lastmatch++;
    }

    if (cpe_extn_remaining > 0)
        cpe_extn_remaining--;

    for(int i=0; extensions[i].version; i++) {
	if (extensions[i].version != pkt->version) continue;
	if (strcmp(extensions[i].name, pkt->extname) != 0)
	    continue;
	if (extensions[i].enabled) return; // Don't repeat it.

	if (extensions[i].nolate && !cpe_pending) {
	    char ebuf[256];
	    saprintf(ebuf,
		"Extension \"%s\" cannot be enabled late.",
		extensions[i].name);
	    fatal(ebuf);
	}

	// Note CC sends ALL the extensions it supports even if we
	// haven't advertised them.
	if (extensions[i].disabled) {
	    //printlog("Not enabling extension %s", extensions[i].name);
	    return;
	}
	// printlog("Enable extension %s", extensions[i].name);

	enabled_extension = 1;
	extensions[i].enabled = 1;
	if (extensions[i].enabled_flag)
	    *extensions[i].enabled_flag = 1;
    }

    if (!enabled_extension && (pkt->version || pkt->extname[0]))
	printlog("Unknown extension %s:%d", pkt->extname, pkt->version);

    if (!customblock_pkt_sent && extn_customblocks) {
	send_customblocks_pkt();
	customblock_pkt_sent = 1;
    }

    if (!cpe_pending) {
	if (enabled_extension)
	    printf_chat("&WClient enabled extension %s late -- Okay.", pkt->extname);
	else
	    printf_chat("&WClient sent unknown extension %s late -- Ignored.", pkt->extname);
    }

    if (cpe_extn_remaining == 0) {
	if (extn_extendblockno) {
	    // NOPE!
	    // Don't even try these! They were deprecated before 10bit blocks appeared.
	    extn_envmapappearance = 0;
	    extn_envmapappearance2 = 0;
	}
	// 2 means a longer packet with more fields.
	extn_envmapappearance |= extn_envmapappearance2;

	/*
	   When the user defines the look of a level the block definitions
	   and the textures are closely linked. Only use these if we do it
	   properly. Otherwise fallback to classic (or CPE) which the user
	   may have actually tested. EnvMapAppearance should be sufficient
	   for this too.

	   We should probably turn off the texture _file_ too.

	   NB: Should extn_extendtexno be included somehow too ?
	       I already make definitions fall back if the texture no
	       is too high, but could they even load a 512 entry texmap?
	*/

	if (extn_blockdefinitions && extn_blockdefinitionsext &&
		(extn_envmapaspect || extn_envmapappearance)) {
	    extn_blockdefn = 1;
	    if (client_block_limit < CPELIMITLO)
		client_block_limit = CPELIMITLO;
	    if (extn_extendblockno && client_block_limit < CPELIMIT)
		client_block_limit = CPELIMIT;

	    level_block_limit = client_block_limit;
	}
    }

    // Note: most of these lengths are not used because the send_*_pkt
    // functions define the size based on contents.

    // The important ones are the ones we _receive_ from the client,
    // for which we must know the size of before be actually decode them.

    // Also for some combinations the PKID_POSN packet has different
    // sizes depending on the direction. This is because the "PlayerID"
    // field is replaced by the "Held Block" in client->server and this
    // field grows when 10bit block numbers are enabled.

    if (extn_fastmap)
	msglen[PKID_LVLINIT] = 5;

    if (extn_extentityposn) {
	// Client Behavior: Client must read 32 instead of 16
	// bit integers for the AddEntity, EntityTeleport(8),
	// ExtAddEntity2 packets. It must write 32 instead of 16
	// bit integers for the Position and Orientation packet
	// sent to the server.
	msglen[PKID_SPAWN] = (2+64+6+2) + 6;
	msglen[PKID_POSN] = (2+6+2) + 6;
	msglen[PKID_ADDENT] = 138 + 6;
	// MCGalaxy/Network/Packets/Packet.cs:389
	msglen[PKID_SETSPAWN] = (1+6+2) + 6;
    }

    if (extn_extendblockno) {
	// Set Block (0x05)          Block type  short     Client -> Server
	msglen[PKID_SETBLOCK] = 9+1;
	// Set Block (0x06)          Block Type  short     Server -> Client
	msglen[PKID_SRVBLOCK] = 8+1;
	// Position (0x08)           Player ID   short     Client -> Server
	msglen[PKID_POSN] = 2+6+2+1;
	// HoldThis (0x14)           BlockToHold short     Server -> Client
	msglen[PKID_HELDBLOCK] = 4;
	// SetBlockPermission (0x1C) BlockType   short     Server -> Client
//	msglen[PKID_BLOCKPERM] = 5;
	// DefineBlock (0x23)        BlockID     short     Server -> Client
	msglen[PKID_BLOCKDEF] = 81;
	// UndefineBlock (0x24)      BlockID     short     Server -> Client
	msglen[PKID_BLOCKUNDEF] = 3;
	// DefineBlockExt (0x25)     BlockID     short     Server -> Client
	msglen[PKID_BLOCKDEF2] = 89;
	// BulkBlockUpdate (0x28)    Blocks      byte[320] Server -> Client
//	msglen[PKID_BULKUPDATE] = 1346;
	// SetInventoryOrder (0x2C)  Order       short     Server -> Client
	// SetInventoryOrder (0x2C)  Block ID    short     Server -> Client
	msglen[PKID_INVORDER] = 5;
	// SetHotbar (0x2D)          BlockID     short     Server -> Client
	msglen[PKID_HOTBAR] = 4;

	if (extn_extendtexno) {
	    msglen[PKID_BLOCKDEF] = 81 + 3;
	    msglen[PKID_BLOCKDEF2] = 89 + 6;
	}

	if (extn_extentityposn) {
	    // This is for client->server only NOT server->client
	    msglen[PKID_POSN] = (2+6+2) + 1 + 6;
	}
    } else {
	if (extn_extendtexno) {
	    msglen[PKID_BLOCKDEF] = 80 + 3;
	    msglen[PKID_BLOCKDEF2] = 88 + 6;
	}
    }

    extn_alltext = (extn_fullcp437 && extn_emotefix && extn_textcolours);

    if (cpe_pending && cpe_extn_remaining <= 0) {
        cpe_pending |= 1;
        if (!extn_customblocks)
	    cpe_pending |= 2;
        if (cpe_pending == 3)
            complete_connection();

	char descbuf[4096];
	strcpy(descbuf, user_id);
	if (*client_ipv4_str)
	    sprintf(descbuf+strlen(descbuf), " [%s]", client_ipv4_str);
	strcat(descbuf, " connected");
	if (*client_software.c)
	    sprintf(descbuf+strlen(descbuf),
		" using \"%s\", ", client_software.c);
	else
	    sprintf(descbuf+strlen(descbuf), " using unnamed ");

	if (cpe_extn_advertised > 9 && classicube_match_len+classicube_lastmatch == cpe_extn_advertised)
	    sprintf(descbuf+strlen(descbuf),
		"Classicube%s (%d extns)", websocket?" web":"", cpe_extn_advertised);
	else

	if (classicube_match_len <= 1)
	    sprintf(descbuf+strlen(descbuf), "%sclient with %d extension%s",
		websocket?"web-":"", cpe_extn_advertised, cpe_extn_advertised==1?"":"s");
	else

	    sprintf(descbuf+strlen(descbuf), "CPE %sclient (CC %d of %d)",
		websocket?"web-":"", classicube_match_len, cpe_extn_advertised);

	printlog("%s", descbuf);
    }
}

