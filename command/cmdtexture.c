
#include "cmdtexture.h"

#if INTERFACE
#define UCMD_TEXTURE \
    {N"texture", &cmd_texture,CMD_HELPARG}
#endif

/*HELP texture H_CMD
&T/texture url
Changes the current level texture
Using 'reset' for [url] will reset to the default.
*/

void
cmd_texture(char * UNUSED(cmd), char *arg)
{
    char * str1 = strarg(arg);
    char * str2 = strarg(0);

    if (strcasecmp(str1, "global") == 0 || strcasecmp(str1, "level") == 0)
	str1 = str2;
    if (!str1 || strcasecmp(str1, "reset") == 0)
	str1 = "";

    if (setvar("level", "texture", str1)) {
	if (!str1 || !*str1)
	    printf_chat("Reset texture to default");
	else
	    printf_chat("Set texture to &T%s", str1);
    }
}
