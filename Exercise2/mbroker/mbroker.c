#include "logging.h"
#include "betterassert.h"
#include "structs.h"
#include "operations.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

void* list_boxes(void *arg) {
    
    pipe_box_code_t *args = (pipe_box_code_t*) arg;
    box_listing_t *reply = (box_listing_t*)malloc(sizeof(box_listing_t));

    reply->code = 8;

    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    close(pipe_fd);

    return NULL;
}

void* remove_box(void *arg) {
    
    pipe_box_code_t *args = (pipe_box_code_t*) arg;
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    reply->code = 6;

    int check_err = tfs_unlink(args->name.box);
    if (check_err == -1) {
        
        reply->ret = check_err;
        strcpy(reply->err_message, "MBroker failed to remove the box.");

    } else {

        reply->ret = 0;
        strcpy(reply->err_message, "\0");
    }

    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    close(pipe_fd);

    return NULL;
}

void* create_box(void *arg) {
    
    pipe_box_code_t *args = (pipe_box_code_t*) arg;
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    printf("enters here\n");

    reply->code = 4;

    int box_fd = tfs_open(args->name.box, TFS_O_CREAT);
    printf("%d\n", box_fd);
    if (box_fd != -1) {

        reply->ret = 0;
        strcpy(reply->err_message, "\0");

        tfs_close(box_fd);

    } else {

        reply->ret = -1;
        strcpy(reply->err_message, "MBroker failed to create the box.");
    
    }

    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    close(pipe_fd);

    return NULL;
}

void* register_publisher(void *arg) {

    pipe_box_code_t *reg = (pipe_box_code_t*) arg;
    ssize_t bytes = 1;
    char read_buff[MESSAGE_SIZE];

    int pipe_fd = open(reg->name.pipe, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->name.pipe);

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
    pipe_box_code_t *args = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
    ssize_t bytes = -1;

        bytes = read(fd, args, sizeof(pipe_box_code_t));
        ALWAYS_ASSERT(bytes == sizeof(pipe_box_code_t), "Could not read registration form.");

        switch (args->code) {
        case 1:
            ALWAYS_ASSERT((pthread_create(&tid[0], NULL, &register_publisher, args) == 0), 
                                                    "Could not create register_publisher thread.");
            break;
        case 3:
            ALWAYS_ASSERT((pthread_create(&tid[0], NULL, &create_box, args) == 0), 
                                                    "Could not create register_publisher thread.");
            break;
        case 5:
            ALWAYS_ASSERT((pthread_create(&tid[0], NULL, &remove_box, args) == 0), 
                                                    "Could not create register_publisher thread.");
            break;
        case 7:
            ALWAYS_ASSERT((pthread_create(&tid[0], NULL, &list_boxes, args) == 0), 
                                                    "Could not create register_publisher thread.");
            break;
        default:
            break;
        }
        ALWAYS_ASSERT((pthread_join(tid[0], NULL) == 0),
                          "Register_publisher thread could not join.");
}

int main(int argc, char **argv) {
    
    (void)argc;
    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    int max_sessions = -1;

    ALWAYS_ASSERT(tfs_init(NULL) == 0, "Could not initiate Tecnico file system.");

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

    ALWAYS_ASSERT(tfs_destroy() == 0, "Could not destroy Tecnico file system.");

    return -1;
}