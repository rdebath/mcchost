
#include "cmddefblk.h"

/*HELP defblk H_CMD
&T/defblk [id] [CopyFrom] "Name" flag1=a,flag2=b
&Tid&S is the block number to define
&TCopyFrom&S is the block to use as the pattern.
If &TCopyFrom&S is &T-&S the block is not copied or is copied from &TAir&S
The &T"Name"&S must be enclosed in quotes or skipped.
See: &T/help defblk flags&S for list of flags.
*/

/*HELP defblk_flags
&Tshape&S Set to true to make a "sprite"
&Tcollide&S Set the collision type &T/help defblk collide
&Tspeed&S Relative speed (0.05..3.95) that player moves over block.
&Talltex&S, &Tsidetex&S, &Ttoptex&S, &Tbottomtex&S, &Tlefttex&S, &Trighttex&S, &Tfronttex&S, &Tbacktex&S Set the textures for a block.
&Tblockslight&S True if casts shadow
&Tsound&S Set the sounds played for this block &T/defblk defblk sound
&Tfullbright&S Block emits light (brighter)
Continued &T/help defblk flags2
*/

/*HELP defblk_flags2
&Tdraw&S Set how the block is drawn for transparency. &T/help defblk draw
&Tfogcolour&S Colour for fog or adjustment colour for blocks with a '#' in the name. The colour is in hex (#RRGGBB)
&Tfogdensity&S Alpha component of fog colour
&Tmin&S, &Tmax&S Minimum and maximum X,Y,Z ordinates for block edges, normally 0..16 but can be negative or larger.
Continued &T/help defblk flags3
*/

/*HELP defblk_flags3
&Tfallback&S Block number to be used if client can't display this one
&Torder&S Position of block in inventory (0 = not present)
&Tstackblock&S Double slab block like this slab (0 - none)
&Tgrass&S Grass block for this block
&Tdirt&S Dirt block for this block
*/

/*HELP defblk_collide
Use a number between '0' and '7' for collision type.
0 - block is walk-through (e.g. air).
1 - block is swim-through/climbable (e.g. rope). (No selection)
2 - block is solid (e.g. dirt).
3 - block is solid, but slippery like ice
4 - block is solid, but even slipperier than ice
5 - block is swim-through like water (No selection)
6 - block is swim-through like lava (No selection, moved down)
7 - block is climbable like rope

If Block 7 is made nonsolid (0,1,5..7) you fall off the edge
*/

/*HELP defblk_sound,defblk_sounds
Sounds
0 = None, 1 = Wood, 2 = Gravel, 3 = Grass, 4 = Stone
5 = Metal, 6 = Glass, 7 = Cloth, 8 = Sand, 9 = Snow
*/

/*HELP defblk_draw
Draw methods
0 = Opaque, Texture 100% Opaque or it X-ray's
1 = Transparent (Like glass), Only edges of a cuboid drawn
2 = Transparent (Like leaves), Internal faces drawn
3 = Translucent (Like ice), PNG alpha channel used for level.
4 = Gas (Like air), Texture ignored. (No selection)
5 = Renders as X sprite
6 = Sprite large size horizontal offset
7 = Sprite large size horizontal and vertical offset
*/

#if INTERFACE
#define CMD_DEFBLK  {N"defblk", &cmd_defblk, CMD_PERM_LEVEL,CMD_HELPARG}
#endif

void
cmd_defblk(char * UNUSED(cmd), char * arg)
{
    if (!perm_level_check(0, 0))
	return;

    char * blk_str = strtok(arg, " ");
    char * cf_str = strtok(0, " ");
    char * val = strtok(0, "");

    block_t blk = Block_Air, cf_blk = Block_Air;

    blk = atoi(blk_str?blk_str:"");
    if (blk <= 0 || blk >= BLOCKMAX) {
	printf_chat("&WBlock number '%s' must be an integer int 1..max", blk_str);
	return;
    }
    if (cf_str) {
	if (strcmp(cf_str, "-") == 0)
	    cf_blk = (block_t)-1;
	else
	    cf_blk = block_id(cf_str);
    }
    if (cf_blk != (block_t)-1 && cf_blk >= BLOCKMAX) {
        printf_chat("&WBlock number '%s' is not a valid source.", blk_str);
        return;
    }
    while (val && *val == ' ') val++;

    if (!level_prop->blockdef[blk].defined && cf_blk == (block_t)-1) {
	if (blk < 66) cf_blk = blk;
	else cf_blk = Block_Air;
    }

    if (cf_blk < BLOCKMAX) {
	if (cf_blk < Block_CPE) {
	    // Always use unmodified when copy from std.
	    level_prop->blockdef[blk] = default_blocks[cf_blk];
	} else if (cf_blk < BLOCKMAX) {
	    if (!level_prop->blockdef[cf_blk].defined) {
		printf_chat("&WBlock '%s' is not defined", blk_str);
		return;
	    }
	    level_prop->blockdef[blk] = level_prop->blockdef[cf_blk];
	}
	level_prop->blockdef[blk].defined = 1;
	level_prop->blockdef[blk].fallback = cf_blk;
	level_prop->blockdef[blk].inventory_order = blk;
    }
    level_prop->blockdef_generation++;
    level_prop->dirty_save = 1;
    level_prop->metadata_generation++;
    level_prop->last_modified = time(0);

    if (!val) return;
    if (*val == '"') {
	char * cname = strtok(val+1, "\"");
	val = strtok(0, "");
	strncpy(level_prop->blockdef[blk].name.c, cname, MB_STRLEN);
	level_prop->blockdef[blk].name.c[MB_STRLEN] = 0;
    }
    if (!val) return;
    while (*val == ' ') val++;
    if (!val || *val == 0) return;

    char * opt = strtok(val, ",");
    do
    {
	char * v = strchr(opt, '=');
	if (!v) v = "";
	else { *v = 0; v++; }
	set_block_opt(blk, opt, v);
    }
    while ((opt = strtok(0, ",")) != 0);

    return;
}

