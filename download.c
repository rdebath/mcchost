#define _XOPEN_SOURCE
#include <sys/stat.h>
#include <fcntl.h>

#include "download.h"

#if INTERFACE
#define MAXHTTPDOWNLOAD	(16777216)
#endif

int
http_download(uint8_t * buf, int buflen)
{
    if (server->disable_web_server) return 0;

    enum { has_host=1, has_accept=2, has_upgrade=4 } has_stuff = 0;

    char *fn = 0;
    int head_only = 0;

    // Basic check that the HTTP request looks ok.
    if (memcmp(buf, "GET /~texture/", 14) == 0) {
	fn = buf + 14;
    } else if (memcmp(buf, "HEAD /~texture/", 15) == 0) {
	fn = buf + 15;
	head_only = 1;
    } else return 0;

    time_t not_modified_date = 0;
    char now_822[64];

    {
	time_t now = time(0);
	struct tm tm[1];
	gmtime_r(&now, tm);
	strftime(now_822, sizeof(now_822), "%a, %d %b %Y %T GMT", tm);
    }

    //log_message("Text received was", buf, buflen);

    for(uint8_t *p = buf; *p; p++) {
	if (p-buf >= buflen) return 0;
	if (*p != '\n') continue;

	if (strncasecmp(p+1, "Host", 4) == 0) has_stuff |= has_host;
	if (strncasecmp(p+1, "Accept", 6) == 0) has_stuff |= has_accept;
	if (strncasecmp(p+1, "Upgrade", 7) == 0) has_stuff |= has_upgrade;
	if (strncasecmp(p+1, "If-Modified-Since: ", 19) == 0) {
	    struct tm tm[1];
	    char * e = strptime(p+20, "%a, %d %b %Y %T GMT", tm);
	    if (e)
		not_modified_date = mktime_t(tm);
	}
    }
    // Does it have everything?
    if ((has_stuff & (has_host+has_upgrade)) != has_host) return http_error(400);

    char * e = strchr(fn, ' ');
    if (!e) e = strchr(fn, '\r');
    if (!e) return http_error(400);
    *e = '\0';

    if (fn[0] == '.' || strchr(fn, '/') != 0) http_error(403);
    if (server->check_web_client_ip && !check_download_ip()) {
	log_message("Text received was", buf, buflen);
	return http_error(403);
    }

    memset(proc_args_mem, 0, proc_args_len);
    snprintf(proc_args_mem, proc_args_len, "%s http download", SWNAME);

    char filename[256];
    saprintf(filename, "texture/%s", fn);

    struct stat st;
    if (stat(filename, &st) < 0) return http_error(404);
    if ((st.st_mode & S_IFMT) != S_IFREG) {
	printlog("Inode %s is not a file", filename);
	http_error(404);
    }
    if (st.st_size > MAXHTTPDOWNLOAD) {
	printlog("Inode %s is too large (%d)", filename, MAXHTTPDOWNLOAD);
	http_error(500);
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
	perror(filename);
	return http_error(400);
    }

    if (not_modified_date == st.st_mtime) {
	// It should be exact; I'm not accepting an approximate

	char obuf[2048];
	saprintf(obuf, "%s\r\n%s %s\r\n%s\r\n\r\n",
	    "HTTP/1.1 304 OK",
	    "Date:", now_822,
	    "Server: Tiny Http");

	// log_message("Text sent was", obuf, strlen(obuf));
	write_to_remote(obuf, strlen(obuf));

	printlog("HTTP unmodified \"%s\" to client %s", fn, client_ipv4_str);

    } else {
	char mod_time[256];
	struct tm tm[1];
	gmtime_r(&st.st_mtime, tm);
	strftime(mod_time, sizeof(mod_time), "%a, %d %b %Y %T GMT", tm);

	char obuf[2048];
	saprintf(obuf, "%s\r\n%s %s\r\n%s\r\n%s\r\n%s %s\r\n%s %d\r\n%s\r\n\r\n",
	    "HTTP/1.1 200 OK",
	    "Date:", now_822,
	    "Server: Tiny Http",
	    "Access-Control-Allow-Origin: *",
	    "Last-Modified:", mod_time,
	    "Content-Length:", (int)st.st_size,
	    "Content-Type: application/binary");

	// log_message("Text sent was", obuf, strlen(obuf));
	write_to_remote(obuf, strlen(obuf));

	if (!head_only) {
	    int cc;
	    while ((cc= read(fd, obuf, sizeof(obuf))) > 0) {
		write_to_remote(obuf, cc);
	    }
	}
	close(fd);

	printlog("Sending %s \"%s\" to client %s", head_only?"HEAD":"file", fn, client_ipv4_str);
    }

    if (fcntl(line_ifd, F_SETFL, 0) < 0)
	perror("fcntl(line_ifd, F_SETFL, 0)");
    // SO_LINGER by default.

    flush_to_remote();
    int max_loop = 120;
    while(bytes_queued_to_send()) {
	msleep(500);	// just a little bodge.
	flush_to_remote();
	if (--max_loop <=0) break;
    }
    exit(0);
}

LOCAL int
http_error(int http_errno)
{
    printlog("Failed connect from %s, sending http %s %d",
	client_ipv4_str, http_errno>=400?"error":"response", http_errno);

    char obuf[2048];
    char * msg = "Error";
    if (http_errno == 404) msg = "File not found";
    if (http_errno == 304) msg = "File not modified";
    saprintf(obuf, "%s %d %s\r\n\r\n", "HTTP/1.1", http_errno, msg);
    write_to_remote(obuf, strlen(obuf));
    return 1;
}

LOCAL int
check_download_ip()
{
    int rv = 0;
    open_client_list();
    if (!shdat.client) return 0;
    for(int i=0; i<MAX_USER; i++)
    {
	if (!shdat.client->user[i].active) continue;
	if (shdat.client->user[i].ip_address == client_ipv4_addr) {
	    rv = 1; break;
	}
    }
    stop_client_list();
    return rv;
}

void
log_message(char * why, uint8_t *buf, int len)
{
    char message[65536];
    int j = 0;
    for(int i = 0; i<len && j<(int)sizeof(message)-4; i++) {
	if (buf[i] >= ' ') message[j++] = buf[i];
	else {
	    message[j++] = '\\';
	    if (buf[i] == '\n') message[j++] = 'n';
	    if (buf[i] == '\r') message[j++] = 'r';
	}
    }
    message[j] = 0;
    printlog("%s: %s", why, message);
}
