#include "s3.h"

//// ---------------------------------------------------------
//// Fundamental shell setup functions

// Constructs shell prompt in powershell/terminal
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
// Prints a shell prompt and reads input from the user
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

//// ---------------------------------------------------------
//// Directory-related functions

// Grab current working directory, store it in string storage lwd
void init_lwd(char lwd[])
{
    getcwd(lwd, MAX_PROMPT_LEN - 6);
}

// Checks if "cd" (change directory) is called as a command
int is_cd(char line[])
{
    if (line == NULL) return 0;
    int i = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i] == 'c' && line[i + 1] == 'd' && (line[i + 2] == ' ' || line[i + 2] == '\t' || line[i + 2] == '\0')){
        return 1;
    }
    return 0;
}
// Specific function call that carries out cd
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

    if (getcwd(lwd, MAX_PROMPT_LEN - 6) == NULL){
        perror("getcwd for new cd failed");
    }
}

//// ---------------------------------------------------------
//// Parsing functions, used for splitting up command line into segments

// Basic, splits into arguments
void parse_command(char line[], char *args[], int *argsc)
{
    char *token = strtok(line, " \t");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1)
    {
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    
    args[*argsc] = NULL; ///args must be null terminated
}
// Pipes, split into pipe requests
void parse_pipes(char line[], char *pipereqs[], int *pipereqc)
{
    int pipe_in_subshell = 0;
    for (int i = 0; line[i] != '\0'; i++){
        if(line[i] == '('){
            pipe_in_subshell++;
        }
        else if(line[i] == ')' && pipe_in_subshell > 0){
            pipe_in_subshell--;
        }
        else if (line[i] == '|' && pipe_in_subshell > 0){
            line[i] = '\x01';                                   // temporary override
        }
    }



    char *command = strtok(line, "|");
    *pipereqc = 0;
    while (command != NULL)
    {
        for (int i = 0; command[i] != '\0'; i++){
            if (command[i] == '\x01'){
                command[i] = '|';
            }
        }
        pipereqs[(*pipereqc)++] = command;
        command = strtok(NULL, "|");
    }

    pipereqs[*pipereqc] = NULL;
}
// Batches, split into individual batches
void parse_batch(char line[], char *batches[], int *batchnum)
{
    int batch_in_subshell = 0;
    for (int i = 0; line[i] != '\0'; i++){
        if(line[i] == '('){
            batch_in_subshell++;
        }
        else if(line[i] == ')' && batch_in_subshell > 0){
            batch_in_subshell--;
        }
        else if (line[i] == ';' && batch_in_subshell > 0){
            line[i] = '\x01';                                   // temporary override
        }
    }



    char *batch = strtok(line, ";");
    *batchnum   = 0;
    while (batch != NULL){
        for (int i = 0; batch[i] != '\0'; i++){
            if (batch[i] == '\x01'){
                batch[i] = ';';
            }
        }
        batches[(*batchnum)++] = batch;
        batch   = strtok(NULL, ";");
    }
    batches[(*batchnum)] = NULL;
}
//// ---------------------------------------------------------
//// Checker function calls

// Checks if there is redirection ('<', '>', ">>")
int command_with_redirection(char line[])
{
    if (line == NULL) return 0;
    for (int i = 0; line[i] != '\0'; i++){
        if (line[i] == '<' || line[i] == '>'){     
            return 1;        
        }
    }
    return 0;
}
// Checks if there is pipe symbol ('|')
int command_with_pipe(char line[])
{

    if (line == NULL) return 0;

    int depth = 0;
    for (int i = 0; line[i] != '\0'; i++){
        if (line[i] == '('){
            depth++;
        }
        else if (line[i] == ')' && depth > 0){
            depth--;
        }
        else if (line[i] == '|' && depth == 0){
            return 1;
        }
    }
    return 0;
}
// Checks if batching is required (';')
int command_with_batch(char line[])
{
    if (line == NULL) return 0;

    int depth = 0;
    for (int i = 0; line[i] != '\0'; i++){
        if (line[i] == '('){
            depth++;
        }
        else if (line[i] == ')' && depth > 0){
            depth--;
        }
        else if (line[i] == ';' && depth == 0){
            return 1;
        }
    }
    return 0;
}
// Checks if there is a subshell (and if it is formatted correctly) ('(' ')')
int is_subshell(char line[])                        // check
{
    if (line == NULL) return 0;

    int i = 0;
    while (line[i] == ' ' || line[i] == '\t'){
        i++;
    }
    if (line[i] != '('){
        return 0;                                   // not a subshell
    }

    for (int j = i + 1; line[j] != '\0'; j++){
        if (line[j] == '('){
            return 0;                               // nested subshells are unsupported
        }
        else if (line[j] == ')'){
            return 1;
        }
    }

    return 0;
}

//// ---------------------------------------------------------
//// Launch related functions

