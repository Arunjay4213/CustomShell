import sys
import subprocess
import shlex

def main():
    while True:
        sys.stdout.write(">> ")
        sys.stdout.flush()
        command = sys.stdin.readline()
        if (command.strip() == ""):
            continue
        if (command.strip() == "exit"):
            break
        args = shlex.split(command)
        subprocess.run(args, shell=True)


if __name__ == "__main__":
    main()