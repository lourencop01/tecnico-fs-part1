#include "logging.h"
#include "betterassert.h"
#include "structs.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void read_registrations(int fd) {
    
    register_client_t *reg = NULL;
    ssize_t bytes = -1; 

    
        bytes = read(fd, reg, sizeof(register_client_t));
        ALWAYS_ASSERT(bytes == sizeof(register_client_t), "Could not read registration form.");
        printf("READ REG! PIPE NAME = %s\n", reg->pipe_name);
        // Sends the register to a thread TODO

    

}

int main(int argc, char **argv) {
    (void)argc;

    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    int max_sessions = -1;

    // Argument parsing of a mbroker process launch.
    strcpy(register_pipe_name, argv[1]);

    int check_err = sscanf(argv[2], "%d", &max_sessions);
    ALWAYS_ASSERT(check_err == 1, "Could not parse max_sessions from command line.");

    //Check if the register pipe name is a valid path name. TODO

    //Checking if register pipe already exists TODO ver lab pipes

    //Creates the register pipe.
    check_err = mkfifo(register_pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Register pipe could not be created.");

    int register_fd = open(register_pipe_name, O_RDONLY);
    ALWAYS_ASSERT(register_fd != -1, "Register pipe could not be open.");

    read_registrations(register_fd);
    



    return -1;
}