/// Subshell
// Subshell initation
void extract_subshell(char line[], char subshell_line[], char tail_line[]){
    if (line == NULL || subshell_line == NULL || tail_line == NULL) return;

    int i = 0;
    while (line[i] == ' ' || line[i] == '\t') i++;
    if (line[i] != '('){
        subshell_line[0] = '\0';
        tail_line[0] = '\0';
        return;
    }

    // expectation: upto this point, line[i] = '('
    i++;                                                    // character after '('
    int j = 0;
    while (line[i] != '\0'){
        if (line[i] == '('){                                // nested_subshells arent implemented for now
            subshell_line[0] = '\0';
            tail_line[0] = '\0';
            return;
        }
        else if (line[i] == ')'){                           // assumed that we only have 1 subshhell
            subshell_line[j] = '\0';
            i++;
            int tail_len = 0;
            while (line[i] != '\0') {                       // if theres any tail leftover
                tail_line[tail_len++] = line[i++];
            }
            tail_line[tail_len] = '\0';
            return;
        }
        else {
            subshell_line[j++] = line[i++];
        }
    }
    subshell_line[j] = '\0';                                // if for some reason, there is no subshell (even though is_subshell would have detected it)
    tail_line[0] = '\0';
}

/// Basic
// Basic child function
void child(char *args[], int argsc)
{
    if (argsc == 0) return;                                 // if no argument -> no need to launch program
    execvp(args[ARG_PROGNAME], args);

    fprintf(stderr, "execvp failed\n");
    exit(1);

}
// Basic launch function
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

/// Redirection
// Child function for (output) redirection
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
    fprintf(stderr, "File %s linked\n", out_loc);                                // debug

    dup2(fd_write, STDOUT_FILENO);                                              // dup2 -> duplicate fd_write location to replace STDOUT_FILENO (standard input)
    close(fd_write);                                                            // close original fd_write after
}
// Child function for (input) redirection
void child_with_input_redirected(char *args[], int argsc, char *in_loc)
{
    if (argsc == 0) return;                                                     // if no argument -> no need to launch program
    int fd_read = open(in_loc, O_RDONLY);                                       // setup reading from input_file as read-only
    if (fd_read == -1){
        perror("Unable to open file to read");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "File %s linked\n", in_loc);                                // debug

    dup2(fd_read, STDIN_FILENO);                                                // dup2 -> duplicate fd_read location to replace STDIN_FILENO (standard input)
    close(fd_read);                                                             // close original fd_read after
}

// Remove redirection tokens before passing the remaining argv to execvp.
void child_redir_args_cleanup(char *args[]){
    for (int i = 0; args[i] != NULL; i++){
        if (strcmp(args[i], ">") == 0 || strcmp(args[i], "<") == 0 || strcmp(args[i], ">>") == 0){
            args[i] = NULL;
            break;
        }
    }
}

void child_redirection_handler(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE,
                               char *in_loc, char *out_loc, int flagin, int flagout){
    if (flagin){
        child_with_input_redirected(args, argsc, in_loc);
        if (FD_PWRITE != NULL){
            dup2(*FD_PWRITE, STDOUT_FILENO);
            close(*FD_PWRITE);
        }
    }
    if (flagout){
        child_with_output_redirected(args, argsc, out_loc);
        if (FD_PREAD != NULL){
            dup2(*FD_PREAD, STDIN_FILENO);
            close(*FD_PREAD);
        }
    }
    child_redir_args_cleanup(args);
}

void locate_redirection_locations(char *args[], char **in_loc, char **out_loc,
                                  int *flagin, int *flagout){
    for (int i = 0; args[i] != NULL; i++){
        if (strcmp(args[i], "<") == 0){
            if (args[i + 1] == NULL){
                fprintf(stderr, "missing argument for input redirection!\n");
                return;
            }
            *in_loc = args[i + 1];
            *flagin = 1;
        }
        else if (args[i][0] == '<' && args[i][1] != '\0'){
            *in_loc = args[i] + 1;
            *flagin = 1;
        }

        if (strcmp(args[i], ">") == 0 || strcmp(args[i], ">>") == 0){
            if (args[i + 1] == NULL){
                fprintf(stderr, "missing argument for output redirection!\n");
                return;
            }
            *out_loc = args[i + 1];
            *flagout = 1;
        }
        else if (args[i][0] == '>' && args[i][1] == '>'){
            *out_loc = args[i] + 2;
            *flagout = 1;
        }
        else if (args[i][0] == '>' && args[i][1] != '\0'){
            *out_loc = args[i] + 1;
            *flagout = 1;
        }
    }
}
// Launch function for redirection
void launch_program_with_redirection(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE)
{
    if (argsc == 0) return;
    int flagin = 0, flagout = 0;
    char *in_loc = NULL, *out_loc = NULL;
    locate_redirection_locations(args, &in_loc, &out_loc, &flagin, &flagout);
    int rc = fork();
    if (rc < 0){ perror("fork failed"); exit(1); }
    if (rc == 0){
        child_redirection_handler(args, argsc, FD_PREAD, FD_PWRITE, in_loc, out_loc, flagin, flagout);
        execvp(args[ARG_PROGNAME], args);
        fprintf(stderr, "redirection child execvp failed\n");
        exit(1);
    }
    else{
        waitpid(rc, NULL, 0);
    }
}


