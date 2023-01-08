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
    (void)argc;

    char register_pipe_name[PIPE_NAME_SIZE];
    memset(register_pipe_name, '\0', PIPE_NAME_SIZE);

    char pipe_name[PIPE_NAME_SIZE];
    memset(pipe_name, '\0', PIPE_NAME_SIZE);

    char box_name[BOX_NAME_SIZE];
    memset(box_name, '\0', BOX_NAME_SIZE);

    // Argument parsing of a manager process launch.
    strcpy(register_pipe_name, argv[1]);
    strcpy(pipe_name, argv[2]);

    pipe_box_code_t *args = (pipe_box_code_t*)malloc(sizeof(pipe_box_code_t));
    strcpy(args->name.pipe, pipe_name);

    int check_err = mkfifo(pipe_name, 0640);
    ALWAYS_ASSERT(check_err == 0, "Pipe could not be created.");

    //Check if pipe_name is a valid path name. TODO

    //Checks if the user wants the manager to list the boxes, or to create/remove a box. 
    if (!strcmp(argv[3], "create")) {
        strcpy(box_name, argv[4]);
        strcpy(args->name.box, box_name);
        args->code = 3;

    } else if (!strcmp(argv[3], "remove")) {
        strcpy(box_name, argv[4]);
        strcpy(args->name.box, box_name);
        args->code = 5;

    } else if (!strcmp(argv[3], "list")) {
        args->code = 7;
        strcpy(args->name.box, box_name);

    } else {
        fprintf(stderr, "Please insert a correct input command.\n");
        exit(EXIT_SUCCESS);
    }

    int register_fd = open(register_pipe_name, O_WRONLY);
    ALWAYS_ASSERT(register_fd != -1, "Could not open the register pipe.");

    ssize_t bytes = write(register_fd, args, sizeof(pipe_box_code_t));
    ALWAYS_ASSERT(bytes > 0, "Could not write to the register pipe.");

    close(register_fd);

    int pipe_fd = open(pipe_name, O_RDONLY);
    ALWAYS_ASSERT(pipe_fd != -1, "Manager could not open %s.", pipe_name);

    if (args->code == 3 || args->code == 5) {

        req_reply_t *reply = (req_reply_t*)malloc(sizeof(req_reply_t));
        bytes = read(pipe_fd, reply, sizeof(req_reply_t));
        ALWAYS_ASSERT(bytes == sizeof(req_reply_t), "Manager failed to read from %s.", pipe_name);

        if (reply->ret == 0) {
            fprintf(stdout, "OK\n");
        } else {
            fprintf(stdout, "ERROR %s\n", reply->err_message);
        }

    } else {

        box_listing_t *reply = (box_listing_t*)malloc(sizeof(box_listing_t));
        bytes = read(pipe_fd, reply, sizeof(box_listing_t));
        //printf("bytes read = %zd, sizeof is %zd\n", bytes, sizeof(box_listing_t));
        //ALWAYS_ASSERT(bytes == sizeof(box_listing_t), "Manager failed to read from %s.", pipe_name);

        int box_number = reply->box_amount;

        if (box_number == 0) {
            fprintf(stdout, "NO BOXES FOUND\n");
        } else {
            box_t *temp_box = (box_t*)malloc(sizeof(box_t));

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

            free(temp_box);

            for (int i = 0; i < box_number; i++) {
                fprintf(stdout, "%s %zu %zu %zu\n", reply->boxes[i].box_name, 
                                            reply->boxes[i].box_size, reply->boxes[i].n_publishers, 
                                                                    reply->boxes[i].n_subscribers);
            }
    }
    }

    close(pipe_fd);

    ALWAYS_ASSERT((unlink(pipe_name) == 0), "Could not delete %s.", pipe_name);

    return -1;
}