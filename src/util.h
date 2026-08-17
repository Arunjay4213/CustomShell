#ifndef CSHELL_UTIL_H
#define CSHELL_UTIL_H

#include <stddef.h>

/* Allocation wrappers that abort instead of returning NULL. The shell has no
   useful way to recover from a failed allocation while building a command. */
void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

/* Growable, NUL-terminated string buffer. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} strbuf;

void  sb_init(strbuf *sb);
void  sb_putc(strbuf *sb, char c);
void  sb_puts(strbuf *sb, const char *s);
/* Hands ownership of the buffer to the caller and resets sb. */
char *sb_release(strbuf *sb);
void  sb_free(strbuf *sb);

/* Returns the working directory in a buffer the caller frees, or NULL if the
   directory cannot be determined (for example after it has been removed). */
char *xgetcwd(void);

/* Prints "cshell: " followed by the formatted message and a newline to stderr. */
void shell_error(const char *fmt, ...);

#endif /* CSHELL_UTIL_H */
