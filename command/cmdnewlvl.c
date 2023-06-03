
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
    char *e;

    if (sz) {
	x = strtol(sx, &e, 10);
	if (*e || x<1 || x>MAPDIMMAX) {
	    printf_chat("&WValue %s is not a valid dimension", sx);
	    return;
	}

	y = strtol(sy, &e, 10);
	if (*e || y<1 || y>MAPDIMMAX) {
	    printf_chat("&WValue %s is not a valid dimension", sy);
	    return;
	}

	z = strtol(sz, &e, 10);
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

    int themeid = DEFAULT_THEME;
    if (th) {
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
    }

    FILE *ifd, *ofd;
    ofd = fopen(buf2, "w");

    // INI file to alter default setup of levels.
    // Define blocks, etc.
    ifd = fopen(MODEL_INI_NAME, "r");
    if (ifd) {
	char buf[4096];
	int c;
	while((c=fread(buf, 1, sizeof(buf), ifd)) > 0)
	    fwrite(buf, 1, c, ofd);
	fclose(ifd);
    }

    fprintf(ofd, "\n[level]\n");

    if (x>0) {
	fprintf(ofd, "Size.X = %d\n", x);
	fprintf(ofd, "Size.Y = %d\n", y);
	fprintf(ofd, "Size.Z = %d\n", z);
    }

    if (themeid >= 0) {
	fprintf(ofd, "Theme = %s\n", themelist[themeid].name);

	if (se) {
	    int l = strlen(se)*4+4;
	    char * buf = malloc(l);
	    convert_to_utf8(buf, l, se);
	    fprintf(ofd, "Seed = %s\n", buf);
	    free(buf);
	} else if (themelist[themeid].setrandom) {
	    char sbuf[MB_STRLEN*2+1] = "";
	    populate_map_seed(sbuf, 0);
	    fprintf(ofd, "Seed = %s\n", sbuf);
	}
    }

    fclose(ofd);

    printf_chat("&SLevel '%s' created", levelname);

    direct_teleport(levelname, 0, 0);
}
