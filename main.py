import sys
import subprocess
import shlex
import os
import platform


def shell_loop():
    while True:
        cwd = os.getcwd()
        sys.stdout.write(f"{cwd} >> ")
        sys.stdout.flush()
        result = run_external_command(sys.stdin.readline().strip())
        if result == "exit":
            break

def run_external_command(command):
    if command == "":
        return
    if command == "exit":
        return "exit"

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

def main():
    shell_loop()

if __name__ == "__main__":
    main()
