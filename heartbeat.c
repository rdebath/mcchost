#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "heartbeat.h"

#define CURLLEN 128

static char curlbuf[CURLLEN];

void
send_heartbeat_poll()
{
    if (!start_heartbeat_task) {
	time_t now;
	time(&now);

	if (last_heartbeat == 0 && server->last_heartbeat_port == tcp_port_no)
	    last_heartbeat = server->last_heartbeat;

	if (alarm_sig == 0 && last_heartbeat != 0 && !term_sig) {
	    if ((now-last_heartbeat) < 45)
		return;
	}
	last_heartbeat = now;
	server->last_heartbeat = last_heartbeat;
	server->last_heartbeat_port = tcp_port_no;
    }

    if (heartbeat_pid) {
	kill(heartbeat_pid, SIGALRM);	// Timeout!
	heartbeat_pid = 0;
	return;
    }

    char cmdbuf[4096];
    char namebuf[256];
    char softwarebuf[256];
    char secretbuf[NB_SLEN];
    int valid_salt = (*server->secret != 0 && *server->secret != '-');

    if (valid_salt)
	convert_secret(secretbuf, heartbeat_url, 0);
    else
	strcpy(secretbuf, "0000000000000000");

    // {"errors":[["Only first 256 unicode codepoints may be used in server names."]],"response":"","status":"fail"}
    //
    // This is a LIE.
    // The codepoints are in cp437, but are encode using the UTF8 bit patterns.
    //
    // Printable ASCII are fine.
    //
    // Control characters (☺☻♥♦♣♠•◘○◙♂♀♪♫☼▶◀↕‼¶§▬↨↑↓→←∟↔▲▼) fail with the
    // above error too.
    //
    // High controls (ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ) are baaad
    // --> Oopsie Woopsie! Uwu We made a fucky wucky!! A wittle fucko boingo!
    // --> The code monkeys at our headquarters are working VEWY HAWD to fix this!
    //
    // The character ⌂ is useable
    // Characters above 160 appear to work "correctly", however, the web site
    // doesn't display them correctly.
    //
    //  áíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐
    //  └┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀
    //  αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ 

    snprintf(cmdbuf, sizeof(cmdbuf),
	"%s?%s%d&%s%d&%s%s&%s%d&%s%d&%s%s&%s%s&%s%s&%s%s",
	heartbeat_url,
	"port=",tcp_port_no,
	"max=",server->max_players,
	"public=",server->private||term_sig?"False":"True",
	"version=",7,
	"users=",unique_ip_count(),
	"salt=",secretbuf,
	"name=",ccnet_cp437_quoteurl(server->name, namebuf, sizeof(namebuf)),
	"software=",ccnet_cp437_quoteurl(server->software, softwarebuf, sizeof(softwarebuf)),
	"web=","False"
	);

    if ((heartbeat_pid = fork()) == 0) {
	if (listen_socket>=0) close(listen_socket);
	char logbuf[256];
	sprintf(logbuf, "log/curl-%d.txt", tcp_port_no);
	char dumpbuf[256];
	sprintf(dumpbuf, "log/curl-%d-h.txt", tcp_port_no);

	// SHUT UP CURL!!
	// -s   Complete silence ... WTF
	// -S   Okay yes, show error message
	// -f   Yes, and error meesages from the web server to stderr.
	if (execlp("curl", "curl",
		"-s", "-S",
		"-o", logbuf,
		"-D", dumpbuf,
		cmdbuf,
		(char*)0) < 0)
	    perror("execlp(\"curl\"...)");

	// Note: Returned string is web client URL, but the last
	//       path part can be used to query the api; it's
	//       the ip and port in ASCII hashed with MD5.
	//       eg: echo -n 8.8.8.8:25565 | md5sum
	exit(127);
    }
    if (heartbeat_pid<0) {
	heartbeat_pid = 0;
	perror("fork()");
    }
}

void
log_heartbeat_response()
{
    char readbuf[CURLLEN];
    char logbuf[256];
    sprintf(logbuf, "log/curl-%d.txt", tcp_port_no);
    int rdlen = 0;

    FILE * fd = fopen(logbuf, "r");
    if (fd) {
	int ch;
	while(rdlen < CURLLEN-5 && (ch=fgetc(fd)) != EOF) {
	    if (ch == '\n') {
		readbuf[rdlen++] = '\\';
		readbuf[rdlen++] = 'n';
	    } else if (ch >= ' ' && ch <= '~' && ch != '\\')
		readbuf[rdlen++] = ch;
	    else {
		readbuf[rdlen++] = '\\';
		readbuf[rdlen++] = '0'+((ch>>6)&7);
		readbuf[rdlen++] = '0'+((ch>>3)&7);
		readbuf[rdlen++] = '0'+(ch&7);
	    }
	    readbuf[rdlen] = 0;
	}
	fclose(fd);
    }

    if (rdlen == 0) strcpy(readbuf, "\\<Nothing>");
    if (strcmp(readbuf, curlbuf) != 0) {
	printlog("Heartbeat response: %s", readbuf);
    }
    strcpy(curlbuf, readbuf);
}

LOCAL char *
ccnet_cp437_quoteurl(char *s, char *dest, int len)
{
    char * d = dest;
    for(; *s && d<dest+len-1; s++) {
	if ( (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
	     (*s >= '0' && *s <= '9') ||
	     (*s == '-') || (*s == '_') || (*s == '.') || (*s == '~'))
	    *d++ = *s;
	else if (*s >= ' ' && (*s & 0x80) == 0) // ASCII
	{
	    char lb[4];
	    if (d>dest+len-4) break;
	    *d++ = '%';
	    sprintf(lb, "%02x", *s & 0xFF);
	    *d++ = lb[0];
	    *d++ = lb[1];
	}
	else if ((*s &0x7F) < ' ') // Control characters both C0 and C1
	{
	    if (*s & 0x80)
		*d++ = cp437_ascii[*s & 0x7F];
	    else
		*d++ = '*';
	}
	else // Workable.
	{
	    int c1, c2, c3;
	    int uc = *s & 0xFF;
	    char lb[10];
	    if (d>dest+len-10) break;
	    *lb = 0;

	    c2 = (uc/64);
	    c1 = uc - c2*64;
	    c3 = (c2/64);
	    c2 = c2 - c3 * 64;
	    if (uc < 2048)
		sprintf(lb, "%%%02x%%%02x", c2+192, c1+128);
	    else if (uc < 65536)
		sprintf(lb, "%%%02x%%%02x%%%02x", c3+224, c2+128, c1+128);

	    for(char *p=lb; *p; p++)
		*d++ = *p;
	}
    }
    *d = 0;
    return dest;
}
