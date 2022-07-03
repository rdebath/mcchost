
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <limits.h>

#include "tcpserver.h"

#if INTERFACE
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef WCOREDUMP
#define WCOREDUMP_X(X) WCOREDUMP(X)
#else
#define WCOREDUMP_X(X) 0
#endif

int start_tcp_server = 0;
int detach_tcp_server = 0;
int log_to_stderr = 0;
int tcp_port_no = 25565;
static int listen_socket = -1;
static int logger_pid = 0, heartbeat_pid = 0, backup_pid = 0;

int enable_heartbeat_poll = 1;
time_t last_heartbeat = 0;

char client_ipv4_str[INET_ADDRSTRLEN+10];
int client_ipv4_port = 0;
uint32_t client_ipv4_addr = 0;
int client_trusted = 0;
int disable_restart = 0;

static volatile int restart_sig = 0, child_sig = 0;
static volatile int alarm_sig = 0, term_sig = 0;
static int signal_available = 0;
int restart_on_unload = 0;

pid_t alarm_handler_pid = 0;

// Addresses to be considered as managment interface, in adddion to localhost
char localnet_cidr[64];
uint32_t localnet_addr = 0x7f000001, localnet_mask = ~0;

static inline int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

void
handle_signal(int signo)
{
    if (signo == SIGHUP) restart_sig = 1;
    if (signo == SIGCHLD) child_sig = 1;
    if (signo == SIGALRM) alarm_sig = 1;
    if (signo == SIGTERM) term_sig = 1;
}

void
dont_panic()
{
    assert(!disable_restart && signal_available);
    restart_sig = 1;
}

int
read_sock_port_no(int sock_fd)
{
    struct sockaddr_in my_addr;
    socklen_t len = sizeof(my_addr);
    if (getsockname(sock_fd, (struct sockaddr *)&my_addr, &len) < 0) {
	return -1;
    }

    if (my_addr.sin_family == AF_INET)
	return ntohs(my_addr.sin_port);
    if (my_addr.sin_family == AF_INET6)
	return ntohs(my_addr.sin_port);

    return -1;
}

void
tcpserver()
{
    listen_socket = start_listen_socket("0.0.0.0", tcp_port_no);

    if (tcp_port_no == 0)
	tcp_port_no = read_sock_port_no(listen_socket);

    if (isatty(2) && !log_to_stderr && !server_runonce)
	fprintf(stderr, "Accepting connections on port %d.\n", tcp_port_no);

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
	if (signal(SIGTERM, handle_signal) == SIG_ERR)
	    perror("signal(SIGTERM,)");

	alarm_handler_pid = getpid(); // For the children to signal us.
    }

    if (!log_to_stderr)
	logger_process();

    if (server_runonce)
	printlog("Waiting for a connection on port %d.", tcp_port_no);
    else
	printlog("Accepting connections on port %d (pid:%d).", tcp_port_no, getpid());

    memset(proc_args_mem, 0, proc_args_len);
    snprintf(proc_args_mem, proc_args_len, "%s port %d", server->software, tcp_port_no);

    convert_localnet_cidr();

    while(!term_sig)
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
#if defined(HAS_CORELIMIT) && defined(WCOREDUMP)
		    if (access("/usr/bin/gdb", X_OK) == 0)
			enable_coredump();
#endif
		    (void)signal(SIGHUP, SIG_DFL);
		    (void)signal(SIGCHLD, SIG_DFL);
		    (void)signal(SIGALRM, SIG_DFL);
		    (void)signal(SIGTERM, SIG_DFL);
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

	check_new_exe();

	if (restart_sig && backup_pid == 0)
	    do_restart();

	cleanup_zombies();

	if (enable_heartbeat_poll)
	    send_heartbeat_poll();

	start_backup_process();

	alarm_sig = 0;
    }

    if (enable_heartbeat_poll && !server->private) {
	printlog("Shutting down service");
	send_heartbeat_poll();
    } else
	printlog("Terminating service");

    alarm_sig = 1;
    start_backup_process();
    printlog("Exit process pid %d", getpid());
    exit(0);
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
	logger_pid = pid2;

	if (start_tcp_server || start_cron_task) {
	    E(dup2(pipefd[1], 1), "dup2(logger,1)");
	    E(dup2(pipefd[1], 2), "dup2(logger,2)");
	    close(pipefd[0]);
	    close(pipefd[1]);

	    // Detach stdin.
	    int nullfd = E(open("/dev/null", O_RDWR), "open(null)");
	    E(dup2(nullfd, 0), "dup2(nullfd,0)");
	    close(nullfd);
	    return;
	}

	// Stdin/out should be line fd.
	E(dup2(pipefd[1], 2), "dup2(logger,2)");
	close(pipefd[0]);
	close(pipefd[1]);

	return;
    }

    stop_system_conf(); // No longer needed.

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
	printlog("| %s", logbuf);
    }

    if (ferror(ilog))
	exit(errno);
    exit(0);
}

