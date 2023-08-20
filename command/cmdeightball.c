
#include "cmdeightball.h"

/*HELP 8ball,magic8ball H_CMD
&T/8ball Yes or no question
Get an answer from the all-knowing 8-ball
Alias: &T/magic8ball
*/

#if INTERFACE
#define UCMD_EIGHTBALL \
    {N"magic8ball", &cmd_eightball, CMD_HELPARG}, \
    {N"8ball", &cmd_eightball, CMD_ALIAS}
#endif

// These are the classic 8-Ball responses.
char * eightballlist[] = {
    "Signs point to yes",
    "As I see it, yes",
    "It is certain",
    "It is decidedly so",
    "My reply is no",
    "Outlook good",
    "Yes definitely",
    "Ask again later",
    "Reply hazy try again",
    "Cannot predict now",
    "Concentrate and ask again",
    "Better not tell you now",
    "Don't count on it",
    "Most likely",
    "Yes",
    "Very doubtful",
    "Outlook not so good",
    "My sources say no",
    "Without a doubt",
    "You may rely on it",
    0
};

static int eightball_selection = 0;
static time_t blocking_til = 0;

void
cmd_eightball(char * UNUSED(cmd), char * arg)
{
    // Todo check for file

    MD5_CTX mdContext;
    MD5Init (&mdContext);
    time_t today = time(0) / 86400;
    char buf[64];
    sprintf(buf, "%d:", (int)today);
    MD5Update (&mdContext, buf, strlen(buf));
    MD5Update (&mdContext, arg, strlen(arg));
    MD5Final (&mdContext);

    time_t now = time(0);
    if (now < blocking_til) {
	printf_chat("The 8-Ball is still recharging, wait another %d second%s",
	    (int)(blocking_til-now), blocking_til-now==1?"":"s");
	return;
    }

    int c;
    for(c = 0; eightballlist[c]; c++);
    c = (mdContext.digest[0]*256 + mdContext.digest[1]) % c;

    printf_chat("@&7%s&S asked the &b8-Ball:&f %s", user_id, arg);
    eightball_selection = c;
    schedule_task(2, say_eightball_selection);
    blocking_til = now+12;
}

void
say_eightball_selection()
{
    printf_chat("@&SThe &b8-Ball&S says:&f %s", eightballlist[eightball_selection]);
}
