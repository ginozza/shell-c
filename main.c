#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 100
#define MAX_PIPES 10
#define HIST_SIZE 20

char *history[HIST_SIZE];
int history_index = 0;
int history_pos = 0;

void handle_sigint(int sig) {
    printf("\n^C\n$");
    fflush(stdout);
}

void save_history(char *command) {
    if (command[0] == '\0') {
        return;
    }

    free(history[history_index]);
    history[history_index] = strdup(command);
    history_index = (history_index + 1) % HIST_SIZE;
    if (history_pos >= HIST_SIZE) {
        history_pos = HIST_SIZE - 1;
    }
}

void print_history() {
    for (int i = 0; i < HIST_SIZE && history[i] != NULL; i++) {
        printf("%d: %s\n", i + 1, history[i]);
    }
}

void execute_single_command(char *command) {
    char *args[MAX_ARGS];
    char *token = strtok(command, " ");
    int i = 0;

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (args[0] == NULL) {
        return;
    }

    int in_redirect = -1, out_redirect = -1;
    for (int j = 0; args[j] != NULL; j++) {
        if (strcmp(args[j], ">") == 0) {
            out_redirect = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            args[j] = NULL;
        } else if (strcmp(args[j], "<") == 0) {
            in_redirect = open(args[j + 1], O_RDONLY);
            args[j] = NULL;
        }
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Failure creating child process");
        return;
    } else if (pid == 0) {
        if (in_redirect != -1) {
            dup2(in_redirect, STDIN_FILENO);
            close(in_redirect);
        }
        if (out_redirect != -1) {
            dup2(out_redirect, STDOUT_FILENO);
            close(out_redirect);
        }

        if (execvp(args[0], args) == -1) {
            perror("Failure to execute command");
        }
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

void execute_piped_commands(char *commands[], int n) {
    int pipefds[2 * (n - 1)];
    pid_t pids[n];

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("Failed to create the pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            if (i > 0) {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < n - 1) {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            for (int j = 0; j < 2 * (n - 1); j++) {
                close(pipefds[j]);
            }

            execute_single_command(commands[i]);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 2 * (n - 1); i++) {
        close(pipefds[i]);
    }

    for (int i = 0; i < n; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

void enable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void disable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

int main() {
    signal(SIGINT, handle_sigint);

    char command[MAX_COMMAND_LENGTH];

    printf("Shell-C (Type 'exit' to leave)\n");

    while (1) {
        history_pos = history_index;

        enable_raw_mode();

        printf("$ ");
        fflush(stdout);

        int i = 0;
        while (1) {
            char c = getchar();
            if (c == 10) {
                command[i] = '\0';
                break;
            } else if (c == 3) {  
                command[0] = '\0';
                break;
            } else if (c == 27) { 
                getchar();  
                char arrow_key = getchar();
                if (arrow_key == 'A' && history_pos > 0) {  
                    history_pos--;
                    strcpy(command, history[history_pos]);
                    printf("\r$ %s", command);
                    i = strlen(command);
                } else if (arrow_key == 'B' && history_pos < history_index) {  
                    history_pos++;
                    if (history_pos < history_index) {
                        strcpy(command, history[history_pos]);
                        printf("\r$ %s", command);
                    } else {
                        command[0] = '\0';
                        printf("\r$ ");
                    }
                    i = strlen(command);
                }
            } else if (c == 127) {  
                if (i > 0) {
                    i--;
                    printf("\b \b");  
                }
            } else {
                command[i++] = c;
                printf("%c", c);
            }
        }

        disable_raw_mode();

        save_history(command);

        if (strcmp(command, "exit") == 0) {
            printf("Leaving...\n");
            break;
        }

        char *commands[MAX_PIPES];
        int num_commands = 0;

        char *pipe_token = strtok(command, "|");
        while (pipe_token != NULL && num_commands < MAX_PIPES) {
            commands[num_commands++] = pipe_token;
            pipe_token = strtok(NULL, "|");
        }

        printf("\n");

        if (num_commands > 1) {
            execute_piped_commands(commands, num_commands);
        } else {
            execute_single_command(commands[0]);
        }

        printf("\n");
    }

    return 0;
}

