#include "logging.h"
#include "betterassert.h"
#include "structs.h"
#include "operations.h"
#include "producer-consumer.h"

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

// Mutex for reading and writing to the boxes array.
pthread_mutex_t boxes_mutex = PTHREAD_MUTEX_INITIALIZER;

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
        ALWAYS_ASSERT(pthread_mutex_destroy(&boxes_mutex) == 0, 
                        "Could not destroy boxes_mutex.");

        exit(EXIT_SUCCESS);

    }

    // Must be SIGQUIT - print a message and terminate the process
    fprintf(stderr, "Caught SIGQUIT - BOOM!\n");
    exit(EXIT_SUCCESS);
}

/*
 * Shutdowns a subscriber.
 * Input:
 *  - reg: form with information about the subscriber.
 */
void shutdown_subscriber(pipe_box_code_t *reg, const char *err_message) {

    // Send shutdown message to the register pipe.
    // Opens the subscriber's pipe for writing.
    int pipe_fd = open(reg->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Shutdown could not open %s.", reg->name.pipe);

    // Creates the shutdown message.
    char err[100] = "ERROR: ";
    strcat(err, err_message);

    // Writes the shutdown message to the pipe.
    ALWAYS_ASSERT(write(pipe_fd, err, strlen(err) + 1) > 0,
                                    "Could not write to %s.", reg->name.pipe);

    // Close the register pipe.
    ALWAYS_ASSERT(close(pipe_fd) != -1, "Could not close register pipe.");

    // Free the memory allocated for the register pipe.
    free(reg);

    return;

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

    pthread_mutex_lock(&boxes_mutex);
    // Looks for a box that corresponds to "name" in boxes and returns its index.
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (boxes[i].taken && !strcmp(boxes[i].box.box_name, name)) {
            pthread_mutex_unlock(&boxes_mutex);
            pthread_mutex_lock(&boxes[i].box_mutex);
            return i;
        }
    }

    pthread_mutex_unlock(&boxes_mutex);
    return -1;
}

/*
 * Finds an empty spot in the boxes.
 * Return value:
 * - index of the empty spot in boxes, if found.
 * - -1, if not found (box is full).
 */
int empty_spot() {

    pthread_mutex_lock(&boxes_mutex);
    //Finds an empty spot in boxes.
    for (int i = 0; i < MAX_BOX_NUMBER; i++) {
        if (!boxes[i].taken) {
            pthread_mutex_unlock(&boxes_mutex);
            return i;
        }
    }

    pthread_mutex_unlock(&boxes_mutex);
    return -1;

}

