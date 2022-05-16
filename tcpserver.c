
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include "tcpserver.h"

int start_tcp_server = 0;
int detach_tcp_server = 0;
int log_to_stderr = 0;
int tcp_port_no = 25565;
static int listen_socket = -1;

int enable_heartbeat_poll = 0;
static time_t last_heartbeat = 0;

int enable_backups = 1;
static time_t last_backup = 0;

static inline int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

char client_ipv4_str[INET_ADDRSTRLEN];
int client_ipv4_port = 0;
int client_ipv4_localhost = 0;
int disable_restart = 0;

static volatile int restart_sig = 0, child_sig = 0, alarm_sig;
static int signal_available = 0;

void
handle_signal(int signo)
{
    if (signo == SIGHUP) restart_sig = 1;
    if (signo == SIGCHLD) child_sig = 1;
    if (signo == SIGALRM) alarm_sig = 1;
}

void
dont_panic()
{
    assert(!disable_restart && signal_available);
    restart_sig = 1;
}

void
tcpserver()
{
    listen_socket = start_listen_socket("0.0.0.0", tcp_port_no);

    if (!server_runonce) {
	if (detach_tcp_server)
	    fork_and_detach();

	if (!disable_restart) {
	    if (signal(SIGHUP, handle_signal) == SIG_ERR) {
		perror("signal(SIGHUP,)");
		exit(1);
	    }
	    signal_available = 1;
	}

	if (signal(SIGCHLD, handle_signal) == SIG_ERR)
	    perror("signal(SIGCHLD,)");
	if (signal(SIGALRM, handle_signal) == SIG_ERR)
	    perror("signal(SIGALRM,)");
    }

    if (!log_to_stderr) {
	logger_process();
	if (detach_tcp_server)
	    fprintf(stderr, "Accepting connections on port %d.\n", tcp_port_no);
    }

    while(1)
    {
	int max_sock = listen_socket;
	fd_set read_fds;
	fd_set write_fds;
	fd_set except_fds;
	struct timeval timeout;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&except_fds);

	FD_SET(listen_socket, &read_fds);
	FD_SET(listen_socket, &except_fds);
	// ... stdin?

	timeout.tv_sec = 1; timeout.tv_usec = 0;
	int rv = select(max_sock+1, &read_fds, &write_fds, &except_fds, &timeout);
	if (rv < 0 && errno != EINTR) { perror("select()"); exit(1); }

	if (rv > 0)
	{
	    if (FD_ISSET(listen_socket, &read_fds))
	    {
		int socket = accept_new_connection();

		int pid = 0;
		if (!server_runonce)
		    pid = E(fork(), "Forking failure");
		if (pid == 0)
		{
		    if (!detach_tcp_server)
			setsid();
		    (void)signal(SIGHUP, SIG_DFL);
		    (void)signal(SIGCHLD, SIG_DFL);
		    (void)signal(SIGALRM, SIG_DFL);
		    signal_available = 0;
		    E(close(listen_socket), "close(listen)");
		    line_ofd = 1; line_ifd = 0;
		    E(dup2(socket, line_ifd), "dup2(S,0)");
		    E(dup2(socket, line_ofd), "dup2(S,1)");
		    // It's connected to stdin/out so don't need the socket fd
		    E(close(socket), "close(socket)");
		    return; // Only the child returns.
		}
		close(socket);
	    }
	    if (FD_ISSET(listen_socket, &except_fds))
		break;
	}

	if (restart_sig)
	    do_restart();

	cleanup_zombies();

	if (enable_heartbeat_poll)
	    send_heartbeat_poll();

	if (enable_backups)
	    start_backup_process();

	alarm_sig = 0;
    }
    /*NOTREACHED*/
}

LOCAL void
fork_and_detach()
{
    int pid = E(fork(), "Forking failure, detach");
    if (pid != 0) exit(0); // Detached process

    setsid();	// New session group
}

void
logger_process()
{
    // Logging pipe
    int pipefd[2];
    E(pipe(pipefd), "cannot create pipe");

    int pid2 = E(fork(), "Forking failure, logger");

    if (pid2 != 0) {
	// Parent process
	E(dup2(pipefd[1], 1), "dup2(logger,1)");
	E(dup2(pipefd[1], 2), "dup2(logger,2)");
	close(pipefd[0]);
	close(pipefd[1]);

	// Detach stdin.
	int nullfd = E(open("/dev/null", O_RDWR), "open(null)");
	E(dup2(nullfd, 0), "dup2(nullfd,0)");
	close(nullfd);

	// Return parent.
	return;
    }

    // Logger
    if (listen_socket>=0)
	E(close(listen_socket), "close(listen)");
    E(close(pipefd[1]), "close(pipe)");

    // Detach logger from everything.
    int nullfd = E(open("/dev/null", O_RDWR), "open(null)");
    E(dup2(nullfd, 0), "dup2(nullfd,0)");
    E(dup2(nullfd, 1), "dup2(nullfd,1)");
    E(dup2(nullfd, 2), "dup2(nullfd,2)");
    close(nullfd);

    memset(proc_args_mem, 0, proc_args_len);
    snprintf(proc_args_mem, proc_args_len, "MCCHost logger");

    FILE * ilog = fdopen(pipefd[0], "r");
    char logbuf[BUFSIZ];
    while(fgets(logbuf, sizeof(logbuf), ilog)) {
	char *p = logbuf+strlen(logbuf);
	while(p>logbuf && p[-1] == '\n') {p--; p[0] = 0; }
	fprintf_logfile("| %s", logbuf);
    }

    if (ferror(ilog))
	exit(errno);
    exit(0);
}

