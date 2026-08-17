#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"
#include "exec.h"
#include "jobs.h"
#include "lexer.h"
#include "parser.h"
#include "shell.h"
#include "util.h"

shell_state shell;

static void handle_sigchld(int sig)
{
    (void)sig;
    jobs_reap();
}

static void handle_sigint(int sig)
{
    /* Nothing to do: the handler exists so that the pending read fails with
       EINTR and the main loop can start a fresh line. */
    (void)sig;
}

static void set_handler(int sig, void (*handler)(int), int flags)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = flags;
    if (sigaction(sig, &sa, NULL) != 0) {
        shell_error("sigaction: %s", strerror(errno));
    }
}

/* Puts the shell in its own process group and gives it the terminal, so that
   every job it starts can own the terminal in turn. */
static void init_job_control(void)
{
    shell.pgid = getpgrp();

    /* Children are reaped in the handler, which keeps finished background jobs
       from lingering as zombies. SA_RESTART leaves the pending read alone. */
    set_handler(SIGCHLD, handle_sigchld, SA_RESTART | SA_NOCLDSTOP);

    if (!shell.interactive) {
        return;
    }

    /* If the shell was started in the background, stop until a job control
       aware parent moves it to the foreground. */
    while (tcgetpgrp(shell.terminal_fd) != (shell.pgid = getpgrp())) {
        kill(-shell.pgid, SIGTTIN);
    }

    /* Ctrl-C must cancel the line being typed, not the shell, and the shell
       must not be stopped or interrupted by its own terminal handling. */
    set_handler(SIGINT, handle_sigint, 0);
    set_handler(SIGQUIT, SIG_IGN, 0);
    set_handler(SIGTSTP, SIG_IGN, 0);
    set_handler(SIGTTIN, SIG_IGN, 0);
    set_handler(SIGTTOU, SIG_IGN, 0);

    shell.pgid = getpid();
    if (setpgid(shell.pgid, shell.pgid) != 0) {
        shell_error("could not put the shell in its own process group: %s",
                    strerror(errno));
        exit(1);
    }
    tcsetpgrp(shell.terminal_fd, shell.pgid);
}

typedef enum {
    READ_OK,
    READ_EOF,
    READ_INTERRUPTED
} read_result;

/* Reads one line of input, growing the buffer as needed so that long lines are
   not truncated. The trailing newline is removed. */
static char *read_line(FILE *in, read_result *result)
{
    strbuf sb;
    char chunk[512];

    sb_init(&sb);
    for (;;) {
        if (fgets(chunk, sizeof(chunk), in) == NULL) {
            if (ferror(in) && errno == EINTR) {
                clearerr(in);
                sb_free(&sb);
                *result = READ_INTERRUPTED;
                return NULL;
            }
            if (sb.len == 0) {
                sb_free(&sb);
                *result = READ_EOF;
                return NULL;
            }
            break; /* end of input without a final newline */
        }
        sb_puts(&sb, chunk);
        if (sb.data[sb.len - 1] == '\n') {
            sb.data[--sb.len] = '\0';
            break;
        }
    }

    *result = READ_OK;
    return sb_release(&sb);
}

static void print_prompt(void)
{
    char *cwd = xgetcwd();
    const char *home = getenv("HOME");
    const char *shown = cwd != NULL ? cwd : "?";
    size_t home_len = home != NULL ? strlen(home) : 0;

    /* Abbreviate the home directory as ~, like most shells do. */
    if (home_len > 0 && cwd != NULL && strncmp(cwd, home, home_len) == 0 &&
        (cwd[home_len] == '\0' || cwd[home_len] == '/')) {
        printf(SHELL_NAME ":~%s$ ", cwd + home_len);
    } else {
        printf(SHELL_NAME ":%s$ ", shown);
    }
    fflush(stdout);
    free(cwd);
}

static int is_blank(const char *line)
{
    while (*line != '\0') {
        if (*line != ' ' && *line != '\t') {
            return 0;
        }
        line++;
    }
    return 1;
}

static void run_line(const char *line)
{
    token_list tokens;
    pipeline pipe_line;

    if (lex(line, &tokens) != 0) {
        shell.last_status = 2;
        return;
    }
    if (parse(&tokens, &pipe_line) != 0) {
        tokens_free(&tokens);
        shell.last_status = 2;
        return;
    }
    tokens_free(&tokens);

    if (pipe_line.ncmds > 0) {
        shell.last_status = exec_pipeline(&pipe_line, line);
    }

    pipeline_free(&pipe_line);
}

int main(void)
{
    /* Line buffering keeps our own output in step with what the children we
       fork write to the same descriptor. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    shell.last_status = 0;
    shell.terminal_fd = STDIN_FILENO;
    shell.interactive = isatty(shell.terminal_fd);
    init_job_control();

    while (!shell.should_exit) {
        char *line;
        read_result result;

        jobs_notify();

        if (shell.interactive) {
            print_prompt();
        }

        line = read_line(stdin, &result);
        if (line == NULL) {
            if (result == READ_INTERRUPTED) {
                /* Ctrl-C: abandon the line and print a fresh prompt. */
                fputc('\n', stdout);
                shell.last_status = 128 + SIGINT;
                continue;
            }
            if (shell.interactive) {
                fputc('\n', stdout);
            }
            break;
        }

        if (!is_blank(line)) {
            history_add(line);
            run_line(line);
        }
        free(line);
    }

    jobs_shutdown();
    history_clear();
    return shell.should_exit ? shell.exit_status : shell.last_status;
}