/*
 * Sends a list of taken boxes to the manager.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void list_boxes(pipe_box_code_t *args) {

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

    return;
}

/*
 * Removes a box from boxes.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void remove_box(pipe_box_code_t *args) {

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
        int box_index = find_box(args->name.box);
        boxes[box_index].taken = false;
        pthread_mutex_unlock(&boxes[box_index].box_mutex);
        pthread_mutex_unlock(&boxes[box_index].pub_box_mutex);
        pthread_mutex_unlock(&boxes[box_index].sub_box_mutex);
        pthread_cond_broadcast(&boxes[box_index].box_cond);
        pthread_mutex_destroy(&boxes[box_index].box_mutex);
        pthread_mutex_destroy(&boxes[box_index].pub_box_mutex);
        pthread_mutex_destroy(&boxes[box_index].sub_box_mutex);
        pthread_cond_destroy(&boxes[box_index].box_cond);

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

    return;
}

/*
 * Creates a box in boxes (by setting its taken value to true).
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void create_box(pipe_box_code_t *args) {

    // Allocate space for a reply.
    req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));

    // Sets the reply's code.
    reply->code = 4;

    // Variable to store an empty spot in boxes.
    int spot = empty_spot();

    // Tries to find the box in boxes.
    int exists = find_box(args->name.box);

    if (exists != -1) {

        // Sets the reamining reply's fields.
        reply->ret = -1;
        strcpy(reply->err_message, "Box already exists.");
        pthread_mutex_unlock(&boxes[exists].box_mutex);

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
            boxes[spot].box.box_size = 0;
            pthread_mutex_init(&boxes[spot].box_mutex, NULL);
            pthread_cond_init(&boxes[spot].box_cond, NULL);
            pthread_mutex_init(&boxes[spot].pub_box_mutex, NULL);
            pthread_mutex_init(&boxes[spot].sub_box_mutex, NULL);

            // Sets the remaining reply's fields.
            reply->ret = 0;
            strcpy(reply->err_message, "\0");

            // Closes the box in tfs.
            tfs_close(box_fd);

        } else {
            // Sets the remaining reply's fields.
            reply->ret = -1;
            strcpy(reply->err_message, "Could not create file in TFS.");
        }


    } else {

        // Sets the remaining reply's fields.
        reply->ret = -1;
        strcpy(reply->err_message, "Maximum box capacity reached.");
    }

    // Open the manager's pipe for writing and write the reply to it.
    int pipe_fd = open(args->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", args->name.pipe);

    ssize_t bytes_written = write(pipe_fd, reply, sizeof(req_reply_t));
    ALWAYS_ASSERT(bytes_written > 0, "MBroker could not write to the %s.", args->name.pipe);

    // Close the pipe and free the reply.
    free(reply);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close %s.", args->name.pipe);

    return;
}

/*
 * Registers a subscriber to a box.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void register_subscriber(pipe_box_code_t *reg) {
    
    // Finds the box to subscribe and increases its subscribers.
    int box_index = find_box(reg->name.box);

    if (box_index != -1) {
        boxes[box_index].box.n_subscribers++;
        pthread_mutex_unlock(&boxes[box_index].box_mutex);
    } else {
        shutdown_subscriber(reg, "Box does not exist.\n");
        return;
    }

    // Opens the subscriber's pipe for writing.
    int pipe_fd = open(reg->name.pipe, O_WRONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->name.pipe);

    // Opens the box in tfs with offset 0.
    int file_fd = tfs_open(reg->name.box, 0);
    if (file_fd == -1) {
        shutdown_subscriber(reg, "MBroker could not open the box for reading.\n");
        return;
    }

    // Creates a buffer to store messages.
    char buffer[MESSAGE_SIZE];
    memset(buffer, '\0', MESSAGE_SIZE);

    // Creates a variable to store the amount of bytes_written and of bytes_read.
    ssize_t bytes_written = 1;
    ssize_t bytes_read = 1;

    // Reads the messages in the box and sends the messages to the subscriber.
    pthread_mutex_lock(&boxes[box_index].sub_box_mutex);
    while (bytes_read > 0) {
        
        bytes_read = tfs_read(file_fd, buffer, MESSAGE_SIZE - 1);

        // If read 0 bytes, must wait for a publisher to write, then reads again.
        while (bytes_read == 0) {

            pthread_cond_wait(&boxes[box_index].box_cond, &boxes[box_index].sub_box_mutex);

            bytes_read = tfs_read(file_fd, buffer, MESSAGE_SIZE - 1);
            
        }
        
        bytes_written = write(pipe_fd, buffer, (size_t)bytes_read);

        if( bytes_written != bytes_read || bytes_written == -1) {

            fprintf(stderr, "An error occured sending the message to the subscriber.\n");
            pthread_mutex_unlock(&boxes[box_index].sub_box_mutex);

            // Closes the box and the pipe.
            ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.", 
                                                                                    reg->name.box);
            ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.", reg->name.pipe);
            
            // Decreases the number of subscribers.
            pthread_mutex_lock(&boxes[box_index].box_mutex);
            boxes[box_index].box.n_subscribers--;
            pthread_mutex_unlock(&boxes[box_index].box_mutex);

            return;

        }
        
    }
    pthread_mutex_unlock(&boxes[box_index].sub_box_mutex);

    // Closes the box and the pipe.
    ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.", reg->name.box);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.", reg->name.pipe);

    // Decreases the number of subscribers.
    pthread_mutex_unlock(&boxes[box_index].box_mutex);
    boxes[box_index].box.n_subscribers--;
    pthread_mutex_unlock(&boxes[box_index].box_mutex);

    return;
}

/*
 * Registers a publisher to a box.
 * Input:
 *  - args: pointer to a pipe_box_code_t struct.
 */