LOCAL void
do_restart()
{
    printlog("Restarting %s ...", program_args[0]);

    (void)signal(SIGHUP, SIG_DFL);
    (void)signal(SIGCHLD, SIG_DFL);
    (void)signal(SIGALRM, SIG_DFL);
    (void)signal(SIGTERM, SIG_DFL);
    signal_available = 0;
    if (listen_socket>=0) close(listen_socket);
    close_logfile();

    // Doing this properly means looking in /proc/self/fd or /dev/fd
    // for(int i=3; i<FD_SETSIZE; i++) close(i);
    // Don't use getrlimit(RLIMIT_NOFILE, &rlim); as this might be immense.

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

    return listen_sock;
}

LOCAL int
accept_new_connection()
{
    client_trusted = 0;

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);
    int new_client_sock;
    new_client_sock = E(accept(listen_socket, (struct sockaddr *)&client_addr, &client_len), "accept()");

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4_str, INET_ADDRSTRLEN);
    client_ipv4_port = ntohs(client_addr.sin_port);
    sprintf(client_ipv4_str+strlen(client_ipv4_str), ":%d", client_ipv4_port);

    if (client_addr.sin_family == AF_INET)
    {
	uint32_t caddr = ntohl(client_addr.sin_addr.s_addr);
	client_ipv4_addr = caddr;
	if ((caddr & ~0xFFFFFFU) == 0x7F000000) // Localhost (network)
	    client_trusted = 1;
	if ((caddr & localnet_mask) == (localnet_addr & localnet_mask))
	    client_trusted = 1;
    }

    if (server_runonce)
	printlog("Connected, closing listen socket.");
    else
	printlog("Incoming connection from %s%s on port %d.",
	    client_trusted?"trusted host ":"",
	    client_ipv4_str, tcp_port_no);

    return new_client_sock;
}

void
check_inetd_connection()
{
    union {
      struct sockaddr         sa;
      struct sockaddr_in      s4;
      struct sockaddr_in6     s6;
      struct sockaddr_storage ss;
    } addr;
    socklen_t client_len = sizeof(addr);

    strcpy(client_ipv4_str, "pipe");
    convert_localnet_cidr();

    if (line_ifd < 0) return;

    client_ipv4_port = -1;

    if (isatty(line_ifd))
	strcpy(client_ipv4_str, "tty");

    if (getpeername(line_ifd, (struct sockaddr *)&addr, &client_len) >= 0) {

	strcpy(client_ipv4_str, "socket");

	if (addr.s4.sin_family == AF_INET)
	{
	    inet_ntop(AF_INET, &addr.s4.sin_addr, client_ipv4_str, INET_ADDRSTRLEN);
	    client_ipv4_port = ntohs(addr.s4.sin_port);
	    sprintf(client_ipv4_str+strlen(client_ipv4_str), ":%d", client_ipv4_port);

	    uint32_t caddr = ntohl(addr.s4.sin_addr.s_addr);
	    client_ipv4_addr = caddr;
	    if ((caddr & ~0xFFFFFFU) == 0x7F000000) // Localhost (network)
		client_trusted = 1;
	    if ((caddr & localnet_mask) == (localnet_addr & localnet_mask))
		client_trusted = 1;
	}

	if (addr.s6.sin6_family == AF_INET6)
	{
	    inet_ntop(AF_INET6, &addr.s6.sin6_addr, client_ipv4_str, INET6_ADDRSTRLEN);
	    client_ipv4_port = ntohs(addr.s6.sin6_port);
	    sprintf(client_ipv4_str+strlen(client_ipv4_str), ":%d", client_ipv4_port);
	}
    }

    // In inetd mode our port is unknown
    tcp_port_no = read_sock_port_no(line_ifd);

    if (tcp_port_no > 0)
	printlog("Inetd connection from %s%s on port %d.",
	    client_trusted?"trusted host ":"",
	    client_ipv4_str, tcp_port_no);
    else
	printlog("%sonnection from %s.",
	    client_trusted?"Trusted c":"C", client_ipv4_str);
}

