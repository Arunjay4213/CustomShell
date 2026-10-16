# CustomShell

`cshell` is a POSIX shell written in C11, in about 1800 lines with no dependencies beyond libc.
It handles quoting and `$VAR` expansion, pipelines of any length, redirection with `<`, `>`, `>>` and `2>`, and background jobs with `&`.
It also does enough terminal job control that `Ctrl-C` kills the running command instead of the shell.
Builtins are `cd`, `pwd`, `echo`, `export`, `unset`, `history`, `jobs` and `exit`.

The project started from the shell project in UW-Madison's CS537 (the OSTEP projects) and has grown well past that spec.

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

You need a C11 compiler and `make`.

```sh
make            # optimised build, produces ./cshell
make debug      # -O0 -g3, objects kept separately under build/debug
make test       # build, then run tests/run_tests.sh against the binary
```

Run `./cshell` for a prompt, or `./cshell script.sh` to read commands from a file with no prompt.
Commands piped in on stdin work too, which is how most of the tests drive it.

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

A line of input goes through those stages in order: `lex()` produces tokens, `parse()` turns them into a `pipeline`, and `exec_pipeline()` runs it and returns the status that `$?` will report.

The lexer builds each word character by character in a growable buffer, and that is what makes quoting easy to get right.
Single quotes copy literally.
Double quotes copy too, but still expand `$` and honour a backslash before `"`, `\` or `$`, and outside quotes a backslash escapes the next character.
Since quotes only change how characters reach the buffer, `"x""y"` comes out as the single word `xy` with no special case.
Expansion happens during that copy, so `$?`, `$NAME` and `${NAME}` are resolved before the parser sees a word.

The parser turns the token list into an array of `command` structures.
Each one holds a NULL-terminated `argv` ready for `execvp`, plus its input file, output file and error file with append flags.
A `|` starts the next stage, a redirection operator consumes the word after it, and `&` is accepted only as the last token.
Anything else is rejected here with a syntax error, so the executor never has to validate input.

For a pipeline of n commands the shell forks n children and calls `pipe()` before every stage except the last.
Each child uses `dup2()` to move the read end of the previous pipe onto descriptor 0 and the write end of the next one onto descriptor 1 and then closes the originals.
Redirections are applied after that wiring, so a file named on the command line wins over the pipe.
The parent closes each pipe end as soon as a child owns it, because a reader only sees end of file once no process still holds the write end open.
A builtin on its own runs inside the shell, with descriptors 0, 1 and 2 saved and restored around it; otherwise `cd` and `export` could not change anything.
Inside a pipeline it runs in a child like any other command, so its effects stay in that child.

For job control, the shell puts itself in its own process group and gives each pipeline a new group led by its first child, so the whole pipeline can be signalled at once.
Before a foreground job runs, the shell hands it the terminal with `tcsetpgrp()` and takes it back afterwards, which is why `Ctrl-C` reaches the job and not the shell.
The shell's own `SIGINT` handler does nothing except make the pending read fail with `EINTR`, so `Ctrl-C` at the prompt throws away the half-typed line.
A `SIGCHLD` handler collects background children.
It only calls `waitpid()` with `WNOHANG` and marks list entries as finished, since a signal handler cannot safely do more than that.
The main loop blocks `SIGCHLD` while it prints and frees those entries, and it holds the same block from the first `fork` of a pipeline until the job is registered or reaped, so the handler can never collect a child the shell does not know about yet.

## What it does not do

- No control flow: no `if`, `while`, `for`, `case` or functions.
- No `;`, `&&` or `||`, so one line is one pipeline.
- No globbing, no tilde expansion in arguments, no command substitution, no arithmetic.
- No `2>&1`, no here-documents, and no descriptors beyond 0, 1 and 2.
- Expansion never splits into more words, and an unset variable leaves an empty argument instead of disappearing.
- Variables live in the environment only, so `export NAME=value` is how you set one, and a bare `NAME=value` is not an assignment.
- `Ctrl-Z` is ignored and there is no `fg` or `bg`, since a stopped job could never be resumed.
- No line editing, no completion, no arrow-key recall; input goes through `fgets` and history is kept in memory for the session only.

## Tests

`tests/run_tests.sh` runs each case against the built binary by feeding it a small script and comparing stdout, stderr or the exit status.
The cases cover quoting and escapes, expansion and `$?`, every builtin, two and three stage pipelines, each redirection form on its own and combined with pipes, background jobs and the `jobs` listing, syntax errors, and reading commands from a file as well as from stdin.