void register_publisher(pipe_box_code_t *reg) {

    // Finds the box to publish and increases its publishers.
    int box_index = find_box(reg->name.box);

    if (box_index == -1) {
        fprintf(stderr, "%s does not exist.\n", reg->name.box);
        return;
    } else if (boxes[box_index].box.n_publishers == 0) {
        boxes[box_index].box.n_publishers++;
        pthread_mutex_unlock(&boxes[box_index].box_mutex);
    } else {
        fprintf(stderr, "%s already has a publisher.\n", reg->name.box);
        pthread_mutex_unlock(&boxes[box_index].box_mutex);
        return;
    }

    // Opens the publisher's pipe for reading.
    int pipe_fd = open(reg->name.pipe, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "MBroker could not open %s.", reg->name.pipe);

    // Opens the box in tfs in append mode.
    int file_fd = tfs_open(reg->name.box, TFS_O_APPEND);
    if (file_fd == -1) {
        fprintf(stderr, "MBroker could not open the box for writing.\n");
        return;
    }

    // Creates a buffer to store messages.
    char read_buff[MESSAGE_SIZE];
    ssize_t bytes_read = 1;
    ssize_t bytes_written = 1;

    // Reads the messages from the pipe and writes them to the box.
    pthread_mutex_lock(&boxes[box_index].pub_box_mutex);
    while (bytes_read > 0) {

        bytes_read = read(pipe_fd, read_buff, MESSAGE_SIZE - 1);
        // Sets last character to \n.
        read_buff[strlen(read_buff)] = '\n';

        bytes_written = tfs_write(file_fd, read_buff, (size_t)bytes_read);
        boxes[box_index].box.box_size += (uint64_t)bytes_written;
        // If writes to the box, signals the subscribers that they can read.
        if (bytes_written > 0 && boxes[box_index].box.n_subscribers > 0) {
            pthread_cond_broadcast(&boxes[box_index].box_cond);
        }

        if (bytes_read != bytes_written) {

            fprintf(stderr, "Bytes written did not match with bytes read!\n");
            pthread_mutex_unlock(&boxes[box_index].pub_box_mutex);

            // Closes the box and the pipe.
            ALWAYS_ASSERT(tfs_close(file_fd) != -1,
                          "MBroker could not close the %s.", reg->name.box);
            ALWAYS_ASSERT(close(pipe_fd) != -1,
                          "MBroker could not close the %s.", reg->name.pipe);

            // Decreases the number of publishers.
            pthread_mutex_lock(&boxes[box_index].box_mutex);
            boxes[box_index].box.n_subscribers--;
            pthread_mutex_unlock(&boxes[box_index].box_mutex);

            return;
        }
    }
    pthread_mutex_unlock(&boxes[box_index].pub_box_mutex);
    // Closes the box and the pipe.
    ALWAYS_ASSERT(tfs_close(file_fd) != -1, "MBroker could not close the %s.",
                  reg->name.box);
    ALWAYS_ASSERT(close(pipe_fd) != -1, "MBroker could not close the %s.",
                  reg->name.pipe);

    // Decreases the number of publishers.
    pthread_mutex_lock(&boxes[box_index].box_mutex);
    boxes[box_index].box.n_publishers--;
    pthread_mutex_unlock(&boxes[box_index].box_mutex);
    return;
}

void *dequeue(void *args) {

    pc_queue_t *queue = (pc_queue_t*)args;

    pipe_box_code_t *req = NULL;

    while (true) {

        req = (pipe_box_code_t *)pcq_dequeue(queue);

        switch (req->code) {
        case 1:
            register_publisher(req);
            break;
        case 2:
            register_subscriber(req);
            break;
        case 3:
            create_box(req);
            break;
        case 5:
            remove_box(req);
            break;
        case 7:
            list_boxes(req);
            break;
        default:
            fprintf(stderr, "Invalid code received.\n");
            break;
        }

    }

    return NULL;
}

/*
 * Processes sessions and working threads.
 * Input:
 *  - register_pipe_name: pointer to a string with the register pipe's name.
 *  - max_sessions: maximum number of sessions to be accepted.
 */
void read_registrations(const char *register_pipe_name, int max_sessions) {

    pc_queue_t *queue = (pc_queue_t*)malloc(sizeof(pc_queue_t));
    pcq_create(queue, (size_t)max_sessions);

    pthread_t tid[max_sessions];

    for (int i = 0; i < max_sessions; i++) {
        ALWAYS_ASSERT((pthread_create(&tid[i], NULL, dequeue, queue) == 0),
                                                    "Could not create register_publisher thread.");
    }
    
    while (true) {

        int register_fd = open(register_pipe_name, O_RDONLY);
        ALWAYS_ASSERT(register_fd != -1, "Register pipe could not be open.");

        pipe_box_code_t *args = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
        ssize_t bytes = -1;

        bytes = read(register_fd, args, sizeof(pipe_box_code_t));
        ALWAYS_ASSERT(bytes == sizeof(pipe_box_code_t), "Could not read registration form.");

        pcq_enqueue(queue, args);

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

    // Check if the number of sessions is valid.
    ALWAYS_ASSERT(max_sessions > 0, "Please insert a value > 0 for the sessions.");

    //Check if the register pipe name is a valid path name.
    ALWAYS_ASSERT(strlen(register_pipe_name) < PIPE_NAME_SIZE, "Register pipe name is too long.");

    // Remove pipe if it does not exist
    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", register_pipe_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    //Initialization of Tecnico's file system.
    ALWAYS_ASSERT(tfs_init(NULL) == 0, "Could not initiate Tecnico file system.");

    //Creates the register pipe.
    int check_err = mkfifo(register_pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Register pipe could not be created.");

    read_registrations(register_pipe_name, max_sessions); 

    return -1;
}