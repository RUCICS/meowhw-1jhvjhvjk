#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#define OPTIMAL_BUFFER_SIZE (256 * 1024)//根据实验结果得出

long get_page_size() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf error determining page size");
        return 4096;
    }
    return page_size;
}

void* align_alloc(size_t size, size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        fprintf(stderr, "错误: alignment 必须是2的幂且不为0\n");
        return NULL;
    }
    if (size == 0) return NULL;
    void *original_ptr = NULL;
    void *aligned_ptr = NULL;
    size_t total_size = size + alignment - 1 + sizeof(void*);
    original_ptr = malloc(total_size);
    if (original_ptr == NULL) {
        perror("align_alloc: malloc失败");
        return NULL;
    }
    uintptr_t temp_ptr_val = (uintptr_t)((char*)original_ptr + sizeof(void*));
    aligned_ptr = (void*)((temp_ptr_val + alignment - 1) & ~(alignment - 1));
    ((void**)aligned_ptr)[-1] = original_ptr;
    return aligned_ptr;
}

void align_free(void* ptr) {
    if (ptr == NULL) return;
    void* original_ptr = ((void**)ptr)[-1];
    free(original_ptr);
}

long get_io_blocksize_task5(long page_size) {

    if (OPTIMAL_BUFFER_SIZE < page_size) {
        return page_size;
    }
    return OPTIMAL_BUFFER_SIZE;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法: %s <文件路径>\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    int fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        perror("错误: 打开文件失败");
        return 1;
    }

    long page_size = get_page_size();
    
    long buffer_size = get_io_blocksize_task5(page_size);

    // Buffer is aligned to page_size, size is buffer_size
    char *buffer = (char *)align_alloc(buffer_size, page_size);
    if (buffer == NULL) {
        fprintf(stderr, "错误: 分配对齐的缓冲区失败\n");
        close(fd);
        return 1;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, buffer_size)) > 0) {
        ssize_t total_bytes_written = 0;
        while (total_bytes_written < bytes_read) {
            ssize_t bytes_written_this_call = write(STDOUT_FILENO, buffer + total_bytes_written, bytes_read - total_bytes_written);
            if (bytes_written_this_call == -1) {
                if (errno == EINTR) continue;
                perror("错误: 写入标准输出失败");
                align_free(buffer);
                close(fd);
                return 1;
            }
            total_bytes_written += bytes_written_this_call;
        }
    }

    if (bytes_read == -1) {
        perror("错误: 读取文件失败");
        align_free(buffer);
        close(fd);
        return 1;
    }

    align_free(buffer);
    if (close(fd) == -1) {
        perror("错误: 关闭文件失败");
        return 1;
    }

    return 0;
}

