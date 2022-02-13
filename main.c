#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include "main.h"

#if INTERFACE
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
#endif

int ifd = 0;
char inbuf[4096];
int insize = 0, inptr = 0;

char user_id[NB_SLEN];
int user_authenticated = 0;
int cpe_requested = 0;

char server_name[NB_SLEN] = "Broken Server";
char server_motd[NB_SLEN] = "Welcome";

char server_salt[NB_SLEN] = "";
int cpe_enabled = 0;

void login(void);
void fatal(char * emsg);
void cpy_nbstring(char *buf, char *str);

char * level_name = "main";

int ofd = 1;

int
main(int argc, char **argv)
{
    login();

    if (cpe_requested && cpe_enabled)
	cpe_requested = 0; // processextinfo();

    send_server_id_pkt(ofd, server_name, server_motd);

    // Open system mmap files.

    // Open level mmap files.
    start_shared(level_name);

    send_map_file(ofd);
    sleep(3);
    // run_request_loop();

    fatal("This server is broken.");
}

void
login()
{
    time_t startup = time(0);
    fcntl(ifd, F_SETFL, (int)O_NONBLOCK);
    while(insize-inptr < 131)
    {
	int cc = read(ifd, inbuf+insize, sizeof(inbuf)-insize);
	if (cc>0) insize += cc;
	if (cc<=0 && errno != EINTR) {
	    if (errno != EAGAIN)
		fatal("Error reading client startup");
	    time_t now = time(0);
	    if (now-startup > 4)
		fatal("Short logon packet received");
	    usleep(100000); // Should never happen.
	}

	if (insize >= 1 && inbuf[inptr] != 0) // Special exit for weird caller.
	    fatal("418 I'm a teapot\n");
	if (insize >= 2 && inbuf[inptr+1] != 7)
	    fatal("Only protocol version seven is supported (with CPE)");
    }
    fcntl(ifd, F_SETFL, 0);

    cpy_nbstring(user_id, inbuf+2);
    if (*user_id == 0 || strlen(user_id) > 16)
	fatal("Usernames must be between 1 and 16 characters");

    for(int i = 0; user_id[i]; i++)
	if (!isascii(user_id[i]) ||
	    (!isalnum(user_id[i]) && user_id[i] != '.' && user_id[i] != '_'))
		fatal("Invalid player name");

    char mppass[NB_SLEN];
    cpy_nbstring(mppass, inbuf+64+2);

    cpe_requested = inbuf[inptr+128+2] == 0x42;

    if (*server_salt != 0) {
	char hashbuf[NB_SLEN*2];
	strcpy(hashbuf, server_salt);
	strcat(hashbuf, user_id);

	MD5_CTX mdContext;
	unsigned int len = strlen (hashbuf);
	MD5Init (&mdContext);
	MD5Update (&mdContext, (unsigned char *)hashbuf, len);
	MD5Final (&mdContext);

	for (int i = 0; i < 16; i++) {
	    sprintf(hashbuf+i*2, "%02x", mdContext.digest[i]);
	}

	if (strcasecmp(hashbuf, mppass) != 0)
	    fatal("Login failed! Close the game and sign in again.");

	user_authenticated = 1;
    }
}

void
fatal(char * emsg)
{
    send_discon_msg_pkt(ofd, emsg);
    exit(1);
}

void
cpy_nbstring(char *buf, char *str)
{
    memcpy(buf, str, NB_SLEN-1);
    buf[NB_SLEN-1] = 0;
    char * p = buf+NB_SLEN-1;
    while (p>buf && (p[-1] == 0 || p[-1] == ' ')) *(--p) = 0;
}
