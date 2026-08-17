#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "exec.h"
#include "lexer.h"
#include "parser.h"
#include "shell.h"
#include "util.h"

shell_state shell;

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
    shell.pgid = getpgrp();

    while (!shell.should_exit) {
        char *line;
        read_result result;

        if (shell.interactive) {
            print_prompt();
        }

        line = read_line(stdin, &result);
        if (line == NULL) {
            if (result == READ_INTERRUPTED) {
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

    history_clear();
    return shell.should_exit ? shell.exit_status : shell.last_status;
}
