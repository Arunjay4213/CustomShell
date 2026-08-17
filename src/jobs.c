#include "jobs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

typedef struct job {
    int          id;
    pid_t        pgid;
    pid_t       *pids;   /* -1 once the process has been reaped */
    size_t       npids;
    size_t       nlive;
    int          status; /* wait status of the last stage of the pipeline */
    char        *cmdline;
    struct job  *next;
} job;

static job *job_list;
static int  next_id = 1;

void jobs_block(sigset_t *old)
{
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, old);
}

void jobs_unblock(const sigset_t *old)
{
    sigprocmask(SIG_SETMASK, old, NULL);
}

int jobs_add(pid_t pgid, pid_t *pids, size_t npids, const char *cmdline)
{
    job *j = xmalloc(sizeof(*j));

    j->id = next_id++;
    j->pgid = pgid;
    j->pids = pids;
    j->npids = npids;
    j->nlive = npids;
    j->status = 0;
    j->cmdline = xstrdup(cmdline);
    j->next = job_list;
    job_list = j;
    return j->id;
}

void jobs_reap(void)
{
    int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        job *j;

        for (j = job_list; j != NULL; j = j->next) {
            size_t i;

            for (i = 0; i < j->npids; i++) {
                if (j->pids[i] != pid) {
                    continue;
                }
                j->pids[i] = -1;
                j->nlive--;
                /* The pipeline as a whole reports the last stage's status. */
                if (i + 1 == j->npids) {
                    j->status = status;
                }
                break;
            }
        }
    }

    errno = saved_errno;
}

static void describe_status(int status, char *buf, size_t size)
{
    if (WIFSIGNALED(status)) {
        snprintf(buf, size, "Terminated (signal %d)", WTERMSIG(status));
    } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        snprintf(buf, size, "Exit %d", WEXITSTATUS(status));
    } else {
        snprintf(buf, size, "Done");
    }
}

static void job_free(job *j)
{
    free(j->pids);
    free(j->cmdline);
    free(j);
}

void jobs_notify(void)
{
    sigset_t old;
    job **link;

    jobs_block(&old);

    link = &job_list;
    while (*link != NULL) {
        job *j = *link;

        if (j->nlive == 0) {
            char text[64];

            describe_status(j->status, text, sizeof(text));
            printf("[%d]  %-22s %s\n", j->id, text, j->cmdline);
            *link = j->next;
            job_free(j);
        } else {
            link = &j->next;
        }
    }

    if (job_list == NULL) {
        next_id = 1;
    }

    jobs_unblock(&old);
}

void jobs_list(void)
{
    sigset_t old;
    job *j;

    jobs_block(&old);
    for (j = job_list; j != NULL; j = j->next) {
        const char *state = j->nlive > 0 ? "Running" : "Done";
        printf("[%d]  %-22s %s\n", j->id, state, j->cmdline);
    }
    jobs_unblock(&old);
}

void jobs_shutdown(void)
{
    sigset_t old;

    jobs_block(&old);
    while (job_list != NULL) {
        job *j = job_list;
        job_list = j->next;
        job_free(j);
    }
    jobs_unblock(&old);
}
