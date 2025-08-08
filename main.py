import sys
import subprocess
import shlex
import os
import platform

BUILTINS = {"cd", "pwd", "clear", "exit", "help"}


def is_builtin_command(command):
    if command.strip() == "":
        return False
    try:
        cmd_name = shlex.split(command)[0]
        return cmd_name in BUILTINS
    except IndexError:
        return False


def handle_help():
    help_text = """
Custom Shell Help

Built-in Commands:
  cd [path]     Change directory (defaults to home if no path is given)
  pwd           Print the current working directory
  clear         Clear the terminal screen
  exit          Exit the shell
  help          Show this help message

External Commands:
  Any other executable in your system PATH (e.g., python, ping, dir)

"""
    print(help_text)


def handle_pwd():
    print(os.getcwd())


def handle_clear():
    if platform.system() == "Windows":
        os.system("cls")
    else:
        os.system("clear")


def handle_cd(args):
    try:
        path = os.path.expanduser("~") if len(args) < 2 else args[1]
        os.chdir(path)
    except FileNotFoundError:
        print(f"cd: no such file or directory: {path}")
    except NotADirectoryError:
        print(f"cd: not a directory: {path}")
    except PermissionError:
        print(f"cd: permission denied: {path}")
    except Exception as e:
        print(f"cd: error: {e}")


def handle_builtin_command(command):
    args = shlex.split(command)
    cmd_name = args[0]
    if cmd_name == "cd":
        return handle_cd(args)
    elif cmd_name == "pwd":
        return handle_pwd()
    elif cmd_name == "clear":
        return handle_clear()
    elif cmd_name == "help":
        return handle_help()
    elif cmd_name == "exit":
        return "exit"


def parse_redirection(tokens):
    command_tokens = []
    stdin_file = None
    stdout_file = None
    append_mode = False

    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token == "<" and i + 1 < len(tokens):
            stdin_file = tokens[i + 1]
            i += 2
        elif token == ">" and i + 1 < len(tokens):
            stdout_file = tokens[i + 1]
            append_mode = False
            i += 2
        elif token == ">>" and i + 1 < len(tokens):
            stdout_file = tokens[i + 1]
            append_mode = True
            i += 2
        else:
            command_tokens.append(token)
            i += 1

    return command_tokens, stdin_file, stdout_file, append_mode


def run_external_command(command):
    if command.strip() == "":
        return
    if is_builtin_command(command):
        return handle_builtin_command(command)

    tokens = shlex.split(command)
    cmd_tokens, stdin_file, stdout_file, append_mode = parse_redirection(tokens)

    stdin = None
    stdout = None
    try:
        if stdin_file:
            stdin = open(stdin_file, "r")
        if stdout_file:
            mode = "a" if append_mode else "w"
            stdout = open(stdout_file, mode)

        subprocess.run(cmd_tokens, stdin=stdin, stdout=stdout, shell=(platform.system() == "Windows"))
    except FileNotFoundError:
        print("Command or file not found")
    except PermissionError:
        print("Permission denied")
    except KeyboardInterrupt:
        print()
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        if stdin:
            stdin.close()
        if stdout:
            stdout.close()


def shell_loop():
    while True:
        sys.stdout.write(f"{os.getcwd()} >> ")
        sys.stdout.flush()
        result = run_external_command(sys.stdin.readline().strip())
        if result == "exit":
            break


def main():
    shell_loop()


if __name__ == "__main__":
    main()
