#include <sys/types.h>
#include <signal.h>

#include "heartbeat.h"

#define CURLLEN 256
#define ASCII_ONLY

static char curlbuf[CURLLEN];

/*
    Betacraft.uk shit:
    Additional required curl arguments:
	"-H", "Accept:",
	"-H", "User-Agent:"
    Does not support their own user list, must have paywalled Minecraft Acct
    Does not support %20 url encoding in server name.

    Failed: "-A", "curl/1.234"
    Failed: "-A", "MCscripts"
    Failed: "-A", "Mozilla/5.0 (X11; Linux x86_64)"
    "--user-agent", "MCGalaxy", // Whitelisted ?
*/

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

    char heartbeat_url[1024];
    int disable_web_client = ini_settings->disable_web_client;
    if (ini_settings->heartbeat_url[0] == 0)
	strcpy(heartbeat_url, "https://www.classicube.net/server/heartbeat/");
    else
	strcpy(heartbeat_url, ini_settings->heartbeat_url);

    // No point enabling websocket on these ports.
    for(int i = 0; browser_blocked_ports[i] != 0 && disable_web_client == 0; i++)
	if (browser_blocked_ports[i] == tcp_port_no)
	    disable_web_client = 1;

    char urlbuf[4096] = "";
    char postbuf[4096] = "";
    char namebuf[256];
    char softwarebuf[256];
    char softwarecons[256];
    char secretbuf[NB_SLEN];
    int valid_salt = (*server->secret != 0 && *server->secret != '-');

    if (valid_salt)
	convert_secret(secretbuf, 0);
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

    if (server->software[0]) {
	if (ini_settings->omit_software_version)
	    saprintf(softwarecons, "%s", server->software);
	else
	    saprintf(softwarecons, "%s %s", server->software, Version);
    } else {
	if (ini_settings->omit_software_version)
	    saprintf(softwarecons, "%s", SWNAME);
	else
	    saprintf(softwarecons, "%s %s", SWNAME, Version);
    }

    if (!ini_settings->use_http_post)
	saprintf(urlbuf,
	    "%s?%s%d&%s%d&%s%s&%s%d&%s%d&%s%s&%s%s&%s%s&%s%s",
	    heartbeat_url,
	    "port=",tcp_port_no,
	    "max=",server->max_players,
	    "public=",ini_settings->private||term_sig?"False":"True",
	    "version=",7,
	    "users=",unique_ip_count(),
	    "salt=",secretbuf,
	    "name=",ccnet_cp437_quoteurl(server->name, namebuf, sizeof(namebuf), 1),
	    "software=",ccnet_cp437_quoteurl(softwarecons, softwarebuf, sizeof(softwarebuf), 0),
	    "web=",disable_web_client?"False":"True"
	    );
    else
	saprintf(postbuf,
	    "&%s%d&%s%d&%s%s&%s%s&%s%d&%s%s&%s%d&%s%s&%s%s",
	    "port=",tcp_port_no,
	    "max=",server->max_players,
	    "name=",ccnet_cp437_quoteurl(server->name,namebuf,sizeof(namebuf), 1),
	    "public=",ini_settings->private||term_sig?"False":"True",
	    "version=",7,
	    "salt=",secretbuf,
	    "users=",unique_ip_count(),
	    "software=",ccnet_cp437_quoteurl(softwarecons, softwarebuf, sizeof(softwarebuf), 0),
	    "web=",disable_web_client?"False":"True"
	    );

    if ((heartbeat_pid = fork()) == 0) {
	if (listen_socket>=0) close(listen_socket);
	char cmdfilebuf[256];
	sprintf(cmdfilebuf, "log/curl-%d-c.txt", tcp_port_no);
	char logbuf[256];
	sprintf(logbuf, "log/curl-%d.txt", tcp_port_no);
	char dumpbuf[256];
	sprintf(dumpbuf, "log/curl-%d-h.txt", tcp_port_no);

	if (!ini_settings->use_http_post) {
	    FILE * fd = fopen(cmdfilebuf, "w");
	    if (fd) {
		fprintf(fd, "curl ... %s\n", urlbuf);
		fclose(fd);
	    }

	    // SHUT UP CURL!!
	    // -s   Complete silence ... WTF
	    // -S   Okay yes, show error message
	    // -f   Yes, and error messages from the web server to stderr.
	    if (execlp("curl", "curl",
		    "-s", "-S", "-4",
		    "--http0.9",
		    "-o", logbuf,
		    "-D", dumpbuf,
		    urlbuf,
		    (char*)0) < 0)
		perror("execlp(\"curl\"...)");

	    // Note: Returned string is web client URL, but the last
	    //       path part can be used to query the api; it's
	    //       the ip and port in ASCII hashed with MD5.
	    //       eg: echo -n 8.8.8.8:25565 | md5sum
	} else {
	    FILE * fd = fopen(cmdfilebuf, "w");
	    if (fd) {
		fprintf(fd, "curl ...\\\n  --data-binary %s \\\n  %s\n",
		    postbuf, heartbeat_url);
		fclose(fd);
	    }

	    // SHUT UP CURL!!
	    // -s   Complete silence ... WTF
	    // -S   Okay yes, show error message
	    // -f   Yes, and error messages from the web server to stderr.
	    if (execlp("curl", "curl",
		    "-s", "-S", "-4",
		    "--http0.9",
		    "-o", logbuf,
		    "-D", dumpbuf,
		    "-H", "Accept:",
		    "-H", "User-Agent:",
		    "--data-binary", postbuf,
		    heartbeat_url,
		    (char*)0) < 0)
		perror("execlp(\"curl\"...)");
	}
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

    if (rdlen == 0) strcpy(readbuf, "\\0");
    if (strcmp(readbuf, curlbuf) != 0) {
	printlog("Port %d heartbeat response: %s", tcp_port_no, readbuf);
    }
    strcpy(curlbuf, readbuf);
}

