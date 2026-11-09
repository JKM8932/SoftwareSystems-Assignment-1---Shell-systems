#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[], char lwd[])
{
    if (lwd != NULL){
        strcpy(shell_prompt, lwd);
        strcat(shell_prompt, " [s3]$");
        // printf("updated directory");                                 debug
    }
    else{
        strcpy(shell_prompt, "[s3]$");
    }
}

// Grab current working directory, store it in string storage lwd
void init_lwd(char lwd[])
{
    getcwd(lwd, MAX_PROMPT_LEN);
}

int is_cd(char line[])
{
    if (line == NULL) return 0;
    if (line[0] == 'c' && line[1] == 'd') return 1;
    // printf("failed") -> has not printed wrongly                      debug
    return 0;
}

void run_cd(char *args[], int argsc, char lwd[])
{
    int result = -1;

    // printf("Triggered run_cd");                                      debug
    if (args[ARG_1] == NULL){
        result = chdir(getenv("HOME"));
    }
    // else if (strcmp(args[ARG_1], ".") == 0){                         no longer need to account for "." and ".." (Passes normally)
    //     result = chdir(".");
    // }
    // else if (strcmp(args[ARG_1], "..") == 0){
    //     // int len = strlen(lwd);                                    depreciated code (not used anymore, not required)
    //     // for (int i = 0; i < len; i++){
    //     //     if (lwd[len - 1 - i] == '/'){
    //     //         lwd[len - 1 - i] = '\0';
    //     //         break;
    //     //     }
    //     // }
    //     result = chdir("..");
    // }
    else{
        result = chdir(args[ARG_1]);
    }
    
    if (result < 0){
        perror("cd failed");
        return;
    }

    if (getcwd(lwd, MAX_PROMPT_LEN) == NULL){
        perror("getcwd for new cd failed");
    }
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[], char lwd[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt, lwd);
    printf("%s", shell_prompt);

    ///See man page of fgets(...)
    if (fgets(line, MAX_LINE, stdin) == NULL)
    {
        perror("fgets failed");
        exit(1);
    }
    ///Remove newline (enter)
    line[strlen(line) - 1] = '\0';
}

void parse_command(char line[], char *args[], int *argsc)
{
    char *token = strtok(line, " ");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1)
    {
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    
    args[*argsc] = NULL; ///args must be null terminated
}

// Function call used to check if redirection is required
// May need to edit to check specifically for the < and > signs as normal sort should be fine
int command_with_redirection(char line[])
{
    for (int i = 0; line[i] != '\0'; i++){
        if (line[i] == '<' || line[i] == '>'){              // if there is ">>", '>' is already detected, there is no need to purposely check for >>
            return 1;
        }
    }
    return 0;
}

///Launch related functions
void child(char *args[], int argsc)
{
    if (argsc == 0) return;                                 // if no argument -> no need to launch program
    execvp(args[ARG_PROGNAME], args);

    fprintf(stderr, "execvp failed\n");
    exit(1);

}

void launch_program(char *args[], int argsc)
{
    if (argsc == 0) return;                                 // if no argument -> no need to launch program
    if (strcmp(args[ARG_PROGNAME], "exit") == 0){
        exit(EXIT_SUCCESS);
    }

    int rc = fork();
    if (rc < 0){                                            // fork failed
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0){                                      // child process
        printf("Child (pid:%d)\n", getpid());
        child(args, argsc);
    }                          
    else{                                                   // parent process
        printf("Parent (pid:%d)\n", getpid());
        waitpid(rc, NULL, 0);
    }             
}

