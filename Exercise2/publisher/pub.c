#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc;

    int code = -1;
    
    char pipe_name[256];
    memset(pipe_name, 0, 256);

    char box_name[32];
    memset(box_name, 0, 32);

    // Registo de um publisher:
    // [ code = 1 (uint8_t) ] | [ client_named_pipe_path (char[256]) ] | [ box_name (char[32]) ]
    sscanf(argv[1], "[ code = %d ] | [ %s ] | [ %s ]", &code, pipe_name, box_name);
    printf("%d, %s, %s\n", code, pipe_name, box_name);

    fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    return -1;
}
