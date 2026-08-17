# CustomShell

`cshell` is a small POSIX shell written in C11.
It runs interactively or from a script file, and it supports quoting, variable expansion, pipelines of any length, I/O redirection, background jobs and terminal job control.

The project started from the shell project in UW-Madison's CS537 (the OSTEP projects) and grew well beyond that spec.

It is a systems programming exercise rather than a replacement for bash: the point is to show how a shell is actually put together out of `fork`, `execvp`, `pipe`, `dup2`, process groups and signals.

## Features

- Interactive read-eval loop with a prompt showing the working directory, and a batch mode that reads commands from a file.
- Tokenizer with single quotes, double quotes and backslash escapes.
- Expansion of `$VAR`, `${VAR}` and `$?`, with expansion suppressed inside single quotes.
- External commands executed with `fork` and `execvp`, so `PATH` is searched the usual way.
- Builtins: `cd` (including `cd -` and bare `cd`), `pwd`, `echo`, `export`, `unset`, `history`, `jobs` and `exit`.
- Pipelines of arbitrary length, such as `a | b | c`.
- Redirection with `<`, `>`, `>>`, `2>` and `2>>`, freely combined with pipes.
- Background jobs with `&`, listed by `jobs` and reaped in a `SIGCHLD` handler so no zombies are left behind.
- Job control: every pipeline runs in its own process group and the foreground job owns the terminal, so `Ctrl-C` interrupts the running command instead of the shell.
- Exit status tracking, including `128 + signal` for a command killed by a signal.
- Comments: a `#` that starts a token comments out the rest of the line.

## Building

The only requirements are a C11 compiler and `make`.
There is no dependency on readline or any other library.

```sh
make            # optimised build, produces ./cshell
make debug      # unoptimised build with debug info
make test       # build, then run the end-to-end tests
make clean      # remove build output
```

Object files land in `build/release` or `build/debug`, so the two builds do not disturb each other.
The code is portable between Linux and macOS.

## Usage

Start the shell with no arguments to get a prompt, or pass a file to run it in batch mode.

```sh
./cshell                # interactive
./cshell script.sh      # batch mode, no prompt
echo 'echo hi' | ./cshell
```

Quoting and expansion.

```sh
cshell:~/projects$ echo "hello   world" 'and $HOME stays literal'
hello   world and $HOME stays literal
cshell:~/projects$ export GREETING=hi
cshell:~/projects$ echo ${GREETING}, status $?
hi, status 0
```

Pipelines and redirection.

```sh
cshell:~/projects$ cat < input.txt | sort | uniq -c > counts.txt
cshell:~/projects$ make 2> errors.log
cshell:~/projects$ echo done >> log.txt
```

Moving around.

```sh
cshell:~/projects$ cd /usr/lib
cshell:/usr/lib$ cd -
/home/user/projects
cshell:~/projects$ pwd
/home/user/projects
```

Background jobs and history.

```sh
cshell:~/projects$ sleep 30 &
[1] 48213
cshell:~/projects$ jobs
[1]  Running                sleep 30 &
cshell:~/projects$ history
    1  sleep 30 &
    2  jobs
    3  history
cshell:~/projects$ exit
```

## Design

The source is in `src/`, one file per stage of the work.

| File | Responsibility |
| --- | --- |
| `main.c` | Read-eval loop, prompt, batch mode, signal and terminal setup |
| `lexer.c` | Characters to tokens: quoting, escapes, expansion |
| `parser.c` | Tokens to a pipeline of commands with their redirections |
| `exec.c` | Running a pipeline: processes, pipes, redirections, waiting |
| `builtins.c` | Commands the shell runs itself, plus the history list |
| `jobs.c` | Background job table and child reaping |
| `util.c` | Allocation wrappers, a growable string, error reporting |

One line of input travels through those stages in order.

```
input line -> lex() -> tokens -> parse() -> pipeline -> exec_pipeline() -> status
```

### Lexer

`lex()` walks the line one character at a time and produces a list of tokens.
A token is either a word or one of the operators `|`, `<`, `>`, `>>`, `2>`, `2>>` and `&`.

