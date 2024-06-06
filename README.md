
# Shell Implementation Documentation

## Overview

This document explains the implementation of a custom shell in C, including signal handling, command parsing, execution, redirection, and piping. The shell supports basic commands, command history, variable assignment, if-else statements, and signal handling for Ctrl-C (SIGINT).

## Components

### Macros and Constants
- `MAX_COMMAND_LEN`: Maximum length of a command (1024).
- `MAX_ARGS`: Maximum number of arguments for a command (10).
- `HISTORY_SIZE`: Maximum number of commands stored in history (50).

### Global Variables
- `command`: Buffer for storing the current command.
- `last_status`: Stores the exit status of the last executed command.
- `last_command`: Pointer to the last executed command.
- `prompt`: The shell prompt (default is "hello:").
- `outfile`, `infile`: File names for output and input redirection.
- `i`, `fd`, `amper`, `redirect`, `piping`, `retid`, `status`, `in_redirect`: Variables for parsing and executing commands.
- `argv`, `argv2`: Arrays for storing command arguments.
- `history`: Array for storing command history.
- `history_index`, `history_count`: Variables for managing command history.
- `variables`, `values`: Arrays for storing shell variables and their values.
- `var_count`: Counter for the number of shell variables.
- `if_block`: Flag for indicating an if-block (not used in current implementation).

### Functions

#### `display_prompt()`
Displays the shell prompt.

#### `handler(int sig)`
Handles the SIGINT signal (Ctrl-C) by displaying a message and resetting the command buffer.

#### `setup_sigint_handler()`
Sets up the SIGINT handler using `sigaction`.

#### `update_last_command(const char *curr_command)`
Updates the `last_command` with the current command using `realloc`.

#### `add_to_history(const char *cmd)`
Adds a command to the history. If history is full, it replaces the oldest entry.

#### `execute_command(char **argv)`
Executes a command using `execvp`.

#### `execute_if_process(char *command)`
Parses and executes if-else statements in the shell.

#### `execute_pipe(char **argv1, char **argv2)`
Executes commands with a pipe (`|`). Sets up pipe file descriptors and forks processes for each side of the pipe.

#### `handle_redirects(int redirect, char *outfile, int in_redirect, char *infile)`
Handles input and output redirection based on the parsed command.

#### `enable_raw_mode()`
Enables raw mode for terminal input, disabling canonical mode and echo.

#### `disable_raw_mode()`
Disables raw mode, restoring canonical mode and echo.

#### `run_shell()`
Main function that runs the shell loop:
- Reads and parses commands.
- Handles special commands (`!!`, `cd`, `echo`, `quit`, `prompt`, `read`).
- Manages command history and variable assignments.
- Handles redirection and pipes.
- Forks and executes commands.

### `main()`
Sets up the SIGINT handler and starts the shell.

## Detailed Functionality

### Command Parsing and Execution
- Commands are read from standard input and stored in the `command` buffer.
- Special commands like `!!`, `cd`, `echo`, `quit`, `prompt`, and `read` are handled separately.
- For other commands, arguments are parsed into the `argv` array.
- Handles input (`<`) and output (`>`, `2>`, `>>`) redirection, as well as pipes (`|`).
- Forks a child process to execute commands using `execvp`.

### Signal Handling
- The shell handles the SIGINT signal to prevent the shell from terminating when Ctrl-C is pressed.
- The `handler` function displays a message and resets the command buffer.

### Command History
- Commands are stored in a circular buffer (`history` array) with a maximum size (`HISTORY_SIZE`).
- The `!!` command recalls the last executed command.

### Shell Variables
- Shell variables are stored in `variables` and `values` arrays.
- Supports variable assignment (`$var = value`) and substitution (`$var`).

### If-Else Statements
- The shell supports basic if-else statements using `if`, `then`, `else`, and `fi`.
- `execute_if_process` parses and executes these conditional statements.

### Redirection and Piping
- Handles input redirection (`<`) and various forms of output redirection (`>`, `2>`, `>>`).
- Supports piping commands using `|`.

### Prompt Customization
- The `prompt` command allows changing the shell prompt (e.g., `prompt = new_prompt`).

### Exiting the Shell
- The `quit` command frees allocated memory and exits the shell.

## Usage

To compile the shell:
```bash
make all
```

To run the shell:
```bash
make run
```
Or
```bash
./myshell
```