LOCAL char *
ccnet_cp437_quoteurl(char *s, char *dest, int len, int nocolour)
{
    char * d = dest;
#ifdef ASCII_ONLY
    char * last_colp = 0;
    for(; *s && d<dest+len-12; s++) {
	last_colp = 0;
	int ch = *s;
	if (ch & 0x80)
	    ch = cp437_ascii[ch & 0x7F];
	if (ch == '&' && nocolour) { if (s[1]) s++; continue; }
	if ( (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
	     (ch >= '0' && ch <= '9') ||
	     (ch == '-') || (ch == '_') || (ch == '.') || (ch == '~'))
	    *d++ = ch;
	else if (ch == '&') {
	    last_colp = d; // Trim trailing colour code.
	    char lb[10];
	    if (s[1] == 0) continue; // Trim trailing ampersand.
	    sprintf(lb, "%%%02x%%%02x", ch, s[1]);
	    s++;
	    for(char *p=lb; *p; p++)
		*d++ = *p;
	}
	else
	{
	    if (ch < ' ' || ch > '~')
		ch = '*';
	    char lb[4];
	    if (d>dest+len-4) break;
	    *d++ = '%';
	    sprintf(lb, "%02x", ch & 0xFF);
	    *d++ = lb[0];
	    *d++ = lb[1];
	}
    }
    if (last_colp) d = last_colp;
#else
    char * last_colp = 0;
    for(; *s && d<dest+len-12; s++) {
	last_colp = 0;
	if ( (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
	     (*s >= '0' && *s <= '9') ||
	     (*s == '-') || (*s == '_') || (*s == '.') || (*s == '~'))
	    *d++ = *s;
	else if (ch == '&') {
	    last_colp = d; // Trim trailing colour code.
	    char lb[10];
	    if (s[1] == 0) continue; // Trim trailing ampersand.
	    sprintf(lb, "%%%02x%%%02x", ch, s[1]);
	    s++;
	    for(char *p=lb; *p; p++)
		*d++ = *p;
	}
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
    if (last_colp) d = last_colp;
#endif
    *d = 0;
    return dest;
}
