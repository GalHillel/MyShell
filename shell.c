#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <termios.h>

#define MAX_COMMAND_LEN 1024
#define MAX_ARGS 10
#define HISTORY_SIZE 50

char command[MAX_COMMAND_LEN];
int last_status = 0;
char *last_command = NULL;
char prompt[] = "hello:";
char *outfile;
char *infile;
int fd, amper, redirect, in_redirect;
char **argvs[10]; // Support for up to 10 commands in a pipeline
int var_count = 0;
char *variables[MAX_ARGS];
char *values[MAX_ARGS];
char *history[HISTORY_SIZE];
int history_index = 0;
int history_count = 0;
int history_cursor = 0;

struct sigaction sa;

void handle_sigint(int sig)
{
    printf("\nYou typed Control-C!\n");
    sigaction(SIGINT, &sa, NULL);
}

void setup_sigint_handler()
{
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
}

void update_last_command(const char *curr_command)
{
    uint string_size = strlen(curr_command);
    last_command = realloc(last_command, string_size + 1);
    if (last_command == NULL)
    {
        perror("error in realloc update command");
        exit(EXIT_FAILURE);
    }
    strcpy(last_command, curr_command);
}

void add_to_history(const char *cmd)
{
    if (history_count < HISTORY_SIZE)
    {
        history[history_count] = strdup(cmd);
        history_count++;
    }
    else
    {
        free(history[history_index]);
        history[history_index] = strdup(cmd);
        history_index = (history_index + 1) % HISTORY_SIZE;
    }
    history_cursor = history_count; // Reset cursor
}

void execute_command(char **argv)
{
    if (execvp(argv[0], argv) == -1)
    {
        perror("execvp");
        exit(EXIT_FAILURE);
    }
}

