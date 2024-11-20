#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

// Function to read and parse a command from standard input
// `cmd` will store the command name
// `par` will store the command parameters
void read_command(char cmd[], char *par[]) {
    char line[1024];       // Buffer to store the input line
    int count = 0, i = 0;  // Counters for line length and number of parameters
    char *array[100], *pch;

    // Read the input line character by character
    while (1) {
        int c = fgetc(stdin);  // Read character by character
        if (c == EOF || c == '\n') break; // Stop when EOF or newline is encountered
        line[count++] = (char)c; // Store the character in the buffer
    }

    // Null-terminate the input line
    line[count] = '\0';

    // If the line is empty, return immediately
    if (count == 0) return;

    // Tokenize the line to separate the command and its parameters
    pch = strtok(line, " \n");
    while (pch != NULL) {
        array[i++] = strdup(pch); // Copy each token into an array
        pch = strtok(NULL, " \n"); // Get the next token
    }

    // Copy the command name into `cmd`
    strcpy(cmd, array[0]);

    // Copy the parameters into the `par` array
    for (int j = 0; j < i; j++) {
        par[j] = array[j];
    }
    par[i] = NULL; // Null-terminate the parameter array
}

// Function to display the prompt in the terminal
void type_prompt() {
    static int first_time = 1;  // Static variable to display the prompt only the first time
    if (first_time) {
        const char *CLEAR_SCREEN_ANSI = " \e[1;1H\e[2J";  // ANSI sequence to clear the screen
        write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);  // Clear the screen
        first_time = 0;  // Ensure it runs only once
    }
    printf("shell-c> ");  // Display the prompt
}

int main() {
    char cmd[100], command[100], *parameters[20];  // Variables for the command and parameters
    char *envp[] = {(char *) "PATH=/bin", 0};  // Define the environment with the PATH

    // Main shell loop
    while (1) {
        type_prompt();  // Display the prompt
        read_command(command, parameters);  // Read the command and parameters

        // Check if the command is "exit", and if so, break out of the loop
        if (strcmp(command, "exit") == 0) {
            break;
        }

        // Create a child process to execute the command
        pid_t pid = fork();
        if (pid != 0) {
            // If it's the parent process, wait for the child to finish
            wait(NULL);
        } else {
            // If it's the child process, execute the command
            strcpy(cmd, "/bin/");  // Prepend "/bin/" to the command
            strcat(cmd, command);   // Concatenate the command name

            // Execute the command using execve
            if (execve(cmd, parameters, envp) == -1) {
                perror("execve failed");  // Print error if execve fails
                exit(EXIT_FAILURE);
            }
        }
    }

    return 0;  // Exit the program
}

