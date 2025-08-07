# 🔧 PyShell – A Custom Python Shell

A minimalist, cross-platform custom shell written in Python, replicating basic shell behavior including built-in commands, external command execution, and I/O redirection (`>`, `>>`, `<`). Designed for educational and experimental use.

---

## 📌 Objective

Build a functional command-line shell in Python that mimics core behavior of Unix shells and Windows `cmd`, including:

- Running external system commands
- Handling built-in commands like `cd`, `pwd`, `clear`, `help`, and `exit`
- Supporting I/O redirection operators (`>`, `>>`, `<`)
- Providing robust error handling and user feedback

---

## 📚 Overview

This shell project aims to:
- Reinforce understanding of `stdin`, `stdout`, `stderr`, and subprocesses
- Provide hands-on practice with parsing, file I/O, and system programming
- Showcase a real-world command interpreter with cross-platform support

---

## 📐 Program Specification

| Component           | Behavior |
|--------------------|----------|
| **Prompt**         | Displays the current working directory followed by `>>` |
| **Input**          | Reads full user commands from standard input |
| **Built-in parser**| Detects and executes internal commands before subprocess fallback |
| **External commands** | Runs via `subprocess.run()` with support for arguments |
| **I/O Redirection** | Detects `>`, `>>`, `<` and reroutes streams to/from files |
| **Exit mechanism** | Gracefully terminates on `exit` |
| **Platform support** | Works on Windows, macOS, and Linux |

---

## ✨ Features

- ✅ Interactive command loop
- ✅ Built-in command support (`cd`, `pwd`, `clear`, `help`, `exit`)
- ✅ Runs any executable in system PATH
- ✅ I/O redirection:
  - `>` Overwrite stdout to file
  - `>>` Append stdout to file
  - `<` Read stdin from file
- ✅ Cross-platform terminal support (`cls` for Windows, `clear` for Unix)
- ✅ Robust error handling for:
  - Invalid commands
  - File/permission errors
  - Keyboard interrupts
- ✅ Code organized into modular functions for clarity and maintainability

---

## 🧾 Built-in Commands

| Command | Description |
|--------|-------------|
| `cd [path]` | Change the current working directory. Defaults to home if no path provided. |
| `pwd` | Show the current directory path |
| `clear` | Clear the terminal screen |
| `help` | Display available commands and usage |
| `exit` | Exit the shell gracefully |

---

## 📤 I/O Redirection Support

| Operator | Description |
|----------|-------------|
| `>` | Redirect stdout to a file (overwrite) |
| `>>` | Redirect stdout to a file (append) |
| `<` | Redirect stdin from a file |

**Examples:**
```bash
echo Hello > out.txt        # Write 'Hello' to out.txt
echo More >> out.txt        # Append 'More' to out.txt
cat < out.txt               # Read from out.txt and print
```

## 🚀 How to Run

### ✅ Requirements

- Python 3.8+
- Works natively on Windows, macOS, and Linux

### ▶️ Run the shell:

```bash
python main.py
```

You should see a prompt like:

```bash
C:\Users\you\ >> 
```

From here, you can type built-in or external commands:

```bash
>> cd ..
>> pwd
>> echo Hello > out.txt
>> cat < out.txt
>> clear
>> help
>> exit
```

---

## 🗂 Project Structure

```
pyshell/
│
├── main.py           # Entry point with shell loop and dispatcher
├── README.md         # Project documentation
└── requirements.txt  # (optional) Dependencies
```

---

## 🧠 Educational Value

This project demonstrates:
- Shell parsing logic
- I/O stream control (`stdin`, `stdout`)
- Subprocess management
- Error handling and system-level interaction
- Portable code design with OS-specific behavior

---
