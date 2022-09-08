#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "newlvl.h"

/*HELP newlvl H_CMD
&T/newlvl name [width height length] [theme] [seed]
Create a new level, if size is not set it uses the default.
The name "+" is a shorthand for your personal level.

Themes are flat, general, plain, pixel, empty, space, rainbow
Seed is:
    for flat: level of grass
    for general, plain, space, rainbow: Random seed.
*/

#if INTERFACE
#define CMD_NEWLVL {N"newlvl", &cmd_newlvl}

typedef struct lvltheme_t lvltheme_t;
struct lvltheme_t {
    char *name;
    int setrandom;
};
#endif

lvltheme_t themelist[] = {
    {"general", 1},
    {"plain", 1},
    {"flat", 0},	// seed defaults to Y/2
    {"pixel", 0},
    {"empty", 0},
    {"air", 0},
    {"space", 1},
    {"rainbow", 1},
    {0}
};

void
cmd_newlvl(char * cmd, char * arg)
{
    char * levelname = strtok(arg, " ");
    char * sx = strtok(0, " ");
    char * sy = strtok(0, " ");
    char * sz = strtok(0, " ");
    char * th = strtok(0, " ");
    char * se = strtok(0, "");

    if (!levelname || (sx && !sz)) return cmd_help(0, cmd);

    char userlevel[256];

    snprintf(userlevel, sizeof(userlevel), "%s+", user_id);
    if (strcmp(levelname, "+") == 0 || strcmp(levelname, userlevel) == 0) {
	levelname = userlevel;
    } else if (!client_trusted) {
	printf_chat("&WPermission denied, your level name is '%s'", userlevel);
	return;
    }

    char fixedname[MAXLEVELNAMELEN*4], buf2[256], lvlname[MAXLEVELNAMELEN+1];

    fix_fname(fixedname, sizeof(fixedname), levelname);
    unfix_fname(lvlname, sizeof(lvlname), fixedname);
    if (!*lvlname) {
	printf_chat("&WUnusable level name, try another");
	return;
    }
    snprintf(buf2, sizeof(buf2), LEVEL_CW_NAME, fixedname);
    if (access(buf2, F_OK) == 0) {
	printf_chat("&WMap '%s' already exists", levelname);
	return;
    }

    int x=0,y=0,z=0;
    char *e;

    if (sz) {
	x = strtol(sx, &e, 10);
	if (*e || x<1 || x>16384) {
	    printf_chat("&WValue %s is not a valid dimension", sx);
	    return;
	}

	y = strtol(sy, &e, 10);
	if (*e || y<1 || y>16384) {
	    printf_chat("&WValue %s is not a valid dimension", sy);
	    return;
	}

	z = strtol(sz, &e, 10);
	if (*e || z<1 || z>16384) {
	    printf_chat("&WValue %s is not a valid dimension", sz);
	    return;
	}

	if ((int64_t)x*y*z > INT_MAX) {
	    printf_chat("&WMap dimensions are too large to use", sz);
	    return;
	}
    }

    int themeid = -1;
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
    if (themeid < 0) themeid = 0;

    FILE *ifd, *ofd;
    ofd = fopen(buf2, "w");

    ifd = fopen(MODEL_INI_NAME, "r");
    if (ifd) {
	char buf[4096];
	int c;
	while((c=fread(buf, 1, sizeof(buf), ifd)) > 0)
	    fwrite(buf, 1, c, ofd);
	fclose(ifd);
    }

    fprintf(ofd, "[level]\n");

    if (x>0) {
	fprintf(ofd, "Size.X = %d\n", x);
	fprintf(ofd, "Size.Y = %d\n", y);
	fprintf(ofd, "Size.Z = %d\n", z);
    }

    fprintf(ofd, "Theme = %s\n", themelist[themeid].name);

    if (se) {
	int l = strlen(se)*4+4;
	char * buf = malloc(l);
	convert_to_utf8(buf, l, se);
	fprintf(ofd, "Seed = %s\n", buf);
	free(buf);
    } else if (themelist[themeid].setrandom) {
	char sbuf[MB_STRLEN*2+1] = "";
	map_random_t rng[1];
	map_init_rng(rng, sbuf);
	fprintf(ofd, "Seed = %s\n", sbuf);
    }

    fclose(ofd);

    printf_chat("&SLevel '%s' created", levelname);
}
