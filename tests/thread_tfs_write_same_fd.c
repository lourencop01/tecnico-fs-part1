/* #include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define THREADS 15

char final[100];
int i = 0;

void* write(void* fhandle) {

    char *str_ext_file = "123456789abcdefghijklmnopqrstuvwxyz";
    int f = *(int*)fhandle;

    char buffer[1];
    ssize_t r;

    while ((r = tfs_write(f, str_ext_file[i++], sizeof(buffer))) > 0) {
    }
    
    return NULL;
}

int main() {

    char *path = "/f1";
    char *path_src = "tests/tfs_read.txt";
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
 
    tfs_close(f);

    assert(strlen(final)==strlen(str_ext_file));
    printf("Successful test.\n");
    return 0;

} */