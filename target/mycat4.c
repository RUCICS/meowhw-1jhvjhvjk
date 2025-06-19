#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h> 

// 获取内存页大小
long get_page_size() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf error determining page size");
        fprintf(stderr, "警告: 无法确定页面大小，将使用备用值 4096 字节。\n");
        return 4096;
    }
    return page_size;
}

// align_alloc 和 align_free 函数与 mycat3.c 中的相同
void* align_alloc(size_t size, size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        fprintf(stderr, "错误: alignment 必须是2的幂且不为0\n");
        return NULL;
    }
    if (size == 0) {
        return NULL;
    }
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
    if (ptr == NULL) {
        return;
    }
    void* original_ptr = ((void**)ptr)[-1];
    free(original_ptr);
}

// 函数用于确定I/O操作的块大小
long get_io_blocksize(int fd, long page_size) {
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat error determining file block size");
        fprintf(stderr, "警告: 无法获取文件状态，将使用页面大小作为缓冲区大小。\n");
        return page_size;
    }

    long fs_block_size = st.st_blksize;

    // 注意事项处理：
    // 1. 文件系统中的每个文件，块大小不总是相同的。 (st_blksize 是针对此文件的)
    // 2. 有的文件系统可能会给出虚假的块大小，这种虚假的文件块大小可能根本不是2的整数次幂。

    // 检查 fs_block_size 是否有效
    if (fs_block_size <= 0) { // 例如，如果fstat成功但st_blksize是0或负数
        fprintf(stderr, "警告: 获取到的文件系统块大小无效 (%ld)，将使用页面大小。\n", fs_block_size);
        return page_size;
    }

    //如果 st_blksize 有效且大于 page_size，则使用 st_blksize。
    // 否则，使用 page_size。这样可以保证缓冲区至少是 page_size。
    // 或者，直接使用 st_blksize 如果它有效，否则 page_size。


    if (fs_block_size > 0 && fs_block_size < (1024 * 1024) ) { // 假设一个合理的上限，例如1MB
        return fs_block_size;
    } else {
        fprintf(stderr, "警告: 文件系统块大小 (%ld) 无效或过大/过小，将使用页面大小。\n", fs_block_size);
        return page_size;
    }
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
    long buffer_size = get_io_blocksize(fd, page_size);



    // 缓冲区仍然按页大小对齐，但其大小由 get_io_blocksize 决定
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
