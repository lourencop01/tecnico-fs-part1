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
#include <pthread.h>

void* register_publisher(void *arg) {

    register_client_t *reg = (register_client_t*) arg;
    ssize_t bytes = 1;
    char read_buff[MESSAGE_SIZE];

    int pipe_fd = open(reg->pipe_name, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->pipe_name);

    while(bytes > 0) {
        bytes = read(pipe_fd, read_buff, MESSAGE_SIZE - 1);
        printf("%s", read_buff);
        fflush(stdin);
    }

    close(pipe_fd);
    return NULL;
}

void read_registrations(int fd, int sessions) {

    pthread_t tid[sessions];  
    register_client_t *reg = (register_client_t*)malloc(sizeof(register_client_t));
    ssize_t bytes = -1;

        bytes = read(fd, reg, sizeof(register_client_t));
        ALWAYS_ASSERT(bytes == sizeof(register_client_t), "Could not read registration form.");

        switch (reg->code) {
        case 1:
            ALWAYS_ASSERT((pthread_create(&tid[0], NULL, &register_publisher, reg) == 0), 
                                                    "Could not create register_publisher thread.");
        default:
            ALWAYS_ASSERT((pthread_join(tid[0], NULL) == 0),
                          "Register_publisher thread could not join.");
            break;
        }
        
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

    read_registrations(register_fd, max_sessions);

    ALWAYS_ASSERT((unlink(register_pipe_name) == 0), "Could not remove %s.", register_pipe_name);

    return -1;
}