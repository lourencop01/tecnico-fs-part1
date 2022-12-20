#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

/*
This test creates 10 files at once to see if no inodes are
overriden, making sure that the tfs_open function is 
thread-safe.
*/

void* create(void* num) {
    int number = *(int*)num;
    char path[10] = "/";

    sprintf(path + 1, "%d", number);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);

    assert(tfs_close(fd) == 0);

    return NULL;
}

int main() {
    int i_table[10] = {1,2,3,4,5,6,7,8,9,0};
    pthread_t tid[10];
    char path[10] = "/";
    int f;
    assert(tfs_init(NULL) != -1);

    for (int i = 0; i < 10; i++) {
        pthread_create(&tid[i], NULL, create, &i_table[i]);
    }

    for (int i = 0; i < 10; i++) {
        pthread_join(tid[i], NULL);
    }

    for(int i = 0; i<10; i++) {
        sprintf(path + 1, "%d", i_table[i]);
        f = tfs_open(path, 0);
        assert(f != -1);
        assert(tfs_close(f) == 0);
    }

    tfs_destroy();

    printf("Successful test.\n");

    return 0;
}