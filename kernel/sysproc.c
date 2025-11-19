#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
//在这里实现三个最基本的调用，getpid, exit, write

// 获取当前进程 PID
int sys_getpid(void) {
    return current_proc->pid;
}

// 退出进程
int sys_exit(void) {
    // 暂时简单实现：打印日志并挂起
    printf_color(COLOR_RED, "Process %d exited.\n", current_proc->pid);
    
    // 标记为 UNUSED (在完整版中应为 ZOMBIE 并通知父进程)
    current_proc->state = UNUSED; 
    
    // 让出 CPU，调度器会切换到下一个进程
    yield(); 
    return 0;
}