void
convert_localnet_cidr()
{
    localnet_addr = 0x7f000001; localnet_mask = ~0; // 127.0.0.1

    if (!*localnet_cidr) return;
    char * class = strchr(localnet_cidr, '/');
    if (class) {
	int netsize = atoi(class+1);
	if ((netsize > 0 || strcmp(class+1, "0") == 0) && netsize < 32) {
	    if (netsize == 0)
		localnet_mask = 0;
	    else
		localnet_mask = ~((1U<<(32-netsize))-1);
	}
	*class = 0;
    }

    struct in_addr ipaddr;
    if (inet_pton(AF_INET, localnet_cidr, &ipaddr) == 1) {
	localnet_addr = ntohl(ipaddr.s_addr);
    } else
	perror("Cannot convert Localnet address");

    if (class) *class = '/';
}

LOCAL void
cleanup_zombies()
{
    // Clean up any zombies.
    int status = 0, pid;
    child_sig = 0;
    char msgbuf[256];
    char userid[64];
    int died_badly = 0, client_process_finished = 0;

    while ((pid = waitpid(-1, &status, WNOHANG)) != 0)
    {
	if (pid < 0) {
	    if (errno == ECHILD || errno == EINTR)
		break;
	    perror("waitpid()");
	    exit(1);
	}
	*userid = *msgbuf = 0;

	if (pid == logger_pid) {
	    // Whup! that's our stderr! Try to respawn it.
	    printlog("Attempting to restart logger process");
	    logger_process();
	} else
	if (pid == heartbeat_pid) {
	    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printlog("Heartbeat process %d failed", pid);
		last_heartbeat = time(0) - 30;
	    }
	    heartbeat_pid = 0;
	} else
	if (pid == backup_pid) {
	    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		printlog("Backup and unload process %d failed", pid);
	    backup_pid = 0;
	} else
	    client_process_finished++;

	// No complaints on clean exit
	if (WIFEXITED(status)) {
	    if (WEXITSTATUS(status)) {
		printlog("Process %d had exit status %d",
		    pid, WEXITSTATUS(status));
		snprintf(msgbuf, sizeof(msgbuf),
		    "kicked by panic with exit status %d",
		    WEXITSTATUS(status));
	    }

	    died_badly = delete_session_id(pid, userid, sizeof(userid));
	}
	if (WIFSIGNALED(status)) {
#if _POSIX_C_SOURCE >= 200809L
	    printlog("Process %d was killed by signal %s (%d)%s",
		pid,
		strsignal(WTERMSIG(status)),
		WTERMSIG(status),
		WCOREDUMP_X(status)?" (core dumped)":"");

	    snprintf(msgbuf, sizeof(msgbuf),
		"kicked by signal %s (%d)%s",
		strsignal(WTERMSIG(status)),
		WTERMSIG(status),
		WCOREDUMP_X(status)?" (core dumped)":"");
#else
	    printlog("Process %d was killed by signal %d %s",
		pid,
		WTERMSIG(status),
		WCOREDUMP_X(status)?" (core dumped)":"");

	    snprintf(msgbuf, sizeof(msgbuf),
		"kicked by signal %d %s",
		WTERMSIG(status),
		WCOREDUMP_X(status)?" (core dumped)":"");
#endif

	    died_badly = delete_session_id(pid, userid, sizeof(userid));

	    // If there was a core dump try to spit out something.
	    if (WCOREDUMP_X(status)) {
		char buf[1024];

		// Are the programs and core file likely okay?
		int pgmok = 0;
		if (program_args[0][0] == '/' || strchr(program_args[0], '/') == 0)
		    pgmok = 1;
		if (pgmok && access("core", F_OK) != 0)
		    pgmok = 0;
		if (pgmok && access("/usr/bin/gdb", X_OK) != 0)
		    pgmok = 0;

		if (pgmok && sizeof(buf) > snprintf(buf, sizeof(buf),
		    "/usr/bin/gdb -batch -ex 'backtrace full' -c core '%s'", program_args[0]))
		    system(buf);
		else
		    printlog("Skipped running /usr/bin/gdb; checking exe and core files failed.");
	    }
	}

	if (died_badly && *userid && *msgbuf)
	{
	    printf_chat("@&W- &7%s: &W%s", userid, msgbuf);
	    stop_chat_queue();
	}

	if (client_process_finished) //
	    alarm_sig = 1;
    }
}

