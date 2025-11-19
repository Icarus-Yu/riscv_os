#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
//文件处理相关

// 引用 syscall.c 中的 argint
int argint(int n, int *ip);

// 写文件系统调用
// 参数: fd (a0), buf (a1), count (a2)
int sys_write(void) {
    int fd;
    uint64_t p; 
    int n;

    // 获取参数
    if (argint(0, &fd) < 0 || argint(2, &n) < 0)
        return -1;
    
    // 获取缓冲区地址 (直接从 a1 读取)
    p = current_proc->trapframe->a1;

    // 目前只支持向 stdout (1) 和 stderr (2) 写入
    if (fd == 1 || fd == 2) {
        // 注意：这里假设我们能直接访问该地址 (内核线程或恒等映射)
        // 在真正的用户进程中，需要通过页表转换地址 (copyin)
        char *s = (char *)p;
        for (int i = 0; i < n; i++) {
            consputc(s[i]);
        }
        return n;
    }
    return -1;
}