void child_with_output_redirected(char *args[], int argsc, char *out_loc)
{
    if (argsc == 0) return;                                                     // if no argument -> no need to launch program
    int fd_write = -1;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            if (strcmp(args[i], ">>") == 0){
                fd_write = open(out_loc, O_WRONLY | O_CREAT | O_APPEND, 0644);  // O_WRONLY allows writing, O_CREAT allows creation, O_APPEND allows appending
                break;
            }
            else{
                fd_write = open(out_loc, O_WRONLY | O_CREAT | O_TRUNC, 0644);   // O_WRONLY allows writing, O_CREAT allows creation, O_TRUNC allows overwrite
                break;
            }
        }
    }

    if (fd_write==-1){
        perror("Unable to open file to write");
        exit(EXIT_FAILURE);
    }
    printf("File %s linked\n", out_loc);                                        // debug

    dup2(fd_write, STDOUT_FILENO);                                              // dup2 -> duplicate fd_write location to replace STDOUT_FILENO (standard input)
    close(fd_write);                                                            // close original fd_write after
}

void child_with_input_redirected(char *args[], int argsc, char *in_loc)
{
    if (argsc == 0) return;                                                     // if no argument -> no need to launch program
    int fd_read = open(in_loc, O_RDONLY);                                       // setup reading from input_file as read-only
    if (fd_read == -1){
        perror("Unable to open file to read");
        exit(EXIT_FAILURE);
    }
    printf("File %s linked\n", in_loc);                                         // debug

    dup2(fd_read, STDIN_FILENO);                                                // dup2 -> duplicate fd_read location to replace STDIN_FILENO (standard input)
    close(fd_read);                                                             // close original fd_read after
}

void launch_program_with_redirection(char *args[], int argsc)
{
    if (argsc == 0) return;
    // STDIN_FILENO = 0;   input from keyboard(default)
    // STDOUT_FILENO = 1;   normal output to terminal(default)
    // STDERR_FILENO = 2;   error messages

    // Setup to check for input and output file locations
    int index = 0, flagin = 0, flagout = 0;
    char *in_loc;
    char *out_loc;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            in_loc = args[i + 1];
            flagin = 1;
        }
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
            out_loc = args[i + 1];
            flagout = 1;
        }
    }

    // FORK SEGMENT
    if(flagin && flagout){                                                      // redirection at input and output
        int rc = fork();
        if (rc < 0){                                                            // fork failed
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        else if (rc == 0){                                                      // child process
            printf("Child (pid:%d)\n", getpid());
            child_with_input_redirected(args, argsc, in_loc);
            child_with_output_redirected(args, argsc, out_loc);
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0 || strcmp(args[i], "<") == 0 || strcmp(args[i], ">>") == 0) {
                    args[i] = NULL;
                    argsc = i+1;
                    break;
                }
            }
            execvp(args[ARG_PROGNAME], args);
        }                          
        else{                                                                   // parent process
            printf("Parent (pid:%d)\n", getpid());
            waitpid(rc, NULL, 0);
        }        
    }

    else if(flagin){                                                            // redirection at input only
        int rc = fork();
        if (rc < 0){                                                            // fork failed
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        else if (rc == 0){                                                      // child process
            printf("Child (pid:%d)\n", getpid());
            child_with_input_redirected(args, argsc, in_loc);
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], "<") == 0) {
                    args[i] = NULL;
                    argsc = i+1;
                    break;
                }
            }
            execvp(args[ARG_PROGNAME], args);
        }                          
        else{                                                                   // parent process
            printf("Parent (pid:%d)\n", getpid());
            waitpid(rc, NULL, 0);
        }
    }

    else if(flagout){                                                           // redirection at output only
        int rc = fork();        
        if (rc < 0){                                                            // fork failed
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        else if (rc == 0){                                                      // child process
            printf("Child (pid:%d)\n", getpid());
            child_with_output_redirected(args, argsc, out_loc);
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
                    args[i] = NULL;
                    argsc = i+1;
                    break;
                }
            }
            execvp(args[ARG_PROGNAME], args);
        }                          
        else{                                                                   // parent process
            printf("Parent (pid:%d)\n", getpid());
            waitpid(rc, NULL, 0);
        }
    }
}