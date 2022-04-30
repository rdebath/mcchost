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

static struct ext_list_t extensions[] = {
    { .name= "CustomBlocks", 1, &extn_customblocks },
    { .name= "ClickDistance",1, &extn_clickdistance },
    { .name= "EmoteFix"    , 1 },
    { .name= "FullCP437"   , 1, &extn_fullcp437 },
    { .name= "LongerMessages",1, &extn_longermessages },
    { .name= "InstantMOTD" , 1, &extn_instantmotd },
    { .name= "SetHotbar"   , 1, &extn_sethotbar },
//  { .name= "SetSpawnpoint",1, &extn_setspawnpoint },
    { .name= "EvilBastard" , 1, &extn_evilbastard },
    {0}
};

int extn_customblocks = 0;
int extn_clickdistance = 0;
int extn_instantmotd = 0;
int extn_sethotbar = 0;
int extn_setspawnpoint = 0;
int extn_evilbastard = 0;
int extn_longermessages = 0;
int extn_fullcp437 = 0;

void
send_ext_list()
{
    int i, count = 0;
    for(i = 0; extensions[i].version; i++)
	count++;

    send_extinfo_pkt(server_software, count);

    for(i = 0; extensions[i].version; i++)
	send_extentry_pkt(extensions[i].name, extensions[i].version);

    send_customblocks_pkt();
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

    if (cpe_pending && cpe_extn_remaining <= 0) {
        cpe_pending |= 1;
        if (!extn_customblocks)
	    cpe_pending |= 2;
        if (cpe_pending == 3)
            complete_connection();
    }
}

