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
#include <errno.h>

int message_count = 0;
/**
 * Signal handler function.
 */
 static void sig_handler(int sig) {
    if (sig == SIGINT) {
        // In some systems, after the handler call the signal gets reverted
        // to SIG_DFL (the default action associated with the signal).
        // So we set the signal handler back to our function after each trap.
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        printf("Messages read: %d\n", message_count);
        exit(EXIT_SUCCESS); // Exit the process
        return; // Resume execution at point of interruption
    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - BOOM!\n");
    exit(EXIT_SUCCESS);
}

void increase_counter(char *message) {
    char *token = strtok(message, "\n");
    while (token != NULL) {
        token = strtok(NULL, "\n");
        message_count++;
    }
}
int main(int argc, char **argv) {

    // Set the signal handler for SIGINT.
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

    //Check if the number of arguments is correct.
    ALWAYS_ASSERT(argc == 4, "Wrong number of arguments.");

    // Creates variables for later parsing of the command line arguments.
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

    // Remove pipe if it does not exist.
    if (unlink(pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", pipe_name,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Check if box_name is a valid box name.
    ALWAYS_ASSERT(strlen(box_name) < BOX_NAME_SIZE, "Box name is too long.");

    // Check if pipe_name is a valid path name.
    ALWAYS_ASSERT(strlen(pipe_name) < PIPE_NAME_SIZE, "Pipe name is too long.");

    // Create the subscriber's pipe
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

    // Close the register pipe.
    ALWAYS_ASSERT(close(register_fd) == 0, "Could not close the register pipe.");

    // Open the pipe for reading and read.
    int pipe_fd = open(pipe_name, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Could not open the pipe.");

    // Read messages from the pipe.
    char message[MESSAGE_SIZE];
    memset(message, '\0', MESSAGE_SIZE);

    ALWAYS_ASSERT(read(pipe_fd, message, MESSAGE_SIZE) != -1, "Could not read"
                                                              "from the pipe.");
    if(!strncmp(message, "ERROR:", 6)) {
        fprintf(stderr, "%s", message);        

        // Close and delete the subscriber's pipe.
        ALWAYS_ASSERT(close(pipe_fd) == 0, "Could not close the pipe.");
        ALWAYS_ASSERT(unlink(pipe_name) == 0, "Could not delete the pipe.");

        return -1;
    }

    while (true) {

        printf("%s", message);
        increase_counter(message);
        memset(message, '\0', MESSAGE_SIZE);
        ALWAYS_ASSERT(read(pipe_fd, message, MESSAGE_SIZE) != -1, "Could not read"                                              "from the pipe.");
    }

    // Close and delete the subscriber's pipe.
    ALWAYS_ASSERT(close(pipe_fd) == 0, "Could not close the pipe.");
    ALWAYS_ASSERT(unlink(pipe_name) == 0, "Could not delete the pipe.");

    // Free the registration form.
    free(reg);

    return -1;

}