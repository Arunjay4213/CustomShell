#include "exec.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "shell.h"
#include "util.h"

/* Turns a wait(2) status into the value a shell reports in $?. */
static int status_to_exit(int status)
{
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

/* Replaces the current process with cmd. Never returns. */
static void child_exec(command *cmd)
{
    execvp(cmd->argv[0], cmd->argv);

    if (errno == ENOENT) {
        shell_error("%s: command not found", cmd->argv[0]);
        _exit(127);
    }
    shell_error("%s: %s", cmd->argv[0], strerror(errno));
    _exit(126);
}

static int wait_for(pid_t pid)
{
    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            shell_error("waitpid: %s", strerror(errno));
            return 1;
        }
    }
    return status_to_exit(status);
}

int exec_pipeline(pipeline *p, const char *cmdline)
{
    command *cmd;
    builtin_fn fn;
    pid_t pid;

    (void)cmdline;

    if (p->ncmds == 0) {
        return shell.last_status;
    }
    if (p->ncmds > 1) {
        shell_error("pipelines are not supported yet");
        return 1;
    }

    cmd = &p->cmds[0];
    fn = builtin_lookup(cmd->argv[0]);
    if (fn != NULL) {
        return fn(cmd);
    }

    /* Flush before forking so buffered output is not written twice. */
    fflush(NULL);

    pid = fork();
    if (pid < 0) {
        shell_error("fork: %s", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        child_exec(cmd);
    }
    return wait_for(pid);
}
