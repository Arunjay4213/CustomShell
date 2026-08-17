#include "exec.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "shell.h"
#include "util.h"

#define FILE_MODE 0666

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

/* Opens path and moves the descriptor onto target_fd. */
static int redirect(const char *path, int flags, int target_fd)
{
    int fd = open(path, flags, FILE_MODE);

    if (fd < 0) {
        shell_error("%s: %s", path, strerror(errno));
        return -1;
    }
    if (fd != target_fd) {
        if (dup2(fd, target_fd) < 0) {
            shell_error("%s: %s", path, strerror(errno));
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

/* Applies the redirections of one command to the current descriptors. They are
   applied after any pipe wiring, so an explicit file always wins over a pipe. */
static int apply_redirections(const command *cmd)
{
    if (cmd->in_file != NULL &&
        redirect(cmd->in_file, O_RDONLY, STDIN_FILENO) != 0) {
        return -1;
    }
    if (cmd->out_file != NULL &&
        redirect(cmd->out_file,
                 O_WRONLY | O_CREAT | (cmd->out_append ? O_APPEND : O_TRUNC),
                 STDOUT_FILENO) != 0) {
        return -1;
    }
    if (cmd->err_file != NULL &&
        redirect(cmd->err_file,
                 O_WRONLY | O_CREAT | (cmd->err_append ? O_APPEND : O_TRUNC),
                 STDERR_FILENO) != 0) {
        return -1;
    }
    return 0;
}

/* Runs a builtin in the shell itself so that effects such as cd or export
   survive. Redirections are applied to saved copies of the standard
   descriptors and undone afterwards. */
static int run_builtin_in_shell(command *cmd, builtin_fn fn)
{
    int saved[3] = { -1, -1, -1 };
    int redirected = cmd->in_file != NULL || cmd->out_file != NULL ||
                     cmd->err_file != NULL;
    int status;
    int i;

    if (redirected) {
        for (i = 0; i < 3; i++) {
            saved[i] = dup(i);
        }
        if (apply_redirections(cmd) != 0) {
            status = 1;
            goto restore;
        }
    }

    status = fn(cmd);

restore:
    if (redirected) {
        fflush(stdout);
        for (i = 0; i < 3; i++) {
            if (saved[i] >= 0) {
                dup2(saved[i], i);
                close(saved[i]);
            }
        }
    }
    return status;
}

/* Runs one stage of a pipeline in the child process. Never returns. */
static void child_run(command *cmd)
{
    builtin_fn fn;

    if (apply_redirections(cmd) != 0) {
        _exit(1);
    }

    fn = builtin_lookup(cmd->argv[0]);
    if (fn != NULL) {
        int status = fn(cmd);
        fflush(NULL);
        _exit(status);
    }

    execvp(cmd->argv[0], cmd->argv);

    if (errno == ENOENT) {
        shell_error("%s: command not found", cmd->argv[0]);
        _exit(127);
    }
    shell_error("%s: %s", cmd->argv[0], strerror(errno));
    _exit(126);
}

/* Forks one child per stage, connecting each stage to the next with a pipe.
   The number of children actually started is stored in *started so that the
   caller can reap them even when a later fork fails. */
static int spawn_pipeline(pipeline *p, pid_t *pids, size_t *started)
{
    size_t i;
    int prev_read = -1;
    int result = 0;

    *started = 0;

    for (i = 0; i < p->ncmds; i++) {
        int pipefd[2] = { -1, -1 };
        pid_t pid;

        if (i + 1 < p->ncmds && pipe(pipefd) < 0) {
            shell_error("pipe: %s", strerror(errno));
            result = -1;
            break;
        }

        fflush(NULL); /* do not let a child inherit unwritten output */

        pid = fork();
        if (pid < 0) {
            shell_error("fork: %s", strerror(errno));
            if (pipefd[0] >= 0) {
                close(pipefd[0]);
                close(pipefd[1]);
            }
            result = -1;
            break;
        }

        if (pid == 0) {
            /* Read end of the previous pipe becomes our stdin. */
            if (prev_read >= 0) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            /* Write end of the next pipe becomes our stdout. */
            if (pipefd[1] >= 0) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            child_run(&p->cmds[i]);
        }

        pids[(*started)++] = pid;

        /* The parent keeps no pipe ends open, otherwise readers never see
           end of file. */
        if (prev_read >= 0) {
            close(prev_read);
        }
        if (pipefd[1] >= 0) {
            close(pipefd[1]);
        }
        prev_read = pipefd[0];
    }

    if (prev_read >= 0) {
        close(prev_read);
    }
    return result;
}

/* Waits for every process of the pipeline; the status of the last stage is the
   status of the pipeline as a whole. */
static int wait_for_pipeline(const pid_t *pids, size_t n)
{
    size_t i;
    int last = 0;

    for (i = 0; i < n; i++) {
        int status = 0;

        for (;;) {
            if (waitpid(pids[i], &status, 0) >= 0) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            shell_error("waitpid: %s", strerror(errno));
            break;
        }

        if (i + 1 == n) {
            last = status_to_exit(status);
        }
    }
    return last;
}

int exec_pipeline(pipeline *p, const char *cmdline)
{
    pid_t *pids;
    size_t started = 0;
    int status;
    int failed;

    (void)cmdline;

    if (p->ncmds == 0) {
        return shell.last_status;
    }
    if (p->background) {
        shell_error("background jobs are not supported yet");
        return 1;
    }

    /* A single builtin runs in the shell; inside a pipeline it has to run in a
       child like everything else, which is why its side effects are lost
       there. */
    if (p->ncmds == 1) {
        builtin_fn fn = builtin_lookup(p->cmds[0].argv[0]);
        if (fn != NULL) {
            return run_builtin_in_shell(&p->cmds[0], fn);
        }
    }

    pids = xmalloc(p->ncmds * sizeof(*pids));
    failed = spawn_pipeline(p, pids, &started);
    status = wait_for_pipeline(pids, started);
    free(pids);

    return failed != 0 ? 1 : status;
}