Words are assembled into a growable buffer, which is what makes quoting straightforward.
Text inside single quotes is copied literally.
Text inside double quotes is copied too, except that `$` still expands and a backslash escapes `"`, `\` and `$`.
Outside quotes a backslash escapes the next character, and unquoted whitespace or an operator ends the word.
Because quotes only affect how characters are copied into the buffer, `"x""y"` naturally becomes the single word `xy`.

Expansion happens while the word is being built.
`$?` becomes the last exit status, `$NAME` and `${NAME}` become the value of the environment variable, and an unset variable expands to nothing.

### Parser

`parse()` reads the token list and fills in a `pipeline`, which is an array of `command` structures plus a background flag.
Each `command` holds a NULL-terminated `argv` ready for `execvp`, the input file, the output file with an append flag, and the same pair for standard error.
A `|` token starts the next command in the pipeline, a redirection operator consumes the word after it, and `&` is only accepted as the last token on the line.
Anything else is reported as a syntax error and the line is discarded, which keeps the executor free of validation.

### Executor

A pipeline of `n` commands normally means `n` children.

For every stage except the last, the shell calls `pipe()` to get a pair of descriptors.
The child then uses `dup2()` to put the read end of the previous pipe on descriptor 0 and the write end of the next pipe on descriptor 1, and closes the originals.
Redirections are applied after that wiring, so a file named on the command line always wins over the pipe.
The parent closes every pipe end as soon as it has been handed to a child, because a reader only sees end of file once no process still holds the write end open.

A builtin that stands alone runs in the shell process itself, otherwise `cd` and `export` could not change anything.
Redirections for that case are applied to saved copies of descriptors 0, 1 and 2, which are restored once the builtin returns.
Inside a pipeline a builtin runs in a child like any other command, so its side effects are local to that child.

The status of a pipeline is the status of its last stage.
A command killed by a signal reports `128 + signal`, which is where the familiar `130` for `Ctrl-C` comes from.

### Jobs and signals

The shell puts itself in its own process group at startup and takes ownership of the terminal with `tcsetpgrp()`.
Every pipeline it starts is placed in a new process group of its own, the first child leading it, so the whole pipeline can be signalled as a unit.
Both the parent and the child call `setpgid()` so that neither ordering of the two processes can lose the race.

Before a foreground job runs, the shell hands the terminal to that group and takes it back afterwards.
That is what makes `Ctrl-C` reach the running command: the terminal driver sends `SIGINT` to the foreground group, which is the job and not the shell.
The shell keeps a handler for `SIGINT` that does nothing except make the pending read fail with `EINTR`, so pressing `Ctrl-C` at the prompt abandons the half-typed line and prints a fresh one.
It ignores `SIGQUIT`, `SIGTSTP`, `SIGTTIN` and `SIGTTOU`, while children get the default dispositions and an empty signal mask restored before `execvp`.

A background pipeline is added to a job list and reported as `[1] 48213`.
Finished children are collected by a `SIGCHLD` handler, which does nothing but call `waitpid()` with `WNOHANG` and mark list entries as finished, since a handler must stay async-signal-safe.
The main loop blocks `SIGCHLD` while it prints the finished jobs and frees them, so the list is never modified from two places at once.
The same block is held from the first `fork` of a pipeline until the job is registered or fully reaped, so the handler cannot collect a child before the shell knows it exists.

## Limitations

These are deliberate: the shell covers process handling rather than the full shell language.

- No scripting constructs, so no `if`, `while`, `for`, `case` or functions.
- No command separators or operators: `;`, `&&` and `||` are not recognised.
- No globbing, so `*`, `?` and `[...]` are passed through as literal text.
- No command substitution with `$(...)` or backticks, and no arithmetic expansion.
- No tilde expansion in arguments, `~` is only used to shorten the prompt.
- No here-documents, no descriptor duplication such as `2>&1`, and no redirection of descriptors other than 0, 1 and 2.
- Expansion happens while a word is read, so the result is never split into further words and an unset variable still leaves an empty argument.
- There are no shell variables separate from the environment, so `export` is how a variable is set and `NAME=value` on its own is not an assignment.
- No suspend and resume: `Ctrl-Z` is ignored and there are no `fg` and `bg` builtins, since a stopped job could never be resumed.
- Background jobs keep running when the shell exits.
- Input is read with `fgets`, so there is no line editing, no completion and no recall of earlier commands with the arrow keys.
- History is kept in memory for the session only and is never written to a file.
- No aliases, no startup file, no prompt customisation.

## Tests

`tests/run_tests.sh` drives the built binary end to end.
Each case feeds a small script to the shell and compares its standard output, standard error or exit status against what is expected.
The suite covers quoting and escapes, expansion and exit status, every builtin, pipelines of two and three stages, all four redirection forms and their combination with pipes, background jobs and the `jobs` listing, syntax errors, and both input modes.

```sh
make test                  # build and run
tests/run_tests.sh ./cshell
```
