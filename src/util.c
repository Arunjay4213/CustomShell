#include "util.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shell.h"

static void out_of_memory(void)
{
    fputs(SHELL_NAME ": out of memory\n", stderr);
    exit(1);
}

void *xmalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL) {
        out_of_memory();
    }
    return p;
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = xmalloc(len);
    memcpy(copy, s, len);
    return copy;
}

void sb_init(strbuf *sb)
{
    sb->cap = 32;
    sb->len = 0;
    sb->data = xmalloc(sb->cap);
    sb->data[0] = '\0';
}

static void sb_reserve(strbuf *sb, size_t extra)
{
    if (sb->len + extra + 1 <= sb->cap) {
        return;
    }
    while (sb->cap < sb->len + extra + 1) {
        sb->cap *= 2;
    }
    sb->data = xrealloc(sb->data, sb->cap);
}

void sb_putc(strbuf *sb, char c)
{
    sb_reserve(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void sb_puts(strbuf *sb, const char *s)
{
    size_t len = strlen(s);
    sb_reserve(sb, len);
    memcpy(sb->data + sb->len, s, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

char *sb_release(strbuf *sb)
{
    char *data = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    return data;
}

void sb_free(strbuf *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

char *xgetcwd(void)
{
    size_t size = 256;
    char *buf = xmalloc(size);

    while (getcwd(buf, size) == NULL) {
        if (errno != ERANGE) {
            free(buf);
            return NULL;
        }
        size *= 2;
        buf = xrealloc(buf, size);
    }
    return buf;
}

void shell_error(const char *fmt, ...)
{
    va_list ap;

    fputs(SHELL_NAME ": ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
