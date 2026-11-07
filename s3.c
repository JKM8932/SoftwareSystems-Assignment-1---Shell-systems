#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[])
{
    strcpy(shell_prompt, "[s3]$ ");
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt);
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

// Function call used to check if redirection is required (sort)
// May need to edit to check specifically for the < and > signs as normal sort should be fine
int command_with_redirection(char line[]){
    char *sort_token = strtok(line, " ");
    if (sort_token != NULL && strcmp(sort_token, "sort") == 0) return 1;
    else return 0;
}


///Launch related functions
void child(char *args[], int argsc)
{
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

void child_with_output_redirected(char *args[], int argsc){

}

void child_with_input_redirected(char *args[], int argsc){
    int chars_read = read(fd, input, MAX_PROMPT_LEN); // check later
    input[chars_read] = '\0';                         // end the string with null terminator
    printf("Read from the file: %s\n", input);

}

void launch_program_with_redirection(char *args[], int argsc){

    // As command_with_redirection checks for the sort command,
    // Not much need to check arguments in case of empty args or argsc == 0

    // STDIN_FILENO = 0;   input from keyboard(default)
    // STDOUT_FILENO = 1;   normal output to terminal(default)
    // STDERR_FILENO = 2;   error messages

    char *check_redir;
    int index = ARG_PROGNAME, flagin = 0, flagout = 0;
    char in_loc[MAX_LINE];
    char out_loc[MAX_LINE];

    while (check_redir != NULL){

        // Input file?
        if (strcmp(check_redir, "<") == 0){
            in_loc[0] = args[index+1];
            flagin  = 1;
        }

        // Output file?
        if (strcmp(check_redir, ">") == 0){
            out_loc[0] = args[index+1];
            flagout = 1;
        }
        check_redir == args[index++];
    }

    if(flagin){
        char input[MAX_LINE];
        int fd = open(in_loc[0], O_RDONLY);         // READ-ONLY
        if (fd == -1){
            perror("open");
            return 1;
        }
        printf("File %s linked\n", in_loc[0]);
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

    if(flagout){

    }

}