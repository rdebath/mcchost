
#include <sys/socket.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <signal.h>

#include "tcpserver.h"

#if INTERFACE
#include <netinet/in.h>
#include <arpa/inet.h>

// Max of INET6_ADDRSTRLEN and INET_ADDRSTRLEN plus the port number
#define IP_ADDRSTRLEN	64
#endif

#ifdef WCOREDUMP
#define WCOREDUMP_X(X) WCOREDUMP(X)
#else
#define WCOREDUMP_X(X) 0
#endif

#define FREQUENT_CHECK	15

int listen_socket = -1;
int logger_pid = 0;
int heartbeat_pid = 0;
int backup_pid = 0;
time_t last_heartbeat = 0;

char client_ipv4_str[IP_ADDRSTRLEN];
int client_ipv4_port = 0;
uint32_t client_ipv4_addr = 0;
int disable_restart = 0;

volatile int restart_sig = 0;
volatile int child_sig = 0;
volatile int alarm_sig = 0;
volatile int term_sig = 0;

static int signal_available = 0;
int restart_on_unload = 0, restart_needed = 0;
static time_t last_execheck = 0;
static time_t last_inicheck = 0;
static int trigger_backup = 0, trigger_unload = 0;
static int exe_generation = -1;

pid_t alarm_handler_pid = 0;

// Addresses to be considered as managment interface, in adddion to localhost
char localnet_cidr[64];
uint32_t localnet_addr = 0x7f000001, localnet_mask = ~0;

static inline int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }
static inline int W(int n, char * err) { if (n == -1) perror(err); return n; }

void
handle_signal(int signo)
{
    if (signo == SIGHUP) restart_sig = 1;
    if (signo == SIGCHLD) child_sig = 1;
    if (signo == SIGALRM) alarm_sig = 1;
    if (signo == SIGTERM) term_sig = 1;
    if (signo == SIGINT) term_sig = 1;
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
    struct sockaddr_storage my_addr;
    socklen_t len = sizeof(my_addr);
    if (getsockname(sock_fd, (struct sockaddr *)&my_addr, &len) < 0) {
	return -1;
    }

    if (my_addr.ss_family == AF_INET)
	return ntohs(((struct sockaddr_in*)&my_addr)->sin_port);
#ifdef AF_INET6
    if (my_addr.ss_family == AF_INET6)
	return ntohs(((struct sockaddr_in6*)&my_addr)->sin6_port);
#endif

    return -1;
}

