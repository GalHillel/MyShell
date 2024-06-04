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
int i, fd, amper, redirect, piping, retid, status, in_redirect;
char *argv[MAX_ARGS];
char *argv2[MAX_ARGS];
char *history[HISTORY_SIZE];
int history_index = 0;
int history_count = 0;
int history_cursor = 0;
char *variables[MAX_ARGS];
char *values[MAX_ARGS];
int var_count = 0;
int if_block = 0;

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

void execute_if_process(char *command)
{
    char *_if = strstr(command, "if");
    char *_then = strstr(command, "then");
    char *_else = strstr(command, "else");
    char *_fi = strstr(command, "fi");

    if (!_if ||
        !_then ||
        !_fi)
    {
        printf("execvp: Bad 'if' syntax\n");
        return;
    }

    // separate each part
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

    // check the condition
    int return_stat = system(cond_exec);

    // if the condition is valid
    if (return_stat == 0)
    {
        system(_then);
    }
    else
    {
        // else run the else command
        if (*_else)
        {
            system(_else);
        }
    }
}

void execute_pipe(char **argv1, char **argv2)
{
    int fildes[2];
    pipe(fildes);
    if (fork() == 0)
    {
        close(STDOUT_FILENO);
        dup(fildes[1]);
        close(fildes[1]);
        close(fildes[0]);
        execvp(argv1[0], argv1);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else
    {
        close(STDIN_FILENO);
        dup(fildes[0]);
        close(fildes[0]);
        close(fildes[1]);
        execvp(argv2[0], argv2);
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
                            pos = strlen(command);
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
                            pos = strlen(command);
                            clear_line();
                            display_prompt();
                        }
                        else
                        {
                            command[0] = '\0';
                            pos = 0;
                            clear_line();
                            display_prompt();
                        }
                    }
                }
            }
        }
        else
        {
            command[pos++] = ch;
            display_prompt();
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

        i = 0;
        piping = 0;
        in_redirect = 0;
        token = strtok(command, " ");
        while (token != NULL)
        {
            argv[i] = token;
            token = strtok(NULL, " ");
            i++;
            if (token && strcmp(token, "|") == 0)
            {
                piping = 1;
                argv[i] = NULL;
                i = 0;
                token = strtok(NULL, " ");
                while (token != NULL)
                {
                    argv2[i] = token;
                    token = strtok(NULL, " ");
                    i++;
                }
                argv2[i] = NULL;
                break;
            }
        }
        argv[i] = NULL;

        if (argv[0] == NULL)
            continue;

        if (!strcmp(argv[i - 1], "&"))
        {
            amper = 1;
            argv[i - 1] = NULL;
        }
        else
            amper = 0;

        if (strcmp(argv[0], "cd") == 0)
        {
            if (argv[1] == NULL)
            {
                printf("cd: missing parameter\n");
                last_status = 1;
            }
            else if (chdir(argv[1]) != 0)
            {
                perror("cd");
                last_status = 1;
            }
            else
            {
                last_status = 0;
            }
            continue;
        }

        if (strcmp(argv[0], "echo") == 0)
        {
            if (argv[1] != NULL && strcmp(argv[1], "$?") == 0)
            {
                printf("%d\n", last_status);
            }
            else
            {
                for (int j = 1; argv[j] != NULL; j++)
                {
                    if (argv[j][0] == '$')
                    {
                        char *var_name = argv[j] + 1;
                        int found = 0;
                        for (int k = 0; k < var_count; k++)
                        {
                            if (strcmp(var_name, variables[k]) == 0)
                            {
                                printf("%s ", values[k]);
                                found = 1;
                                break;
                            }
                        }
                        if (!found)
                        {
                            printf(" ");
                        }
                    }
                    else
                    {
                        printf("%s ", argv[j]);
                    }
                }
                printf("\n");
            }
            last_status = 0;
            continue;
        }

        if (strcmp(argv[0], "quit") == 0)
        {
            if (last_command != NULL)
            {
                free(last_command);
            }
            for (int i = 0; i < history_count; i++)
            {
                free(history[i]);
            }
            for (int i = 0; i < var_count; i++)
            {
                free(variables[i]);
                free(values[i]);
            }
            exit(0);
        }

        if (argv[0][0] == '$')
        {
            if (argv[1] && strcmp(argv[1], "=") == 0)
            {
                int found = 0;
                for (int k = 0; k < var_count; k++)
                {
                    if (strcmp(argv[0] + 1, variables[k]) == 0)
                    {
                        values[k] = strdup(argv[2]);
                        found = 1;
                        break;
                    }
                }
                if (!found)
                {
                    variables[var_count] = strdup(argv[0] + 1);
                    values[var_count] = strdup(argv[2]);
                    var_count++;
                }
                last_status = 0;
                continue;
            }
        }

        if (i > 1 && strcmp(argv[i - 2], ">") == 0)
        {
            redirect = 1;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }
        else if (i > 1 && strcmp(argv[i - 2], "2>") == 0)
        {
            redirect = 2;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }
        else if (i > 1 && strcmp(argv[i - 2], ">>") == 0)
        {
            redirect = 3;
            argv[i - 2] = NULL;
            outfile = argv[i - 1];
        }
        else if (i > 1 && strcmp(argv[i - 2], "<") == 0)
        {
            in_redirect = 1;
            argv[i - 2] = NULL;
            infile = argv[i - 1];
        }

        else
        {
            redirect = 0;
        }

        if (strcmp(argv[0], "prompt") == 0 && argv[1] && strcmp(argv[1], "=") == 0)
        {
            strcpy(prompt, argv[2]);
            last_status = 0;
            continue;
        }

        if (strcmp(argv[0], "read") == 0)
        {
            if (argv[1] != NULL)
            {
                printf("%s ", argv[1]);
                char input[MAX_COMMAND_LEN];
                fgets(input, MAX_COMMAND_LEN, stdin);
                input[strlen(input) - 1] = '\0';
                int found = 0;
                for (int k = 0; k < var_count; k++)
                {
                    if (strcmp(argv[1], variables[k]) == 0)
                    {
                        free(values[k]);
                        values[k] = strdup(input);
                        found = 1;
                        break;
                    }
                }
                if (!found)
                {
                    variables[var_count] = strdup(argv[1]);
                    values[var_count] = strdup(input);
                    var_count++;
                }
            }
            continue;
        }

        if (fork() == 0)
        {
            handle_redirects(redirect, outfile, in_redirect, infile);

            if (piping)
            {
                execute_pipe(argv, argv2);
            }
            else
            {
                execute_command(argv);
            }
        }
        else
        {
            if (!amper)
            {
                retid = wait(&status);
                last_status = WEXITSTATUS(status);
            }
        }

        // NOTE: https://www.geeksforgeeks.org/strstr-in-ccpp/
        if (strstr(command, "if") != NULL ||
            strstr(command, "then") != NULL ||
            strstr(command, "else") != NULL ||
            strstr(command, "fi") != NULL)
        {
            execute_if_process(command);
            continue;
        }
    }
}

int main()
{
    setup_sigint_handler();
    run_shell();
    return 0;
}
