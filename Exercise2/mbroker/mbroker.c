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
#include <pthread.h>
#include <errno.h> 

pthread_cond_t pub_sub_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t pub_sub_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t boxes_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sessions = PTHREAD_MUTEX_INITIALIZER;

// Number of active sessions (clients).
int active_sessions = 0;

// Array of boxes (initialized at taken = false for all boxes).
box_status_t boxes[MAX_BOX_NUMBER];

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

        ALWAYS_ASSERT(tfs_destroy() == 0,
                      "Could not destroy Tecnico file system.");

        exit(EXIT_SUCCESS);

    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - BOOM!\n");
    exit(EXIT_SUCCESS);
}

/*
 * Finds a box.
 * Input:
 *  - name: name of the box to be found.
 * Return value:
 * - index of the box in boxes, if found.
 * - -1, if not found.
 */
int find_box(const char *name) {

    // Looks for a box that corresponds to "name" in boxes and returns its index.
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (boxes[i].taken && !strcmp(boxes[i].box.box_name, name)) {
            return i;
        }
    }

    return -1;
}

/*
 * Finds an empty spot in the boxes.
 * Return value:
 * - index of the empty spot in boxes, if found.
 * - -1, if not found (box is full).
 */
int empty_spot() {

    //Finds an empty spot in boxes.
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (!boxes[i].taken) {
            return i;
        }
    }
    return -1;

}

