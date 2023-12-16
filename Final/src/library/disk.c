/* disk.c: SimpleFS disk emulator */

#include "sfs/disk.h"
#include "sfs/logging.h"



#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


/* Internal Prototyes */
bool    disk_sanity_check(Disk *disk, size_t blocknum, const char *data);
Disk* allocate_and_initialize_disk(const char* path, size_t blocks);
int open_disk_file(const char* path);
void custom_error_handler(const char* message, int error_code);

/* External Functions */

/**
 *
 * Opens disk at specified path with the specified number of blocks by doing
 * the following:
 *
 *  1. Allocates Disk structure and sets appropriate attributes.
 *
 *  2. Opens file descriptor to specified path.
 *
 *  3. Truncates file to desired file size (blocks * BLOCK_SIZE).
 *
 * @param       path        Path to disk image to create.
 * @param       blocks      Number of blocks to allocate for disk image.
 *
 * @return      Pointer to newly allocated and configured Disk structure (NULL
 *              on failure).
 **/
Disk *	disk_open(const char *path, size_t blocks) {
    // Allocate Disk structure
    Disk *disk = malloc(sizeof(Disk));
    if (!disk) {
        return NULL;
    }

    // Open file descriptor
    int fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        free(disk);
        return NULL;
    }

    // Set attributes
    disk->fd = fd;
    disk->blocks = blocks;
    disk->reads = 0;
    disk->writes = 0;

    // Truncate file to desired size
    if (ftruncate(fd, blocks * BLOCK_SIZE) == -1) {
        close(fd);
        free(disk);
        return NULL;
    }

    return disk;
}

void log_write(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}

/**
 * Close disk structure by doing the following:
 *
 *  1. Close disk file descriptor.
 *
 *  2. Report number of disk reads and writes.
 *
 *  3. Releasing disk structure memory.
 *
 * @param       disk        Pointer to Disk structure.
 */
void	disk_close(Disk *disk) {
        if (!disk) {
        return;
    }

    // Close file descriptor
    close(disk->fd);

    // Report disk operation counts
    info("Disk closed: %zu reads, %zu writes", disk->reads, disk->writes);

    // Free Disk structure
    free(disk);

}

/**
 * Read data from disk at specified block into data buffer by doing the
 * following:
 *
 *  1. Performing sanity check.
 *
 *  2. Seeking to specified block.
 *
 *  3. Reading from block to data buffer (must be BLOCK_SIZE).
 *
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Number of bytes read.
 *              (BLOCK_SIZE on success, DISK_FAILURE on failure).
 **/
ssize_t disk_read(Disk *disk, size_t block, char *data) {
    if (!disk_sanity_check(disk, block, data)) {
        return DISK_FAILURE;
    }

    off_t offset = block * BLOCK_SIZE;
    if (lseek(disk->fd, offset, SEEK_SET) == (off_t)-1) {
        return DISK_FAILURE;
    }

    ssize_t result = read(disk->fd, data, BLOCK_SIZE);
    if (result == BLOCK_SIZE) {
        disk->reads++;
        return result; // Return the number of bytes read
    } else {
        return DISK_FAILURE; // Return DISK_FAILURE on partial read or error
    }
}

/**
 * Write data to disk at specified block from data buffer by doing the
 * following:
 *
 *  1. Performing sanity check.
 *
 *  2. Seeking to specified block.
 *
 *  3. Writing data buffer (must be BLOCK_SIZE) to disk block.
 *
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Number of bytes written.
 *              (BLOCK_SIZE on success, DISK_FAILURE on failure).
 **/
ssize_t disk_write(Disk *disk, size_t block,  char *data) {
    if (!disk_sanity_check(disk, block, data)) {
        return DISK_FAILURE;
    }

    off_t offset = block * BLOCK_SIZE;
    if (lseek(disk->fd, offset, SEEK_SET) == (off_t)-1) {
        return DISK_FAILURE;
    }

    ssize_t result = write(disk->fd, data, BLOCK_SIZE);
    if (result == BLOCK_SIZE) {
        disk->writes++;
        return result; // Return the number of bytes written
    } else {
        return DISK_FAILURE; // Return DISK_FAILURE on partial write or error
    }
}



/* Internal Functions */

/**
 * Perform sanity check before read or write operation:
 *
 *  1. Check for valid disk.
 *
 *  2. Check for valid block.
 *
 *  3. Check for valid data.
 *
 * @param       disk        Pointer to Disk structure.
 * @param       block       Block number to perform operation on.
 * @param       data        Data buffer.
 *
 * @return      Whether or not it is safe to perform a read/write operation
 *              (true for safe, false for unsafe).
 **/
bool disk_sanity_check(Disk *disk, size_t block, const char *data) {
    if (!disk || !data) {
        return false;
    }

    if (block >= disk->blocks) {
        return false;
    }

    return true;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */






// #include "sfs/disk.h"
// #include "sfs/logging.h"


// #include <fcntl.h>
// #include <unistd.h>
// #include <errno.h>
// #include <sys/types.h>
// #include <sys/stat.h> 


// /* Internal Functions */
// static bool disk_sanity_check(Disk *disk, size_t block, const char *data);

// /* External Functions */
// Disk *disk_open(const char *path, size_t blocks) {
//     Disk *disk = malloc(sizeof(Disk));
//     if (disk == NULL) {
//         error("Failed to allocate disk structure");
//         return NULL;
//     }

//     disk->fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
//     if (disk->fd == -1) {
//         error("Failed to open disk file: %s", path);
//         free(disk);
//         return NULL;
//     }

//     disk->blocks = blocks;
//     disk->reads = 0;
//     disk->writes = 0;
//     disk->mounted = false;

//     if (ftruncate(disk->fd, blocks * BLOCK_SIZE) == -1) {
//         error("Failed to truncate disk file: %s", path);
//         close(disk->fd);
//         free(disk);
//         return NULL;
//     }

//     return disk;
// }

// void disk_close(Disk *disk) {
//     if (disk == NULL) {
//         return;
//     }

//     if (close(disk->fd) == -1) {
//         error("Failed to close disk file descriptor");
//     }

//     free(disk);
// }

// ssize_t disk_read(Disk *disk, size_t block, char *data) {
//     if (!disk_sanity_check(disk, block, data)) {
//         return DISK_FAILURE;
//     }

//     off_t offset = block * BLOCK_SIZE;
//     if (lseek(disk->fd, offset, SEEK_SET) == (off_t)-1) {
//         error("Failed to seek in disk read");
//         return DISK_FAILURE;
//     }

//     ssize_t result = read(disk->fd, data, BLOCK_SIZE);
//     if (result != BLOCK_SIZE) {
//         error("Failed to read the full block from disk");
//         return DISK_FAILURE;
//     }

//     disk->reads++;
//     return result;
// }

// ssize_t disk_write(Disk *disk, size_t block, char *data) {
//     if (!disk_sanity_check(disk, block, data)) {
//         return DISK_FAILURE;
//     }

//     off_t offset = block * BLOCK_SIZE;
//     if (lseek(disk->fd, offset, SEEK_SET) == (off_t)-1) {
//         error("Failed to seek in disk write");
//         return DISK_FAILURE;
//     }

//     ssize_t result = write(disk->fd, data, BLOCK_SIZE);
//     if (result != BLOCK_SIZE) {
//         error("Failed to write the full block to disk");
//         return DISK_FAILURE;
//     }

//     disk->writes++;
//     return result;
// }

// /* Internal Functions */
// static bool disk_sanity_check(Disk *disk, size_t block, const char *data) {
//     return disk != NULL && disk->fd != -1 && block < disk->blocks && data != NULL;
// }
