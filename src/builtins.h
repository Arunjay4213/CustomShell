#ifndef CSHELL_BUILTINS_H
#define CSHELL_BUILTINS_H

#include "parser.h"

/* A builtin runs in the current process (or in a forked child when it is part
   of a pipeline) and returns the exit status to record in $?. */
typedef int (*builtin_fn)(command *cmd);

/* Returns the implementation of name, or NULL if name is not a builtin. */
builtin_fn builtin_lookup(const char *name);

/* Command history, kept in memory for the life of the session. */
void history_add(const char *line);
void history_clear(void);

#endif /* CSHELL_BUILTINS_H */
