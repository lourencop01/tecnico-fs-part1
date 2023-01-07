#include "betterassert.h"
#include "logging.h"
#include "structs.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        // In some systems, after the handler call the signal gets reverted
        // to SIG_DFL (the default action associated with the signal).
        // So we set the signal handler back to our function after each trap.
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        return; // Resume execution at point of interruption
    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - BOOM!\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    (void)argc;

    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    char pipe_name[PIPE_NAME_SIZE];
    memset(pipe_name, '\0', PIPE_NAME_SIZE);

    char box_name[BOX_NAME_SIZE];
    memset(box_name, '\0', BOX_NAME_SIZE);

    // Argument parsing of a subscriber process launch.
    strcpy(register_pipe_name, argv[1]);
    strcpy(pipe_name, argv[2]);
    strcpy(box_name, argv[3]);

    // Check if register pipe exists. TODO

    // Check if box_name exists. TODO

    // Check if pipe_name is a valid path name. TODO

    // Check if pipe_name already exists. TODO

    int value = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(value == 0, "Pipe could not be created.");

    ALWAYS_ASSERT((signal(SIGINT, sig_handler) != SIG_ERR), "Could not catch SIGINT.");

    return -1;
}