LOCAL void
set_block_opt(block_t bno, char * varname, char * value)
{
    if (!varname || !*varname) return;

    char vbuf[256];
    saprintf(vbuf, "block.%d", bno);
    ini_state_t stv = {.no_unsafe=1}, *st = &stv;
    st->curr_section = vbuf;
    if (!value) value = "";

    if (extra_block_prop(st, varname, value))
	return;
    if (!level_ini_fields(st, varname, &value))
	printf_chat("&WOption %s not found", varname);
}

int
extra_block_prop(ini_state_t *st, char * varname, char * value)
{
    char nbuf[256], tbuf[256];
    for(int op = 0; extra_props[op].alias; op++) {
	if (strcasecmp(varname, extra_props[op].alias) != 0) continue;

	switch(extra_props[op].variant) {
	case ep_std:
	    return level_ini_fields(st, extra_props[op].name, &value);
	case ep_alltex:
	    level_ini_fields(st, "texture.bottom", &value);
	    level_ini_fields(st, "texture.top", &value);
	/*FALLTHROUGH*/
	case ep_sidetex:
	    level_ini_fields(st, "texture.left", &value);
	    level_ini_fields(st, "texture.right", &value);
	    level_ini_fields(st, "texture.front", &value);
	    level_ini_fields(st, "texture.back", &value);
	    return 1;
	case ep_xyz:
	    {
		char * y = strtok(0, ","); // EVIL!
		char * z = strtok(0, ",");
		saprintf(nbuf, "%s.X", extra_props[op].name);
		level_ini_fields(st, nbuf, &value);
		saprintf(nbuf, "%s.Y", extra_props[op].name);
		if (y)
		    level_ini_fields(st, nbuf, &y);
		saprintf(nbuf, "%s.Z", extra_props[op].name);
		if (z)
		    level_ini_fields(st, nbuf, &z);
		return 1;
	    }
	case ep_toggle:
	    {
		int v = !ini_read_bool(value);
		saprintf(tbuf, "%d", v);
		value = tbuf;
		return level_ini_fields(st, extra_props[op].name, &value);
	    }
	case ep_colour:
	    {
		int colour = strtol(value, 0, 0);
		if (*value == '#')
		    colour = strtol(value+1, 0, 16);
		value = tbuf;
		saprintf(nbuf, "%s.R", extra_props[op].name);
		saprintf(tbuf, "%d", (colour>>16)&0xFF);
		level_ini_fields(st, nbuf, &value);
		saprintf(nbuf, "%s.G", extra_props[op].name);
		saprintf(tbuf, "%d", (colour>>8)&0xFF);
		level_ini_fields(st, nbuf, &value);
		saprintf(nbuf, "%s.B", extra_props[op].name);
		saprintf(tbuf, "%d", (colour)&0xFF);
		level_ini_fields(st, nbuf, &value);
		return 1;
	    }
	}
	break;
    }
    return 0;
}

#if INTERFACE
enum extra_props_e { ep_std=0, ep_alltex, ep_sidetex, ep_xyz, ep_toggle, ep_colour };
typedef struct extra_props_t extra_props_t;
struct extra_props_t {
    char * alias;
    char * name;
    enum extra_props_e variant;
};
#endif

extra_props_t extra_props[] = {
    { "toptex", "texture.top", 0 },
    { "bottomtex", "texture.bottom", 0 },
    { "lefttex", "texture.left", 0 },
    { "righttex", "texture.right", 0 },
    { "fronttex", "texture.front", 0 },
    { "backtex", "texture.back", 0 },
    { "top", "texture.top", 0 },
    { "bottom", "texture.bottom", 0 },
    { "left", "texture.left", 0 },
    { "right", "texture.right", 0 },
    { "front", "texture.front", 0 },
    { "back", "texture.back", 0 },
    { "bright", "fullbright", 0 },
    { "sound", "walksound", 0 },
    { "blockdraw", "draw", 0 },
    { "fallbackid", "fallback", 0 },
    { "fallbackblock", "fallback", 0 },
    { "stackid", "stackblock", 0 },
    { "sound", "walksound", 0 },
    { "grass", "grassblock", 0 },
    { "dirt", "dirtblock", 0 },
    { "density", "fog.den", 0 },
    { "fogdensity", "fog.den", 0 },

    { "all", "texture", ep_alltex },
    { "alltex", "texture", ep_alltex },
    { "side", "texture", ep_sidetex },
    { "sidetex", "texture", ep_sidetex },
    { "sides", "texture", ep_sidetex },
    { "sidestex", "texture", ep_sidetex },

    { "mincoords", "min", ep_xyz },
    { "maxcoords", "max", ep_xyz },
    { "min", "min", ep_xyz },
    { "max", "max", ep_xyz },

    { "light", "TransmitsLight", ep_toggle },
    { "blockslight", "TransmitsLight", ep_toggle },

    { "col", "fog", ep_colour },
    { "fogcol", "fog", ep_colour },
    { "fogcolor", "fog", ep_colour },
    { "fogcolour", "fog", ep_colour },

    {0,0,0}
};
