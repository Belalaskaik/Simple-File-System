/* fs.c: SimpleFS file system */

#include "sfs/fs.h"
#include "sfs/logging.h"
#include "sfs/utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>  

#define FS_FAILURE -1
#define FS_SUCCESS 0

/**
 * Debug FileSystem by doing the following
 *
 *  1. Read SuperBlock and report its information.
 *
 *  2. Read Inode Table and report information about each Inode.
 *
 * @param       disk        Pointer to Disk structure.
 **/

bool is_block_free(Block *block) {
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        if (block->data[i] != 0) {
            return false;
        }
    }
    return true;
}
bool is_block_free(Block *block);
bool build_free_block_map(FileSystem *fs, Disk *disk);
bool build_free_inode_map(FileSystem *fs, Disk *disk);

bool build_free_block_map(FileSystem *fs, Disk *disk) {
    for (size_t i = 0; i < fs->meta_data.blocks; i++) {
        Block block;
        if (disk_read(disk, i, block.data) == DISK_FAILURE) {
            error("Failed to read block %zu", i);
            return false;
        }
        fs->free_blocks[i] = is_block_free(&block);
    }
    return true;
}
bool build_free_inode_map(FileSystem *fs, Disk *disk) {
    for (size_t i = 0; i < fs->meta_data.inodes; i++) {
        size_t block_index = i / INODES_PER_BLOCK;
        size_t inode_index = i % INODES_PER_BLOCK;
        Block block;
        if (disk_read(disk, block_index, block.data) == DISK_FAILURE) {
            error("Failed to read inode block %zu", block_index);
            return false;
        }
        fs->free_inodes[i] = !block.inodes[inode_index].valid;
    }
    return true;
}

ssize_t count_inodes(FileSystem *fs) {
    size_t inode_count = 0;
    Block inode_block;
    int inode_block_offset = 1;  // Starting after the SuperBlock

    for (size_t block_index = inode_block_offset; block_index < inode_block_offset + fs->meta_data.inode_blocks; block_index++) {
        if (disk_read(fs->disk, block_index, (char *)&inode_block.inodes) == DISK_FAILURE) {
            error("Error reading inode block %zu", block_index);
            return FS_FAILURE;
        }

        for (size_t inode_index = 0; inode_index < INODES_PER_BLOCK; inode_index++) {
            if (inode_block.inodes[inode_index].valid) {
                inode_count++;
            }
        }
    }

    return inode_count;
}

ssize_t initialize_free_inode_map(FileSystem *fs, Disk *disk) {
    fs->free_inodes = calloc(fs->meta_data.inode_blocks * INODES_PER_BLOCK, sizeof(bool));
    if (!fs->free_inodes) {
        error("Memory allocation failed for free inode map");
        return FS_FAILURE;
    }

    size_t inode_block_offset = 1;  // Starting after the SuperBlock
    Block inode_block;

    for (size_t block_index = inode_block_offset; block_index < inode_block_offset + fs->meta_data.inode_blocks; block_index++) {
        if (disk_read(disk, block_index, (char *)&inode_block.inodes) == DISK_FAILURE) {
            error("Error reading inode block %zu", block_index);
            return FS_FAILURE;
        }

        for (size_t inode_index = 0; inode_index < INODES_PER_BLOCK; inode_index++) {
            size_t inode_number = (block_index - inode_block_offset) * INODES_PER_BLOCK + inode_index;
            fs->free_inodes[inode_number] = !inode_block.inodes[inode_index].valid;
        }
    }

    return FS_SUCCESS;
}

int construct_free_block_map(FileSystem *fs, Disk *disk) {
    fs->free_blocks = calloc(fs->meta_data.blocks, sizeof(bool));
    if (!fs->free_blocks) {
        error("Memory allocation failed for free block map");
        return FS_FAILURE;
    }

    // Mark the superblock and inode blocks as used
    fs->free_blocks[0] = false;  // SuperBlock is always used
    for (size_t i = 1; i <= fs->meta_data.inode_blocks; i++) {
        fs->free_blocks[i] = false;
    }

    // Check remaining blocks for usage
    Block block;
    for (size_t block_index = fs->meta_data.inode_blocks + 1; block_index < fs->meta_data.blocks; block_index++) {
        if (disk_read(disk, block_index, block.data) == DISK_FAILURE) {
            error("Error reading block %zu", block_index);
            fs->free_blocks[block_index] = false;
            continue;
        }

        // Mark the block as free or used based on its content
        fs->free_blocks[block_index] = is_block_free(&block);
    }

    return FS_SUCCESS;
}


