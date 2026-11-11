#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[], char lwd[])
{
    if (lwd != NULL){
        strcpy(shell_prompt, lwd);
        strcat(shell_prompt, " [s3]$");
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
    return 0;
}

void run_cd(char *args[], int argsc, char lwd[])
{
    int result = -1;

    if (args[ARG_1] == NULL){
        result = chdir(getenv("HOME"));
    }
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

    if (fgets(line, MAX_LINE, stdin) == NULL)
    {
        perror("fgets failed");
        exit(1);
    }
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
int command_with_redirection(char line[])
{
    for (int i = 0; line[i] != '\0'; i++){
        if (line[i] == '<' || line[i] == '>'){     
            return 1;        
        }
    }
    return 0;
}

///Checks if there is pipe symbol
int command_with_pipe(char line[])
{
    for (int i = 0; line[i] != '\0'; i++){
        if (line[i] == '|'){
            return 1;
        }
    }
    return 0;
}


///Separates line[] into individual segments based on pipe symbol as a divider 
void parse_pipes(char line[], char *pipereqs[], int *pipereqc)
{
    char *command = strtok(line, "|");
    *pipereqc = 0;
    while (command != NULL)
    {
        pipereqs[(*pipereqc)++] = command;
        command = strtok(NULL, "|");
    }

    pipereqs[*pipereqc] = NULL;
}

///Launch related functions
void child(char *args[], int argsc)
{
    if (argsc == 0) return;                                 // if no argument -> no need to launch program
    execvp(args[ARG_PROGNAME], args);

    fprintf(stderr, "execvp failed\n");
    exit(1);

}

void launch_program(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE)
{
    if (argsc == 0) return;                                 // if no argument -> no need to launch program
    if (strcmp(args[ARG_PROGNAME], "exit") == 0){
        exit(EXIT_SUCCESS);
    }

    int wait_child = 1;
    if (FD_PREAD != NULL || FD_PWRITE != NULL){
        wait_child = 0;
    }

    int rc = fork();
    if (rc < 0)
    {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }

    else if (rc == 0)
    {                                      // child process
        if (FD_PREAD != NULL){
            dup2(*FD_PREAD, STDIN_FILENO);
            close(*FD_PREAD);
        }
        if (FD_PWRITE != NULL){
            dup2(*FD_PWRITE, STDOUT_FILENO);
            close(*FD_PWRITE);
        }
        fprintf(stderr, "(Read Pipe) Child (pid:%d)\n", getpid());
        child(args, argsc);
    }                

    else
    {
        if (wait_child){
            fprintf(stderr, "Parent (pid:%d)\n", getpid());
            waitpid(rc, NULL, 0);
        }
        else{
            if (FD_PREAD != NULL){
                close(*FD_PREAD);
            }
            if (FD_PWRITE != NULL){
                close(*FD_PWRITE);
            }
            fprintf(stderr, "(Read Pipe) Parent (pid:%d), Skipping...\n", getpid());
        }
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

void launch_program_with_redirection(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE)
{
    if (argsc == 0) return;
    // STDIN_FILENO = 0;   input from keyboard(default)
    // STDOUT_FILENO = 1;   normal output to terminal(default)
    // STDERR_FILENO = 2;   error messages

    // Setup to check for input and output file locations
    int index = 0, flagin = 0, flagout = 0;
    char *in_loc;
    char *out_loc;
    int wait_child = 1;
    if (FD_PREAD != NULL|| FD_PWRITE != NULL){
        wait_child = 0;
    }

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
    if(flagin && flagout)
    {                                                      // redirection at input and output
        int rc = fork();
        if (rc < 0){                                                            // fork failed
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        else if (rc == 0){                                                      // child process
            fprintf(stderr, "(Redirection) Child (pid:%d)\n", getpid());
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
            fprintf(stderr, "(Redirection) Parent (pid:%d)\n", getpid());
            waitpid(rc, NULL, 0);
        }        
    }

    else if(flagin)
    {                                                            // redirection at input only
        int rc = fork();
        if (rc < 0){                                                            // fork failed
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        else if (rc == 0){                                                      // child process
            fprintf(stderr, "(Redirection) Child (pid:%d)\n", getpid());
            child_with_input_redirected(args, argsc, in_loc);
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], "<") == 0) {
                    args[i] = NULL;
                    argsc = i+1;
                    break;
                }
            }
            if(FD_PWRITE != NULL){
                dup2(*FD_PWRITE, STDOUT_FILENO);
                close(*FD_PWRITE);
            }
            execvp(args[ARG_PROGNAME], args);
        }                          
        else{                                                                   // parent process
            if (wait_child){
                fprintf(stderr, "(Redirection) Parent (pid:%d)\n", getpid());
                waitpid(rc, NULL, 0);
            }
            else{
                close(*FD_PWRITE);
                fprintf(stderr, "(Write Pipe) (Redirection) Parent (pid:%d), Skipping... \n", getpid());
            }
        }
    }

    else if(flagout)
    {                                                           // redirection at output only
        int rc = fork();        
        if (rc < 0){                                                            // fork failed
            fprintf(stderr, "fork failed\n");
            exit(1);
        }
        else if (rc == 0){                                                      // child process
            fprintf(stderr, "(Redirection) Child (pid:%d)\n", getpid());
            child_with_output_redirected(args, argsc, out_loc);
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0) {
                    args[i] = NULL;
                    argsc = i+1;
                    break;
                }
            }
            if(FD_PREAD != NULL){
                dup2(*FD_PREAD, STDIN_FILENO);
                close(*FD_PREAD);
            }
            execvp(args[ARG_PROGNAME], args);
        }      

        else{                                                                   // parent process
            if (wait_child){
                fprintf(stderr, "(Redirection) Parent (pid:%d)\n", getpid());
                waitpid(rc, NULL, 0);
            }
            else{
                close(*FD_PREAD);
                fprintf(stderr, "(Write Pipe) (Redirection) Parent (pid:%d), Skipping... \n", getpid());
            }
        }
    }
}

void launch_program_pipe(char *pipereqs[], int pipereqc)
{
    // Pipe left & right processing

    int prev_FD_PREAD = -1;                  // initially, no need to read from pipe, not until after writing is done

    for (int i = 0; i < pipereqc; i++){

        int redir = command_with_redirection(pipereqs[i]);
        int next_pipe = (i+1 < pipereqc);

        // Pipe initialisation
        int fd[2];
        if (next_pipe){
            int rc = pipe(fd);
            if (rc < 0){
                perror("pipe failed");
                exit(EXIT_FAILURE);
            }
        }

        int *FD_PREAD = (i > 0) ? &prev_FD_PREAD : NULL;
        int *FD_PWRITE = (next_pipe) ? &fd[1] : NULL;


        // Setup to do individual command line (rolling basis)
        char *args[MAX_ARGS];
        int argsc;
        parse_command(pipereqs[i], args, &argsc);

        // Launch
        if(redir){
            launch_program_with_redirection(args, argsc, FD_PREAD, FD_PWRITE);
        }
        else{
            launch_program(args, argsc, FD_PREAD, FD_PWRITE);
        }

        // Cleanup/Setup for next cycle
        if (i > 0){
            close(prev_FD_PREAD);
        }
        if (next_pipe){
            prev_FD_PREAD = fd[0];
        }
    }
}