LOCAL void
do_restart()
{
    (void)signal(SIGHUP, SIG_DFL);
    (void)signal(SIGCHLD, SIG_DFL);
    (void)signal(SIGALRM, SIG_DFL);
    signal_available = 0;
    close(listen_socket);
    close_logfile();

    fprintf(stderr, "Restarting %s ...\n", program_args[0]);
    execvp(program_args[0], program_args);
    perror(program_args[0]);
    exit(126);
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

    if (server_runonce)
	fprintf(stderr, "Waiting for connection on port %d.\n", (int)port);
    else
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

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4_str, INET_ADDRSTRLEN);
    client_ipv4_port = client_addr.sin_port;

    if (server_runonce)
	fprintf(stderr, "Connected, closing listen socket.\n");
    else
	fprintf(stderr, "Incoming connection from %s:%d.\n", client_ipv4_str, client_addr.sin_port);

    client_ipv4_localhost = !strcmp("127.0.0.1", client_ipv4_str);

    return new_client_sock;
}

LOCAL void
cleanup_zombies()
{
    // Clean up any zombies.
    int status = 0, pid;
    child_sig = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) != 0)
    {
	if (pid < 0) {
	    if (errno == ECHILD || errno == EINTR)
		break;
	    perror("waitpid()");
	    exit(1);
	}
	// No complaints on clean exit
	if (WIFEXITED(status)) {
	    if (WEXITSTATUS(status))
		fprintf(stderr, "Process %d had exit status %d\n",
		    pid, WEXITSTATUS(status));
	    continue;
	}
	if (WIFSIGNALED(status)) {
	    fprintf(stderr, "Process %d was killed by signal %s (%d)%s\n",
		pid,
		strsignal(WTERMSIG(status)),
		WTERMSIG(status),
		WCOREDUMP(status)?" (core dumped)":"");

	    delete_session_id(pid);

	    // If there was a core dump try to spit out something.
	    if (WCOREDUMP(status)) {
		char buf[1024];

		// Are the programs and core file likely okay?
		int pgmok = 0;
		if (program_name[0] == '/' || strchr(program_name, '/') == 0)
		    pgmok = 1;
		if (pgmok && access("core", F_OK) != 0)
		    pgmok = 0;
		if (pgmok && access("/usr/bin/gdb", X_OK) != 0)
		    pgmok = 0;

		if (pgmok && sizeof(buf) > snprintf(buf, sizeof(buf),
		    "/usr/bin/gdb -batch -ex 'backtrace full' -c core '%s'", program_name))
		    system(buf);
		else
		    fprintf(stderr, "Skipped running /usr/bin/gdb; checking exe and core files failed.\n");
	    }
	}
    }
}

LOCAL void
send_heartbeat_poll()
{
    time_t now;
    time(&now);
    if (alarm_sig == 0 && last_heartbeat != 0) {
	if ((now-last_heartbeat) < 45)
	    return;
    }
    last_heartbeat = now;

    char cmdbuf[4096];
    char namebuf[256];
    char softwarebuf[256];
    int valid_salt = (*server_secret != 0 && *server_secret != '-');

    // {"errors":[["Only first 256 unicode codepoints may be used in server names."]],"response":"","status":"fail"}
    //
    // This is a LIE.
    // The codepoints are in cp437, but are encode using the UTF8 bit patterns.
    //
    // Printable ASCII are fine.
    //
    // Control characters (☺☻♥♦♣♠•◘○◙♂♀♪♫☼▶◀↕‼¶§▬↨↑↓→←∟↔▲▼) appear to fail
    // The website doesn't error, but the ClassiCube doesn't show it.
    //
    // High controls (ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ) are baaad
    // --> Oopsie Woopsie! Uwu We made a fucky wucky!! A wittle fucko boingo!
    // --> The code monkeys at our headquarters are working VEWY HAWD to fix this!
    //
    // Characters above 160 appear to work "correctly".

    snprintf(cmdbuf, sizeof(cmdbuf),
	"%s?%s%d&%s%d&%s%s&%s%d&%s%d&%s%s&%s%s&%s%s&%s%s",
	heartbeat_url,
	"port=",tcp_port_no,
	"max=",255,
	"public=",server_private?"False":"True",
	"version=",7,
	"users=",current_user_count(),
	"salt=",valid_salt?server_secret:"0000000000000000",
	"name=",ccnet_cp437_quoteurl(server_name, namebuf, sizeof(namebuf)),
	"software=",ccnet_cp437_quoteurl(server_software, softwarebuf, sizeof(softwarebuf)),
	"web=","False"
	);

    if (fork() == 0) {
	close(listen_socket);
	// SHUT UP CURL!!
	E(execlp("curl", "curl", "-s", "-S", "-o", "log/curl_resp.txt", cmdbuf, (char*)0), "exec of curl failed");
	// Note: Returned string is web client URL, but the last
	//       path part can be used in the standard client.
	exit(0);
    }
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
	else if ((*s &0x7F) < ' ') // Control characters.
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

void
start_backup_process()
{
    time_t now;
    time(&now);
    if (alarm_sig == 0 && last_backup != 0) {
        if ((now-last_backup) < 300)
            return;
    }
    last_backup = now;

    if (E(fork(),"fork() for backup") != 0) return;

    scan_and_save_levels();
    exit(0);
}
