
#include "cmdtrust.h"

#if INTERFACE
#define UCMD_TRUST \
    {N"trust", &cmd_trust,CMD_HELPARG}
#endif

/*HELP trust H_CMD
&T/Trust [player]
Turns off anti-grief for player.
*/

void
cmd_trust(char * UNUSED(cmd), char *arg)
{
    char * str1 = strtok(arg, " ");

    int uid = find_online_player(str1, 1, 0);
    if (uid < 0) return;

    client_entry_t c = shdat.client->user[uid];
    if (!c.active) return;

    int v = !c.trusted;
    shdat.client->user[uid].trusted = v;

    printf_chat("%s's trust status: %s", c.name.c, v?"True":"False");

    char buf[NB_SLEN];
    saprintf(buf, "Your trust status was changed to: %s", v?"True":"False");
    post_chat(1, uid, 0, buf, strlen(buf));
    return;
}