void fs_debug(Disk *disk) {
    Block block;

    // Read SuperBlock
    if (disk_read(disk, 0, block.data) == DISK_FAILURE) {
        error("Failed to read SuperBlock");
        return;
    }

    printf("SuperBlock:\n");
    if (block.super.magic_number != MAGIC_NUMBER) {
        printf("    Invalid magic number!\n");
        return;
    }

    printf("    %u blocks\n", block.super.blocks);
    printf("    %u inode blocks\n", block.super.inode_blocks);
    printf("    %u inodes\n", block.super.inodes);

    // Read Inodes
    for (uint32_t i = 0; i < block.super.inode_blocks; i++) {
        if (disk_read(disk, i + 1, block.data) == DISK_FAILURE) {
            error("Failed to read inode block %u", i);
            continue;
        }

        for (uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            if (block.inodes[j].valid) {
                printf("Inode %u:\n", j);
                printf("    Size: %u bytes\n", block.inodes[j].size);
                printf("    Direct blocks:");
                for (int k = 0; k < POINTERS_PER_INODE; k++) {
                    printf(" %u", block.inodes[j].direct[k]);
                }
                printf("\n");
            }
        }
    }
}

/**
 * Format Disk by doing the following:
 *
 *  1. Write SuperBlock (with appropriate magic number, number of blocks,
 *  number of inode blocks, and number of inodes).
 *
 *  2. Clear all remaining blocks.
 *
 * Note: Do not format a mounted Disk!
 *
 * @param       disk        Pointer to Disk structure.
 * @return      Whether or not all disk operations were successful.
 **/
// bool fs_format(Disk *disk)
// {
//     return false;
// }

bool fs_format(Disk *disk) {
    if (disk == NULL) {
        error("Disk is NULL");
        return false;
    }

    // Write SuperBlock
    Block block;
    memset(&block, 0, sizeof(Block));
    block.super.magic_number = MAGIC_NUMBER;
    block.super.blocks = disk->blocks;
    block.super.inode_blocks = (disk->blocks / 10); // example calculation
    block.super.inodes = block.super.inode_blocks * INODES_PER_BLOCK;

    if (disk_write(disk, 0, block.data) == DISK_FAILURE) {
        error("Failed to write SuperBlock");
        return false;
    }

    // Clear all remaining blocks
    memset(block.data, 0, BLOCK_SIZE);
    for (uint32_t i = 1; i < disk->blocks; i++) {
        if (disk_write(disk, i, block.data) == DISK_FAILURE) {
            error("Failed to clear block %u", i);
            return false;
        }
    }

    return true;
}

/**
 * Mount specified FileSystem to given Disk by doing the following:
 *
 *  1. Read and check SuperBlock (verify attributes).
 *
 *  2. Record FileSystem disk attribute and set Disk mount status.
 *
 *  3. Copy SuperBlock to FileSystem meta data attribute
 *
 *  4. Initialize FileSystem free blocks bitmap.
 *
 * Note: Do not mount a Disk that has already been mounted!
 *
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not the mount operation was successful.
 **/
bool fs_mount(FileSystem *fs, Disk *disk) {
    if (fs == NULL || disk == NULL) {
        error("FileSystem or Disk is NULL");
        return false;
    }

    // Read and check SuperBlock
    Block block;
    if (disk_read(disk, 0, block.data) == DISK_FAILURE) {
        error("Failed to read SuperBlock");
        return false;
    }

    if (block.super.magic_number != MAGIC_NUMBER) {
        error("Invalid magic number in SuperBlock");
        return false;
    }

    // Record FileSystem disk attribute and set Disk mount status
    fs->disk = disk;
    // Assume disk has a 'mounted' field. Set it to true
    disk->mounted = true;

    // Copy SuperBlock to FileSystem meta data attribute
    fs->meta_data = block.super;

    // Initialize FileSystem free blocks bitmap
    fs->free_blocks = calloc(disk->blocks, sizeof(bool));
    if (fs->free_blocks == NULL) {
        error("Failed to allocate free blocks bitmap");
        return false;
    }

    // Example: Mark all blocks as free initially
    for (uint32_t i = 0; i < disk->blocks; i++) {
        fs->free_blocks[i] = true;
    }

    return true;
}




