#ifndef __STRUCTS_H__
#define __STRUCTS_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

typedef struct {
    char pipe[PIPE_NAME_SIZE];
    char box[BOX_NAME_SIZE];
} pipe_box_t;

typedef struct {
    pipe_box_t name;
    char message[MESSAGE_SIZE];
} message_t;

typedef struct {
    pipe_box_t name;
    uint8_t code;
} pipe_box_code_t;

typedef struct {
    uint8_t code;
    int32_t ret;
    char err_message[MESSAGE_SIZE];
} req_reply_t;


#endif // __STRUCTS_H__