#include "s3.h"
// gcc *.c -o s3

int main(int argc, char *argv[]){

    ///Stores the command line input
    char line[MAX_LINE];

    ///The last (previous) working directory 
    char lwd[MAX_PROMPT_LEN-6]; 

    init_lwd(lwd);///Implement this function: initializes lwd with the cwd (using getcwd)

    //Stores pointers to command arguments.
    ///The first element of the array is the command name.
    char *args[MAX_ARGS];

    ///Stores the number of arguments
    int argsc = 0;

    ///Stores the individual pipe processes
    char *pipereqs[MAX_LINE];

    ///Stores the number of individual pipe commands
    int pipereqc = 0;

    ///Stores the individual batch processes
    char *batches[MAX_LINE];

    ///Stores the number of individual batch processes
    int batchnum;

    while (1) {

        read_command_line(line, lwd);

        // Batch
        if (command_with_batch(line)){
            parse_batch(line, batches, &batchnum);
            launch_program_batch(batches, batchnum, pipereqs, pipereqc, args, argsc, lwd);
            reap();
        }

        // "cd"
        else if(is_cd(line)){
            parse_command(line, args, &argsc);
            run_cd(args, argsc, lwd); 
        }

        // Pipes
        else if(command_with_pipe(line)){ 
            parse_pipes(line, pipereqs, &pipereqc);
            launch_program_pipe(pipereqs, pipereqc, args, argsc);
            reap();
        }

        // Subshell
        else if (is_subshell(line)){
            char subshell_line[MAX_LINE];
            char tail_line[MAX_LINE];
            extract_subshell(line, subshell_line, tail_line);
            launch_subshell(subshell_line, tail_line);
            reap();
        }

        // Redirection
        else if(command_with_redirection(line)){
            parse_command(line, args, &argsc);
            launch_program_with_redirection(args, argsc, NULL, NULL);
            reap();
        }

        // Basic
        else{
            parse_command(line, args, &argsc);
            launch_program(args, argsc, NULL, NULL);
            reap();
        }
    }

    return 0;
}
