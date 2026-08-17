#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

void pipeline_init(pipeline *p)
{
    p->cap = 4;
    p->ncmds = 0;
    p->background = 0;
    p->cmds = xmalloc(p->cap * sizeof(*p->cmds));
}

void pipeline_free(pipeline *p)
{
    size_t i, j;

    for (i = 0; i < p->ncmds; i++) {
        command *cmd = &p->cmds[i];
        for (j = 0; j < cmd->argc; j++) {
            free(cmd->argv[j]);
        }
        free(cmd->argv);
        free(cmd->in_file);
        free(cmd->out_file);
        free(cmd->err_file);
    }
    free(p->cmds);
    p->cmds = NULL;
    p->ncmds = 0;
    p->cap = 0;
}

static command *pipeline_new_command(pipeline *p)
{
    command *cmd;

    if (p->ncmds == p->cap) {
        p->cap *= 2;
        p->cmds = xrealloc(p->cmds, p->cap * sizeof(*p->cmds));
    }
    cmd = &p->cmds[p->ncmds++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->argv_cap = 8;
    cmd->argv = xmalloc(cmd->argv_cap * sizeof(*cmd->argv));
    cmd->argv[0] = NULL;
    return cmd;
}

static void command_add_arg(command *cmd, char *arg)
{
    if (cmd->argc + 2 > cmd->argv_cap) {
        cmd->argv_cap *= 2;
        cmd->argv = xrealloc(cmd->argv, cmd->argv_cap * sizeof(*cmd->argv));
    }
    cmd->argv[cmd->argc++] = arg;
    cmd->argv[cmd->argc] = NULL;
}

static const char *token_name(token_type type)
{
    switch (type) {
    case TOK_PIPE:             return "|";
    case TOK_REDIR_IN:         return "<";
    case TOK_REDIR_OUT:        return ">";
    case TOK_REDIR_APPEND:     return ">>";
    case TOK_REDIR_ERR:        return "2>";
    case TOK_REDIR_ERR_APPEND: return "2>>";
    case TOK_AMP:              return "&";
    case TOK_WORD:             break;
    }
    return "word";
}

static void syntax_error(token_type type)
{
    shell_error("syntax error near unexpected token '%s'", token_name(type));
}

/* Consumes the filename that follows a redirection operator. */
static int take_redirect_target(const token_list *tokens, size_t *i, char **slot,
                                int *append_flag, int append)
{
    const token *op = &tokens->items[*i];

    if (*i + 1 >= tokens->count || tokens->items[*i + 1].type != TOK_WORD) {
        if (*i + 1 < tokens->count) {
            syntax_error(tokens->items[*i + 1].type);
        } else {
            shell_error("syntax error near unexpected token '%s'", token_name(op->type));
        }
        return -1;
    }

    free(*slot);
    *slot = xstrdup(tokens->items[*i + 1].text);
    if (append_flag != NULL) {
        *append_flag = append;
    }
    *i += 2;
    return 0;
}

int parse(const token_list *tokens, pipeline *out)
{
    command *cmd;
    size_t i = 0;

    pipeline_init(out);
    if (tokens->count == 0) {
        return 0;
    }

    cmd = pipeline_new_command(out);

    while (i < tokens->count) {
        const token *tok = &tokens->items[i];

        switch (tok->type) {
        case TOK_WORD:
            command_add_arg(cmd, xstrdup(tok->text));
            i++;
            break;

        case TOK_PIPE:
            if (cmd->argc == 0) {
                syntax_error(TOK_PIPE);
                goto fail;
            }
            if (i + 1 >= tokens->count) {
                shell_error("syntax error: expected a command after '|'");
                goto fail;
            }
            cmd = pipeline_new_command(out);
            i++;
            break;

        case TOK_AMP:
            /* Background marker; it must be the last token on the line. */
            if (i + 1 != tokens->count) {
                syntax_error(tokens->items[i + 1].type);
                goto fail;
            }
            if (cmd->argc == 0) {
                syntax_error(TOK_AMP);
                goto fail;
            }
            out->background = 1;
            i++;
            break;

        case TOK_REDIR_IN:
            if (take_redirect_target(tokens, &i, &cmd->in_file, NULL, 0) != 0) {
                goto fail;
            }
            break;

        case TOK_REDIR_OUT:
            if (take_redirect_target(tokens, &i, &cmd->out_file, &cmd->out_append, 0) != 0) {
                goto fail;
            }
            break;

        case TOK_REDIR_APPEND:
            if (take_redirect_target(tokens, &i, &cmd->out_file, &cmd->out_append, 1) != 0) {
                goto fail;
            }
            break;

        case TOK_REDIR_ERR:
            if (take_redirect_target(tokens, &i, &cmd->err_file, &cmd->err_append, 0) != 0) {
                goto fail;
            }
            break;

        case TOK_REDIR_ERR_APPEND:
            if (take_redirect_target(tokens, &i, &cmd->err_file, &cmd->err_append, 1) != 0) {
                goto fail;
            }
            break;
        }
    }

    /* Every stage needs a command name; "ls |" or "> out" are errors. */
    for (i = 0; i < out->ncmds; i++) {
        if (out->cmds[i].argc == 0) {
            shell_error("syntax error: missing command");
            goto fail;
        }
    }

    return 0;

fail:
    pipeline_free(out);
    return -1;
}
