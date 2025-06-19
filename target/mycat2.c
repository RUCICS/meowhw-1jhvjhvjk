#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

long get_io_blocksize() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf error determining page size");
        fprintf(stderr, "Warning: Could not determine page size, using fallback of 4096 bytes.\n");
        return 4096;
    }
    return page_size;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    int fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    long buffer_size = get_io_blocksize();
    if (buffer_size <= 0) { 
        fprintf(stderr, "Error: Invalid buffer size determined.\n");
        close(fd);
        return 1;
    }


    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        perror("Error allocating buffer");
        close(fd);
        return 1;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, buffer_size)) > 0) {
        ssize_t total_bytes_written = 0;
        while (total_bytes_written < bytes_read) {
            ssize_t bytes_written_this_call = write(STDOUT_FILENO, buffer + total_bytes_written, bytes_read - total_bytes_written);
            if (bytes_written_this_call == -1) {
                perror("Error writing to stdout");
                free(buffer);
                close(fd);
                return 1;
            }
            total_bytes_written += bytes_written_this_call;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading file");
        free(buffer);
        close(fd);
        return 1;
    }

    free(buffer);
    if (close(fd) == -1) {
        perror("Error closing file");
        return 1;
    }

    return 0;
}