#include <signal.h>
#include <sys/wait.h>

#include "wait.h"

#ifdef WCOREDUMP
#define WCOREDUMP_X(X) WCOREDUMP(X)
#else
#define WCOREDUMP_X(X) 0
#endif

pid_t level_processor_pid = 0;

void
check_waitpid()
{
    int status = 0, pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) != 0)
    {
	if (pid < 0) {
	    if (errno != ECHILD && errno != EINTR)
		perror("check waitpid()");
	    break;
	}

	if (pid == level_loader_pid) level_loader_pid = 0;
	if (pid == level_processor_pid) level_processor_pid = 0;

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
	    process_status_message(status, pid, 0);
    }
}

void
process_status_message(int status, int pid, char * id)
{
    // If we're the listen process we may have logouts and should chat about them.
    int top_proc = (alarm_handler_pid == getpid());

    int died_badly = 0, unclean_disconnect = 0;
    char msgbuf[256], userid[64];
    *userid = *msgbuf = 0;
    id = id?id:"";

    stop_client_list();

    // No complaints on clean exit
    if (WIFEXITED(status)) {
	if (WEXITSTATUS(status)) {
	    printlog("Process %d%s had exit status %d",
		pid, id, WEXITSTATUS(status));
	    saprintf(msgbuf,
		"kicked by panic with exit status %d",
		WEXITSTATUS(status));
	}

	if (top_proc)
	    died_badly = delete_session_id(pid, userid, sizeof(userid));
    }
    if (WIFSIGNALED(status)) {
#if _POSIX_VERSION >= 200809L
	printlog("Process %d%s was killed by signal %s (%d)%s",
	    pid, id,
	    strsignal(WTERMSIG(status)),
	    WTERMSIG(status),
	    WCOREDUMP_X(status)?" (core dumped)":"");

	saprintf(msgbuf,
	    "kicked by signal %s (%d)%s",
	    strsignal(WTERMSIG(status)),
	    WTERMSIG(status),
	    WCOREDUMP_X(status)?" (core dumped)":"");
#else
	printlog("Process %d%s was killed by signal %d %s",
	    pid, id,
	    WTERMSIG(status),
	    WCOREDUMP_X(status)?" (core dumped)":"");

	saprintf(msgbuf,
	    "kicked by signal %d %s",
	    WTERMSIG(status),
	    WCOREDUMP_X(status)?" (core dumped)":"");
#endif

	if (top_proc)
	    died_badly = delete_session_id(pid, userid, sizeof(userid));

	if (WTERMSIG(status) == SIGPIPE) {
	    unclean_disconnect = died_badly;
	    died_badly = 0;
	}

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

    if (top_proc && *userid && *msgbuf) {
	if (died_badly) {
	    printf_chat("@&W- &7%s: &W%s", userid, msgbuf);
	    stop_chat_queue();
	}
	if (unclean_disconnect) {
	    printf_chat("@&W- &7%s &S%s", userid, "disconnected");
	    stop_chat_queue();
	}
    }
}
