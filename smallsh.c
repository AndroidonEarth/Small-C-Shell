/*************************************************************************************************************************
 * 
 * NAME
 *    smallsh.c - a small shell program
 * SYNOPSIS
 *    A basic shell to run command line instructions and return results similar to other shells, but without many of
 *       their fancier features.
 *    Allows for redirection of standard input and standard output, and supports both foreground and background
 *       processes (controllable by the command line and by receiving signals).
 *    Supports three built in commands: exit, cd, and status. Also supports comments, which are lines beginning with
 *       the # character.
 * INSTRUCTIONS
 *    Compile the run program with the command line:
 *       gcc -o smallsh smallsh.c
 *    To run the shell, use the command:
 *       smallsh
 *    To exit the shell, use the command:
 *       exit
 * DESCRIPTION
 *    The colon : symbol is the prompt for each command line. The general syntax of a command line is (items in
 *    square brackets are optional):
 *       command [arg1 arg2 ...] [< input_file] [> output_file] [&]
 *    The command must be made up of words separated by spaces.
 *       > The special symbols <, >, and & are recognized, but they must be surrounded by spaces like other words.
 *       > If the command is to be executed in the background, the last word must be &.
 *            If the & character appears anywhere else, it is treated like normal text.
 *       > If the standard input or output is to be redirected, the > or < words followed by a filename word must
 *            appear after all the arguments. Input redirection can appear before or after output redirection.
 *    Does not support any quoting - arguments with spaces in them are not possible.
 *    The common shell operater pipe | is also not available.
 *    Supports command lines with a maximum length of 2048 characters, and a maximum of 512 arguments.
 *       No error checking is performed on the syntax of the command line.
 *    Blank lines and comments are allowed.
 *       > Any line that begins with a # character is a comment and is ignored.
 *       > A blank line does nothing.
 *       > If a blank line or a comment line is received, the shell simply re-prompts for another command.
 * AUTHOR
 *    Written by Andrew Swaim
 *
*************************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#define MAX_ARGS   512          // Maximum number of word arguments that can be used in a command.
#define MAX_CHARS 2048          // Maximum number of characters that a command line can be.
#define STR_BUFFER 256          // Arbitrary string buffer.
#define MAX_PIDS   100          // Maximum number of background PIDs to track.
#define TOK_DELIM  " \t\r\n\a"  // The delimeter for using strtok()

// Create bool type for C89/C99 compilation.
typedef enum { false, true } bool;

// Flag for foreground only mode.
bool fgOnly = false;

// Struct to store commands
//struct Cmds {}

/*************************************************************************************************************************
 * Function Declarations
*************************************************************************************************************************/

void getCmd(char*[], int*, bool*, char[], char[], int);
//void execCmd();
void redirIn(char[], bool);
void redirOut(char[]);
void catchSIGTSTP(int);
void printStatus(int);
void checkbgpids(int[], int*, int*, bool);
void expandpids(char[], char*, int);

/*************************************************************************************************************************
 * Main 
*************************************************************************************************************************/