void
tcpserver()
{
    int early_logger = 0;

    if (!log_to_stderr && !server_runonce) {
	if (isatty(2)) {
	    // Don't clutter the console .
	    char * nm = ttyname(2);
	    if (!nm || strcmp(nm, "/dev/console") == 0)
		early_logger = 1;
	} else
	    early_logger = 1; // No pipe chats.
    }

    if (early_logger)
	logger_process();

    listen_socket = start_listen_socket("", tcp_port_no);

    if (tcp_port_no == 0)
	tcp_port_no = read_sock_port_no(listen_socket);

    if (!log_to_stderr && !server_runonce && !early_logger && isatty(2))
	fprintf(stderr, "Accepting connections on port %d.\n", tcp_port_no);

    if (!server_runonce) {
	if (detach_tcp_server)
	    fork_and_detach();

	if (!log_to_stderr && !early_logger)
	    logger_process();

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
	if (signal(SIGINT, handle_signal) == SIG_ERR)
	    perror("signal(SIGINT,)");

	alarm_handler_pid = getpid(); // For the children to signal us.
    } else {
	if (!log_to_stderr)
	    logger_process();
    }

    if (server_runonce)
	printlog("Waiting for a connection on port %d.", tcp_port_no);
    else
	printlog("Accepting connections on port %d (pid:%d).", tcp_port_no, getpid());

    proctitle("%s port %d", SWNAME, tcp_port_no);

    convert_localnet_cidr();

    restart_sig = 0;
    last_execheck = time(0);

    create_cmd_queue();
    stop_cmd_queue();

    printf_chat("@Listener process started for port %d", tcp_port_no);
    stop_chat_queue();
    close_logfile();

    if (server->no_unload_main)
	auto_load_main(0);

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
		static int concount = 0;
		int ok = 0, socket = accept_new_connection();
		if (!allow_connection()) {
		    concount = (concount+1)%100;
		    if (concount == 1)
			printlog("Too many connections from %s", client_ipv4_str);
		} else {
		    concount = 0;
		    ok = 1;
		    if (server_runonce)
			printlog("Connected, closing listen socket.");
		    else
			printlog("Incoming connection from %s%s on port %d.",
			    client_trusted?"trusted host ":"",
			    client_ipv4_str, tcp_port_no);

		    close_userdb();
		    close_logfile();
		    stop_client_list();
		    stop_system_conf(); open_system_conf(); // Cleanup?

		    int pid = 0;
		    if (!server_runonce)
			pid = W(fork(), "Failed to fork() worker");

		    if (pid == 0)
		    {
			if (!detach_tcp_server && !server_runonce)
			    setsid();
#if defined(HAS_CORELIMIT) && defined(WCOREDUMP)
			if (access("/usr/bin/gdb", X_OK) == 0)
			    enable_coredump();
#endif
			(void)signal(SIGHUP, SIG_DFL);
			(void)signal(SIGCHLD, SIG_DFL);
			(void)signal(SIGALRM, SIG_DFL);
			(void)signal(SIGTERM, SIG_DFL);
			(void)signal(SIGINT, SIG_DFL);
			signal_available = 0;
			E(close(listen_socket), "close(listen)");
			listen_socket = -1;
			line_ofd = 1; line_ifd = 0;
			E(dup2(socket, line_ifd), "dup2(S,0)");
			E(dup2(socket, line_ofd), "dup2(S,1)");
			// It's connected to stdin/out so don't need the socket fd
			E(close(socket), "close(socket)");
			return; // Only the child returns.
		    }
		}
		close(socket);

		if (ok && ini_settings->void_for_login && server->loaded_levels == 0)
		    auto_load_main(1);
	    }
	    if (FD_ISSET(listen_socket, &except_fds))
		break;
	}

	check_new_exe();

	check_system_ini_time();

	if (restart_sig)
	    do_restart();

	cleanup_zombies();

	if (enable_heartbeat_poll)
	    send_heartbeat_poll();

	start_backup_process();

	alarm_sig = 0;
    }

    if (listen_socket>=0) { close(listen_socket); listen_socket = -1; }

    if (enable_heartbeat_poll && !ini_settings->private) {
	printlog("Shutting down service");
	send_heartbeat_poll();
    } else
	printlog("Terminating service");

    trigger_backup = 1;
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
    lock_stop(system_lock);
    close_logfile();

    // Logging pipe
    int pipefd[2];
    E(pipe(pipefd), "cannot create pipe");

    int pid2 = E(fork(), "Forking failure, logger");

    if (pid2 != 0) {
	// Parent process
	logger_pid = pid2;

	if (start_tcp_server || start_heartbeat_task || start_backup_task) {
	    E(dup2(pipefd[1], 1), "dup2(logger,1)");
	    E(dup2(pipefd[1], 2), "dup2(logger,2)");
	    close(pipefd[0]);
	    close(pipefd[1]);

	    // Detach stdin.
	    int nullfd = E(open("/dev/null", O_RDWR), "open(null)");
	    E(dup2(nullfd, 0), "dup2(nullfd,0)");
	    close(nullfd);

	    lock_start(system_lock);
	    return;
	}

	// Stdin/out could be line fd.
	E(dup2(pipefd[1], 2), "dup2(logger,2)");
	close(pipefd[0]);
	close(pipefd[1]);

	lock_start(system_lock);
	return;
    }

    if (listen_socket>=0) { close(listen_socket); listen_socket = -1; }

    // No longer needed -- we have one job.
    stop_system_conf();
    stop_client_list();

    proctitle("MCCHost logger");

    // Logger
    E(close(pipefd[1]), "close(pipe)");
    if (pipefd[0] != 0) {
	E(dup2(pipefd[0], 0), "dup2(pipefd[0],0)");
	close(pipefd[0]);
    }

    // Detach logger from output too.
    int nullfd = E(open("/dev/null", O_RDWR), "open(null)");
    E(dup2(nullfd, 1), "dup2(nullfd,1)");
    E(dup2(nullfd, 2), "dup2(nullfd,2)");
    close(nullfd);

    char logbuf[BUFSIZ];
    while(fgets(logbuf, sizeof(logbuf), stdin)) {
	char *p = logbuf+strlen(logbuf);
	while(p>logbuf && p[-1] == '\n') {p--; p[0] = 0; }
	printlog("| %s", logbuf);
    }

    if (ferror(stdin))
	exit(errno);
    exit(0);
}