void handle_redirects(int redirect, char *outfile, int in_redirect, char *infile)
{
    if (in_redirect == 1)
    {
        fd = open(infile, O_RDONLY);
        if (fd == -1)
        {
            perror("open for input redirection");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (redirect == 1)
    {
        fd = creat(outfile, 0660);
        close(STDOUT_FILENO);
        dup(fd);
        close(fd);
    }
    else if (redirect == 2)
    {
        fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("2> on open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
    else if (redirect == 3)
    {
        fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1)
        {
            perror(">> on open");
            exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

void enable_raw_mode()
{
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void disable_raw_mode()
{
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void display_prompt()
{
    printf("\r%s %s", prompt, command);
    fflush(stdout);
}

void clear_line()
{
    printf("\r\033[K");
    fflush(stdout);
}

void handle_input()
{
    char ch;
    int len = 0;
    int pos = 0;
    memset(command, 0, MAX_COMMAND_LEN);
    display_prompt();

    while (read(STDIN_FILENO, &ch, 1) == 1)
    {
        if (ch == '\n')
        {
            command[pos] = '\0';
            printf("\n");
            break;
        }
        else if (ch == 127 || ch == '\b') // Handle backspace
        {
            if (pos > 0)
            {
                len--;
                pos--;
                command[pos] = '\0';
                clear_line();
                display_prompt();
            }
        }
        else if (ch == 27) // Arrow keys
        {
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1)
            {
                if (seq[0] == '[')
                {
                    if (seq[1] == 'A') // Up arrow
                    {
                        if (history_cursor > 0)
                        {
                            history_cursor--;
                            strcpy(command, history[history_cursor]);
                            pos = len = strlen(command);
                            clear_line();
                            display_prompt();
                        }
                    }
                    else if (seq[1] == 'B') // Down arrow
                    {
                        if (history_cursor < history_count - 1)
                        {
                            history_cursor++;
                            strcpy(command, history[history_cursor]);
                            pos = len = strlen(command);
                            clear_line();
                            display_prompt();
                        }
                        else
                        {
                            command[0] = '\0';
                            pos = len = 0;
                            clear_line();
                            display_prompt();
                        }
                    }
                    else if (seq[1] == 'C') // Right arrow
                    {
                        if (pos < len)
                        {
                            pos++;
                            printf("\033[C");
                            fflush(stdout);
                        }
                    }
                    else if (seq[1] == 'D') // Left arrow
                    {
                        if (pos > 0)
                        {
                            pos--;
                            printf("\033[D");
                            fflush(stdout);
                        }
                    }
                }
            }
        }
        else
        {
            command[pos++] = ch;
            len++;
            clear_line();
            display_prompt();
        }
    }
}

void parse_command(char *cmd, char ***argv, int *argc)
{
    *argc = 0;
    char *token = strtok(cmd, " ");
    while (token != NULL)
    {
        (*argv)[*argc] = token;
        (*argc)++;
        token = strtok(NULL, " ");
    }
    (*argv)[*argc] = NULL;
}

void execute_if_process(char *command)
{
    char *_if = strstr(command, "if");
    char *_then = strstr(command, "then");
    char *_else = strstr(command, "else");
    char *_fi = strstr(command, "fi");

    if (!_if || !_then || !_fi)
    {
        printf("execvp: Bad 'if' syntax\n");
        return;
    }

    *_then = '\0';
    _then += 5;
    char *cond_exec = _if + 3;

    if (_else)
    {
        *_else = '\0';
        _else += 5;
        *_fi = '\0';
    }
    else
    {
        _else = _fi;
        _else[0] = '\0';
    }

    int return_stat = system(cond_exec);

    if (return_stat == 0)
    {
        system(_then);
    }
    else
    {
        if (*_else)
        {
            system(_else);
        }
    }
}

void run_shell()
{
    char *token;

    while (1)
    {
        enable_raw_mode();
        handle_input();
        disable_raw_mode();

        if (strcmp(command, "!!") == 0)
        {
            if (last_command == NULL)
            {
                printf("No command in history.\n");
                continue;
            }
            strcpy(command, last_command);
        }
        update_last_command(command);
        add_to_history(command);

        if (strstr(command, "if ") != NULL && strstr(command, " then ") != NULL && strstr(command, " fi") != NULL)
        {
            execute_if_process(command);
            continue;
        }

        int argc = 0;
        int num_pipes = 0;
        char *commands[10];
        char *token = strtok(command, "|");
        while (token != NULL)
        {
            commands[num_pipes++] = token;
            token = strtok(NULL, "|");
        }

        for (int i = 0; i < num_pipes; i++)
        {
            argvs[i] = (char **)malloc(MAX_ARGS * sizeof(char *));
            parse_command(commands[i], &argvs[i], &argc);
        }

        int is_background = 0;
        if (argc > 0 && strcmp(argvs[num_pipes - 1][argc - 1], "&") == 0)
        {
            is_background = 1;
            argvs[num_pipes - 1][argc - 1] = NULL;
        }

        if (argvs[0] == NULL)
            continue;

        if (strcmp(argvs[0][0], "cd") == 0)
        {
            if (argvs[0][1] == NULL)
            {
                char *home = getenv("HOME");
                if (home == NULL)
                {
                    printf("cd: HOME environment variable not set\n");
                    last_status = 1;
                }
                else if (chdir(home) != 0)
                {
                    perror("chdir");
                    last_status = 1;
                }
                else
                {
                    last_status = 0;
                }
            }
            else if (chdir(argvs[0][1]) != 0)
            {
                perror("chdir");
                last_status = 1;
            }
            else
            {
                last_status = 0;
            }
        }

        if (strcmp(argvs[0][0], "echo") == 0)
        {
            if (argvs[0][1] != NULL && strcmp(argvs[0][1], "$?") == 0)
            {
                printf("%d\n", last_status);
            }
            else
            {
                for (int i = 1; argvs[0][i] != NULL; i++)
                {
                    if (argvs[0][i][0] == '$')
                    {
                        for (int k = 0; k < var_count; k++)
                        {
                            if (strcmp(argvs[0][i] + 1, variables[k]) == 0)
                            {
                                printf("%s ", values[k]);
                                break;
                            }
                        }
                    }
                    else
                    {
                        printf("%s ", argvs[0][i]);
                    }
                }
                printf("\n");
            }
            continue;
        }

        if (strcmp(argvs[0][0], "quit") == 0)
        {
            printf("Exiting shell...\n");
            free(last_command);
            for (int i = 0; i < history_count; i++)
            {
                free(history[i]);
            }
            for (int i = 0; i < var_count; i++)
            {
                free(variables[i]);
                free(values[i]);
            }
            for (int i = 0; i < num_pipes; i++)
            {
                free(argvs[i]);
            }
            exit(0);
        }

        if (argvs[0][0][0] == '$')
        {
            if (argvs[0][1] && strcmp(argvs[0][1], "=") == 0)
            {
                int found = 0;
                for (int k = 0; k < var_count; k++)
                {
                    if (strcmp(argvs[0][0] + 1, variables[k]) == 0)
                    {
                        values[k] = strdup(argvs[0][2]);
                        found = 1;
                        break;
                    }
                }
                if (!found)
                {
                    variables[var_count] = strdup(argvs[0][0] + 1);
                    values[var_count] = strdup(argvs[0][2]);
                    var_count++;
                }
                last_status = 0;
                continue;
            }
        }

        if (argc > 1 && strcmp(argvs[0][argc - 2], ">") == 0)
        {
            redirect = 1;
            argvs[0][argc - 2] = NULL;
            outfile = argvs[0][argc - 1];
        }
        else if (argc > 1 && strcmp(argvs[0][argc - 2], "2>") == 0)
        {
            redirect = 2;
            argvs[0][argc - 2] = NULL;
            outfile = argvs[0][argc - 1];
        }
        else if (argc > 1 && strcmp(argvs[0][argc - 2], ">>") == 0)
        {
            redirect = 3;
            argvs[0][argc - 2] = NULL;
            outfile = argvs[0][argc - 1];
        }
        else if (argc > 1 && strcmp(argvs[0][argc - 2], "<") == 0)
        {
            in_redirect = 1;
            argvs[0][argc - 2] = NULL;
            infile = argvs[0][argc - 1];
        }
        else
        {
            redirect = 0;
        }

        if (strcmp(argvs[0][0], "prompt") == 0 && argvs[0][1] && strcmp(argvs[0][1], "=") == 0)
        {
            strcpy(prompt, argvs[0][2]);
            last_status = 0;
            continue;
        }

        if (strcmp(argvs[0][0], "read") == 0)
        {
            if (argvs[0][1] != NULL)
            {
                printf("%s ", argvs[0][1]);
                char input[MAX_COMMAND_LEN];
                fgets(input, MAX_COMMAND_LEN, stdin);
                input[strlen(input) - 1] = '\0';
                int found = 0;
                for (int k = 0; k < var_count; k++)
                {
                    if (strcmp(argvs[0][1], variables[k]) == 0)
                    {
                        free(values[k]);
                        values[k] = strdup(input);
                        found = 1;
                        break;
                    }
                }
                if (!found)
                {
                    variables[var_count] = strdup(argvs[0][1]);
                    values[var_count] = strdup(input);
                    var_count++;
                }
            }
            continue;
        }

        int pid;
        for (int i = 0; i < num_pipes; i++)
        {
            int fildes[2];
            pipe(fildes);
            if ((pid = fork()) == 0)
            {
                if (i != 0)
                {
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (i != num_pipes - 1)
                {
                    dup2(fildes[1], STDOUT_FILENO);
                    close(fildes[1]);
                }
                close(fildes[0]);

                handle_redirects(redirect, outfile, in_redirect, infile);

                execvp(argvs[i][0], argvs[i]);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            else
            {
                wait(&last_status);
                close(fildes[1]);
                fd = fildes[0];
            }
        }
    }
}

int main()
{
    setup_sigint_handler();
    run_shell();
    return 0;
}
