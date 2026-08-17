#ifndef CSHELL_EXEC_H
#define CSHELL_EXEC_H

#include "parser.h"

/* Runs a parsed pipeline and returns the exit status to record in $?.
   cmdline is the original input line, kept for job listings. */
int exec_pipeline(pipeline *p, const char *cmdline);

#endif /* CSHELL_EXEC_H */
