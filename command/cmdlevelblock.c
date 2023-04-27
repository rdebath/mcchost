
#include "cmdlevelblock.h"

#if INTERFACE
#define UCMD_LEVELBLK \
    {N"levelblock", &cmd_levelblock, CMD_PERM_LEVEL,CMD_HELPARG, .map=1}, \
    {N"lb", &cmd_levelblock, CMD_ALIAS}, \
    {N"levelblock-info", &cmd_levelblock_info, CMD_ALIAS,CMD_HELPARG, .map=1}, \
    {N"lb-info", &cmd_levelblock_info, CMD_ALIAS}, \
    {N"levelblock-list", &cmd_levelblock_list, CMD_ALIAS, .map=1}, \
    {N"lb-list", &cmd_levelblock_list, CMD_ALIAS}
#endif

#define TOKEN_LIST(Mac) \
    Mac(edit) Mac(remove) Mac(copy) Mac(list) Mac(info) Mac(copyall) Mac(add)

#define GEN_TOK_STRING(NAME) #NAME,
#define GEN_TOK_ENUM(NAME) token_ ## NAME,
enum token { TOKEN_LIST(GEN_TOK_ENUM) token_count};
const char* tokennames[] = { TOKEN_LIST(GEN_TOK_STRING) 0};

void
cmd_levelblock(char * UNUSED(cmd), char * arg)
{
    char * sub_cmd = strarg(arg);
    char * blk_str = strarg(0);
    enum token subcmd = -1;

    for(int i = 0; i < token_count; i++) {
	if (strcasecmp(sub_cmd, tokennames[i]) == 0) {
	    subcmd = i;
	    break;
	}
    }

    if (subcmd == -1 || subcmd == token_copyall || subcmd == token_add) {
	if (subcmd == -1)
	    printf_chat("&WUnknown subcommand /lb %s", sub_cmd);
	else
	    printf_chat("&W/lb %s not implemented", sub_cmd);
	return;
    }

    block_t blk = atoi(blk_str?blk_str:"");
    block_t to_blk = blk;
    block_t min_block = 1;
    char * p = 0;
    if (blk_str && (p=strchr(blk_str, '-')) != 0)
	to_blk = atoi(p+1);

    if (subcmd == token_copy) {
	min_block = 0;
	if (blk == to_blk) {
	    char * s = strarg(0);
	    if (s == 0) {
		return; //TODO pick highest undefined block.
	    } else
		to_blk = atoi(s);
	}
    }

    if (blk < min_block || blk >= BLOCKMAX || to_blk <= 0 || to_blk >= BLOCKMAX) {
	printf_chat("&WBlock numbers must be between 1 and %d", BLOCKMAX-1);
	return;
    }

    if (subcmd == token_remove) {
	for(block_t b = blk; b <= to_blk; b++)
	    set_block_opt(b, "defined", "0", 0, 0);
	set_dirty_blockdef();
	return;
    }

    if (subcmd == token_copy) {
	copy_blk_def(blk, to_blk);
	set_dirty_blockdef();
	return;
    }

    if (subcmd == token_edit) {
	char * propname = strarg(0);
	char * propval = 0, *propval2 = 0, *propval3 = 0;

	if (strcasecmp(propname, "min") == 0 ||
	    strcasecmp(propname, "max") == 0 ||
	    strcasecmp(propname, "mincoords") == 0 ||
	    strcasecmp(propname, "maxcoords") == 0)
	{
	    propval = strarg(0);
	    propval2 = strarg(0);
	    propval3 = strarg(0);
	} else
	    propval = strarg_rest();

	set_dirty_blockdef();

	for(block_t b = blk; b <= to_blk; b++) {
	    if (!level_prop->blockdef[b].defined)
		copy_blk_def((block_t)-1, b);
	    set_block_opt(b, propname, propval, propval2, propval3);
	}

	return;
    }

    printf_chat("&W/lb %s not implemented", sub_cmd);
}

void
set_dirty_blockdef()
{
    level_prop->blockdef_generation++;
    level_prop->dirty_save = 1;
    level_prop->metadata_generation++;
    level_prop->last_modified = time(0);
}