void
check_stdio_fd()
{
    // Make sure we have a stdin, stdout and stderr.
    int nullfd = E(open("/dev/null", O_RDWR), "open(null)");
    if (nullfd <= 2) nullfd = E(open("/dev/null", O_RDWR), "open(null)");
    if (nullfd <= 2) nullfd = E(open("/dev/null", O_RDWR), "open(null)");
    if (nullfd > 2) close(nullfd);
}

LOCAL void
do_restart()
{
    printlog("Restarting %s (pid:%d) ...", program_args[0], getpid());

    close_userdb();
    signal_available = 0;
    if (listen_socket>=0) close(listen_socket);
    listen_socket = 0;
    restart_sig = 0;
    restart_needed = 0;
    close_logfile();

    // This might be something we want to do ...
    //for(int i=3; i<100; i++) (void)close(i);

    // Exec clears function call configs, make sure we don't get killed.
    (void)signal(SIGHUP, SIG_IGN);
    (void)signal(SIGCHLD, SIG_IGN);
    (void)signal(SIGALRM, SIG_IGN);
    (void)signal(SIGTERM, SIG_DFL); // Or do.
    (void)signal(SIGINT, SIG_DFL); // Or do.

    // Note execvp never returns as it starts the shell on error.
    if (program_args[0][0] == '/')
	execv(program_args[0], program_args);
    else
	execvp(program_args[0], program_args);
    perror(program_args[0]);

    printlog("Restart failed, attempting to continue ...");

    listen_socket = start_listen_socket("", tcp_port_no);
    signal_available = 1;

    if (signal(SIGHUP, handle_signal) == SIG_ERR)
	perror("signal(SIGHUP,)");
    if (signal(SIGCHLD, handle_signal) == SIG_ERR)
	perror("signal(SIGCHLD,)");
    if (signal(SIGALRM, handle_signal) == SIG_ERR)
	perror("signal(SIGALRM,)");
    if (signal(SIGTERM, handle_signal) == SIG_ERR)
	perror("signal(SIGTERM,)");
    if (signal(SIGINT, handle_signal) == SIG_ERR)
	perror("signal(SIGINT,)");
}

/* Start listening socket listen_sock. */
LOCAL int
start_listen_socket(char * listen_addr, int port)
{
    int listen_sock;
    // Obtain a file descriptor for our "listening" socket.
#if defined(AF_INET6) && !defined(IPV4_ONLY)
    listen_sock = E(socket(AF_INET6, SOCK_STREAM, 0), "socket");

    struct sockaddr_in6 my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin6_family = AF_INET6;
    if (listen_addr && *listen_addr)
	E(inet_pton (my_addr.sin6_family, listen_addr, my_addr.sin6_addr.s6_addr), "inet_pton");
    else
	my_addr.sin6_addr = (struct in6_addr)IN6ADDR_ANY_INIT;
    my_addr.sin6_port = htons(port);

    // For FreeBSD '/etc/sysctl.conf' must have the line "net.inet6.ip6.v6only=0"
    // Or this must be set to allow IPv4 too.
    int flg = 0;
    W(setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &flg, sizeof(flg)), "setsockopt ipv6");

#else
    listen_sock = E(socket(AF_INET, SOCK_STREAM, 0), "socket");

    if (!listen_addr || !*listen_addr) listen_addr = "0.0.0.0";
    struct sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(listen_addr);
    my_addr.sin_port = htons(port);
#endif

    int reuse = 1;
    W(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)), "setsockopt");

    // Allow this to retry because of possible issues on restart.
    int c = 0;
    for(;;) {
	int n = bind(listen_sock, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if (n != -1) break;
	if (errno != EADDRINUSE || ++c == 6) {
	    perror("bind");
	    exit(1);
	}
	if (c == 1) perror("bind listen retrying");
	msleep(500);
    }

    // start accept client connections (queue 10)
    E(listen(listen_sock, 10), "listen");

    return listen_sock;
}

