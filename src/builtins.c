#include "builtins.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jobs.h"
#include "shell.h"
#include "util.h"

extern char **environ;

static char **history_lines;
static size_t history_count;
static size_t history_cap;

void history_add(const char *line)
{
    if (history_count == history_cap) {
        history_cap = history_cap == 0 ? 64 : history_cap * 2;
        history_lines = xrealloc(history_lines, history_cap * sizeof(*history_lines));
    }
    history_lines[history_count++] = xstrdup(line);
}

void history_clear(void)
{
    size_t i;

    for (i = 0; i < history_count; i++) {
        free(history_lines[i]);
    }
    free(history_lines);
    history_lines = NULL;
    history_count = 0;
    history_cap = 0;
}

static int builtin_history(command *cmd)
{
    size_t i;

    (void)cmd;
    for (i = 0; i < history_count; i++) {
        printf("%5zu  %s\n", i + 1, history_lines[i]);
    }
    return 0;
}

static int builtin_cd(command *cmd)
{
    const char *target;
    char *oldpwd;
    char *newpwd;
    int announce = 0;

    if (cmd->argc > 2) {
        shell_error("cd: too many arguments");
        return 1;
    }

    if (cmd->argc == 1) {
        target = getenv("HOME");
        if (target == NULL) {
            shell_error("cd: HOME not set");
            return 1;
        }
    } else if (strcmp(cmd->argv[1], "-") == 0) {
        target = getenv("OLDPWD");
        if (target == NULL) {
            shell_error("cd: OLDPWD not set");
            return 1;
        }
        announce = 1;
    } else {
        target = cmd->argv[1];
    }

    oldpwd = xgetcwd();
    if (chdir(target) != 0) {
        shell_error("cd: %s: %s", target, strerror(errno));
        free(oldpwd);
        return 1;
    }

    newpwd = xgetcwd();
    if (oldpwd != NULL) {
        setenv("OLDPWD", oldpwd, 1);
    }
    if (newpwd != NULL) {
        setenv("PWD", newpwd, 1);
        if (announce) {
            puts(newpwd);
        }
    }
    free(oldpwd);
    free(newpwd);
    return 0;
}

static int builtin_pwd(command *cmd)
{
    char *cwd;

    (void)cmd;
    cwd = xgetcwd();
    if (cwd == NULL) {
        shell_error("pwd: %s", strerror(errno));
        return 1;
    }
    puts(cwd);
    free(cwd);
    return 0;
}

static int builtin_echo(command *cmd)
{
    size_t i = 1;
    int newline = 1;

    if (cmd->argc > 1 && strcmp(cmd->argv[1], "-n") == 0) {
        newline = 0;
        i = 2;
    }
    for (; i < cmd->argc; i++) {
        fputs(cmd->argv[i], stdout);
        if (i + 1 < cmd->argc) {
            fputc(' ', stdout);
        }
    }
    if (newline) {
        fputc('\n', stdout);
    }
    return 0;
}

static int builtin_exit(command *cmd)
{
    int status = shell.last_status;

    if (cmd->argc > 2) {
        shell_error("exit: too many arguments");
        return 1;
    }
    if (cmd->argc == 2) {
        char *end;
        long value = strtol(cmd->argv[1], &end, 10);
        if (*end != '\0' || end == cmd->argv[1]) {
            shell_error("exit: %s: numeric argument required", cmd->argv[1]);
            status = 2;
        } else {
            status = (int)(value & 0xff);
        }
    }

    shell.should_exit = 1;
    shell.exit_status = status;
    return status;
}

static int valid_name(const char *name)
{
    size_t i;

    if (!isalpha((unsigned char)name[0]) && name[0] != '_') {
        return 0;
    }
    for (i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            return 0;
        }
    }
    return 1;
}

static int builtin_export(command *cmd)
{
    size_t i;
    int status = 0;

    if (cmd->argc == 1) {
        char **env;
        for (env = environ; *env != NULL; env++) {
            printf("export %s\n", *env);
        }
        return 0;
    }

    for (i = 1; i < cmd->argc; i++) {
        char *arg = cmd->argv[i];
        char *eq = strchr(arg, '=');

        if (eq == NULL) {
            /* Without a value the variable is created empty if it does not
               exist yet; there are no unexported shell variables here. */
            if (!valid_name(arg)) {
                shell_error("export: %s: not a valid identifier", arg);
                status = 1;
                continue;
            }
            if (getenv(arg) == NULL && setenv(arg, "", 1) != 0) {
                shell_error("export: %s: %s", arg, strerror(errno));
                status = 1;
            }
        } else {
            char *name = xmalloc((size_t)(eq - arg) + 1);
            memcpy(name, arg, (size_t)(eq - arg));
            name[eq - arg] = '\0';
            if (!valid_name(name)) {
                shell_error("export: %s: not a valid identifier", arg);
                status = 1;
            } else if (setenv(name, eq + 1, 1) != 0) {
                shell_error("export: %s: %s", name, strerror(errno));
                status = 1;
            }
            free(name);
        }
    }
    return status;
}

static int builtin_unset(command *cmd)
{
    size_t i;
    int status = 0;

    for (i = 1; i < cmd->argc; i++) {
        if (!valid_name(cmd->argv[i])) {
            shell_error("unset: %s: not a valid identifier", cmd->argv[i]);
            status = 1;
            continue;
        }
        if (unsetenv(cmd->argv[i]) != 0) {
            shell_error("unset: %s: %s", cmd->argv[i], strerror(errno));
            status = 1;
        }
    }
    return status;
}

static int builtin_jobs(command *cmd)
{
    (void)cmd;
    jobs_list();
    return 0;
}

static const struct {
    const char *name;
    builtin_fn  fn;
} builtins[] = {
    { "cd",      builtin_cd },
    { "echo",    builtin_echo },
    { "exit",    builtin_exit },
    { "export",  builtin_export },
    { "history", builtin_history },
    { "jobs",    builtin_jobs },
    { "pwd",     builtin_pwd },
    { "unset",   builtin_unset },
};

builtin_fn builtin_lookup(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return builtins[i].fn;
        }
    }
    return NULL;
}
