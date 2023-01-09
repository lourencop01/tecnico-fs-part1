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

int main(int argc, char **argv) {
    
    // // Checks if the correct amount of arguments is passed.
    ALWAYS_ASSERT((argc != 4 || argc != 5), "Number of arguments is incorrect, please try again.");

    // Creates variables for later parsing of the command line arguments.
    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    char pipe_name[PIPE_NAME_SIZE];
    memset(pipe_name, '\0', PIPE_NAME_SIZE);

    char box_name[BOX_NAME_SIZE];
    memset(box_name, '\0', BOX_NAME_SIZE);

    // Argument parsing of a manager process launch.
    strcpy(register_pipe_name, argv[1]);
    strcpy(pipe_name, argv[2]);

    // Create a manager registration form.
    pipe_box_code_t *reg = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
    strcpy(reg->name.pipe, pipe_name);

    // Create a pipe for the manager to communicate with the MBroker.
    int check_err = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Pipe could not be created.");

    // Check if pipe_name is a valid path name. TODO
    ALWAYS_ASSERT((strlen(pipe_name) < PIPE_NAME_SIZE), "Pipe name is too long, please try again.");

    // Checks if the user wants the manager to list the boxes, or to create/remove a box.
    // Parses the remaining arguments accordingly.
    if (!strcmp(argv[3], "create")) {
        strcpy(box_name, argv[4]);
        strcpy(reg->name.box, box_name);
        reg->code = 3;

    } else if (!strcmp(argv[3], "remove")) {
        strcpy(box_name, argv[4]);
        strcpy(reg->name.box, box_name);
        reg->code = 5;

    } else if (!strcmp(argv[3], "list")) {
        reg->code = 7;
        strcpy(reg->name.box, box_name);

    } else {
        fprintf(stderr, "Please insert a correct input command.\n");
        exit(EXIT_SUCCESS);
    }

    // Opens the register pipe and writes the registration form to it.
    int register_fd = open(register_pipe_name, O_WRONLY);
    ALWAYS_ASSERT(register_fd != -1, "Could not open the register pipe.");

    ssize_t bytes = write(register_fd, reg, sizeof(pipe_box_code_t));
    ALWAYS_ASSERT(bytes > 0, "Could not write to the register pipe.");

    // Closes the register pipe.
    ALWAYS_ASSERT(close(register_fd) == 0, "Could not close the register pipe.");

    // Opens the pipe in read mode for the manager to communicate with the MBroker.
    int pipe_fd = open(pipe_name, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Manager could not open %s.", pipe_name);

    // Checks if the manager is supposed to list the boxes, or create/remove a box.
    if (reg->code == 3 || reg->code == 5) {

        // Creates a box creation/removal form to read the reply from the MBroker.
        req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));
        
        // Reads the reply.
        bytes = read(pipe_fd, reply, sizeof(req_reply_t));
        ALWAYS_ASSERT(bytes == sizeof(req_reply_t), "Manager failed to read from %s.", pipe_name);

        // Checks if the box was created/removed successfully.
        if (reply->ret == 0) {
            fprintf(stdout, "OK\n");
        } else {
            fprintf(stdout, "ERROR %s\n", reply->err_message);
        }

    } else {

        // Creates a box listing form to read the reply from the MBroker.
        box_listing_t *reply = (box_listing_t*)malloc(sizeof(box_listing_t));

        // Reads the reply.
        bytes = read(pipe_fd, reply, sizeof(box_listing_t));
        ALWAYS_ASSERT(bytes == sizeof(box_listing_t), "Manager failed to read from %s.", pipe_name);

        // Sets the number of boxes to an int variable.
        int box_number = reply->box_amount;

        // Checks if there are no boxes to list.
        if (box_number == 0) {
            fprintf(stdout, "NO BOXES FOUND\n");
        } else {

            // Creates a temporary box to store values.
            box_t *temp_box = (box_t*)malloc(sizeof(box_t));
            
            // Organizes the box list alphabetically.
            for (int i = 0; i < box_number; i++) {
                for (int j = i + 1; j < box_number; j++) {
                    if (strcmp(reply->boxes[i].box_name,reply->boxes[j].box_name) > 0) {

                        temp_box->box_size = reply->boxes[i].box_size;
                        temp_box->n_publishers = reply->boxes[i].n_publishers;
                        temp_box->n_subscribers = reply->boxes[i].n_subscribers;
                        strcpy(temp_box->box_name,reply->boxes[i].box_name);

                        reply->boxes[i].box_size = reply->boxes[j].box_size;
                        reply->boxes[i].n_publishers = reply->boxes[j].n_publishers;
                        reply->boxes[i].n_subscribers = reply->boxes[j].n_subscribers;
                        strcpy(reply->boxes[i].box_name,reply->boxes[j].box_name);

                        reply->boxes[j].box_size = temp_box->box_size;
                        reply->boxes[j].n_publishers = temp_box->n_publishers;
                        reply->boxes[j].n_subscribers = temp_box->n_subscribers;
                        strcpy(reply->boxes[j].box_name,temp_box->box_name);
                    
                    }
                }
            }

            // Frees the memory allocated for the temporary box.
            free(temp_box);

            // Prints the box list.
            for (int i = 0; i < box_number; i++) {
                fprintf(stdout, "%s %zu %zu %zu\n", reply->boxes[i].box_name, 
                                            reply->boxes[i].box_size, reply->boxes[i].n_publishers, 
                                                                    reply->boxes[i].n_subscribers);
            }
        }
    }

    // Closes and deletes the client's pipe.
    ALWAYS_ASSERT(close(pipe_fd) == 0, "Could not close %s.", pipe_name);
    ALWAYS_ASSERT((unlink(pipe_name) == 0), "Could not delete %s.", pipe_name);

    // Exits the program.
    return -1;
}