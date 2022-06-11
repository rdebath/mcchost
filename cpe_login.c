#include <stdio.h>
#include <string.h>

#include "cpe_login.h"

#if INTERFACE
typedef struct ext_list_t ext_list_t;
struct ext_list_t {
    char * name;
    int version;
    int *enabled_flag;
    int enabled;
};
#endif

#define N .name= /*STFU*/
static struct ext_list_t extensions[] = {
//  { N"EnvMapAppearance",    1  },				//Old
    { N"ClickDistance",       1, &extn_clickdistance },
    { N"CustomBlocks",        1, &extn_customblocks },
    { N"HeldBlock",           1, &extn_heldblock },
//  { N"TextHotKey",          1  },				//ServerConf
//  { N"ExtPlayerList",       2  },
    { N"EnvColors",           1, &extn_envcolours },
//**{ N"SelectionCuboid",     1  },				//MapConf
    { N"BlockPermissions",    1, &extn_block_permission },	//UserMapConf
//  { N"ChangeModel",         1  },				//UserBotConf
//  { N"EnvMapAppearance",    2  },				//Old
    { N"EnvWeatherType",      1, &extn_weathertype },
//  { N"HackControl",         1  },
    { N"EmoteFix",            1  }, // Included in FullCP437
    { N"MessageTypes",        1, &extn_messagetypes },
    { N"LongerMessages",      1, &extn_longermessages },
    { N"FullCP437",           1, &extn_fullcp437 },
    { N"BlockDefinitions",    1, &extn_blockdefn },
    { N"BlockDefinitionsExt", 2, &extn_blockdefnext },
//  { N"TextColors",          1  },				//ServerConf
//  { N"BulkBlockUpdate",     1  },
    { N"EnvMapAspect",        1, &extn_envmapaspect },
//  { N"PlayerClick",         1  },
//  { N"EntityProperty",      1  },
//  { N"ExtEntityPositions",  1  },
//**{ N"TwoWayPing",          1  },
    { N"InventoryOrder",      1, &extn_inventory_order },
    { N"InstantMOTD",         1, &extn_instantmotd },
    { N"FastMap",             1, &extn_fastmap },
    { N"ExtendedTextures",    1, &extn_extendtexno },
    { N"SetHotbar",           1, &extn_sethotbar },
//  { N"SetSpawnpoint",       1, },
//  { N"VelocityControl",     1, },
//  { N"CustomParticles",     1, },				//Conf&Use
//  { N"CustomModels",        1, },				//Server,MapConf
//  { N"PluginMessages",      1, },

    { N"ExtendedBlocks",      1, &extn_extendblockno },

    { N"EvilBastard" ,        1, &extn_evilbastard },
    {0}
};
#undef N

int extn_blockdefn = 0;
int extn_blockdefnext = 0;
int extn_clickdistance = 0;
int extn_customblocks = 0;
int extn_envcolours = 0;
int extn_block_permission = 0;
int extn_envmapaspect = 0;
int extn_evilbastard = 0;
int extn_extendblockno = 0;
int extn_extendtexno = 0;
int extn_fastmap = 0; // CC Crashes with 10bit gzip maps.
int extn_fullcp437 = 0;
int extn_heldblock = 0;
int extn_instantmotd = 0;
int extn_inventory_order = 0;
int extn_longermessages = 0;
int extn_messagetypes = 0;
int extn_sethotbar = 0;
int extn_setspawnpoint = 0;
int extn_weathertype = 0;

int customblock_pkt_sent = 0;
int customblock_enabled = 0;

void
send_ext_list()
{
    int i, count = 0;
    for(i = 0; extensions[i].version; i++)
	count++;

    send_extinfo_pkt(server.software, count);

    for(i = 0; extensions[i].version; i++)
	send_extentry_pkt(extensions[i].name, extensions[i].version);
}

void
process_extentry(pkt_extentry * pkt)
{
    if (cpe_extn_remaining > 0)
        cpe_extn_remaining--;

    for(int i=0; extensions[i].version; i++) {
	if (extensions[i].version != pkt->version) continue;
	if (strcmp(extensions[i].name, pkt->extname) != 0)
	    continue;
	extensions[i].enabled = 1;
	if (extensions[i].enabled_flag)
	    *extensions[i].enabled_flag = 1;
    }

    if (!customblock_pkt_sent && extn_customblocks) {
	send_customblocks_pkt();
	customblock_pkt_sent = 1;
    }

    if (cpe_pending && cpe_extn_remaining <= 0) {
        cpe_pending |= 1;
        if (!extn_customblocks)
	    cpe_pending |= 2;
        if (cpe_pending == 3)
            complete_connection();
    }

    if (extn_blockdefn && extn_blockdefnext) {
	if (client_block_limit < CPELIMITLO)
	    client_block_limit = CPELIMITLO;
	if (extn_extendblockno && client_block_limit < CPELIMIT)
	    client_block_limit = CPELIMIT;
    }

    // Note: most of these lengths are not used because the send_*_pkt
    // functions define the size based on contents.
    if (extn_fastmap)
	msglen[PKID_LVLINIT] = 5;

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
    } else {
	if (extn_extendtexno) {
	    msglen[PKID_BLOCKDEF] = 80 + 3;
	    msglen[PKID_BLOCKDEF2] = 88 + 6;
	}
    }
}

