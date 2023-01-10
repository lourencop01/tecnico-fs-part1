#include "betterassert.h"
#include "logging.h"
#include "structs.h"

#include <signal.h>
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
        exit(EXIT_SUCCESS); // Exit the process
        return; // Resume execution at point of interruption
    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - BOOM!\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    //Check if the number of arguments is correct.
    ALWAYS_ASSERT(argc == 4, "Wrong number of arguments.");

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

    unlink(pipe_name);

    // Check if box_name is a valid box name
    ALWAYS_ASSERT(strlen(box_name) < BOX_NAME_SIZE, "Box name is too long.");

    // Check if pipe_name is a valid path name. TODO
    ALWAYS_ASSERT(strlen(pipe_name) < PIPE_NAME_SIZE, "Pipe name is too long.");

    // Check if pipe_name already exists. TODO
    ALWAYS_ASSERT(access(pipe_name, F_OK) == -1, "Pipe %s already exists.", pipe_name);

    int check_err = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Pipe could not be created.");

    // Create a registration form.
    pipe_box_code_t *reg = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
    reg->code = 2;
    strcpy(reg->name.pipe, pipe_name);
    strcpy(reg->name.box, box_name);

    // Open the register pipe and write the registration form.
    int register_fd = open(register_pipe_name, O_WRONLY);
    ALWAYS_ASSERT(register_fd != -1, "Could not open the register pipe.");

    ssize_t bytes_written = write(register_fd, reg, sizeof(pipe_box_code_t));
    ALWAYS_ASSERT(bytes_written > 0, "Could not write to the register pipe.");

    free(reg);

    // Close the register pipe.
    ALWAYS_ASSERT(close(register_fd) == 0, "Could not close the register pipe.");

    // Open the pipe for reading and read.
    int pipe_fd = open(pipe_name, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Could not open the pipe.");

    ssize_t bytes_read = 0;
    while(true) {

        bytes_read = read(pipe_fd, stdout, sizeof(pipe_box_code_t));
        printf("Bytes read: %ld\n", bytes_read);

    }

    return -1;
}