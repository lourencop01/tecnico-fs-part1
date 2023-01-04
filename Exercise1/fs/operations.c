#include "operations.h"
#include "config.h"
#include "state.h"
#include "betterassert.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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
    }
    else {
        params = tfs_default_params();
    }

    if (state_init(params) != 0) {
        return -1;
    }
    
    // Create root inode.
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
    return name != NULL && strlen(name) > 1 && name[0] == '/'
                && strlen(name) <= (MAX_FILE_NAME + 1);
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

    ALWAYS_ASSERT(root_inode->i_data_block == 0, 
                "The inode_t argument does not correspond to the root inode.");

    if (!valid_pathname(name)) {
        return -1;
    }

    // Skip the initial '/' character.
    name++;
    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {

    // Checks if the path name is valid.
    if (!valid_pathname(name)) {
        return -1;
    }

    inode_t * root = root_inode(false);

    int inum = tfs_lookup(name, root);
    size_t offset;

    if (inum >= 0) {

        // The file already exists.
        inode_t *inode = inode_get(inum, false);
        ALWAYS_ASSERT(inode != NULL, "tfs_open: directory files must have an inode");

        // Checks if the inode belongs to a symbolic link and recursively
        // looks for the final target.
        if (inode->i_node_type == T_SYMLINK) {

            ALWAYS_ASSERT(pthread_rwlock_unlock(&inode->inode_lock) == 0, 
                        "Could not unlock the inode.");

            if (tfs_lookup(inode->sym_path, root) == -1) {
                fprintf(stderr, "The file linked to this symbolic link has been deleted!.\n");
                ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                            "Could not unlock the root inode.");
                return -1;
            }

            ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                        "Could not unlock the root inode.");
            return tfs_open(inode->sym_path, mode);
        }

        // Truncate (if requested).
        if (mode & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inode->i_size = 0;
            }
        }

        // Determine initial offset.
        if (mode & TFS_O_APPEND) {
            offset = inode->i_size;
        }
        else {
            offset = 0;
        }
        
        ALWAYS_ASSERT(pthread_rwlock_unlock(&inode->inode_lock) == 0, "Could not unlock the inode.");
    }
    else if (mode & TFS_O_CREAT) {

        // The file does not exist; the mode specified that it should be created.
        // Create inode.
        inum = inode_create(T_FILE);
        if (inum == -1)
        {
            ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                        "Could not unlock the root inode.");
            return -1; // No space in inode table.
        }

        // Add entry in the root directory
        if (add_dir_entry(root, name + 1, inum) == -1)
        {
            inode_delete(inum);
            ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                        "Could not unlock the root inode.");
            return -1; // No space in directory.
        }
        offset = 0;
    }
    else {
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                "Could not unlock the root inode.");

    // Finally, add entry to the open file table and return the corresponding
    // handle.
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    
    inode_t * root = root_inode(false);
    
    // Checks if this link already exists.
    if (tfs_lookup(link_name, root) == 0) {
        fprintf(stderr, "This file already exists. Please try a different name.\n");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Checks is the link name is valid.
    if (!valid_pathname(link_name)) {
        fprintf(stderr, "The link name you entered in invalid. "
                    "Please try using the following format: /...\n");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }
    // Checks if the target file is in the root directory.
    if (tfs_lookup(target, root) == -1) {

        fprintf(stderr, "The target was not found in the root directory. "
                    "Please make sure you entered the correct path name.\n");

        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Creates an inode of type SYMLINK for the symbolic link. Also checks
    // if did it successfully.
    int link_inumber = inode_create(T_SYMLINK);
    if (link_inumber == -1) {
        fprintf(stderr, "There are no more free slots in the inode table.\n");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Fetches the link inode and checks if it has been done successfully.
    inode_t *link_inode = inode_get(link_inumber, false);
    ALWAYS_ASSERT(link_inode != NULL, "Couldn't fetch link's inode.");

    // Copy the target path to the sym_path variable in the inode.
    link_inode->sym_path = (char *)malloc(strlen(target));
    strcpy(link_inode->sym_path, target);
    ALWAYS_ASSERT(pthread_rwlock_unlock(&link_inode->inode_lock) == 0, 
                "Could not unlock the link inode.");

    // Remove the '/' from the link name
    link_name++;
    // Add the symbolic link to the root directory while checking it any
    // errors occured.
    if (add_dir_entry(root, link_name, link_inumber) == -1) {
        fprintf(stderr, "There was a problem adding %s to the root directory.\n", link_name);
    }

    ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                "Could not unlock the root inode.");

    return 0;
}

int tfs_link(char const *target, char const *link_name) {

    inode_t * root = root_inode(false);

    // Checks if this link already exists.
    if (tfs_lookup(link_name, root) == 0) {
        fprintf(stderr, "This file already exists. Please try a different name.\n");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }
    
    // Checks is the link name is valid.
    if (!valid_pathname(link_name)) {
        fprintf(stderr, "The link name you entered in invalid. "
                    "Please try using the following format: /...\n");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Retrieves the number of the inode (inumber) of the target file.
    // Also checks if any errors occured while looking for the inumber.
    int target_inumber = tfs_lookup(target, root);
    if (target_inumber == -1) {
        fprintf(stderr, "The target file %s couldn't be found in the TécnicoFS. "
                    "Please check if you inserted the correct path.\n", target);
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Retrieves the target inode and checks if it could be found.
    inode_t *target_inode = inode_get(target_inumber, false);
    ALWAYS_ASSERT(target_inode != NULL, "Target inode was not found.");

    // Checks if the target inode is a symbolic link.
    if (target_inode->i_node_type == T_SYMLINK) {
        fprintf(stderr, "Unable to proceed. Reason: target file is a soft link.\n");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                    "Could not unlock the target inode.");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Removes the "/" from the link_name.
    link_name++;

    // Adds an entry to the root directory with the altered link_name and sets
    // its inumber (d_inumber) to the target's inumber.
    // Also checks if any problems occured.
    if (add_dir_entry(root, link_name, target_inumber) == -1) {
        fprintf(stderr, "There was a problem adding %s to the root directory.\n", link_name);

        ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                    "Could not unlock the target inode.");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Increases the target file's hard link count by 1.
    target_inode->hard_link_counter++;
    ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                "Could not unlock the target inode.");

    ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                "Could not unlock the root inode.");

    return 0;
}

int tfs_unlink(char const *target) {

    inode_t * root = root_inode(false);
    
    // Retrieves the number of the inode (inumber) of the target file.
    // Also checks if any errors occured while looking for the inumber.
    int target_inumber = tfs_lookup(target, root);
    if (target_inumber == -1) {
        fprintf(stderr, "The target file %s couldn't be found in the TécnicoFS. "
                    "Please check if you inserted the correct path.\n", target);
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Retrieves the target inode and checks if it could be found.
    inode_t *target_inode = inode_get(target_inumber, false);
    ALWAYS_ASSERT(target_inode != NULL, "Target inode was not found.\n");

    // Option where the file is completely removed and won't be accesible
    // anymore.
    if (target_inode->hard_link_counter == 1 &&
                target_inode->i_node_type != T_SYMLINK) {
         if (is_file_open(target_inumber)) {
            fprintf(stderr, "The file you are trying to delete is currently open. "
                        "Please close it and try again.\n");
            ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0,
                        "Could not unlock the target inode.");
            ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                        "Could not unlock the root inode.");
            return -1;
        }
        ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                    "Could not unlock the target inode.");
        inode_delete(target_inumber);

    // Checks if the target inode is a symbolic link.
    } else if (target_inode->i_node_type == T_SYMLINK) {
        ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                    "Could not unlock the target inode.");
        inode_delete(target_inumber);

    // Else, if the target inode still has multiple hard links, decreases its
    // count by 1.
    } else if (target_inode->hard_link_counter > 1) {
        target_inode->hard_link_counter--;
        ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                    "Could not unlock the target inode.");
    } else {
        ALWAYS_ASSERT(pthread_rwlock_unlock(&target_inode->inode_lock) == 0, 
                    "Could not unlock the target inode.");
        ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                    "Could not unlock the root inode.");
        return -1;
    }

    // Removes the "/" from the target name.
    target++;

    // Removes the target entry from the directory's entries while
    // assessing if it has been done correctly.
    ALWAYS_ASSERT(clear_dir_entry(root, target) == 0, 
                "Could not remove the link file from the directory.");
    
    ALWAYS_ASSERT(pthread_rwlock_unlock(&root->inode_lock) == 0, 
                "Could not unlock the root inode.");

    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        ALWAYS_ASSERT(pthread_mutex_unlock(&file->open_file_lock) == 0, 
                    "Could not unlock the file's lock.");
        return -1; // Invalid fd.
    }

    remove_from_open_file_table(fhandle);
    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        ALWAYS_ASSERT(pthread_mutex_unlock(&file->open_file_lock) == 0, 
                    "Could not unlock the file's lock.");
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get(file->of_inumber, false);
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
                ALWAYS_ASSERT(pthread_rwlock_unlock(&inode->inode_lock) == 0, 
                            "Could not unlock the inode lock.");
                ALWAYS_ASSERT(pthread_mutex_unlock(&file->open_file_lock) == 0, 
                            "Could not unlock the file's lock.");
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

    ALWAYS_ASSERT(pthread_rwlock_unlock(&inode->inode_lock) == 0, 
                "Could not unlock the inode lock.");
    ALWAYS_ASSERT(pthread_mutex_unlock(&file->open_file_lock) == 0, 
                "Could not unlock the file's lock.");

    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        ALWAYS_ASSERT(pthread_mutex_unlock(&file->open_file_lock) == 0, 
                    "Could not unlock the file's lock.");
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get(file->of_inumber, true);
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

    ALWAYS_ASSERT(pthread_rwlock_unlock((pthread_rwlock_t *)&inode->inode_lock) == 0, 
                "Could not unlock the file's lock.");
    ALWAYS_ASSERT(pthread_mutex_unlock(&file->open_file_lock) == 0, 
                "Could not unlock the file's lock.");

    return (ssize_t)to_read;
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
        fprintf(stderr, "Destination file creation error.\n");
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
        if (bytes_read != bytes_wrote) {
            fprintf(stderr, "There was a problem writing to the destination file.\n");
            // Closes source and destination files while ensuring that it has been
            // done successfully.
            ALWAYS_ASSERT(fclose(source_fp) == 0, "There was a problem closing the source file.");
            ALWAYS_ASSERT(tfs_close(dest_fp) == 0, "There was a problem closing the destination file.");
            
            return -1;
        }
        bytes_read = fread(buffer, 1, SIZE_OF_BUFFER, source_fp);
    }

    // Closes source and destination files while ensuring that it has been
    // done successfully.
    ALWAYS_ASSERT(fclose(source_fp) == 0, "There was a problem closing the source file.");
    ALWAYS_ASSERT(tfs_close(dest_fp) == 0, "There was a problem closing the destination file."); 

    return 0;
}
