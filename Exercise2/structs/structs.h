#ifndef __STRUCTS_H__
#define __STRUCTS_H__

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024
#define MAX_BOX_NUMBER 63

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

typedef struct {
    uint8_t last;
    char box_name[BOX_NAME_SIZE];
    uint64_t box_size;
    uint64_t n_publishers;
    uint64_t n_subscribers;

    char tfs_file[MAX_FILE_NAME];
} box_t;

typedef struct {
    bool taken;
    box_t box;
    pthread_mutex_t box_mutex;
    pthread_mutex_t pub_box_mutex;
    pthread_mutex_t sub_box_mutex;
    pthread_cond_t box_cond;
} box_status_t;

typedef struct {
    uint8_t code;
    box_t boxes[MAX_BOX_NUMBER];
    int box_amount;
} box_listing_t;


#endif // __STRUCTS_H__