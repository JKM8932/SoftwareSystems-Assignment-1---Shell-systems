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
    if (line[i] == 'c' && line[i + 1] == 'd' &&
        (line[i + 2] == ' ' || line[i + 2] == '\t' || line[i + 2] == '\0')) return 1;
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

    int depthofparam = 0, i = 0;
    while (line[i] == ' ' || line[i] == '\t'){
        i++;
    }
    if (line[i] != '('){
        return 0;                                   // not a subshell
    }

    for (int j = i; line[j] != '\0'; j++){
        if (line[j] == '('){
            depthofparam++;
        }
        else if (line[j] == ')'){
            if (depthofparam == 0){
                return 0;
            }
            depthofparam--;
            if (depthofparam == 0){
                // The caller can handle anything after the matching outer ')'.
                return 1;
            }
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

    i++;
    int j = 0;
    while (line[i] != '\0' && line[i] != ')'){
        subshell_line[j++] = line[i++];
    }
    subshell_line[j] = '\0';

    if (line[i] == ')') i++;
    int tail_len = 0;
    while (line[i] != '\0'){
        tail_line[tail_len++] = line[i++];
    }
    tail_line[tail_len] = '\0';
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

typedef struct {
    char *input_path;
    char *output_path;
    int append_output;
} Redirections;

// Extract redirection operators and targets, leaving argv with command arguments only.
static int parse_redirections(char *args[], int *argsc, Redirections *redir){
    int read_index = 0;
    int write_index = 0;

    redir->input_path = NULL;
    redir->output_path = NULL;
    redir->append_output = 0;

    while (read_index < *argsc){
        char *token = args[read_index];
        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, ">>") == 0){
            if (read_index + 1 >= *argsc){
                fprintf(stderr, "missing filename after redirection\n");
                return -1;
            }
            if (token[0] == '<'){
                redir->input_path = args[read_index + 1];
            } else {
                redir->output_path = args[read_index + 1];
                redir->append_output = (strcmp(token, ">>") == 0);
            }
            read_index += 2;
        }
        else if (token[0] == '<' && token[1] != '\0'){
            redir->input_path = token + 1;
            read_index++;
        }
        else if (token[0] == '>' && token[1] == '>'){
            if (token[2] == '\0'){
                if (read_index + 1 >= *argsc){
                    fprintf(stderr, "missing filename after redirection\n");
                    return -1;
                }
                redir->output_path = args[read_index + 1];
                read_index += 2;
            } else {
                redir->output_path = token + 2;
                read_index++;
            }
            redir->append_output = 1;
        }
        else if (token[0] == '>' && token[1] != '\0'){
            redir->output_path = token + 1;
            redir->append_output = 0;
            read_index++;
        }
        else {
            args[write_index++] = args[read_index++];
        }
    }

    args[write_index] = NULL;
    *argsc = write_index;
    return 0;
}

static int apply_redirections(const Redirections *redir){
    if (redir->input_path != NULL){
        int fd = open(redir->input_path, O_RDONLY);
        if (fd < 0){ perror("open input redirection"); return -1; }
        if (dup2(fd, STDIN_FILENO) < 0){ perror("dup2 input redirection"); close(fd); return -1; }
        close(fd);
    }
    if (redir->output_path != NULL){
        int flags = O_WRONLY | O_CREAT | (redir->append_output ? O_APPEND : O_TRUNC);
        int fd = open(redir->output_path, flags, 0644);
        if (fd < 0){ perror("open output redirection"); return -1; }
        if (dup2(fd, STDOUT_FILENO) < 0){ perror("dup2 output redirection"); close(fd); return -1; }
        close(fd);
    }
    return 0;
}
// Launch function for redirection
void launch_program_with_redirection(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE)
{
    if (argsc == 0) return;
    int wait_child = 1;
    if (FD_PREAD != NULL|| FD_PWRITE != NULL){
        wait_child = 0;
    }
    int rc = fork();
    if (rc < 0){ perror("fork failed"); exit(EXIT_FAILURE); }
    if (rc == 0){
        if (FD_PREAD != NULL && dup2(*FD_PREAD, STDIN_FILENO) < 0){ perror("dup2 pipe input"); _exit(1); }
        if (FD_PWRITE != NULL && dup2(*FD_PWRITE, STDOUT_FILENO) < 0){ perror("dup2 pipe output"); _exit(1); }
        if (FD_PREAD != NULL) close(*FD_PREAD);
        if (FD_PWRITE != NULL) close(*FD_PWRITE);
        Redirections redir;
        if (parse_redirections(args, &argsc, &redir) < 0 || apply_redirections(&redir) < 0) _exit(1);
        if (argsc == 0) _exit(0);
        execvp(args[ARG_PROGNAME], args);
        perror("execvp");
        _exit(127);
    }
    else{
        if (!wait_child){
            if (FD_PREAD != NULL) close(*FD_PREAD);
            if (FD_PWRITE != NULL) close(*FD_PWRITE);
        } else {
        waitpid(rc, NULL, 0);
        }
    }
}


/// Pipes
static void child_subshell_handler(char subshell[]);

