
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "tcpserver.h"

static int listen_socket = -1;
static time_t last_heartbeat = 0;

static inline
int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

void
tcpserver()
{
    listen_socket = start_listen_socket("0.0.0.0", tcp_port_no);

    while(1)
    {
        int max_sock = listen_socket;
        fd_set read_fds;
        fd_set write_fds;
        fd_set except_fds;
        struct timeval tv;

        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&except_fds);

        FD_SET(listen_socket, &read_fds);
        FD_SET(listen_socket, &except_fds);
        // ... stdin?

        tv.tv_sec = 1; tv.tv_usec = 0;
        int rv = select(max_sock+1, &read_fds, &write_fds, &except_fds, &tv);
        if (rv < 0 && errno != EINTR) { perror("select()"); exit(1); }

        if (rv > 0)
        {
            if (FD_ISSET(listen_socket, &read_fds))
            {
                int socket = accept_new_connection();

                int pid = E(fork(), "Forking failure");
                if (pid == 0)
                {
                    setsid();
                    E(close(listen_socket), "close(listen)");
                    line_ofd = 1; line_ifd = 0;
                    E(dup2(socket, line_ifd), "dup2(S,0)");
                    E(dup2(socket, line_ofd), "dup2(S,1)");
                    process_connection();
                    exit(0);
                }
                close(socket);
            }
            if (FD_ISSET(listen_socket, &except_fds))
                break;
        }

	cleanup_zombies();

	if (enable_heartbeat_poll)
	    send_heartbeat_poll();

    }
}

/* Start listening socket listen_sock. */
LOCAL int
start_listen_socket(char * listen_addr, int port)
{
    int listen_sock;
    // Obtain a file descriptor for our "listening" socket.
    listen_sock = E(socket(AF_INET, SOCK_STREAM, 0), "socket");

    int reuse = 1;
    E(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)), "setsockopt");

    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(listen_addr);
    my_addr.sin_port = htons(port);

    E(bind(listen_sock, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)), "bind");

    // start accept client connections (queue 10)
    E(listen(listen_sock, 10), "listen");

    fprintf(stderr, "Accepting connections on port %d.\n", (int)port);

    return listen_sock;
}

LOCAL int
accept_new_connection()
{
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);
    int new_client_sock;
    new_client_sock = E(accept(listen_socket, (struct sockaddr *)&client_addr, &client_len), "accept()");

    char client_ipv4_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4_str, INET_ADDRSTRLEN);

    fprintf(stderr, "Incoming connection from %s:%d.\n", client_ipv4_str, client_addr.sin_port);

    return new_client_sock;
}

LOCAL void
cleanup_zombies()
{
    // Clean up any zombies.
    int status = 0, pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) != 0)
    {
	if (pid < 0) {
	    if (errno == ECHILD || errno == EINTR)
		break;
	    perror("waitpid()");
	    exit(1);
	}
	// No complaints about normal processes.
	if (WIFEXITED(status)) continue;
	if (WIFSIGNALED(status)) {
	    fprintf(stderr, "Process %d was killed by signal %d\n",
		pid, WTERMSIG(status));

	   delete_session_id(pid);
	}
    }
}

LOCAL void
send_heartbeat_poll()
{
    time_t now;
    time(&now);
    if (last_heartbeat != 0) {
	if ((now-last_heartbeat) < 45)
	    return;
    }
    last_heartbeat = now;

    char cmdbuf[4096];
    char namebuf[256];
    char softwarebuf[256];
    int valid_salt = (*server_salt != 0 && *server_salt != '-');

    snprintf(cmdbuf, sizeof(cmdbuf),
	"%s?%s%d&%s%d&%s%s&%s%d&%s%d&%s%s&%s%s&%s%s",
	heartbeat_url,
	"port=",tcp_port_no,
	"max=",255,
	"public=",server_private?"False":"True",
	"version=",7,
	"users=",current_user_count(),
	"salt=",valid_salt?server_salt:"0000000000000000",
	"name=",quoteurl(server_name, namebuf, sizeof(namebuf)),
	"software=",quoteurl("Custom", softwarebuf, sizeof(softwarebuf))
	);

    if (fork() == 0) {
	close(listen_socket);
	// SHUT UP CURL!!
	E(execlp("curl", "curl", "-s", "-S", "-o", "/dev/null", cmdbuf, (char*)0), "exec of curl failed");
	// Note: Returned string is web client URL
	exit(0);
    }
}

LOCAL char *
quoteurl(char *s, char *dest, int len)
{
    char * d = dest;
    for(; *s && d<dest+len-1; s++) {
	if ( (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
	     (*s >= '0' && *s <= '9') ||
	     (*s == '-') || (*s == '_') || (*s == '.') || (*s == '~'))
	    *d++ = *s;
	else {
	    char lb[4];
	    if (d>dest+len-4) break;
	    *d++ = '%';
	    sprintf(lb, "%02x", *s & 0xFF);
	    *d++ = lb[0];
	    *d++ = lb[1];
	}
    }
    *d = 0;
    return dest;
}