/*
 * Sends a list of taken boxes to the manager.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void* list_boxes(void *arg) {

    // Increase the number of active sessions.
    pthread_mutex_lock(&sessions);
    active_sessions ++;
    pthread_mutex_unlock(&sessions);

    // Cast the argument to a pipe_box_code_t struct.
    pipe_box_code_t *args = (pipe_box_code_t*) arg;

    // Allocate space for a reply.
    box_listing_t *reply = (box_listing_t*)malloc(sizeof(box_listing_t));

    // Initialize some of the reply's fields.
    reply->code = 8;
    reply->box_amount = 0;

    // Finds taken boxes and adds them to the reply.
    pthread_mutex_lock(&boxes_mutex);
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (boxes[i].taken){
        reply->boxes[i] = boxes[i].box;
        reply->box_amount++;
        }
    }
    pthread_mutex_unlock(&boxes_mutex);

    // Open the manager's pipe for writing and write the reply to it.
    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(box_listing_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    // Close the pipe and free the reply.
    free(reply);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close %s.", args->name.pipe);

    return NULL;
}

/*
 * Removes a box from boxes.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void* remove_box(void *arg) {

    // Increase the number of active sessions.
    pthread_mutex_lock(&sessions);
    active_sessions ++;
    pthread_mutex_unlock(&sessions);

    // Cast the argument to a pipe_box_code_t struct.
    pipe_box_code_t *args = (pipe_box_code_t*) arg;

    // Allocate space for a reply.
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    // Initialize the reply's code field.
    reply->code = 6;

    // Tries to remove the box from tfs.
    int check_err = tfs_unlink(args->name.box);
    if (check_err == -1) {

        // Sets the remaining reply's fields.
        reply->ret = check_err;
        strcpy(reply->err_message, "MBroker failed to remove the box.");

    } else {

        // Finds box and sets its taken value to false.
        pthread_mutex_lock(&boxes_mutex);
        int box_index = find_box(args->name.box);
        boxes[box_index].taken = false;
        pthread_mutex_unlock(&boxes_mutex);

        // Sets the remaining reply's fields.
        reply->ret = 0;
        strcpy(reply->err_message, "\0");
    }

    // Open the manager's pipe for writing and write the reply to it.
    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    // Close the pipe and free the reply.
    free(reply);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close %s.", args->name.pipe);

    return NULL;
}

/*
 * Creates a box in boxes (by setting its taken value to true).
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void* create_box(void *arg) {

    // Increase the number of active sessions.
    pthread_mutex_lock(&sessions);
    active_sessions ++;
    pthread_mutex_unlock(&sessions);
    
    // Cast the argument to a pipe_box_code_t struct.
    pipe_box_code_t *args = (pipe_box_code_t*) arg;

    // Allocate space for a reply.
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    // Sets the reply's code.
    reply->code = 4;

    // Tries to find an empty spot in the boxes.
    pthread_mutex_lock(&boxes_mutex);
    int spot = empty_spot();

    if (find_box(args->name.box) != -1) {

        // Sets the reamining reply's fields.
        reply->ret = -1;
        strcpy(reply->err_message, "Box already exists.");

    } else if (spot != -1) {

        // Creates the box in tfs.
        int box_fd = tfs_open(args->name.box, TFS_O_CREAT);

        
        if (box_fd != -1) {

            // Sets the box's fields.
            boxes[spot].taken = true;
            strcpy(boxes[spot].box.box_name, args->name.box);
            strcpy(boxes[spot].box.tfs_file, args->name.box);
            boxes[spot].box.n_publishers = 0;
            boxes[spot].box.n_subscribers = 0;

            // Sets the remaining reply's fields.
            reply->ret = 0;
            strcpy(reply->err_message, "\0");

            // Closes the box in tfs.
            tfs_close(box_fd);
        }

    } else {

        // Sets the remaining reply's fields.
        reply->ret = -1;
        strcpy(reply->err_message, "Maximum box capacity reached.");

    }
    pthread_mutex_unlock(&boxes_mutex);

    // Open the manager's pipe for writing and write the reply to it.
    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    // Close the pipe and free the reply.
    free(reply);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close %s.", args->name.pipe);

    return NULL;
}

/*
 * Registers a subscriber to a box.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void* register_subscriber(void *arg) {

    // Increase the number of active sessions.
    pthread_mutex_lock(&sessions);
    active_sessions ++;
    pthread_mutex_unlock(&sessions);

    // Cast the argument to a pipe_box_code_t struct.
    pipe_box_code_t *reg = (pipe_box_code_t*) arg;
    
    // Finds the box to subscribe and increases its subscribers.
    pthread_mutex_lock(&boxes_mutex);
    int box_index = find_box(reg->name.box);

    if (box_index != -1) {
        boxes[box_index].box.n_subscribers++;
    } else {
        fprintf(stderr, "%s does not exist.\n", reg->name.box);
        pthread_mutex_unlock(&boxes_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&boxes_mutex);

    // Opens the subscriber's pipe for writing.
    int pipe_fd = open(reg->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->name.pipe);

    // Opens the box in tfs with offset 0.
    int file_fd = tfs_open(reg->name.box, 0);
    if (file_fd == -1) {
        fprintf(stderr, "MBroker could not open the box for writing.\n");
        return NULL;
    }

    // Creates a buffer to store messages.
    char buffer[MESSAGE_SIZE];
    memset(buffer, '\0', MESSAGE_SIZE);

    // Creates a variable to store the amount of bytes_written and of bytes_read.
    ssize_t bytes_written = 1;
    ssize_t bytes_read = 1;

    // Reads the messages in the box and sends the messages to the subscriber.
    pthread_mutex_lock(&pub_sub_mutex);
    printf("sub locks mutex\n");
    while (bytes_read > 0) {
        
        bytes_read = tfs_read(file_fd, buffer, MESSAGE_SIZE - 1);

        // If read 0 bytes, must wait for a publisher to write, then reads again.
        if (bytes_read == 0) {
            printf("sub waits\n");
            pthread_cond_wait(&pub_sub_cond, &pub_sub_mutex);
            printf("sub got signalled.\n");
            bytes_read = tfs_read(file_fd, buffer, MESSAGE_SIZE - 1);
        }

        bytes_written = write(pipe_fd, buffer, (size_t)bytes_read);

        if( bytes_written != bytes_read) {

            fprintf(stderr, "An error occured sending the message to the subscriber.\n");
            
            pthread_mutex_unlock(&pub_sub_mutex);
            
            // Closes the box and the pipe.
            ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.", 
                                                                                    reg->name.box);
            ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.", reg->name.pipe);
            
            // Decreases the number of subscribers.
            pthread_mutex_lock(&boxes_mutex);
            boxes[box_index].box.n_subscribers--;
            pthread_mutex_unlock(&boxes_mutex);
            
            return NULL;

        }
        
    }
    pthread_mutex_unlock(&pub_sub_mutex);

    // Closes the box and the pipe.
    ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.", reg->name.box);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.", reg->name.pipe);

    // Decreases the number of subscribers.
    pthread_mutex_lock(&boxes_mutex);
    boxes[box_index].box.n_subscribers--;
    pthread_mutex_unlock(&boxes_mutex);

    return NULL;
}

/*
 * Registers a publisher to a box.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void* register_publisher(void *arg) {

    // Increase the number of active sessions.
    pthread_mutex_lock(&sessions);
    active_sessions ++;
    pthread_mutex_unlock(&sessions);

    // Casts the argument to a pipe_box_code_t struct.
    pipe_box_code_t *reg = (pipe_box_code_t*) arg;

    // Finds the box to publish and increases its publishers.
    pthread_mutex_lock(&boxes_mutex);
    int box_index = find_box(reg->name.box);

    if (box_index == -1) {
        fprintf(stderr, "%s does not exist.\n", reg->name.box);
        pthread_mutex_unlock(&boxes_mutex);
        return NULL;
    }
    else if (boxes[box_index].box.n_publishers == 0) {
        boxes[box_index].box.n_publishers++;
    } else {
        fprintf(stderr, "%s already has a publisher.\n", reg->name.box);
        pthread_mutex_unlock(&boxes_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&boxes_mutex);

    // Opens the publisher's pipe for reading.
    int pipe_fd = open(reg->name.pipe, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->name.pipe);

    // Opens the box in tfs in append mode.
    int file_fd = tfs_open(reg->name.box, TFS_O_APPEND);
    if (file_fd == -1) {
        fprintf(stderr, "MBroker could not open the box for writing.\n");
        return NULL;
    }

    // Creates a buffer to store messages.
    char read_buff[MESSAGE_SIZE];
    ssize_t bytes_read = 1;
    ssize_t bytes_written = 1;

    // Reads the messages from the pipe and writes them to the box.
    pthread_mutex_lock(&pub_sub_mutex);
    printf("pub locks mutex\n");
    while(bytes_read > 0) {

        bytes_read = read(pipe_fd, read_buff, MESSAGE_SIZE - 1);
        // Sets last character to \n.
        read_buff[strlen(read_buff)] = '\n';

        bytes_written = tfs_write(file_fd, read_buff, (size_t)bytes_read);
        // If writes to the box, signals the subscribers that they can read.
        if (bytes_written > 0) {
            printf("pub signals\n");
            pthread_cond_signal(&pub_sub_cond);
        }

        if (bytes_read != bytes_written) {
            
            fprintf(stderr, "Bytes written did not match with bytes read!\n");
            pthread_mutex_unlock(&pub_sub_mutex);

            // Closes the box and the pipe.
            ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.", 
                                                                                    reg->name.box);
            ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.", reg->name.pipe);

            // Decreases the number of publishers.
            pthread_mutex_lock(&boxes_mutex);
            boxes[box_index].box.n_publishers--;
            pthread_mutex_unlock(&boxes_mutex);
            
            return NULL;
        }

    }

    pthread_mutex_unlock(&pub_sub_mutex);
    printf("pub unlocked mutex\n");

    // Closes the box and the pipe.
    ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.", reg->name.box);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.", reg->name.pipe);

    // Decreases the number of publishers.
    pthread_mutex_lock(&boxes_mutex);
    boxes[box_index].box.n_publishers--;
    pthread_mutex_unlock(&boxes_mutex);

    return NULL;
}

/*
 * Processes sessions and working threads.
 * Input:
 *  - register_pipe_name: pointer to a string with the register pipe's name.
 *  - max_sessions: maximum number of sessions to be accepted.
 */