static void run_pipeline_stage(char command[], int fd_in, int fd_out){
    if (fd_in >= 0 && dup2(fd_in, STDIN_FILENO) < 0){ perror("dup2 pipe input"); _exit(1); }
    if (fd_out >= 0 && dup2(fd_out, STDOUT_FILENO) < 0){ perror("dup2 pipe output"); _exit(1); }
    if (fd_in >= 0) close(fd_in);
    if (fd_out >= 0) close(fd_out);

    if (is_subshell(command)){
        char body[MAX_LINE], tail[MAX_LINE];
        char *tail_args[MAX_ARGS];
        int tail_argsc = 0;
        Redirections redir = { NULL, NULL, 0 };
        extract_subshell(command, body, tail);
        if (tail[0] != '\0'){
            parse_command(tail, tail_args, &tail_argsc);
            if (parse_redirections(tail_args, &tail_argsc, &redir) < 0 || tail_argsc != 0 || apply_redirections(&redir) < 0) _exit(1);
        }
        child_subshell_handler(body);
        _exit(0);
    }

    char *args[MAX_ARGS];
    int argsc = 0;
    Redirections redir = { NULL, NULL, 0 };
    parse_command(command, args, &argsc);
    if (parse_redirections(args, &argsc, &redir) < 0 || apply_redirections(&redir) < 0) _exit(1);
    if (argsc == 0) _exit(0);
    if (strcmp(args[0], "cd") == 0){
        char lwd[MAX_PROMPT_LEN - 6];
        init_lwd(lwd);
        run_cd(args, argsc, lwd);
        _exit(0);
    }
    if (strcmp(args[0], "exit") == 0) _exit(0);
    execvp(args[0], args);
    perror("execvp");
    _exit(127);
}

// Start every stage before waiting, so a writer cannot block on an unopened reader.
void launch_program_pipe(char *pipereqs[], int pipereqc){
    if (pipereqc <= 0) return;
    pid_t pids[MAX_LINE];
    int pid_count = 0;
    int prev_read = -1;

    for (int i = 0; i < pipereqc; i++){
        int fd[2] = { -1, -1 };
        int next_pipe = (i + 1 < pipereqc);
        if (next_pipe && pipe(fd) < 0){ perror("pipe failed"); exit(EXIT_FAILURE); }

        pid_t rc = fork();
        if (rc < 0){ perror("fork failed"); exit(EXIT_FAILURE); }
        if (rc == 0){
            if (next_pipe) close(fd[0]);
            run_pipeline_stage(pipereqs[i], prev_read, next_pipe ? fd[1] : -1);
            _exit(0);
        }

        pids[pid_count++] = rc;
        if (prev_read >= 0) close(prev_read);
        if (next_pipe){
            close(fd[1]);
            prev_read = fd[0];
        } else {
            prev_read = -1;
        }
    }

    for (int i = 0; i < pid_count; i++) waitpid(pids[i], NULL, 0);
}


/// Batch
// launch function for batch command lines
void launch_program_batch(char *batches[], int batchnum, char lwd[])
{
    char *args[MAX_ARGS];
    char *pipereqs[MAX_LINE];
    int argsc = 0;
    int pipereqc = 0;

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
            launch_program_pipe(pipereqs, pipereqc);
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

static void child_subshell_handler(char subshell[]){
    if (command_with_batch(subshell)){
        char lwd[MAX_PROMPT_LEN - 6];
        char *batches[MAX_LINE];
        int batchnum = 0;
        init_lwd(lwd);
        parse_batch(subshell, batches, &batchnum);
        launch_program_batch(batches, batchnum, lwd);
    }
    else if (command_with_pipe(subshell)){
        char *pipereqs[MAX_LINE];
        int pipereqc = 0;
        parse_pipes(subshell, pipereqs, &pipereqc);
        launch_program_pipe(pipereqs, pipereqc);
    }
    else {
        char *args[MAX_ARGS];
        int argsc = 0;
        parse_command(subshell, args, &argsc);
        if (argsc == 0) _exit(0);
        if (strcmp(args[0], "cd") == 0){
            char lwd[MAX_PROMPT_LEN - 6];
            init_lwd(lwd);
            run_cd(args, argsc, lwd);
        }
        else {
            Redirections redir;
            if (parse_redirections(args, &argsc, &redir) < 0 || apply_redirections(&redir) < 0) _exit(1);
            if (argsc > 0){
                if (strcmp(args[0], "exit") == 0) _exit(0);
                execvp(args[0], args);
                perror("execvp");
                _exit(127);
            }
        }
    }
    _exit(0);
}

void launch_subshell(char subshell[], char tail_line[])
{
    char *tail_args[MAX_ARGS];
    int tail_argsc = 0;
    Redirections redir = { NULL, NULL, 0 };

    if (tail_line[0] != '\0'){
        parse_command(tail_line, tail_args, &tail_argsc);
        if (parse_redirections(tail_args, &tail_argsc, &redir) < 0 || tail_argsc != 0){
            fprintf(stderr, "unsupported text after subshell\n");
            return;
        }
    }

    int rc = fork();
    if (rc < 0){ perror("fork subshell failed"); return; }
    if (rc == 0){
        if (apply_redirections(&redir) < 0) _exit(1);
        child_subshell_handler(subshell);
        _exit(0);
    }
    waitpid(rc, NULL, 0);
}
   