int main()
{
    // Command variables.
    int argc;                         // The number of arguments in a command. 
    char* argv[MAX_ARGS];             // An array of arguments in a command.
    char inFile[STR_BUFFER];          // The name of an input file for redirection.
    char outFile[STR_BUFFER];         // The name of an output file for redirection.
    bool bg;                          // Flag to determine if a process should be spawned in the background.

    // PID variables.
    int bgpids[MAX_PIDS];             // An array of background process ID's to track.
    int numbgpids = 0;                // The number of background process ID's.
    pid_t spawnPid;                   // Process ID of a spawned child.
    int pid = getpid();               // The pid of the current shell's process ID.

    // Status variables.
    int status = 0;                   // The exit status of a process.
    bool quit = false;                // The exit status of the main shell loop.

    // Set signal handling options.
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    // Ignore SIGINT ^C
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Redirect SIGTSTP ^Z
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Main shell loop.
    int i;
    do {

        // Check if there are any zombies or background processes to kill.
        checkbgpids(bgpids, &numbgpids, &status, false);

        // Get the user input.
        getCmd(argv, &argc, &bg, inFile, outFile, pid);

        // If the command is a comment "#" or is blank (no arguments are captured in either case) then do nothing.
        if (argc == 0 || argv[0] == NULL) { status = 0; }

        // If the command was "EXIT", kill background processes before exiting.
        else if (strcmp(argv[0], "exit") == 0) { checkbgpids(bgpids, &numbgpids, &status, true); quit = true; }

        // If the command was "CD", change directories to the target directory, or HOME if none specified.
        else if (strcmp(argv[0], "cd") == 0) {

            // If argument was given for cd, attempt to change to that directory.
            if ( argv[1] != NULL) {

                if (chdir(argv[1]) != 0) { 
                    fprintf(stderr, "ERROR: Directory does not exist or is not reachable\n"); 
                    fflush(stdout);
                    status = -1;
                }
                else { status = 0; }
            } 

            // Otherwise change to the home directory.
            else { chdir(getenv("HOME")); status = 0; }
        }

        // If the command was "STATUS", display the last exit status value.
        else if (strcmp(argv[0], "status") == 0) { printStatus(status); }

        // If the command was anything else.
        else {
        
            // Fork it.
            spawnPid = fork();
        
            // If the fork failed.
            if (spawnPid < 0) { 
                fprintf(stderr, "ERROR: fork() failure in main()\n"); 
                fflush(stdout); exit(1); 
            }

            // Child process.
            else if (spawnPid == 0) {

                // If background flag is not set, allow foreground process to receive interupt signal ^C.
                if (!bg) { SIGINT_action.sa_handler = SIG_DFL; sigaction(SIGINT, &SIGINT_action, NULL); }

                // Redirect input and output.
                redirIn(inFile, bg);
                redirOut(outFile);

                // Execute process.
                if (execvp(argv[0], argv) < 0) { 
                    fprintf(stderr, "ERROR: command not recognized or cannot be executed\n"); 
                    fflush(stdout); exit(1);
                }

                // If this point is reached, something went wrong.
                fprintf(stderr,"ERROR: Something went wrong in a child process in main()\n"); 
                fflush(stdout); exit(1);
            }
            
            // Parent process.
            else {

                // If the child should be run in the background, add it to the list and continue on.
                if (bg && !fgOnly) { 
                
                    bgpids[numbgpids] = spawnPid; numbgpids++;
                    printf("background pid is %d\n", spawnPid); fflush(stdout);
                }

                // Otherwise wait for the process to finish.
                else { waitpid(spawnPid, &status, 0); }
            }
        }

        // Free memory allocated by strdup in getCmd()
        for (i = 0; i < argc; i++) { free(argv[i]); }
        
    } while (!quit);

    return 0;
}

/*************************************************************************************************************************
 * Function Definitions 
*************************************************************************************************************************/

// Function to prompt for and get user input, and then parse that input into the function arguments. 
void getCmd(char* argv[], int* argc, bool* bg, char inFile[], char outFile[], int pid) {

    // Variables to get user input.
    char* input = NULL;
    size_t inputBuffer = MAX_CHARS;

    // Variables to parse user input.
    int i;
    char* token;
    char arg[STR_BUFFER];

    // Initialize all variables.
    for (i = 0; i < MAX_ARGS; i++) { argv[i] = NULL; }
    memset(inFile, '\0', STR_BUFFER); 
    memset(outFile, '\0', STR_BUFFER);
    *argc = 0; 
    *bg = false;

    // Prompt for and get user input (remove newline character).
    printf(": ");
    fflush(stdout);
    getline(&input, &inputBuffer, stdin);

    // Check for comment or blank line.
    if (input[0] == '#' || strlen(input) == 0) { free(input); return; }

    // Otherwise parse the input, copying each argument into the array while capturing key arguments.
    token = strtok(input, TOK_DELIM);
    while (token != NULL) {

        // Check for input file redirection
        if (strcmp(token, "<") == 0 && inFile[0] == '\0') {

            // Get the next argument and assign it as the input file name, with any pids expanded.
            token = strtok(NULL, TOK_DELIM);
            expandpids(arg, token, pid);
            strcpy(inFile, arg);
        }

        // Check for output file redirection
        else if (strcmp(token, ">") == 0 && outFile[0] == '\0') {

            // Get the next argument and assign it as the output file name, with any pids expanded.
            token = strtok(NULL, TOK_DELIM);
            expandpids(arg, token, pid);
            strcpy(outFile, arg);
        }

        // If the run-in-background command is detected.
        else if (strcmp(token, "&") == 0) { *bg = true; }

        // Otherwise this part of the command is an argument.
        else {

            // Expand any instance of PID argument "$$" and copy result to the array.
            expandpids(arg, token, pid);
            argv[*argc] = strdup(arg);
            (*argc)++;
        }

        // Get the next argument for processing.
        token = strtok(NULL, TOK_DELIM);
    }

    // Free the initial input memory allocated by getline before returning.
    free(input);
}

// Takes a string and puts it into the argument character array with any pid variables "$$" expanded.
void expandpids(char arg[], char* token, int pid) {

    int i, j = 0;
    int pidlen = 0;
    int temp = pid;

    // Initialize the argument variable.
    memset(arg, '\0', STR_BUFFER);

    // Get the length (number of digits) of the shell pid.
    while (temp != 0) { temp /= 10; pidlen++; }

    // Convert to null terminated char array.
    char strpid[pidlen+1];
    sprintf(strpid, "%d", pid);
    strpid[pidlen] = '\0';

    for (i = 0; i < strlen(token); i++) {

        // If a pid expansion variable "$$" is encountered.
        if (token[i] == '$' && token[i+1] == '$') {

            // Check if expanding the pid would overflow the arg buffer.
            if ((strlen(arg) + pidlen) >= STR_BUFFER) {
                
                fprintf(stderr, "ERROR: PID expansion would result in buffer overflow in expandpids()\n"); 
                fflush(stdout); exit(1);
            }

            // Otherwise expand the pid and re-position the iterators.
            else { strcat(arg, strpid); i++; j += pidlen; }
        } 

        // Otherwise copy the character from the token to the argument.
        else { arg[j] = token[i]; j++; }
    }
}

