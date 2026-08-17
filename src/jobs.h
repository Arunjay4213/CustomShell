#ifndef CSHELL_JOBS_H
#define CSHELL_JOBS_H

#include <signal.h>
#include <stddef.h>
#include <sys/types.h>

/* Background jobs started with '&'.

   The list is shared between the main loop and the SIGCHLD handler. The
   handler only marks entries as finished; entries are printed and freed from
   the main loop, which blocks SIGCHLD while it touches the list. */

/* Blocks SIGCHLD and stores the previous mask in old. */
void jobs_block(sigset_t *old);
void jobs_unblock(const sigset_t *old);

/* Registers a background job and returns its job number. Takes ownership of
   pids. Must be called with SIGCHLD blocked. */
int jobs_add(pid_t pgid, pid_t *pids, size_t npids, const char *cmdline);

/* Reaps finished children. Called from the SIGCHLD handler, so it must stay
   free of anything that is not async-signal-safe. */
void jobs_reap(void);

/* Reports jobs that finished since the last call and removes them. */
void jobs_notify(void);

/* The jobs builtin. */
void jobs_list(void);

void jobs_shutdown(void);

#endif /* CSHELL_JOBS_H */