LOCAL int
accept_new_connection()
{
    struct sockaddr_storage client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_len = sizeof(client_addr);
    int new_client_sock;
    new_client_sock = E(accept(listen_socket, (struct sockaddr *)&client_addr, &client_len), "accept()");

    extract_sockaddr((struct sockaddr *)&client_addr);

    return new_client_sock;
}

void
extract_sockaddr(void * addr)
{
    int ip_is_local = 0;
    client_ipv4_port = 0;
    client_ipv4_addr = 0xFFFFFFFF;

    struct sockaddr_in *s4 = addr;
    struct sockaddr_in6 *s6 = addr;

    strcpy(client_ipv4_str, "socket");

    if (s4->sin_family == AF_INET)
    {
	inet_ntop(AF_INET, &s4->sin_addr, client_ipv4_str, sizeof(client_ipv4_str));
	client_ipv4_port = ntohs(s4->sin_port);
	sprintf(client_ipv4_str+strlen(client_ipv4_str), ":%d", client_ipv4_port);

	uint32_t caddr = ntohl(s4->sin_addr.s_addr);
	client_ipv4_addr = caddr;
    }

#ifdef AF_INET6
    if (s6->sin6_family == AF_INET6)
    {
	char client_ipv6_str[IP_ADDRSTRLEN];
	inet_ntop(AF_INET6, &s6->sin6_addr, client_ipv6_str, sizeof(client_ipv6_str));
	client_ipv4_port = ntohs(s6->sin6_port);
	saprintf(client_ipv4_str, "[%s]:%d", client_ipv6_str, client_ipv4_port);

	if (IN6_IS_ADDR_V4MAPPED(&s6->sin6_addr))
	    client_ipv4_addr = ntohl(((struct in_addr*)(s6->sin6_addr.s6_addr+12))->s_addr);
	else if (IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr))
	    client_ipv4_addr = 0x7F000001; // IPv6 Local -> IPv4
	if (client_ipv4_addr != 0xFFFFFFFF) {
	    uint32_t n = htonl(client_ipv4_addr);
	    inet_ntop(AF_INET, &n, client_ipv6_str, sizeof(client_ipv6_str));
	    saprintf(client_ipv4_str, "%s:%d", client_ipv6_str, client_ipv4_port);
	}
    }
#endif

    if ((client_ipv4_addr & ~0xFFFFFFU) == 0x7F000000) // Localhost (network)
	ip_is_local = 1;
    if ((client_ipv4_addr & localnet_mask) == (localnet_addr & localnet_mask))
	ip_is_local = 1;

    client_trusted = ip_is_local;
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
	extract_sockaddr((struct sockaddr *)&addr);
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
    int client_process_finished = 0;
    char * id = 0;

    while ((pid = waitpid(-1, &status, WNOHANG)) != 0)
    {
	if (pid < 0) {
	    if (errno == ECHILD || errno == EINTR)
		break;
	    perror("waitpid()");
	    exit(1);
	}

	if (pid == logger_pid) {
	    // Whup! that's our stderr! Try to respawn it.
	    printlog("Attempting to restart logger process");
	    logger_process();
	    id = " (logger)";
	} else
	if (pid == heartbeat_pid) {
	    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printlog("Heartbeat process %d failed", pid);
		last_heartbeat = time(0) - 30;
	    }
	    heartbeat_pid = 0;

	    log_heartbeat_response();
	    id = " (heartbeat)";
	} else
	if (pid == backup_pid) {
	    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		printlog("Backup and unload process %d failed", pid);
	    backup_pid = 0;
	    if (restart_on_unload) last_execheck = time(0) - 300;
	    id = " (saved)";
	} else {
	    client_process_finished++;
	    if (server->connected_sessions == 0)
		last_execheck = time(0) - FREQUENT_CHECK+1;
	    id = " (user)";
	}

	process_status_message(status, pid, id);

	if (client_process_finished)
	    trigger_unload = 1;
    }
}

// Run the timer tasks from the command line.
void
run_timer_tasks()
{
    logger_process();
    if (enable_heartbeat_poll && start_heartbeat_task)
	send_heartbeat_poll();

    if (start_backup_task)
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

	char * id = pid==heartbeat_pid?" (heartbeat)":pid==backup_pid?" (backup)":" ?";

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            process_status_message(status, pid, id);

	if (pid == heartbeat_pid) heartbeat_pid = 0;
	if (pid == backup_pid) backup_pid = 0;
    }
    exit(0);
}