LOCAL void
send_heartbeat_poll()
{
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

    if (heartbeat_pid) {
	heartbeat_pid = 0;
	return;
    }

    char cmdbuf[4096];
    char namebuf[256];
    char softwarebuf[256];
    int valid_salt = (*server->secret != 0 && *server->secret != '-');

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
    // Characters above 160 appear to work "correctly".
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
	"salt=",valid_salt?server->secret:"0000000000000000",
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
	// -f   Yes, and error meesages from the web server.
	E(execlp("curl", "curl",
	    "-s", "-S", "-f",
	    "-o", logbuf,
	    "-D", dumpbuf,
	    cmdbuf,
	    (char*)0), "exec of curl failed");

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

LOCAL char *
ccnet_cp437_quoteurl(volatile char *s, char *dest, int len)
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

// Run the timer tasks from the command line.
void
run_timer_tasks()
{
    logger_process();
    if (enable_heartbeat_poll)
	send_heartbeat_poll();

    start_backup_process();

    // If we are being called from Systemd it has the nasty habit of killing
    // All our processes when we return. So we MUST wait for all our children.

    int stderr_open = 1;
    while(1) {
	// printlog("Waiting for %d, %d, %d", backup_pid, heartbeat_pid, logger_pid);

	// Close the pipe to the logger
	if (stderr_open && backup_pid <= 0 && heartbeat_pid <= 0) {
	    close(1); close(2);
	    // And wait for it to die.
	    stderr_open = 0;
	}

	int status = 0;
	pid_t pid = waitpid(-1, &status, 0);
	if (pid == logger_pid || pid == -1) break;

	char * id = pid==heartbeat_pid?"(heartbeat)":pid==backup_pid?"(backup)":"?";

	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
	    printlog("Process %d %s exited with status %d",
		pid, id, WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
	    printlog("Process %d %s was killed by signal %d%s",
		pid, id, WTERMSIG(status), WCOREDUMP_X(status)?" (core dumped)":"");

	if (pid == heartbeat_pid) heartbeat_pid = 0;
	if (pid == backup_pid) backup_pid = 0;
    }
    exit(0);
}

void
start_backup_process()
{
    time_t now;
    time(&now);
    if (alarm_sig == 0 && server->last_backup != 0) {
        if ((now-server->last_backup) < server->save_interval) {
	    if ((now-server->last_unload) >= 15) {
		scan_and_save_levels(1);
		server->last_unload = now;
	    }
            return;
	}
    }
    server->last_backup = now;
    server->last_unload = now;

    backup_pid = E(fork(),"fork() for backup");
    if (backup_pid != 0) return;

    if (listen_socket>=0) close(listen_socket);

    scan_and_save_levels(0);
    exit(0);
}

void
check_new_exe()
{
static time_t last_execheck = 0;

    if (!proc_self_exe_ok) return;

    // Normally check 30 seconds
    time_t now;
    time(&now);
    if (alarm_sig == 0 && now-last_execheck <= 30)
	return;
    last_execheck = now;

    // Is /proc/self/exe still okay?
    char buf[PATH_MAX*2];
    int l = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;

    // /proc/self/exe gives something runnable.
    if (l > 0 && access(buf, X_OK) == 0) {
	// Yup
	return;
    }

    // The link has broken, take that as a signal to restart.
    // BUT only if nobody is logged on.

    // Though, don't want to do it if our exe has gone.
    if (access(proc_self_exe, X_OK) != 0)
	return;

    open_client_list();
    if (!shdat.client) return;
    lock_fn(system_lock);

    if (server->loaded_levels == 0)
	restart_sig = 1;
    else
	restart_on_unload = 1;

    unlock_fn(system_lock);
    stop_client_list();
}
