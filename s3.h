#ifndef _S3_H_
#define _S3_H_

///See reference for what these libraries provide
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

///Constants for array sizes, defined for clarity and code readability
#define MAX_LINE 1024
#define MAX_ARGS 128
#define MAX_PROMPT_LEN 256
#define MAX_PIPES 20                                                            // new parameter, not really any specific reason, just that pipes past 20 might not seem normal?

///Enum for readable argument indices (use where required)
enum ArgIndex
{
    ARG_PROGNAME,
    ARG_1,
    ARG_2,
    ARG_3,
};

///With inline functions, the compiler replaces the function call 
///with the actual function code;
///inline improves speed and readability; meant for short functions (a few lines).
///the static here avoids linker errors from multiple definitions (needed with inline).
static inline void reap()
{
    wait(NULL);
}

///Shell I/O and related functions (add more as appropriate)
void read_command_line(char line[], char lwd[]);
void construct_shell_prompt(char shell_prompt[], char lwd[]);

///Checker-related functions
void parse_command(char line[], char *args[], int *argsc);
void parse_pipes(char line[], char *pipereqs[], int *pipereqc);
void parse_batch(char line[], char *batches[], int *batchnum);
void extract_subshell(char line[], char subshell_line[], char tail_line[]);
int command_with_redirection(char line[]);
int command_with_pipe(char line[]);
int command_with_batch(char line[]);
int is_subshell(char line[]);

///Directory-related
void init_lwd(char lwd[]);
int is_cd(char line[]);
void run_cd(char *args[], int argsc, char line[]);

///Child functions (add more as appropriate)
void child(char *args[], int argsc);
void child_with_input_redirected(char *args[], int argsc, char *in_loc);
void child_with_output_redirected(char *args[], int argsc, char *out_loc);

///Program launching functions (add more as appropriate)
void launch_program(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE);
void launch_program_with_redirection(char *args[], int argsc, int *FD_PREAD, int *FD_PWRITE);
void launch_program_pipe(char *pipereqs[], int pipereqc);
void launch_program_batch(char *batches[], int batchnum, char lwd[]);
void launch_subshell(char subshell[], char tail_line[]);
#endif
