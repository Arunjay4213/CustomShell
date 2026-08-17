#!/bin/sh
#
# End-to-end tests for cshell.
#
# Every case feeds a small script to the shell binary and compares what comes
# back on stdout, on stderr or in the exit status. Run it with:
#
#   make test
#   tests/run_tests.sh ./cshell

set -u

BIN=${1:-./cshell}

if [ ! -x "$BIN" ]; then
    echo "cannot run tests: $BIN is not an executable, build it with make" >&2
    exit 1
fi

# Absolute path, because the tests run from a scratch directory.
BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")

WORK=$(mktemp -d)
WORK=$(cd "$WORK" && pwd -P)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
cd "$WORK" || exit 1

passed=0
failed=0

# run <script>: executes the script text and prints its stdout. Anything the
# shell writes to stderr is left in $WORK/stderr.
run() {
    printf '%s\n' "$1" > "$WORK/script"
    "$BIN" "$WORK/script" 2> "$WORK/stderr"
}

report_pass() {
    passed=$((passed + 1))
    echo "ok   - $1"
}

report_fail() {
    failed=$((failed + 1))
    echo "FAIL - $1"
    shift
    for line in "$@"; do
        echo "       $line"
    done
}

check() { # name expected actual
    if [ "$2" = "$3" ]; then
        report_pass "$1"
    else
        report_fail "$1" "expected: [$2]" "actual:   [$3]"
    fi
}

check_contains() { # name needle haystack
    case "$3" in
    *"$2"*) report_pass "$1" ;;
    *) report_fail "$1" "expected to contain: [$2]" "actual: [$3]" ;;
    esac
}

echo "running cshell tests with $BIN"

# --- words, quoting and escapes ------------------------------------------

check "echo with several arguments" \
    "hello world" \
    "$(run 'echo hello world')"

check "runs of spaces collapse outside quotes" \
    "a b c" \
    "$(run 'echo   a    b   c')"

check "double quotes keep spacing" \
    "two   spaces" \
    "$(run "echo \"two   spaces\"")"

check "single quotes are literal" \
    'single $HOME and "quotes"' \
    "$(run "echo 'single \$HOME and \"quotes\"'")"

check "backslash escapes a space" \
    "a b" \
    "$(run 'echo a\ b')"

check "adjacent quoted parts join into one word" \
    "xy" \
    "$(run 'echo "x""y"')"

check "echo -n omits the newline" \
    "nonewline" \
    "$(run 'echo -n nonewline')"

check "a comment is ignored" \
    "before" \
    "$(run 'echo before # this is a comment')"

# --- expansion ------------------------------------------------------------

check "exported variable expands" \
    "value value! " \
    "$(run 'export NAME=value
echo -n "$NAME ${NAME}! "')"

check "unset variable expands to nothing" \
    "[]" \
    "$(run 'export GONE=here
unset GONE
echo "[$GONE]"')"

check "expansion does not happen in single quotes" \
    '$NAME' \
    "$(run 'export NAME=value
echo '"'"'$NAME'"'"'')"

check "successful command sets \$? to 0" \
    "0" \
    "$(run 'true
echo $?')"

check "failing command sets \$? to 1" \
    "1" \
    "$(run 'false
echo $?')"

check "unknown command sets \$? to 127" \
    "127" \
    "$(run 'definitely_not_a_real_command
echo $?')"

# --- builtins -------------------------------------------------------------

check "cd changes the working directory" \
    "$WORK" \
    "$(run "cd $WORK
pwd")"

mkdir -p "$WORK/sub"
check "cd - returns to the previous directory" \
    "$WORK
$WORK/sub
$WORK" \
    "$(run "cd $WORK
pwd
cd $WORK/sub
pwd
cd -")"

check "cd to a missing directory reports an error" \
    "1" \
    "$(run 'cd /no/such/directory
echo $?')"
check_contains "cd error mentions the directory" \
    "/no/such/directory" \
    "$(cat "$WORK/stderr")"

check "history lists the commands that ran" \
    "    1  echo first
    2  echo second
    3  history" \
    "$(run 'echo first
echo second
history' | grep -v '^first$\|^second$')"

check "exit passes its argument on as the status" \
    "7" \
    "$(run 'exit 7' > /dev/null; echo $?)"

check "exit stops the script" \
    "reached" \
    "$(run 'echo reached
exit 0
echo not reached')"

# --- pipelines ------------------------------------------------------------

check "two stage pipeline" \
    "HELLO" \
    "$(run 'echo hello | tr a-z A-Z')"

check "three stage pipeline" \
    "2" \
    "$(run 'printf "a\nb\nc\n" | grep -v b | wc -l' | tr -d ' ')"

check "pipeline status comes from the last stage" \
    "0" \
    "$(run 'false | true
echo $?')"

check "a builtin works inside a pipeline" \
    "3" \
    "$(run 'echo one
echo two
history | wc -l' | tail -n 1 | tr -d ' ')"

# --- redirection ----------------------------------------------------------

check "output redirection writes the file" \
    "written" \
    "$(run "echo written > $WORK/out
cat $WORK/out")"

check "append adds to the file" \
    "first
second" \
    "$(run "echo first > $WORK/out
echo second >> $WORK/out
cat $WORK/out")"

check "input redirection reads the file" \
    "first
second" \
    "$(run "cat < $WORK/out")"

check "output redirection truncates" \
    "only" \
    "$(run "echo only > $WORK/out
cat < $WORK/out")"

run "definitely_not_a_real_command 2> $WORK/err" > /dev/null
check_contains "stderr redirection captures the error" \
    "command not found" \
    "$(cat "$WORK/err")"

check "redirection combines with a pipeline" \
    "FIRST
SECOND" \
    "$(run "printf 'first\nsecond\n' > $WORK/in
cat < $WORK/in | tr a-z A-Z > $WORK/out
cat $WORK/out")"

check "a builtin can be redirected" \
    "$WORK" \
    "$(run "cd $WORK
pwd > $WORK/out
cat $WORK/out")"

# --- background jobs ------------------------------------------------------

check_contains "background job is reported when it starts" \
    "[1]" \
    "$(run 'sleep 1 &')"

check_contains "jobs lists a running background job" \
    "Running" \
    "$(run 'sleep 1 &
jobs')"

check_contains "a finished background job is announced" \
    "Done" \
    "$(run 'sleep 1 &
sleep 2
echo waited')"

check "the shell does not wait for a background job" \
    "immediate" \
    "$(run 'sleep 5 &
echo immediate' | tail -n 1)"

# --- errors and input modes ----------------------------------------------

run 'echo unfinished |' > /dev/null
check_contains "an incomplete pipeline is a syntax error" \
    "syntax error" \
    "$(cat "$WORK/stderr")"

run 'echo "unterminated' > /dev/null
check_contains "an unterminated quote is reported" \
    "unterminated double quote" \
    "$(cat "$WORK/stderr")"

run '> lonely' > /dev/null
check_contains "a redirection without a command is a syntax error" \
    "syntax error" \
    "$(cat "$WORK/stderr")"

check "commands can be read from stdin" \
    "from stdin" \
    "$(printf 'echo from stdin\n' | "$BIN")"

check "a missing script file is reported" \
    "1" \
    "$("$BIN" "$WORK/no_such_script" 2> /dev/null; echo $?)"

# --- summary --------------------------------------------------------------

echo
echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
