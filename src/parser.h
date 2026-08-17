#ifndef CSHELL_PARSER_H
#define CSHELL_PARSER_H

#include <stddef.h>

#include "lexer.h"

/* One stage of a pipeline: an argument vector plus its redirections. */
typedef struct {
    char **argv;       /* NULL-terminated, owned */
    size_t argc;
    size_t argv_cap;
    char  *in_file;    /* <  */
    char  *out_file;   /* >  or >> */
    char  *err_file;   /* 2> or 2>> */
    int    out_append;
    int    err_append;
} command;

typedef struct {
    command *cmds;
    size_t   ncmds;
    size_t   cap;
    int      background; /* the line ended with & */
} pipeline;

void pipeline_init(pipeline *p);
void pipeline_free(pipeline *p);

/* Builds a pipeline from tokens. Returns 0 on success, or -1 after reporting a
   syntax error to stderr. An empty token list yields a pipeline with no
   commands, which the caller should simply ignore. */
int parse(const token_list *tokens, pipeline *out);

#endif /* CSHELL_PARSER_H */
