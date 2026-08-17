#ifndef CSHELL_SHELL_H
#define CSHELL_SHELL_H

#include <sys/types.h>

/* Name used as the prefix of every diagnostic message. */
#define SHELL_NAME "cshell"

/* State that every module needs to see. */
typedef struct {
    int   last_status;  /* exit status of the last foreground command, for $? */
    int   interactive;  /* non-zero when stdin is a terminal */
    int   terminal_fd;  /* descriptor used for terminal control */
    pid_t pgid;         /* process group of the shell itself */
    int   should_exit;  /* set by the exit builtin */
    int   exit_status;  /* status the shell itself exits with */
} shell_state;

extern shell_state shell;

#endif /* CSHELL_SHELL_H */
