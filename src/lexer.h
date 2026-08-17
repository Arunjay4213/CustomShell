#ifndef CSHELL_LEXER_H
#define CSHELL_LEXER_H

#include <stddef.h>

typedef enum {
    TOK_WORD,          /* a command name, argument or redirection target */
    TOK_PIPE,          /* |  */
    TOK_REDIR_IN,      /* <  */
    TOK_REDIR_OUT,     /* >  */
    TOK_REDIR_APPEND,  /* >> */
    TOK_REDIR_ERR,     /* 2> */
    TOK_REDIR_ERR_APPEND, /* 2>> */
    TOK_AMP            /* &  */
} token_type;

typedef struct {
    token_type type;
    char      *text; /* owned; only meaningful for TOK_WORD */
} token;

typedef struct {
    token *items;
    size_t count;
    size_t cap;
} token_list;

void tokens_init(token_list *list);
void tokens_free(token_list *list);

/* Splits line into tokens, resolving quotes, backslash escapes and $ expansion.
   Returns 0 on success, or -1 after reporting a lexical error to stderr. */
int lex(const char *line, token_list *out);

#endif /* CSHELL_LEXER_H */
