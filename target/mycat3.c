#include <stdio.h>
#include <stdlib.h> 
#include <stdint.h> 
#include <fcntl.h>
#include <unistd.h> 
#include <errno.h>  

// 获取内存页大小
long get_page_size() {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        return 4096;
    }
    return page_size;
}

/**
 * @brief 分配对齐的内存
 *
 * @param size 希望分配的最小字节数
 * @param alignment 对齐边界，必须是2的幂
 * @return void* 成功时返回对齐的内存指针，失败时返回NULL
 *
 * 工作原理:
 * 1. 计算需要额外分配的空间：alignment-1 用于保证在分配的块内能找到对齐的地址，
 *    sizeof(void*) 用于存储原始malloc返回的指针。
 * 2. 使用malloc分配总空间：size + (alignment - 1) + sizeof(void*)。
 * 3. 在分配的内存中找到对齐的地址：
 *    - 原始指针 + sizeof(void*) 得到可以开始对齐调整的地址。
 *    - ( (uintptr_t)ptr_before_alignment + alignment - 1 ) & ~(alignment - 1)
 *      这个公式将地址向上舍入到最近的alignment倍数。
 * 4. 在对齐地址的前面 (aligned_ptr - sizeof(void*)) 存储原始malloc返回的指针。
 * 5. 返回对齐后的地址。
 */
void* align_alloc(size_t size, size_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) { // 检查alignment是否为2的幂
        fprintf(stderr, "错误: alignment 必须是2的幂且不为0\n");
        return NULL;
    }
    if (size == 0) {
        return NULL; // 或者根据需求返回一个对齐的非NULL指针，但大小为0没有意义
    }

    // 为对齐和存储原始指针预留额外空间
    // alignment - 1: 保证在[ptr, ptr + alignment - 1]区间内必有一个地址是alignment的倍数
    // sizeof(void*): 用来存储原始malloc返回的指针，以便后续free
    void *original_ptr = NULL;
    void *aligned_ptr = NULL;
    size_t total_size = size + alignment - 1 + sizeof(void*);

    original_ptr = malloc(total_size);
    if (original_ptr == NULL) {
        perror("align_alloc: malloc失败");
        return NULL;
    }

    // 计算对齐后的指针
    // 1. (char*)original_ptr + sizeof(void*): 跳过存储原始指针的空间，得到可以开始进行对齐调整的地址
    // 2. (uintptr_t)...: 转换为整数类型进行位运算
    // 3. ... + alignment - 1: 确保加上这个值后，向下取整到alignment的倍数时，不会比原始需求小
    // 4. & ~(alignment - 1): 将末尾的几位置零，实现向下取整到alignment的倍数的效果 (因为alignment是2的幂)
    //    例如，alignment=4096 (0x1000), alignment-1 = 4095 (0xFFF), ~(alignment-1) = ~0xFFF
    uintptr_t temp_ptr_val = (uintptr_t)((char*)original_ptr + sizeof(void*));
    aligned_ptr = (void*)((temp_ptr_val + alignment - 1) & ~(alignment - 1));

    // 在对齐指针的前面存储原始malloc返回的指针
    // ((void**)aligned_ptr)[-1] 意思是：将aligned_ptr看作一个void*数组的指针，然后访问它前面的一个元素
    ((void**)aligned_ptr)[-1] = original_ptr;

    return aligned_ptr;
}

/**
 * @brief 释放由align_alloc分配的内存
 *
 * @param ptr 由align_alloc返回的对齐指针
 *
 * 工作原理:
 * 1. 从对齐指针ptr的前面 (ptr - sizeof(void*)) 读取之前存储的原始malloc指针。
 * 2. 使用获取到的原始指针调用free。
 */
void align_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    // 读取存储在对齐指针前方的原始malloc指针
    void* original_ptr = ((void**)ptr)[-1];
    free(original_ptr);
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <文件路径>\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    int fd = open(filepath, O_RDONLY);

    if (fd == -1) {
        perror("错误: 打开文件失败");
        return 1;
    }

    long page_size = get_page_size();
    if (page_size <= 0) {
        fprintf(stderr, "错误: 获取的页大小无效。\n");
        close(fd);
        return 1;
    }
    // printf("信息: 使用页大小作为缓冲区大小: %ld 字节\n", page_size);

    // 使用 align_alloc 分配缓冲区
    char *buffer = (char *)align_alloc(page_size, page_size);
    if (buffer == NULL) {
        fprintf(stderr, "错误: 分配对齐的缓冲区失败\n");
        close(fd);
        return 1;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, page_size)) > 0) {
        ssize_t total_bytes_written = 0;
        while (total_bytes_written < bytes_read) {
            ssize_t bytes_written_this_call = write(STDOUT_FILENO, buffer + total_bytes_written, bytes_read - total_bytes_written);
            if (bytes_written_this_call == -1) {
                if (errno == EINTR) continue; // 处理中断信号
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

