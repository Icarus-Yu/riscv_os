#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
//获取系统调用参数的外部函数
extern int argint(int n, int *ip);

int sys_getpid(void) {
    return current_proc->pid;
}

int sys_exit(void) {
    int n;
    if(argint(0, &n) < 0) // 获取参数 status
        return -1;
    exit(n);
    return 0; // 不会执行到这里
}

int sys_fork(void) {
    return fork();
}

int sys_wait(void) {
    // 简化版：暂时忽略传入的地址参数，默认 wait(0)
    // 完整版需要：uint64 p; argaddr(0, &p);
    return wait(0);
}

int sys_sleep(void) {
    int n;
    if(argint(0, &n) < 0) // 获取睡眠时间
        return -1;
    
    // 这里简单实现：利用 yield 模拟睡眠 n 次调度周期
    // 实际 OS 应该利用时钟中断计数 ticks
    for(int i=0; i<n; i++) {
        yield();
    }
    return 0;
}
