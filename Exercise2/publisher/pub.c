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

void send_message(int fd) {

    char message[MESSAGE_SIZE];
    ssize_t bytes = -1;

    printf("You can now start typing your messages!\n");

    while (fgets(message, MESSAGE_SIZE, stdin) != NULL) {
        bytes = write(fd, message, strlen(message) + 1);
        ALWAYS_ASSERT(bytes == (strlen(message) + 1), "Number of bytes written is not equal to"
                                                                    " the number of bytes read.");
    }

    close(fd);
}

int main(int argc, char **argv) {

    (void)argc;

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

    //Check if box_name exists. TODO

    //Check if pipe_name is a valid path name. TODO

    // Check if pipe_name already exists. TODO

    int check_err = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Pipe could not be created.");

    pipe_box_code_t *reg = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
    strcpy(reg->name.box, box_name);
    strcpy(reg->name.pipe, pipe_name);
    reg->code = 1;

    int register_fd = open(register_pipe_name, O_WRONLY);
    ALWAYS_ASSERT(register_fd != -1, "Could not open the register pipe.");

    ssize_t bytes_written = write(register_fd, reg, sizeof(pipe_box_code_t));
    ALWAYS_ASSERT(bytes_written > 0, "Could not write to the register pipe.");

    close(register_fd);

    int pipe_fd = open(pipe_name, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Could not open the publisher's pipe.");

    send_message(pipe_fd);

    free(reg);
    ALWAYS_ASSERT((unlink(pipe_name) == 0), "Could not delete %s.", pipe_name);

    return -1;
}