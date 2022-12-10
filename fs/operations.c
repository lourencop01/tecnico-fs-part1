#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}

int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
    if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    // TODO: assert that root_inode is the root directory
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");

        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }

        offset = 0;
    } else {
        return -1;
    }

    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    (void)target;
    (void)link_name;
    return 0;
}

int tfs_link(char const *target, char const *link_name) {

    // Checks if the link_name is valid
    if (!valid_pathname(link_name)) {
        fprintf(stderr, "The provided link name is invalid. "
                    "Please use the following format: /...\n");
        return -1;
    }

    // TODO: Checks if the target file is a soft link 

    // Assigns the root inode to a pointer(*root_dir_inode) and checks if it
    // exists.
    inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL, "Root inode does not exist.");

    // Retrieves the number of the inode (inumber) of the target file.
    // Also checks if any errors occured while looking for the inumber.
    int target_inumber = tfs_lookup(target, root_dir_inode);
    if(target_inumber == -1) {
        fprintf(stderr, 
                    "The target file %s couldn't be found in the TécnicoFS. "
                    "Please check if you inserted the correct path.\n", target);
        return -1;
    }

    // Removes the "/" from the link_name.
    link_name++;

    // Checks if this link already exists.
    if (find_in_dir(root_dir_inode, link_name) == 0) {
        fprintf(stderr, "This link already exists. Please try a different name"
                    ".\n");
    }

    // Adds an entry to the root directory with the altered link_name and sets
    // its inumber (d_inumber) to the target's inumber.
    // Also checks if any problems occured.
    int link_entry = add_dir_entry(root_dir_inode, link_name, target_inumber);
    if (link_entry == -1){
        fprintf(stderr, "There was a problem adding %s to the root directory."
                    "\n", link_name);
    }

    // Assigns the target's inode to a pointer and increases the target file's
    // hard link count by 1.
    // Also checks if it could find the target file inode
    inode_t *target_inode = inode_get(target_inumber);
    ALWAYS_ASSERT(root_dir_inode != NULL, "Target inode does not exist.\n");
    target_inode->hard_link_counter++;

    return 0;

} 

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }

    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }

    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    (void)target;
    // ^ this is a trick to keep the compiler from complaining about unused
    // variables. TODO: remove

    PANIC("TODO: tfs_unlink");
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    
    // Creates a buffer to store the copied data from the source file,
    // allocates memory space for it and sets every entry to 0.
    char buffer[SIZE_OF_BUFFER];
    memset(buffer, 0, SIZE_OF_BUFFER);

    // Creates a file handler for the source file and opens it in read mode.
    FILE *source_fp = fopen(source_path, "r");
    if (source_fp == NULL) {
        fprintf(stderr, "Source file open error: %s\n", strerror(errno));
        return -1;
    }

    // Creates or truncates the destination file while setting dest_fp as its
    // file descriptor.
    int dest_fp = tfs_open(dest_path, TFS_O_CREAT | TFS_O_TRUNC);
    if (dest_fp == -1) {
        fprintf(stderr, "Destination file creation error: %s\n", 
                    strerror(errno));
        return -1;
    }

    // Reads the first line of the source file and stores it in the buffer.
    // The number of bytes read is stored in bytes_read.
    size_t bytes_read = fread(buffer, 1, SIZE_OF_BUFFER, source_fp);
    
    // Creates a variable to store the number of bytes written and writes to
    // the destination file.
    ssize_t bytes_wrote = 0;

    // While loop for copying the source file to the destination file.
    // Aborts if the number of bytes copied is different from the number of
    // bytes written.
    while (bytes_read > 0) {
        bytes_wrote = tfs_write(dest_fp, buffer, bytes_read);
        if (bytes_read > bytes_wrote) {
            fprintf(stderr, "The source file's size exceeds the limit. " 
                        "Only %d bytes were copied.\n", MAX_BLOCK_SIZE);
            break;
        }
        ALWAYS_ASSERT(bytes_wrote == bytes_read, 
                        "There was a problem writing to the destination file.");
        bytes_read = fread(buffer, 1, SIZE_OF_BUFFER, source_fp);
    }
    
    // Closes source and destination files while ensuring that it has been
    // done successfully.
    ALWAYS_ASSERT(fclose(source_fp) == 0, 
                "There was a problem closing the source file.");
    ALWAYS_ASSERT(tfs_close(dest_fp) == 0, 
                "There was a problem closing the destination file.");

    return 0;
}

