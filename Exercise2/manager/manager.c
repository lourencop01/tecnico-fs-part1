#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
} */

int main(int argc, char **argv) {
    (void)argc;

    char register_pipe_name[256];
    memset(register_pipe_name, '\0', 256);

    char pipe_name[256];
    memset(pipe_name, '\0', 256);

    char box_name[32];
    memset(box_name, '\0', 32);

    // Argument parsing of a manager process launch.
    strcpy(register_pipe_name, argv[1]);
    strcpy(pipe_name, argv[2]);

    //Check if register pipe exists. TODO

    //Check if pipe_name is a valid path name. TODO

    //Checks if the user wants the manager to list the boxes, or to create/remove a box. 
    if (!strcmp(argv[2], "create") || !strcmp(argv[2], "remove")) {
        strcpy(box_name, argv[3]);
        //Check if box_name already exists. TODO
    }
    else {
        //Lists all boxes. TODO
        printf("Should list all boxes\n");
    }

    return -1;
}
