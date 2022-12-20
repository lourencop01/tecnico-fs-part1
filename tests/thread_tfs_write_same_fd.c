#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREADS 20
#define BUFFER 100

/*
This test tests if the tfs_write function is
thread-safe. It tries to use differente threads
all writing to the same file through the same 
file descriptor. If it weren't thread-safe, some
character may get overriden. Hence, the last
assertion must be verified.
*/

char *str_ext_file = "1234";

void* write(void* fhandle) {
    int f = *(int*)fhandle;
    
    assert(tfs_write(f, str_ext_file, strlen(str_ext_file)) != -1);
    
    return NULL;
}

int main() {

    char *path = "/f1";
    char buffer[BUFFER];
    memset(buffer, 0, BUFFER);
    pthread_t tid[THREADS];

    assert(tfs_init(NULL) != -1);

    int f;

    f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    for (int i = 0; i < THREADS; i++) {
        pthread_create(&tid[i], NULL, write, (void*)&f);
    }

    for (int i = 0; i < THREADS; i++){
        pthread_join(tid[i], NULL);
    }

    assert(tfs_close(f) == 0);

    int fd = tfs_open(path, 0);
    assert(fd != -1);

    assert(tfs_read(fd, buffer, BUFFER) != -1);
    
    assert(tfs_close(fd) == 0);

    assert(strlen(buffer) == (THREADS * strlen(str_ext_file)));

    printf("Successful test.\n");

    return 0;
}