
#include "cmdcalc.h"

/*HELP calc H_CMD
&T/calc [Expression]
Calculate the expression and display the result.
Operators: + - * / % ^ < > <= >= == !=
Unary Ops: + - !
Functions: sqrt(x) sin(x) cos(x) atan2(x,x) exp(x) log(x) int(x) rand(x)
*/

#if INTERFACE
#define UCMD_CALC {N"calc", &cmd_calc,CMD_HELPARG}
#endif

#define MAWK "/usr/bin/mawk"

void
cmd_calc(char * UNUSED(cmd), char * arg)
{
static char op_chars[] = "!%()*+,-/:<=>?^| ";
static char *funcs[] = { "atan2", "cos", "exp", "int", "log", "rand", "sin", "sqrt", 0 };

    int l = strlen(arg)*16 + 64;
    char * pcmd = malloc(l);
    if (!pcmd) return;

    // Use mawk if available because (g)awk is sometime weird/broken.
    // Use our RNG to seed awk so we don't get the same seed for repeated.
    sprintf(pcmd, "%s 2>&1 'BEGIN{OFMT=\"%%.17g\"; srand(%d); v= ",
	(access(MAWK, X_OK) == 0) ? MAWK: "awk",
	bounded_random(0x7fffffff));

    // We're not allowing '&' so if there are any, change them to '%'.
    // NB this is too late for the command log.
    for(char * s = arg; *s; s++) if (*s == '&') *s = '%';

    // Security: Awk is roo powerful to allow unfiltered access.
    // So only allow words and characters that work and are known safe.
    // Highlights to avoid include 'system("command")' and 'getline'
    // Redirection using '> filename' is avoided above by using a variable.
    char * p = pcmd+strlen(pcmd), *ident = 0;
    char * strt = p;
    int state = 0;
    for(char * s = arg; *s; s++) {
	int ch = *s;
	if (ch < ' ' || ch > '~' || ch == '\'') ch = ' ';
	switch(state) {
	case 0:
	    if (strchr(op_chars, ch) != 0) { *p++ = ch; continue; }
	    if (isdigit(ch) || ch == '.') {
		*p++ = ch; continue;
	    }
	    if (!isalpha(ch)) {
		*p = 0;
		printf_chat("&WSyntax error \"%s\" -->\"%s\"", strt, s);
		free(pcmd);
		return;
	    }
	    ident = p;
	    state = 1;
	    /*FALLTHROUGH*/
	case 1:
	    *p++ = ch;
	    if (isalnum(s[1])) continue;
	    *p = 0;
	    for(int i = 0; funcs[i]; i++) {
		if (strcasecmp(funcs[i], ident) == 0) {
		    strcpy(ident, funcs[i]);
		    state = 3;
		    break;
		}
	    }
	    if (state != 3) {
		printf_chat("&WUnknown function: \"%s\"", ident);
		return;
	    }
	    state = 0;
	}
    }
    *p = 0;
    // Include a newline so "at end of line" errors are generated.
    strcpy(p, "\nprint v;}'");

    //printlog("Run: %s", pcmd);

    FILE * pfd = popen(pcmd, "r");
    free(pcmd);
    char result[160] = "";
    if (!pfd) {
	printf_chat("&WCalc failed: %m");
	return;
    }
    int c = fread(result, 1, sizeof(result), pfd);
    result[c>0?c:0] = 0;
    pclose(pfd);
    if (*result == 0) {
	printf_chat("&WCalculator program returned no result");
	return;
    }

    for(char * d = result; *d; d++)
	if (*d == '\n') *d = ' ';

    char * r = result, *t;

    if ((t = strstr(r, "awk: ")) != 0) r = t+5;
    if ((t = strstr(r, "line 1: ")) == r) r = t+8;
    printf_chat("#&TResult&f: %s&f = %s", arg, r);
}