// Takes the name of an input file and redirects stdin depending on if the process is going to run in the background.
void redirIn(char inFile[], bool bg) {

    // Source file descriptor.
    int sourceFD;

    // If this is a background process and no file is specified, redirect stdin to /dev/null
    if (bg && inFile[0] == '\0') {

        // Open /dev/null as the source.
        sourceFD = open("/dev/null", O_RDONLY);
        if (sourceFD < 0) {
            fprintf(stderr, "ERROR: opening /dev/null in redirIn()\n"); 
            fflush(stdout); exit(1);
        }

        // Redirect stdin to /dev/null
        if (dup2(sourceFD, 0) < 0) { 
            fprintf(stderr, "ERROR: redirecting stdin to /dev/null in redirIn()\n"); 
            fflush(stdout); exit(1);
        }
    }

    // Else if an input file is specified, redirect stdin to the specified input file.
    else if (inFile[0] != '\0') {

        // Open inFile as the source.
        sourceFD = open(inFile, O_RDONLY);
        if (sourceFD < 0) {
            fprintf(stderr, "ERROR: opening file %s in redirIn()\n", inFile); 
            fflush(stdout); exit(1);
        }

        // Redirect stdin to the specified input file.
        if (dup2(sourceFD, 0) < 0) {
            fprintf(stderr, "ERROR: redirecting stdin to file %s in redirIn()\n", inFile); 
            fflush(stdout); exit(1);
        }
    }

    // Close on exec.
    fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
}

// Takes an output file name as an argument and redirects stdout to that file.
void redirOut(char outFile[]) {

    // Target file descriptor.
    int targetFD;

    // If an output file is specified, redirect stdout to the specified output file.
    if (outFile[0] != '\0') {
        
        // Open outFile as the target, creating and truncating if necessary.
        targetFD = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD < 0) {
            fprintf(stderr, "ERROR: opening/creating file %s in redirOut()\n", outFile); 
            fflush(stdout); exit(1);
        }

        // Redirect stdout to the specified output file.
        if (dup2(targetFD, 1) < 0) {
            fprintf(stderr, "ERROR: redirecting stdout to file %s in redirOut()\n", outFile); 
            fflush(stdout); exit(1);
        }
    }

    // Close on exec.
    fcntl(targetFD, F_SETFD, FD_CLOEXEC);
}

// Signal handler to catch SIGTSTP and toggle foreground-only mode if caught.
void catchSIGTSTP(int signo) {

   // Toggle foreground-only mode.
   if (fgOnly) {
        
       char* message = "Exiting foreground-only mode\n";
       write(STDOUT_FILENO, message, 29);
       fflush(stdout);
       fgOnly = false;
   } 
   else {

       char* message = "Entering foreground-only mode (& is now ignored)\n";
       write(STDOUT_FILENO, message, 49);
       fflush(stdout);
       fgOnly = true;
   }
}

// Function to check or kill the list of process ID's running in the background.
void checkbgpids(int bgpids[], int* numbgpids, int* status, bool killbgpids) {

    int i, j;
    pid_t childPid;

    // Loop through the background processes and see if any of them are finished.
    for (i = 0; i < *numbgpids; i++) {
        
        // If the kill flag is set, wait for them to finish
        if (killbgpids) { childPid = waitpid(bgpids[i], status, 0); }

        // Otherwise simply check them and move on.
        else { childPid = waitpid(bgpids[i], status, WNOHANG); }

        // If a child process ended, display a message and status and shift the background process ID array down.
        if (childPid > 0) {
            printf("background pid %d is done: ", childPid);
            printStatus(*status);

            // Shift the bgpids array down.
            if (i < *numbgpids) {
                for (j = i; j < (*numbgpids)-1; j++) {

                    bgpids[j] = bgpids[j+1];
                }
                bgpids[*numbgpids] = 0;
                (*numbgpids)--;
            }
        }
    }
}

// Prints the status of the last exited process.
void printStatus(int status) {

    // If the process exited normally
    if (WIFEXITED(status)) { printf("exit value %d\n", WEXITSTATUS(status)); }

    // If the process exited by a signal
    else if (WIFSIGNALED(status)) { printf("terminated by signal %d\n", WTERMSIG(status)); }
}

