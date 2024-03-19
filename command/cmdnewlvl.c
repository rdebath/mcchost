
#include "cmdnewlvl.h"

/*HELP newlvl H_CMD
&T/newlvl name [width height length] [theme] [seed]
Create a new level, if size is not set it uses the default.
The name "+" is a shorthand for your personal level which will be named after your user name with an "+" appended.

Themes are flat, general, plain, pixel, empty, space, rainbow
Seed is:
    for flat: level of grass
    for general, plain, space, rainbow: Random seed.
*/

#if INTERFACE
#define UCMD_NEWLVL {N"newlvl", &cmd_newlvl, CMD_HELPARG}

typedef struct lvltheme_t lvltheme_t;
struct lvltheme_t {
    char *name;
    int setrandom;
};
#endif

#define DEFAULT_THEME -1

lvltheme_t themelist[] = {
    {"flat", 0},	// seed defaults to Y/2
    {"general", 1},
    {"plain", 1},
    {"plasma", 1},
    {"pixel", 0},
    {"empty", 0},
    {"space", 0},
    {"rainbow", 0},
    {"bw", 0},
    {"air", 0},
    {0}
};

void
cmd_newlvl(char * UNUSED(cmd), char * arg)
{
    char * lvlarg = strarg(arg);
    char * sx = strarg(0);
    char * sy = strarg(0);
    char * sz = strarg(0);
    char * th = strarg(0);
    char * se = strarg_rest();

    if (!lvlarg || (sx && !sz)) {
	printf_chat("&WNeed more arguments to specify level size");
	return;
    }

    char levelname[256];
    saprintf(levelname, "%s", lvlarg);
    if (!perm_level_check(levelname, 1, 0))
	return;

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), levelname);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    if (!*lvlname) {
	printf_chat("&WUnusable level name, try another");
	return;
    }
    saprintf(buf2, LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) == 0) {
	printf_chat("&WMap '%s' already exists", levelname);
	return;
    }

    int x=0,y=0,z=0;

    if (sz) {
	char *e;
	x = strtoi(sx, &e, 10);
	if (*e || x<1 || x>MAPDIMMAX) {
	    printf_chat("&WValue %s is not a valid dimension", sx);
	    return;
	}

	y = strtoi(sy, &e, 10);
	if (*e || y<1 || y>MAPDIMMAX) {
	    printf_chat("&WValue %s is not a valid dimension", sy);
	    return;
	}

	z = strtoi(sz, &e, 10);
	if (*e || z<1 || z>MAPDIMMAX) {
	    printf_chat("&WValue %s is not a valid dimension", sz);
	    return;
	}

	if ((int64_t)x*y*z > INT_MAX) {
	    printf_chat("&WMap dimensions are too large to use, %jd cells",
		(intmax_t)x*y*z);
	    return;
	}
    }

    char * theme = 0;
    char * seed = 0;
    if (th) {
	int themeid = DEFAULT_THEME;
	for(int i=0; themelist[i].name; i++) {
	    if (strcasecmp(th, themelist[i].name) == 0) {
		themeid = i;
		break;
	    }
	}

	if (themeid < 0) {
	    printf_chat("&STheme '%s' was not found", th);
	    return;
	}
	theme = themelist[themeid].name;

	if (se) {
	    int l = strlen(se)*4+4;
	    seed = malloc(l);
	    convert_to_utf8(seed, l, se);
	} else if (themelist[themeid].setrandom) {
	    seed = malloc(MB_STRLEN*2+1);
	    *seed = 0;
	    populate_map_seed(seed, 0);
	}
    }

    int rv = create_level(buf2, theme, seed, x, y, z);
    if (seed) free(seed);
    if (!rv) return;

    printf_chat("&SLevel '%s' created", levelname);

    direct_teleport(levelname, 0, 0);
}

LOCAL int
create_level(char * filename, char * theme, char * seed, int x, int y, int z)
{
    char *levelsect = "level";
    char value[256];

    ini_file_t ini[1] = {0};
    (void) load_ini_txt_file(ini, MODEL_INI_NAME, 1);

    if (x>0) {
	saprintf(value, "%d", x);
	add_ini_txt_line(ini, levelsect, "Size.X", value);
	saprintf(value, "%d", y);
	add_ini_txt_line(ini, levelsect, "Size.Y", value);
	saprintf(value, "%d", z);
	add_ini_txt_line(ini, levelsect, "Size.Z", value);
    }

    if (theme) {
	add_ini_txt_line(ini, levelsect, "Theme", theme);
	if (seed)
	    add_ini_txt_line(ini, levelsect, "Seed", seed);
    }

    check_mkdir(filename);
    int rv = (save_ini_txt_file(ini, filename) >= 0);
    clear_ini_txt(ini);
    return rv;
}
