# CustomShell

`cshell` is a POSIX shell written in C11, in about 1800 lines with no dependencies beyond libc.
It handles quoting and `$VAR` expansion, pipelines of any length, redirection with `<`, `>`, `>>` and `2>`, background jobs with `&`, and enough terminal job control that `Ctrl-C` kills the running command instead of the shell.
Builtins are `cd`, `pwd`, `echo`, `export`, `unset`, `history`, `jobs` and `exit`.

The project started from the shell project in UW-Madison's CS537 (the OSTEP projects) and grew well beyond that spec.

## A session

```
cshell:~$ cat todo.txt
TODO write the parser
TODO handle quoting
done: pipelines
done: redirection
cshell:~$ wc -l < todo.txt
4
cshell:~$ cat todo.txt | grep TODO | tr a-z A-Z
TODO WRITE THE PARSER
TODO HANDLE QUOTING
cshell:~$ sort todo.txt > sorted.txt
cshell:~$ head -1 sorted.txt
TODO handle quoting
cshell:~$ cat missing.txt 2> errors.log
cshell:~$ echo $?
1
cshell:~$ cat errors.log
cat: missing.txt: No such file or directory
cshell:~$ sleep 2 &
[1] 37296
cshell:~$ jobs
[1]  Running                sleep 2 &
cshell:~$ echo waiting
waiting
[1]  Done                   sleep 2 &
cshell:~$ exit
```

## Building

A C11 compiler and `make` are all that is needed.

```sh
make            # optimised build, produces ./cshell
make debug      # -O0 -g3, objects kept separately under build/debug
make test       # build, then run tests/run_tests.sh against the binary
```

Run `./cshell` for a prompt, or `./cshell script.sh` to read commands from a file with no prompt.
Commands piped in on stdin work too, which is what most of the tests do.

## Design

| File | Responsibility |
| --- | --- |
| `src/main.c` | Read-eval loop, prompt, batch mode, signal and terminal setup |
| `src/lexer.c` | Characters to tokens: quoting, escapes, expansion |
| `src/parser.c` | Tokens to a pipeline of commands with their redirections |
| `src/exec.c` | Running a pipeline: processes, pipes, redirections, waiting |
| `src/builtins.c` | Commands the shell runs itself, plus the history list |
| `src/jobs.c` | Background job table and child reaping |
| `src/util.c` | Allocation wrappers, a growable string, error reporting |

A line of input moves through those stages in order: `lex()` produces tokens, `parse()` turns them into a `pipeline`, `exec_pipeline()` runs it and returns the status that `$?` will report.

**Lexer.**
Words are assembled character by character into a growable buffer, which is what makes quoting simple to get right.
Single quotes copy literally, double quotes copy but still expand `$` and honour a backslash before `"`, `\` or `$`, and outside quotes a backslash escapes the next character.
Since quotes only change how characters reach the buffer, `"x""y"` falls out as the single word `xy` without any special case.
Expansion happens during that copy, so `$?`, `$NAME` and `${NAME}` are resolved before the parser ever sees a word.

**Parser.**
The token list becomes an array of `command` structures, each holding a NULL-terminated `argv` ready for `execvp` plus its input file, output file and error file with append flags.
A `|` starts the next stage, a redirection operator consumes the word after it, and `&` is accepted only as the last token.
Everything else is rejected here with a syntax error, so the executor never has to validate anything.

**Pipelines.**
For a pipeline of n commands the shell forks n children, calling `pipe()` before each stage but the last.
Each child moves the read end of the previous pipe onto descriptor 0 and the write end of the next one onto descriptor 1 with `dup2()`, then closes the originals.
Redirections are applied after that wiring, so a file named on the command line beats the pipe.
The parent closes every pipe end as soon as a child owns it, since a reader only sees end of file once no process still holds the write end open.
A builtin on its own runs in the shell itself, with descriptors 0, 1 and 2 saved and restored around it, because otherwise `cd` and `export` could not change anything.
Inside a pipeline it runs in a child like any other command, so its effects are local to that child.

**Jobs and signals.**
The shell puts itself in its own process group and gives each pipeline a new group led by its first child, so a pipeline can be signalled as a unit.
Before a foreground job runs the shell hands it the terminal with `tcsetpgrp()` and takes it back afterwards, which is exactly why `Ctrl-C` reaches the job and not the shell.
The shell's own `SIGINT` handler does nothing except make the pending read fail with `EINTR`, so `Ctrl-C` at the prompt just abandons the half-typed line.
Background children are collected by a `SIGCHLD` handler that only calls `waitpid()` with `WNOHANG` and marks list entries finished, since a handler cannot safely do more than that.
The main loop blocks `SIGCHLD` while it prints and frees those entries, and holds the same block from the first `fork` of a pipeline until the job is registered or reaped, so the handler can never collect a child the shell does not yet know about.

## What it does not do

- No control flow: no `if`, `while`, `for`, `case` or functions.
- No `;`, `&&` or `||`, so one line is one pipeline.
- No globbing, no tilde expansion in arguments, no command substitution, no arithmetic.
- No `2>&1`, no here-documents, and no descriptors beyond 0, 1 and 2.
- Expansion never splits into more words, and an unset variable leaves an empty argument rather than disappearing.
- Variables live in the environment only, so `export NAME=value` is how one is set and a bare `NAME=value` is not an assignment.
- `Ctrl-Z` is ignored and there is no `fg` or `bg`, since a stopped job could never be resumed.
- No line editing, no completion, no arrow-key recall; input goes through `fgets` and history is kept in memory for the session only.

## Tests

`tests/run_tests.sh` runs 41 cases against the built binary, feeding each one a small script and comparing stdout, stderr or the exit status.
They cover quoting and escapes, expansion and `$?`, every builtin, two and three stage pipelines, each redirection form and its combination with pipes, background jobs and the `jobs` listing, syntax errors, and reading commands from a file as well as from stdin.
