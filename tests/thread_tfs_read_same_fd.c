#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREADS 15

void* read(void* fhandle) {

    int f = *(int*)fhandle;

    char buffer[2];
    ssize_t r; 

    while ((r = tfs_read(f, buffer, sizeof(buffer)-1)) > 0) {
        printf("%s", buffer);
    }
    
    return NULL;
}

int main() {

    //char *str_ext_file = "123456789abcdefghijklmnopqrstuvwxyz";
    char *path_copied_file = "/f1";
    char *path_src = "tests/tfs_read.txt";
    pthread_t tid[THREADS];

    assert(tfs_init(NULL) != -1);

    int f;

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    //read((void*)&f);
    for (int i = 0; i < THREADS; i++) {
        pthread_create(&tid[i], NULL, read, (void*)&f);
    }

    for (int i = 0; i < THREADS; i++){
        pthread_join(tid[i], NULL);
    }
 
    tfs_close(f);
    return 0;

}