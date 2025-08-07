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
        # Default to home directory if no path is given
        if len(args) < 2:
            path = os.path.expanduser("~")
        else:
            path = args[1]

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
    if (cmd_name == "cd"):
        return handle_cd(args)
    elif cmd_name == "pwd":
        return handle_pwd()
    elif cmd_name == "clear":
        return handle_clear()
    elif cmd_name == "help":
        return handle_help()
    elif cmd_name == "exit":
        return "exit"


def run_external_command(command):
    if command == "":
        return
    if command == "exit":
        return "exit"
    if is_builtin_command(command):
        return handle_builtin_command(command)


    use_shell = platform.system() == "Windows"

    try:
        if use_shell:
            # On Windows, use shell=True and pass raw command
            subprocess.run(command, shell=True)
        else:
            # On Unix, split command and pass as list
            args = shlex.split(command)
            subprocess.run(args)
    except FileNotFoundError:
        print("Command not found")
    except PermissionError:
        print("You do not have permission to run this")
    except KeyboardInterrupt:
        print()  # Move to next line without crashing
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


def shell_loop():
    while True:
        cwd = os.getcwd()
        sys.stdout.write(f"{cwd} >> ")
        sys.stdout.flush()
        result = run_external_command(sys.stdin.readline().strip())
        if result == "exit":
            break

def main():
    shell_loop()

if __name__ == "__main__":
    main()
