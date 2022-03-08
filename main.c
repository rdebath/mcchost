#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <sys/socket.h>

// Note: This header is generated by "makeheaders"
// Currently: https://fossil-scm.org/home/file/src/makeheaders.c
#include "main.h"

#if INTERFACE
#define MB_STRLEN 64
#define NB_SLEN	(MB_STRLEN+1)
#endif

int line_ofd = 1;
int line_ifd = 0;
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

int
main(int argc, char **argv)
{
    (void)argc; (void)argv;

    login();

    send_server_id_pkt(server_name, server_motd, cpe_requested);

    // Open system mmap files.
    create_chat_queue();

    // Open level mmap files.
    start_shared(level_name);
    send_map_file();

    {
	char buf[256];
	sprintf(buf, "&a+ &7%s &econnected", user_id);
	post_chat(buf, strlen(buf));
    }

    send_spawn_pkt(255, user_id, level_prop->spawn);
    send_message_pkt(0, "&eWelcome to this broken server");

    run_request_loop();

    return 0;
}

void
login()
{
    time_t startup = time(0);
    fcntl(line_ifd, F_SETFL, (int)O_NONBLOCK);
    while(insize-inptr < 131)
    {
	int cc = read(line_ifd, inbuf+insize, sizeof(inbuf)-insize);
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
    fcntl(line_ifd, F_SETFL, 0);

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
    send_discon_msg_pkt(emsg);
    shutdown(line_ofd, SHUT_RDWR);
    shutdown(line_ifd, SHUT_RDWR);
    exit(1);
}

void
cpy_nbstring(char *buf, char *str)
{
    memcpy(buf, str, MB_STRLEN);
    for(int i = 0; i<MB_STRLEN; i++) if (buf[i] == 0) buf[i] = ' ';
    buf[MB_STRLEN] = 0;
    char * p = buf+MB_STRLEN;
    while (p>buf && p[-1] == ' ') *(--p) = 0;
}
