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
#include <signal.h>

int active_sessions = 0;

box_status_t boxes[MAX_BOX_NUMBER];

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        // In some systems, after the handler call the signal gets reverted
        // to SIG_DFL (the default action associated with the signal).
        // So we set the signal handler back to our function after each trap.
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "Caught SIGINT\n");
        return; // Resume execution at point of interruption
    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - BOOM!\n");
    return;
}

void* list_boxes(void *arg) {

    active_sessions ++;
    
    pipe_box_code_t *args = (pipe_box_code_t*) arg;
    box_listing_t *reply = (box_listing_t*)malloc(sizeof(box_listing_t));

    reply->code = 8;
    reply->box_amount = 0;

    
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (boxes[i].taken){
        //TODO ver se isto funciona ou se temos de copiar valor a valor
        reply->boxes[i] = boxes[i].box;
        reply->box_amount++;
        }
    }

    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(box_listing_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    close(pipe_fd);

    return NULL;
}

void* remove_box(void *arg) {

    active_sessions ++;
    
    pipe_box_code_t *args = (pipe_box_code_t*) arg;
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    reply->code = 6;

    int check_err = tfs_unlink(args->name.box);
    if (check_err == -1) {
        
        reply->ret = check_err;
        strcpy(reply->err_message, "MBroker failed to remove the box.");

    } else {

        for (int i = 0; i < MAX_BOX_NUMBER; i++) {
            if (!strcmp(boxes[i].box.box_name, args->name.box) && boxes[i].taken) {
                boxes[i].taken = false;
            }
        }
        
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

    active_sessions ++;
    
    pipe_box_code_t *args = (pipe_box_code_t*) arg;
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    reply->code = 4;

    bool found = false;
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (!strcmp(boxes[i].box.box_name, args->name.box) && boxes[i].taken) {                
            found = true;
            reply->ret = -1;
            strcpy(reply->err_message, "Box already exists.");
            break;
        }
    }

    if (!found) {
        int box_fd = tfs_open(args->name.box, TFS_O_CREAT);

        if (box_fd != -1) {

            for (int i = 0; i < MAX_BOX_NUMBER; i++) {
                if (!boxes[i].taken) {
                    boxes[i].taken = true;
                    strcpy(boxes[i].box.box_name, args->name.box);
                    strcpy(boxes[i].box.tfs_file, args->name.box);
                    boxes[i].box.n_publishers = 0;
                    boxes[i].box.n_subscribers = 0;
                    break;
                }
            }

            reply->ret = 0;
            strcpy(reply->err_message, "\0");

            tfs_close(box_fd);

        } else {

            reply->ret = -1;
            strcpy(reply->err_message, "MBroker failed to create the box.");
        
        }
    }

    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    close(pipe_fd);

    return NULL;
}

void* register_publisher(void *arg) {

    active_sessions ++;

    int box_index = -1;

    pipe_box_code_t *reg = (pipe_box_code_t*) arg;

    for (box_index = 0; box_index < MAX_BOX_NUMBER; box_index++) {
        if (!strcmp(reg->name.box, boxes[box_index].box.box_name) && boxes[box_index].taken) {
            if (boxes[box_index].box.n_publishers == 0) {
                boxes[box_index].box.n_publishers++;
            } else {
                fprintf(stderr, "%s already has a publisher.\n", reg->name.box);
                return NULL;
            }
            break;
        }
    }

    int pipe_fd = open(reg->name.pipe, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->name.pipe);
    
    int file_fd = tfs_open(reg->name.box, TFS_O_APPEND);
    if (file_fd == -1) {
        fprintf(stderr, "MBroker could not open the box for writing.\n");
        return NULL;
    }

    char read_buff[MESSAGE_SIZE];
    ssize_t bytes_read = 1;
    ssize_t bytes_written = 1;

    while(bytes_read > 0) {
        bytes_read = read(pipe_fd, read_buff, MESSAGE_SIZE - 1);
        bytes_written = tfs_write(file_fd, read_buff, (size_t)bytes_read);
        printf("Wrote %s\n bytes = %zu", read_buff, bytes_written);
        if (bytes_read != bytes_written) {
            fprintf(stderr, "Bytes written did not match with bytes read!\n");
            return NULL;
        }
    }

    tfs_close(file_fd);

    // TODO DELETE THIS IS JUST TO TEST
    ssize_t bytessss = 1;
    int new_fd = tfs_open(reg->name.box, 0);
    
    while(bytessss > 0) {
        printf("TA A LER DO FICHEIRO\n");
        bytessss = tfs_read(new_fd, read_buff, MESSAGE_SIZE - 1);
        printf("LIDO: %s\n", read_buff);
    }
    tfs_close(new_fd);

    
    close(pipe_fd);

    boxes[box_index].box.n_publishers--;

    return NULL;
}

