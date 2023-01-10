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

/**
 * This function is used to send messages by the publisher to
 * its designed box.
 * 
 * Input:
 *  - fd: The file descriptor of the publisher's pipe.
*/
void send_message(int fd) {

    // Creates a buffer to store the message.
    char message[MESSAGE_SIZE];

    // Creates a variable to store the number of bytes written.
    ssize_t bytes = -1;

    printf("Insert nuclear codes:\n");

    // While loop to read the messages from stdin until CTRL+D (EOF) is pressed.
    while (fgets(message, MESSAGE_SIZE, stdin) != NULL) {
        //remove the '\n' character from the message
        message[strlen(message) - 1] = '\0';
        bytes = write(fd, message, strlen(message) + 1);
        printf("Message sent has %zu bytes\n", bytes);
        ALWAYS_ASSERT(bytes == (strlen(message) + 1), "Number of bytes written is not equal to"
                                                                    " the number of bytes read.");
    }

    // Close the publisher's pipe.
    ALWAYS_ASSERT(close(fd) == 0, "Could not close the publisher's pipe.");
}

int main(int argc, char **argv) {

    // Checks if the correct amount of arguments is passed.
    ALWAYS_ASSERT(argc == 4, "This process takes 4 arguments, please try again.");

    // Creates variables for later parsing of the command line arguments.
    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    char pipe_name[PIPE_NAME_SIZE];
    memset(pipe_name, '\0', PIPE_NAME_SIZE);

    char box_name[BOX_NAME_SIZE];
    memset(box_name, '\0', BOX_NAME_SIZE);

    // Argument parsing of a publisher process launch.
    strcpy(register_pipe_name, argv[1]);
    strcpy(pipe_name, argv[2]);
    strcpy(box_name, argv[3]);

    //Check if box_name exists nd is valid. TODO
    ALWAYS_ASSERT(strlen(box_name) < BOX_NAME_SIZE, "Box name is too long. Please try again");

    //Check if box_name already exists in tfs. TODO CHECK IF THIS WORKS
    ALWAYS_ASSERT(access(box_name, F_OK) == -1, "Box does not exist. Try a different name.");

    //Check if pipe_name is a valid path name. TODO
    ALWAYS_ASSERT(strlen(pipe_name) < PIPE_NAME_SIZE, "Pipe name is too long. Please try again");

    // Check if pipe_name already exists. TODO
    ALWAYS_ASSERT(access(pipe_name, F_OK) == -1, "Pipe already exists. Try a different name.");

    // Create the client pipe.
    int check_err = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Pipe could not be created.");

    // Create a registration form.
    pipe_box_code_t *reg = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
    strcpy(reg->name.box, box_name);
    strcpy(reg->name.pipe, pipe_name);
    reg->code = 1;

    // Open the register pipe and write the registration form.
    int register_fd = open(register_pipe_name, O_WRONLY);
    ALWAYS_ASSERT(register_fd != -1, "Could not open the register pipe.");

    ssize_t bytes_written = write(register_fd, reg, sizeof(pipe_box_code_t));
    ALWAYS_ASSERT(bytes_written > 0, "Could not write to the register pipe.");

    // Close the register pipe.
    ALWAYS_ASSERT(close(register_fd) == 0, "Could not close the register pipe.");

    // Open the publisher's pipe for writing.
    int pipe_fd = open(pipe_name, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Could not open the publisher's pipe.");

    // Send the messages.
    send_message(pipe_fd);

    // Free the registration form and delete the pipe.
    free(reg);
    ALWAYS_ASSERT((unlink(pipe_name) == 0), "Could not delete %s.", pipe_name);

    // Exit the program.
    return -1;
}