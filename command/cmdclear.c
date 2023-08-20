
#include "cmdclear.h"

#if INTERFACE
#define UCMD_CLEAR \
    {N"clear", &cmd_clear}, {N"cls", &cmd_clear, CMD_ALIAS}
#endif

/*HELP clear,cls H_CMD
&T/Clear &SClear your chat.
Alias: &T/cls
*/

void
cmd_clear(char * UNUSED(cmd), char * UNUSED(arg))
{
    for(int i = 0; i<30; i++)
        printf_chat(" ");;
    printf_chat("&WCleared");;
}