/// Pipes
void launch_program_pipe(char *pipereqs[], int pipereqc, char *args[], int argsc)
{
    int prev_FD_PREAD = -1;

    for (int i = 0; i < pipereqc; i++){
        int redir = command_with_redirection(pipereqs[i]);
        int next_pipe = (i + 1 < pipereqc);
        int fd[2];
        if (next_pipe){
            int rc = pipe(fd);
            if (rc < 0){
                perror("pipe failed");
                exit(EXIT_FAILURE);
            }
        }

        int *FD_PREAD = (i > 0) ? &prev_FD_PREAD : NULL;
        int *FD_PWRITE = next_pipe ? &fd[1] : NULL;
        parse_command(pipereqs[i], args, &argsc);

        if (redir){
            launch_program_with_redirection(args, argsc, FD_PREAD, FD_PWRITE);
        }
        else{
            launch_program(args, argsc, FD_PREAD, FD_PWRITE);
        }

        if (i > 0) close(prev_FD_PREAD);
        if (next_pipe) prev_FD_PREAD = fd[0];
    }
    while (wait(NULL) > 0){

    }
}


/// Batch
// launch function for batch command lines
void launch_program_batch(char *batches[], int batchnum, char *pipereqs[], int pipereqc, char *args[], int argsc, char lwd[])
{
    for (int i = 0; i < batchnum; i++){
        if (is_subshell(batches[i])){
            char subshell_line[MAX_LINE], tail_line[MAX_LINE];
            extract_subshell(batches[i], subshell_line, tail_line);
            launch_subshell(subshell_line, tail_line);
        }
        else if (is_cd(batches[i])){
            parse_command(batches[i], args, &argsc);
            run_cd(args, argsc, lwd); 
        }
        else if (command_with_pipe(batches[i])){
            parse_pipes(batches[i], pipereqs, &pipereqc);
            launch_program_pipe(pipereqs, pipereqc, args, argsc);
        }
        else if (command_with_redirection(batches[i])){
            parse_command(batches[i], args, &argsc);
            launch_program_with_redirection(args, argsc, NULL, NULL);
        }
        else {
            parse_command(batches[i], args, &argsc);
            launch_program(args, argsc, NULL, NULL);
        }
    }

}

void child_subshell_handler(char subshell[]){
    char *args[MAX_ARGS];
    int argsc = 0;
    char *pipereqs[MAX_LINE];
    int pipereqc = 0;

    if (command_with_batch(subshell)){
        char lwd[MAX_PROMPT_LEN - 6];
        char *batches[MAX_LINE];
        int batchnum = 0;
        init_lwd(lwd);
        parse_batch(subshell, batches, &batchnum);
        launch_program_batch(batches, batchnum, pipereqs, pipereqc, args, argsc, lwd);
    }

    else if (command_with_pipe(subshell)){
        parse_pipes(subshell, pipereqs, &pipereqc);
        launch_program_pipe(pipereqs, pipereqc, args, argsc);
    }

    else {
        char *args[MAX_ARGS];
        int argsc = 0;
        int redir = command_with_redirection(subshell);

        parse_command(subshell, args, &argsc);

        if (argsc == 0) {
            exit(0);
        }

        if (strcmp(args[0], "cd") == 0){
            char lwd[MAX_PROMPT_LEN - 6];
            init_lwd(lwd);
            run_cd(args, argsc, lwd);
        }

        else if (redir){
            launch_program_with_redirection(args, argsc, NULL, NULL);
        }

        else {
            launch_program(args, argsc, NULL, NULL);
        }
    }
    exit(0);
}

void launch_subshell(char subshell[], char tail_line[])
{
    char *tail_args[MAX_ARGS];
    int tail_argsc = 0;
    int flagin = 0, flagout = 0;
    char *in_loc = NULL, *out_loc = NULL;
    tail_args[0] = NULL;

    if (tail_line[0] != '\0'){
        parse_command(tail_line, tail_args, &tail_argsc);
        locate_redirection_locations(tail_args, &in_loc, &out_loc, &flagin, &flagout);
    }

    int rc = fork();
    if (rc < 0){ 
        perror("fork subshell failed"); 
        return; 
    }
    else if (rc == 0){
        if (flagin || flagout){
            child_redirection_handler(tail_args, tail_argsc, NULL, NULL, in_loc, out_loc, flagin, flagout);
        }
        child_subshell_handler(subshell);
        exit(0);
    }
    else{
        waitpid(rc, NULL, 0);
    }
}
   
