#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

    char buffer[1]; // Read one character at a time
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t bytes_written = write(STDOUT_FILENO, buffer, bytes_read);
        if (bytes_written == -1) {
            perror("Error writing to stdout");
            close(fd);
            return 1;
        }
        if (bytes_written != bytes_read) {
            fprintf(stderr, "Partial write error\n");
            close(fd);
            return 1;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading file");
        close(fd);
        return 1;
    }

    if (close(fd) == -1) {
        perror("Error closing file");
        return 1;
    }

    return 0;
}


