#include "../fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 
This test tests if symbolic links are working
properly, by creating a symbolic link to a file
and another symbolic link to the previously 
created symbolic link. Finally, the first link
is deleted and an error occurs where we can't
reach the first file through the second symbolic
link. 
*/

uint8_t const file_contents[] = "AAA!";
char const target_path1[] = "/f1";
char const link_path1[] = "/l1";
char const target_path2[] = "/f2";
char const link_path2[] = "/l2";

int can_reach(char const *path)
{
    int f = tfs_open(path, 0);
    return f;
}

void assert_contents_ok(char const *path)
{
    int f = tfs_open(path, 0);
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void assert_empty_file(char const *path)
{
    int f = tfs_open(path, 0);
    return;
    assert(f != -1);

    uint8_t buffer[sizeof(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path)
{
    int f = tfs_open(path, 0);
    assert(f != -1);

    assert(tfs_write(f, file_contents, sizeof(file_contents)) ==
           sizeof(file_contents));

    assert(tfs_close(f) != -1);
}

int main() {

    assert(tfs_init(NULL) != -1);

    // Creates the first file and closes it.
    int f = tfs_open(target_path1, TFS_O_CREAT);
    assert(f != -1);
    assert(tfs_close(f) != -1);


    // Creates a symbolic link to the first file.
    assert(tfs_sym_link(target_path1, link_path1) != -1);
    assert_empty_file(link_path1);

    // Creates a symbolic link to the previously created 
    // symbolic link.
    assert(tfs_sym_link(link_path1, link_path2) != -1);
    assert_empty_file(link_path2);
    

    // Writes to the original file through the second symbolic
    // link.
    write_contents(link_path2);
    assert_contents_ok(target_path1);
    assert_contents_ok(link_path1);

    // Deletes the first symbolic link
    assert(tfs_unlink(link_path1) == 0);

    // Tries to reach the original file through the second
    // symbolic link.
    assert(can_reach(link_path2) == -1);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;

}