#ifndef __STRUCTS_H__
#define __STRUCTS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

typedef struct {
    char pipe_name[256];
    char box_name[32];
    char message[1024];
} message_t;

typedef struct {
    char pipe_name[256];
    char box_name[32];
    uint8_t code;
} register_client_t;

#endif // __STRUCTS_H__