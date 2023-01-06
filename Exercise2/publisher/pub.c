#include "betterassert.h"
#include "logging.h"
#include "structs/structs.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

    //Check if register pipe exists. TODO

    //Check if box_name exists. TODO

    //Check if pipe_name is a valid path name. TODO

    //Check if pipe_name already exists. TODO

    int check_err = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Pipe could not be created.");

    register_client_t *reg = (register_client_t*)malloc(sizeof(register_client_t));
    strcpy(reg->box_name, box_name);
    strcpy(reg->pipe_name, pipe_name);
    reg->code = 1;

    int register_fd = open(register_pipe_name, O_WRONLY);
    ALWAYS_ASSERT(register_fd != -1, "Could not open the register pipe.");

    ssize_t bytes_written = write(register_fd, reg, sizeof(reg));
    ALWAYS_ASSERT(bytes_written > 0, "Could not write to the register pipe.");

    int pipe_fd = open(pipe_name, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Could not open the publisher's pipe.");

    printf("You can now start typing your messages.\n");
    char message[MESSAGE_SIZE];
    while (fgets(message, MESSAGE_SIZE, stdin) != NULL) {
        bytes_written = write(pipe_fd, message, strlen(message));
        ALWAYS_ASSERT(bytes_written == strlen(message), "Number of bytes written is not equal to"
                                                        " the number of bytes read.");
    }

    return -1;
}