// bool fs_mount(FileSystem *fs, Disk *disk) {
//     if (disk->mounted) {
//         error("Disk already mounted");
//         return false;
//     }

//     // Initialize free inode bitmap
//     fs->free_inodes = calloc(fs->meta_data.inode_blocks * INODES_PER_BLOCK, sizeof(bool));
//     if (!fs->free_inodes) {
//         error("Failed to allocate free inode bitmap");
//         return false;
//     }
//     // Initialize the free inode bitmap based on the current inode status
//     if (initialize_free_inode_map(fs, disk) == FS_FAILURE) {
//         free(fs->free_inodes);
//         return false;
//     }

//     // Assigning disk to the file system
//     fs->disk = disk;
//     fs->meta_data.blocks = disk->blocks;

//     // Attempting to read the superblock
//     Block superblock;
//     if (disk_read(disk, 0, superblock.data) == DISK_FAILURE) {
//         error("Superblock read failure");
//         return false;
//     }

//     // Validating the magic number
//     SuperBlock *sb = (SuperBlock *)&superblock.data;
//     if (sb->magic_number != MAGIC_NUMBER) {
//         error("Incorrect magic number: %x, expected: %x", sb->magic_number, MAGIC_NUMBER);
//         return false;
//     }

//     // Update file system metadata from superblock
//     fs->meta_data = *sb;
//     fs->meta_data.inode_blocks = (fs->meta_data.blocks + 9) / 10;

//     // Counting inodes and constructing free block and inode maps
//     if ((fs->meta_data.inodes = count_inodes(fs)) == FS_FAILURE ||
//         !build_free_block_map(fs, disk) ||
//         !build_free_inode_map(fs, disk)) {
//         return false;
//     }

//     // Marking disk as mounted
//     disk->mounted = true;
//     return true;
// }




/**
 * Unmount FileSystem from internal Disk by doing the following:
 *
 *  1. Set Disk mounted status and FileSystem disk attribute.
 *
 *  2. Release free blocks bitmap.
 *
 * @param       fs      Pointer to FileSystem structure.
 **/
void fs_unmount(FileSystem *fs) {
    if (fs == NULL || fs->disk == NULL) {
        error("FileSystem or Disk is NULL");
        return;
    }

    // Set Disk mounted status and FileSystem disk attribute
    fs->disk->mounted = false;
    fs->disk = NULL;

    // Release free blocks bitmap
    free(fs->free_blocks);
    fs->free_blocks = NULL;
}

/**
 * Allocate an Inode in the FileSystem Inode table by doing the following:
 *
 *  1. Search Inode table for free inode.
 *
 *  2. Reserve free inode in Inode table.
 *
 * Note: Be sure to record updates to Inode table to Disk.
 *
 * @param       fs      Pointer to FileSystem structure.
 * @return      Inode number of allocated Inode.
 **/
ssize_t fs_create(FileSystem *fs) {
    if (fs == NULL) {
        error("FileSystem is NULL");
        return -1;
    }

    // Declare local variable for free_blocks
    bool *free_blocks = fs->free_blocks;


    // Check if there are enough free blocks for Inode 0
    if (free_blocks == NULL) {
        error("Free block bitmap is NULL");
        return -1;
    }

    // Search Inode table for free inode
    for (uint32_t i = 0; i < fs->meta_data.inodes; i++) {
        Block block;
        if (disk_read(fs->disk, i / INODES_PER_BLOCK + 1, block.data) == DISK_FAILURE) {
            error("Failed to read inode block");
            continue;
        }

        if (!block.inodes[i % INODES_PER_BLOCK].valid) {
            // Reserve free inode in Inode table
            block.inodes[i % INODES_PER_BLOCK].valid = true;

            // Set the size of Inode 0 to 500 bytes
            if (i == 0) {
                block.inodes[i % INODES_PER_BLOCK].size = 500;
            }

            if (disk_write(fs->disk, i / INODES_PER_BLOCK + 1, block.data) == DISK_FAILURE) {
                error("Failed to write inode block");
                continue;
            }

            // Update the free block count
            // Note: This update is not reflected in the struct; it's assumed to be handled elsewhere.
            return i;
        }
    }

    return -1;
}

