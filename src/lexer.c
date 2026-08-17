#include "lexer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "util.h"

void tokens_init(token_list *list)
{
    list->cap = 8;
    list->count = 0;
    list->items = xmalloc(list->cap * sizeof(*list->items));
}

void tokens_free(token_list *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        free(list->items[i].text);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static void tokens_push(token_list *list, token_type type, char *text)
{
    if (list->count == list->cap) {
        list->cap *= 2;
        list->items = xrealloc(list->items, list->cap * sizeof(*list->items));
    }
    list->items[list->count].type = type;
    list->items[list->count].text = text;
    list->count++;
}

static int is_operator_char(char c)
{
    return c == '|' || c == '<' || c == '>' || c == '&';
}

static int is_word_end(char c)
{
    return c == '\0' || isspace((unsigned char)c) || is_operator_char(c);
}

static int is_name_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_name_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Appends the value of the parameter starting at line[*pos] (which points at
   the '$') to out, and advances *pos past the reference. Unknown names expand
   to nothing, matching the usual shell behaviour. */
static void expand_parameter(const char *line, size_t *pos, strbuf *out)
{
    size_t i = *pos + 1; /* skip '$' */
    char name[256];
    size_t len = 0;
    int braced = 0;
    const char *value;

    if (line[i] == '?') {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", shell.last_status);
        sb_puts(out, buf);
        *pos = i + 1;
        return;
    }

    if (line[i] == '{') {
        braced = 1;
        i++;
    }

    if (!is_name_start(line[i])) {
        /* Not a parameter reference, so the dollar sign is a literal. */
        sb_putc(out, '$');
        if (braced) {
            sb_putc(out, '{');
        }
        *pos = i;
        return;
    }

    while (is_name_char(line[i]) && len + 1 < sizeof(name)) {
        name[len++] = line[i++];
    }
    name[len] = '\0';

    if (braced) {
        if (line[i] != '}') {
            /* Unterminated ${...}; keep what we consumed as literal text. */
            sb_puts(out, "${");
            sb_puts(out, name);
            *pos = i;
            return;
        }
        i++;
    }

    value = getenv(name);
    if (value != NULL) {
        sb_puts(out, value);
    }
    *pos = i;
}

/* Reads one word, resolving quotes, escapes and parameter expansion. Returns 0
   on success and -1 if a quote or escape is left open at the end of the line. */
static int read_word(const char *line, size_t *pos, strbuf *out)
{
    size_t i = *pos;

    while (!is_word_end(line[i])) {
        char c = line[i];

        if (c == '\'') {
            /* Single quotes are fully literal, including backslashes. */
            i++;
            while (line[i] != '\'') {
                if (line[i] == '\0') {
                    shell_error("unterminated single quote");
                    return -1;
                }
                sb_putc(out, line[i++]);
            }
            i++;
        } else if (c == '"') {
            i++;
            while (line[i] != '"') {
                if (line[i] == '\0') {
                    shell_error("unterminated double quote");
                    return -1;
                }
                if (line[i] == '\\' && (line[i + 1] == '"' || line[i + 1] == '\\' ||
                                        line[i + 1] == '$')) {
                    sb_putc(out, line[i + 1]);
                    i += 2;
                } else if (line[i] == '$') {
                    expand_parameter(line, &i, out);
                } else {
                    sb_putc(out, line[i++]);
                }
            }
            i++;
        } else if (c == '\\') {
            if (line[i + 1] == '\0') {
                shell_error("trailing backslash");
                return -1;
            }
            sb_putc(out, line[i + 1]);
            i += 2;
        } else if (c == '$') {
            expand_parameter(line, &i, out);
        } else {
            sb_putc(out, c);
            i++;
        }
    }

    *pos = i;
    return 0;
}

int lex(const char *line, token_list *out)
{
    size_t i = 0;

    tokens_init(out);

    while (line[i] != '\0') {
        char c = line[i];

        if (isspace((unsigned char)c)) {
            i++;
            continue;
        }

        /* A '#' at the start of a token comments out the rest of the line. */
        if (c == '#') {
            break;
        }

        if (c == '|') {
            tokens_push(out, TOK_PIPE, NULL);
            i++;
            continue;
        }
        if (c == '&') {
            tokens_push(out, TOK_AMP, NULL);
            i++;
            continue;
        }
        if (c == '<') {
            tokens_push(out, TOK_REDIR_IN, NULL);
            i++;
            continue;
        }
        if (c == '>') {
            if (line[i + 1] == '>') {
                tokens_push(out, TOK_REDIR_APPEND, NULL);
                i += 2;
            } else {
                tokens_push(out, TOK_REDIR_OUT, NULL);
                i++;
            }
            continue;
        }
        /* "2>" and "2>>" only count as redirections when they open a token;
           inside a word such as "log2>file" the digit belongs to the word. */
        if (c == '2' && line[i + 1] == '>') {
            if (line[i + 2] == '>') {
                tokens_push(out, TOK_REDIR_ERR_APPEND, NULL);
                i += 3;
            } else {
                tokens_push(out, TOK_REDIR_ERR, NULL);
                i += 2;
            }
            continue;
        }

        {
            strbuf word;

            sb_init(&word);
            if (read_word(line, &i, &word) != 0) {
                sb_free(&word);
                tokens_free(out);
                return -1;
            }
            tokens_push(out, TOK_WORD, sb_release(&word));
        }
    }

    return 0;
}