void
start_backup_process()
{
    if (backup_pid != 0) return;

    if (shdat.client == 0) open_client_list();

    if (alarm_sig)
	shdat.client->generation++;

    if (alarm_sig == 0 && trigger_backup == 0 && start_backup_task == 0 && restart_on_unload == 0) {
	if (shdat.client->generation == shdat.client->cleanup_generation &&
	    server->loaded_levels == server->pinned_levels)
	    return;
    }

    time_t now = time(0);
    if (start_backup_task) trigger_backup = 1;
    if (alarm_sig) trigger_backup = 1;
    if (server->last_backup == 0) server->last_backup = now;

    if (trigger_backup == 0) {
	if ((now-server->last_backup) >= server->save_interval)
	    trigger_backup = 1;
	else {
	    if ((now-server->last_unload) >= FREQUENT_CHECK) {
		if (server->loaded_levels != server->pinned_levels)
		    trigger_unload = 1;
	    }
	}
    }

    if (!trigger_unload)
	trigger_unload = scan_levels();

    if (trigger_backup) {
	server->last_backup = now;
	server->last_unload = now;
    }
    if (trigger_unload) server->last_unload = now;

    if (!trigger_backup && !trigger_unload) return;

    lock_restart(system_lock);

    backup_pid = W(fork(),"Failed to fork() for backup");
    if (backup_pid != 0) {
	trigger_backup = trigger_unload = 0;
	return;
    }
    if (listen_socket>=0) { close(listen_socket); listen_socket = -1; }

    proctitle("MCCHost saver");

    if (trigger_unload)
	trigger_backup |= scan_and_save_levels(0);
    if (trigger_backup)
	scan_and_save_levels(1);

    msleep(1000);
    exit(0);
}

void
check_new_exe()
{
    if (exe_generation < 0)
	exe_generation = server->exe_generation;
    if (exe_generation != server->exe_generation)
	restart_sig = 1;

    if (!proc_self_exe_ok) return;

    if (server->magic != TY_MAGIC)
	restart_sig = 1;

    // Normally check FREQUENT_CHECK seconds
    time_t now;
    time(&now);
    if (alarm_sig == 0 && now-last_execheck <= FREQUENT_CHECK)
	return;
    last_execheck = now;

    // Is /proc/self/exe still okay?
    char buf[PATH_MAX] = "";
    int l = readlink("/proc/self/exe", buf, sizeof(buf)-1);
    if (l > 0) buf[l] = 0;

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
    else if (server->connected_sessions == 0) {
	restart_on_unload = 1;
	trigger_backup = 1;
    } else
	restart_needed = 1;

    unlock_fn(system_lock);
}

void
check_system_ini_time()
{
    // Normally check FREQUENT_CHECK seconds
    time_t now = time(0);
    if (now-last_inicheck <= FREQUENT_CHECK)
	return;
    last_inicheck = now;
    check_reload_server_ini();
}

void
auto_load_main(int user_load)
{
    int autoload_pid = fork();
    if (autoload_pid < 0) perror("fork() for load of main");
    if (autoload_pid != 0) return;

#if defined(HAS_CORELIMIT) && defined(WCOREDUMP)
    if (access("/usr/bin/gdb", X_OK) == 0)
	enable_coredump();
#endif

    if (listen_socket>=0) { close(listen_socket); listen_socket = -1; }

    proctitle("MCCHost load main");

    // If "user_load" the client usually loads the map first.
    if (user_load) msleep(200); else msleep(2000);

    open_client_list();

    // If not user_load we load the map to ensure it's ok and unload
    // it only if NoUnloadMain is unset.
    if (!user_load || current_user_count() > 0) {
	// Open level mmap files.
	char fixname[MAXLEVELNAMELEN*4];
	fix_fname(fixname, sizeof(fixname), main_level());
	if (!start_level(main_level(), 0)) exit(1);

	open_level_files(main_level(), 0, 0, fixname, 0);
    }

    stop_client_list();
    msleep(1000);

    exit(0);
}