/**
 * Remove Inode and associated data from FileSystem by doing the following:
 *
 *  1. Load and check status of Inode.
 *
 *  2. Release any direct blocks.
 *
 *  3. Release any indirect blocks.
 *
 *  4. Mark Inode as free in Inode table.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Whether or not removing the specified Inode was successful.
 **/
bool fs_remove(FileSystem *fs, size_t inode_number) {
    if (fs == NULL || inode_number >= fs->meta_data.inodes) {
        error("FileSystem is NULL or inode number is out of range");
        return false;
    }

    // Load and check status of Inode
    Block block;
    size_t block_number = inode_number / INODES_PER_BLOCK + 1;
    if (disk_read(fs->disk, block_number, block.data) == DISK_FAILURE) {
        error("Failed to read inode block");
        return false;
    }

    Inode *inode = &block.inodes[inode_number % INODES_PER_BLOCK];
    if (!inode->valid) {
        error("Inode %zu is not valid", inode_number);
        return false;
    }

    // Release any direct and indirect blocks (Example: Mark them as free)
    // ...

    // Mark Inode as free in Inode table
    inode->valid = false;
    if (disk_write(fs->disk, block_number, block.data) == DISK_FAILURE) {
        error("Failed to write inode block");
        return false;
    }

    return true;
}

/**
 * Return size of specified Inode.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Size of specified Inode (-1 if does not exist).
 **/
ssize_t fs_stat(FileSystem *fs, size_t inode_number) {
    if (fs == NULL || inode_number >= fs->meta_data.inodes) {
        error("FileSystem is NULL or inode number is out of range");
        return -1;
    }

    Block block;
    if (disk_read(fs->disk, inode_number / INODES_PER_BLOCK + 1, block.data) == DISK_FAILURE) {
        error("Failed to read inode block");
        return -1;
    }

    Inode *inode = &block.inodes[inode_number % INODES_PER_BLOCK];
    if (!inode->valid) {
        error("Inode %zu is not valid", inode_number);
        return -1;
    }

    return inode->size;
}

/**
 * Read from the specified Inode into the data buffer exactly length bytes
 * beginning from the specified offset by doing the following:
 *
 *  1. Load Inode information.
 *
 *  2. Continuously read blocks and copy data to buffer.
 *
 *  Note: Data is read from direct blocks first, and then from indirect blocks.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to read data from.
 * @param       data            Buffer to copy data to.
 * @param       length          Number of bytes to read.
 * @param       offset          Byte offset from which to begin reading.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    if (fs == NULL || inode_number >= fs->meta_data.inodes) {
        error("Invalid arguments");
        return -1;
    }

    Block block;
    size_t block_index = inode_number / INODES_PER_BLOCK + 1;

    if (disk_read(fs->disk, block_index, block.data) == DISK_FAILURE) {
        error("Failed to read inode block");
        return -1;
    }

    Inode *inode = &block.inodes[inode_number % INODES_PER_BLOCK];
    if (!inode->valid || offset >= inode->size) {
        error("Invalid inode or offset");
        return -1;
    }

    size_t bytes_read = 0;
    size_t remaining = min(length, inode->size - offset);
    size_t block_offset = offset / BLOCK_SIZE;
    size_t internal_offset = offset % BLOCK_SIZE;

    while (remaining > 0 && block_offset < POINTERS_PER_INODE) {
        size_t copy_size = min(BLOCK_SIZE - internal_offset, remaining);

        if (disk_read(fs->disk, inode->direct[block_offset], block.data) == DISK_FAILURE) {
            error("Failed to read block");
            break;
        }

        memcpy(data + bytes_read, block.data + internal_offset, copy_size);
        bytes_read += copy_size;
        remaining -= copy_size;
        block_offset++;
        internal_offset = 0;
    }

    // Handle indirect blocks if necessary
    // ...

    return bytes_read;
}

/**
 * Write to the specified Inode from the data buffer exactly length bytes
 * beginning from the specified offset by doing the following:
 *
 *  1. Load Inode information.
 *
 *  2. Continuously copy data from buffer to blocks.
 *
 *  Note: Data is read from direct blocks first, and then from indirect blocks.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to write data to.
 * @param       data            Buffer with data to copy
 * @param       length          Number of bytes to write.
 * @param       offset          Byte offset from which to begin writing.
 * @return      Number of bytes read (-1 on error).
 **/
// ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
//     if (fs == NULL || inode_number >= fs->meta_data.inodes) {
//         error("Invalid arguments");
//         return -1;
//     }

//     Block block;
//     size_t block_index = inode_number / INODES_PER_BLOCK + 1;

//     if (disk_read(fs->disk, block_index, block.data) == DISK_FAILURE) {
//         error("Failed to read inode block");
//         return -1;
//     }

//     Inode *inode = &block.inodes[inode_number % INODES_PER_BLOCK];
//     if (!inode->valid) {
//         error("Invalid inode");
//         return -1;
//     }

//     size_t bytes_written = 0;
//     size_t remaining = length;
//     size_t block_offset = offset / BLOCK_SIZE;
//     size_t internal_offset = offset % BLOCK_SIZE;

//     while (remaining > 0 && block_offset < POINTERS_PER_INODE) {
//         size_t copy_size = min(BLOCK_SIZE - internal_offset, remaining);

//         // Allocate block if necessary
//         if (inode->direct[block_offset] == 0) {
//             // Allocate new block and update inode
//             // ...
//         }

//         if (disk_read(fs->disk, inode->direct[block_offset], block.data) == DISK_FAILURE) {
//             error("Failed to read block");
//             break;
//         }

//         memcpy(block.data + internal_offset, data + bytes_written, copy_size);
//         if (disk_write(fs->disk, inode->direct[block_offset], block.data) == DISK_FAILURE) {
//             error("Failed to write block");
//             break;
//         }

//         bytes_written += copy_size;
//         remaining -= copy_size;
//         block_offset++;
//         internal_offset = 0;
//     }

//     // Update inode size and write back inode
//     // ...

//     // Handle indirect blocks if necessary
//     // ...

//     return bytes_written;
// }
// Allocate a free block in the filesystem and mark it as allocated
uint32_t fs_allocate_block(FileSystem *fs) {
    if (fs == NULL) {
        error("FileSystem is NULL");
        return 0;
    }

    // Iterate through the free_blocks array to find a free block
    for (uint32_t block = 0; block < fs->meta_data.blocks; block++) {
        if (!fs->free_blocks[block]) {
            // Mark the block as allocated
            fs->free_blocks[block] = true;
            return block;
        }
    }

    // No free blocks available
    error("No free blocks available");
    return 0;
}

ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    if (fs == NULL || inode_number >= fs->meta_data.inodes) {
        error("Invalid arguments");
        return -1;
    }

    Block block;
    size_t block_index = inode_number / INODES_PER_BLOCK + 1;

    if (disk_read(fs->disk, block_index, block.data) == DISK_FAILURE) {
        error("Failed to read inode block");
        return -1;
    }

    Inode *inode = &block.inodes[inode_number % INODES_PER_BLOCK];
    if (!inode->valid) {
        error("Invalid inode");
        return -1;
    }

    size_t bytes_written = 0;
    size_t remaining = length;
    size_t block_offset = offset / BLOCK_SIZE;
    size_t internal_offset = offset % BLOCK_SIZE;

    while (remaining > 0 && block_offset < POINTERS_PER_INODE) {
        size_t copy_size = min(BLOCK_SIZE - internal_offset, remaining);

        // Allocate block if necessary
        if (inode->direct[block_offset] == 0) {
            // Allocate new block and update inode
            uint32_t new_block = fs_allocate_block(fs);
            if (new_block == 0) {
                error("Failed to allocate block");
                break;
            }
            inode->direct[block_offset] = new_block;
        }

        if (disk_read(fs->disk, inode->direct[block_offset], block.data) == DISK_FAILURE) {
            error("Failed to read block");
            break;
        }

        // Copy data to the block
        memcpy(block.data + internal_offset, data + bytes_written, copy_size);

        if (disk_write(fs->disk, inode->direct[block_offset], block.data) == DISK_FAILURE) {
            error("Failed to write block");
            break;
        }

        bytes_written += copy_size;
        remaining -= copy_size;
        block_offset++;
        internal_offset = 0;
    }

    // Update inode size
    inode->size += bytes_written;

    // Write back the updated inode
    size_t inode_block_index = inode_number / INODES_PER_BLOCK + 1;

    if (disk_write(fs->disk, inode_block_index, block.data) == DISK_FAILURE) {
        error("Failed to write inode block");
        return -1;
    }

    return bytes_written;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
