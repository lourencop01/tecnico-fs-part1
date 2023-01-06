#include "betterassert.h"
#include "logging.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;

    char register_pipe_name[256];
    memset(register_pipe_name, '\0', 256);

    char pipe_name[256];
    memset(pipe_name, '\0', 256);

    char box_name[32];
    memset(box_name, '\0', 32);

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

    return -1;
}
