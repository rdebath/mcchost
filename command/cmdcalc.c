
#include "cmdcalc.h"

/*HELP calc H_CMD
&T/calc [Expression]
Calculate the expression and display the result.
*/

#if INTERFACE
#define UCMD_CALC {N"calc", &cmd_calc,CMD_HELPARG}
#endif

void
cmd_calc(char * UNUSED(cmd), char * arg)
{
    int l = strlen(arg)*16 + 64;
    char * pcmd = malloc(l);
    if (!pcmd) return;

    strcpy(pcmd, "units -t -- '");
    char * p = pcmd+strlen(pcmd);
    int flg = 0;
    for(char * s = arg; *s; s++) {
	int ch = *s;
	if (ch < ' ' || ch > '~') ch = ' ';
	if (ch == '\'') { *p++ = '\''; *p++ = '\\'; *p++ = '\''; *p++ = '\''; }
	else if (ch == ',' && flg == 0) {
	    *p++ = '\''; *p++ = ' '; *p++ = '\''; flg = 1;
	}
	else *p++ = ch;
    }
    *p++ = '\'';
    *p = 0;

    FILE * pfd = popen(pcmd, "r");
    char result[160];
    int c = fread(result, 1, sizeof(result), pfd);
    result[c>0?c:0] = 0;
    pclose(pfd);

    for(char * p = result; *p; p++)
	if (*p == '\n') *p = ' ';

    printf_chat("&TResult&f: %s = %s", arg, result);
    free(pcmd);
}
