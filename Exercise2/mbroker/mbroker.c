#include "logging.h"
#include "betterassert.h"

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

    int max_sessions = -1;

    // Argument parsing of a mbroker process launch.
    strcpy(register_pipe_name, argv[1]);

    //Check if the register pipe name is a valid path name. TODO

    ALWAYS_ASSERT((sscanf(argv[2], "%d", &max_sessions) == 1), 
                "Could not parse max_sessions from command line.");


    //Checking if register pipe already exists TODO ver lab pipes

    //Creates the register pipe.
    int value = mkfifo(register_pipe_name, 0640);
    ALWAYS_ASSERT(value == 0, "Register pipe could not be created.");



    return -1;
}
