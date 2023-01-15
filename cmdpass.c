#include <fcntl.h>

#include "cmdpass.h"

/* This is not the best example of password handling.
 *
 * The password is hashed and salt'ed, however the hash used
 * is a fast cryptographical one and so can be bypassed if
 * the password is poor and the attacker gets hold of the
 * hashed password. This would still be fine with passwords/keys
 * that have a large amount of entropy, but not general user
 * *chosen* ones (which should be avoided if at all possible).
 *
 * The best way to avoid user chosen passwords is to offload
 * that headache onto someone else ... ie use mppass.
 *
 * For this application the main issue is that this password
 * is transmitted over an unencrypted connection. (use mmpass!)
 */

/*HELP pass,setpass H_CMD
&T/pass [password]
Login to your session
&T/setpass [password]
Set a password for your session
*/

/*HELP lostpassword
Try again or contact the OPs to reset it
*/

/* NOTE: To reset a password delete the secret/${user}.pwl file */

#if INTERFACE
#define CMD_PASS \
    {N"pass", &cmd_pass, CMD_HELPARG}, {N"setpass", &cmd_pass, .nodup=1, .dup=1}
#endif

static int testing_just_set_password = 0;

void
cmd_pass(char * cmd, char * arg)
{
    if (strcmp(cmd, "setpass") == 0) return do_setpass(arg);
    else if (!do_pass(arg, 0)) return;

    // Right pass.
    if (user_authenticated)
	printf_chat("That was the correct password");
    else
	printf_chat("You are now logged on");

    int reopen = 0;
    if (!user_authenticated) {
	if (ini_settings->void_for_login || !level_prop) {
	    reopen = 1;
	} else {
	    send_posn_pkt(-1, &player_posn, level_prop->spawn);
	    player_posn = level_prop->spawn;
	}
    }

    testing_just_set_password = 0;
    user_authenticated = 1;

    if (reopen) open_main_level();
}

void
do_setpass(char * passwd)
{
    if (!passwd || !*passwd) return cmd_help(0, "/setpass");

static char base62[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    char salt[NB_SLEN], hashpw[NB_SLEN];
    if (!user_authenticated && !testing_just_set_password) {
	readpw_file(user_id, salt, hashpw);
	if (*hashpw != 0) {
	    printf_chat("&WYou must verify with &T/pass [pass]&W before changing your password");
	    return;
	}
    }

    if (!user_authenticated)
	testing_just_set_password = 1;

    init_rand_gen();

    for(int i=0; i<22; i++) {
        int ch = bounded_random(62);
        salt[i] = base62[ch];
        salt[i+1] = 0;
    }

    char buf[NB_SLEN*4];
    char sha_buf[128];

    saprintf(buf, "%s.%s", salt, passwd);

    sha256digest(0, sha_buf, buf, strlen(buf));

    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, user_id);
    char pwbuf[NB_SLEN*8];
    saprintf(pwbuf, "%s\n%s\n", salt, sha_buf);
    char passfile[NB_SLEN*4];
    saprintf(passfile, SECRET_PW_NAME, user_key);

    int fd = open(passfile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) {
	printf_chat("&WUnable to save password\n");
	return;
    }
    int cc = write(fd, pwbuf, strlen(pwbuf));
    if (cc != strlen(pwbuf)) {
	printf_chat("&WUnable to write password\n");
	return;
    }
    close(fd);
    if (!user_authenticated)
	printf_chat("Password saved, now check it by using /pass or reset it with /setpass");
    else
	printf_chat("Password saved");
}

int
do_pass(char * passwd, int quiet)
{
    char salt[NB_SLEN], hashpw[NB_SLEN];
    readpw_file(user_id, salt, hashpw);
    if (!*salt || !*hashpw){
	if (!quiet) printf_chat("&WYou need to set a password first with &T/setpass 1234");
	return 0;
    }

    char buf[NB_SLEN*4];
    char sha_buf[128];

    saprintf(buf, "%s.%s", salt, passwd);

    sha256digest(0, sha_buf, buf, strlen(buf));
    if (strcmp(hashpw, sha_buf) != 0) {
	if (quiet) return 0;
	printf_chat("&WWrong password&S Remember your password is case sensitive");
	if (testing_just_set_password)
	    printf_chat("Try again or you can try using /setpass again");
	else
	    cmd_help(0, "lostpassword");
	return 0;
    }
    testing_just_set_password = 0; // Done
    return 1;
}

void
trim_ws(char * buf)
{
    char *p = buf + strlen(buf);
    for(;p>buf && (p[-1] == '\n' || p[-1] == '\r' ||
		   p[-1] == ' ' || p[-1] == '\t'); p--)
	p[-1] = 0;
}

void
readpw_file(char * userid, char * salt, char * hashpw)
{
    uint8_t user_key[NB_SLEN*4];
    copy_user_key(user_key, userid);
    *hashpw = *salt = 0;

    char passwd[NB_SLEN*4];
    saprintf(passwd, SECRET_PW_NAME, user_key);
    FILE * fd = fopen(passwd, "r");
    if (fd) {
	fgets(salt, NB_SLEN, fd);
	trim_ws(salt);
	fgets(hashpw, NB_SLEN, fd);
	trim_ws(hashpw);
	fclose(fd);
    }
}

/*

Modes:
    1) No authentication
    2) mppass
    3) Unauthenicated can use /setpass and /pass
    4) Unauthenicated can use /pass
	--> mppass field can contain pass
	--> user field can be user:pass
    5) IP address authenticated

    +) mppass plus /pass ?

--> First that succeeds
enable_ipauth = true
enable_mppass = true
enable_pass = false
--> Allow /setpass to unauthenticated
enable_setpass = true

User authorisation
    sysadmin
	ipauth
    leveladmin
	Personal level
    user
*/
