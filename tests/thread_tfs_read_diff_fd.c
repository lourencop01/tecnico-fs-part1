#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREADS 16 //Max open files is 16.

void* read(void* fhandle) {

    char *str_ext_file = "123456789abcdefghijklmnopqrstuvwxyz";

    int f = *(int*)fhandle;

    char buffer[80];
    ssize_t r; 

    r = tfs_read(f, buffer, sizeof(buffer)-1);
    assert(r != -1);

    assert(memcmp(buffer, str_ext_file, strlen(str_ext_file)) == 0);
    
    return NULL;
}

int main() {

    char *path_copied_file = "/f1";
    char *path_src = "tests/tfs_read.txt";
    pthread_t tid[THREADS];

    assert(tfs_init(NULL) != -1);

    int f;

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    int fd[THREADS];

    for (int i = 0; i < THREADS; i++) {
        fd[i] = tfs_open(path_copied_file, TFS_O_CREAT);
        assert(fd[i] != -1);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_create(&tid[i], NULL, read, (void*)&fd[i]);
    }

    for (int i = 0; i < THREADS; i++){
        pthread_join(tid[i], NULL);
    }
 
    tfs_close(f);

    printf("Successful test.\n");
    return 0;

}