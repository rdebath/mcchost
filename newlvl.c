#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#include "newlvl.h"

/*HELP newlvl H_CMD
&T/newlvl name [width height length]
Create a new level, if size is not set it uses the default.
The name "+" is a shorthand for your personal level.
*/

#if INTERFACE
#define CMD_NEWLVL {N"newlvl", &cmd_newlvl}
#endif

void
cmd_newlvl(char * cmd, char * arg)
{
    char * levelname = strtok(arg, " ");
    char * sx = strtok(0, " ");
    char * sy = strtok(0, " ");
    char * sz = strtok(0, " ");

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

    if (sx) {
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

    // ... Spawn? EdgeLevel? Cloudheight? Other?

    fclose(ofd);

    printf_chat("&SLevel '%s' created", levelname);
}