void read_registrations(const char *register_pipe_name, int max_sessions) {

    pthread_t tid[max_sessions];

    while (true) {

        printf("ABRE PARA READ\n");
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
            case 2:
                ALWAYS_ASSERT((pthread_create(&tid[1], NULL, register_subscriber, args) == 0),
                                                    "Could not create register_subscriber thread.");
                break;
            case 3:
                ALWAYS_ASSERT((pthread_create(&tid[2], NULL, create_box, args) == 0), 
                                                            "Could not create create_box thread.");
                break;
            case 5:
                ALWAYS_ASSERT((pthread_create(&tid[3], NULL, remove_box, args) == 0), 
                                                            "Could not create remove_box thread.");
                break;
            case 7:
                ALWAYS_ASSERT((pthread_create(&tid[4], NULL, list_boxes, args) == 0), 
                                                            "Could not create list_boxes thread.");
                break;
            default:
                fprintf(stderr, "Invalid code received.\n");
                break;
            }
            pthread_join(tid[0], NULL);
            pthread_join(tid[1], NULL);
            pthread_join(tid[2], NULL);
            pthread_join(tid[3], NULL);
            pthread_join(tid[4], NULL);

        pthread_mutex_lock(&sessions);
        active_sessions --;
        pthread_mutex_unlock(&sessions);

        }
        
        close(register_fd);
        
    }
}

int main(int argc, char **argv) {

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }

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

    // Remove pipe if it does not exist
    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", register_pipe_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    //Creates the register pipe.
    int check_err = mkfifo(register_pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Register pipe could not be created.");

    read_registrations(register_pipe_name, max_sessions); 

    return -1;
}