void
cmd_levelblock_list(char * UNUSED(cmd), char * arg)
{
    int offset = 0;

    if (strcmp(arg, "all") == 0)
	offset = 0;
    else if (arg && *arg)
	offset = atoi(arg);
    else
	offset = 1;

    int count = 0, total = 0, more = 0;
    int end_offset = offset;
    for(block_t b = 1; b < BLOCKMAX; b++) {
	if (!level_prop->blockdef[b].defined) continue;
	total++;

	if (offset) {
	    count++;
	    if (count < offset) continue;
	    if (count >= offset+8) {more = 1; continue;}
	}
	printf_chat("Custom block &T%d&S has name &T%s",
	    b, level_prop->blockdef[b].name.c);
	end_offset = total;
    }
    if (offset == 0) { offset = 1; end_offset = total;}
    if (more)
	printf_chat("Showing custom blocks %d-%d (out of %d) Next: /lb list %d",
	    offset, end_offset, total, end_offset+1);
    else
	printf_chat("Showing custom blocks %d-%d (out of %d)", offset, end_offset, total);

}

void
cmd_levelblock_info(char * UNUSED(cmd), char * arg)
{
    char * blk_str = strarg(arg);

    block_t blk = atoi(blk_str?blk_str:"");
    block_t to_blk = blk;
    block_t min_block = 1;

    char * p;
    if (blk_str && (p=strchr(blk_str, '-')) != 0)
	to_blk = atoi(p+1);

    if (blk < min_block || blk >= BLOCKMAX || to_blk <= 0 || to_blk >= BLOCKMAX) {
	printf_chat("&WBlock numbers must be between 1 and %d", BLOCKMAX-1);
	return;
    }

    for(block_t b = blk; b <= to_blk; b++) {
	if (!level_prop->blockdef[b].defined) {
	    printf_chat("&WThere is no custom block with the id \"%d\".", b);
	} else {
	    blockdef_t *bp = &level_prop->blockdef[b];
	    printf_chat("About %s (%d)", bp->name.c, b);
	    printf_chat("  Draw type: %d, Blocks light: %s, collide type: %d",
		bp->draw, bp->transmits_light?"False":"True", bp->collide);
	    printf_chat("  Fallback ID: %d, Sound: %d, Speed: %0.2f",
		bp->fallback>=BLOCKMAX?0:bp->fallback, bp->walksound, bp->speed/1024.0);
	    int fogv = (bp->fog[1]<<16)+(bp->fog[2]<<8)+bp->fog[3];
	    if (bp->fog[0] == 0)
		printf_chat("  Block does not use fog");
	    else
		printf_chat("  Fog density: %d, color: %06x", bp->fog[0], fogv);
	    if (fogv && strchr(bp->name.c, '#') != 0)
		printf_chat("  Tint color: %06x", fogv);

	    if (bp->shape == 0) {
		printf_chat("  Block is a sprite, Texture ID %d", bp->textures[2]);
	    } else {
		printf_chat("  Block is a cuboid from (%d, %d, %d) to (%d, %d, %d)",
		    bp->coords[0], bp->coords[1], bp->coords[2],
		    bp->coords[3], bp->coords[4], bp->coords[5]);

		if (bp->textures[0] == bp->textures[1] &&
		    bp->textures[0] == bp->textures[2] &&
		    bp->textures[0] == bp->textures[3] &&
		    bp->textures[0] == bp->textures[4] &&
		    bp->textures[0] == bp->textures[5])
		{
		    printf_chat("  Texture IDs: All %d", bp->textures[0]);
		} else if (bp->textures[1] == bp->textures[2] &&
			   bp->textures[1] == bp->textures[3] &&
		           bp->textures[1] == bp->textures[4])
		{
		    printf_chat("  Texture IDs: Top %d, Sides %d, Bottom %d",
			bp->textures[0], bp->textures[1], bp->textures[5]);
		} else
		    printf_chat("  Texture IDs: Top %d, Left %d, Right %d, Front %d, Back %d, Bottom %d",
			bp->textures[0], bp->textures[1], bp->textures[2],
			bp->textures[3], bp->textures[4], bp->textures[5]);

	    }

	    if (bp->inventory_order >= BLOCKMAX)
		printf_chat("  Order: None");
	    else if (bp->inventory_order == 0)
		printf_chat("  Order: Hidden from inventory");
	    else
		printf_chat("  Order: %d", bp->inventory_order);
	}
    }
}