void read_registrations(const char *register_pipe_name, int max_sessions) {

    pthread_t tid[max_sessions];

    while (true) {

        int register_fd = open(register_pipe_name, O_RDONLY);
        ALWAYS_ASSERT(register_fd != -1, "Register pipe could not be open.");

        pipe_box_code_t *args = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
        ssize_t bytes = -1;

        bytes = read(register_fd, args, sizeof(pipe_box_code_t));
        ALWAYS_ASSERT(bytes == sizeof(pipe_box_code_t), "Could not read registration form.");

        if (active_sessions < max_sessions) {

            switch (args->code) {
            case 1:
                ALWAYS_ASSERT((pthread_create(&tid[0], NULL, register_publisher, args) == 0), 
                                                    "Could not create register_publisher thread.");
                break;
            case 3:
                ALWAYS_ASSERT((pthread_create(&tid[0], NULL, create_box, args) == 0), 
                                                    "Could not create register_publisher thread.");
                break;
            case 5:
                ALWAYS_ASSERT((pthread_create(&tid[0], NULL, remove_box, args) == 0), 
                                                    "Could not create register_publisher thread.");
                break;
            case 7:
                ALWAYS_ASSERT((pthread_create(&tid[0], NULL, list_boxes, args) == 0), 
                                                    "Could not create register_publisher thread.");
                break;
            default:
                break;
            }
            ALWAYS_ASSERT((pthread_join(tid[0], NULL) == 0),
                                                    "Register_publisher thread could not join.");

            active_sessions--;
        
        }
        
        close(register_fd);
        
    }
}

int main(int argc, char **argv) {
    
    ALWAYS_ASSERT(argc == 3, "Mbroker has not received the correct amount of arguments.\n"
                                                                            "Please try again.");
    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        boxes[i].taken = false;
    }

    // Argument parsing of a mbroker process launch.
    strcpy(register_pipe_name, argv[1]);

    int max_sessions = atoi(argv[2]);
    ALWAYS_ASSERT(max_sessions > 0, "Please insert a value > 0 for the sessions.");

    //Initialization of Tecnico's file system.
    ALWAYS_ASSERT(tfs_init(NULL) == 0, "Could not initiate Tecnico file system.");

    //Check if the register pipe name is a valid path name. TODO

    //Checking if register pipe already exists TODO ver lab pipes

    //Creates the register pipe.
    int check_err = mkfifo(register_pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Register pipe could not be created.");

    read_registrations(register_pipe_name, max_sessions);

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
    exit(EXIT_FAILURE);
    }

    //ALWAYS_ASSERT((signal(SIGINT, sig_handler) != SIG_ERR), "Could not catch SIGINT.");

    ALWAYS_ASSERT((unlink(register_pipe_name) == 0), "Could not remove %s.", register_pipe_name);

    ALWAYS_ASSERT(tfs_destroy() == 0, "Could not destroy Tecnico file system.");    

    return -1;
}