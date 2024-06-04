# My Custom Shell

## Overview

This is a custom shell program written in C that supports a variety of features, including background execution, output redirection, piping, and built-in commands.

## Features

1. **Basic Command Execution**: Executes standard shell commands.
2. **Background Execution**: Commands can be run in the background using `&`.
3. **Output Redirection**: Redirects output to a file using `>` and appends using `>>`.
4. **Error Redirection**: Redirects stderr to a file using `2>`.
5. **Built-in Commands**:
   - `cd <directory>`: Changes the current directory.
   - `echo <arguments>`: Prints the arguments to stdout.
   - `status`: Prints the status of the last executed command.
   - `quit`: Exits the shell.
   - `!!`: Repeats the last command.
6. **Signal Handling**: Captures `Ctrl-C` and prints a message instead of terminating the shell.
7. **Piping**: Supports chaining commands using pipes (`|`).

## Compilation

To compile the shell program, use the following command:

```bash
gcc shell3.c -o myshell
