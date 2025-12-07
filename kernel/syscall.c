#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
//内核处理系统调用的核心，读取寄存器a7中的系统调用号，并将结果写入a0

// 声明外部函数
extern int sys_write(void);
extern int sys_read(void);
extern int sys_getpid(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_wait(void);
extern int sys_sleep(void);

// 系统调用函数表
static int (*syscalls[])(void) = {
    [SYS_write]   sys_write,
    [SYS_read]    sys_read,
    [SYS_getpid]  sys_getpid,
    [SYS_exit]    sys_exit,
    [SYS_fork]    sys_fork,
    [SYS_wait]    sys_wait,
    [SYS_sleep]   sys_sleep,
};

// 辅助函数：获取第 n 个 int 类型参数
// RISC-V 约定参数存放在 a0-a5 寄存器中
// 我们从当前进程的 trapframe 中读取这些值 [cite: 1499-1501]
int argint(int n, int *ip) {
    struct proc *p = current_proc;
    struct trapframe *tf = p->trapframe;

    switch (n) {
    case 0: *ip = tf->a0; break;
    case 1: *ip = tf->a1; break;
    case 2: *ip = tf->a2; break;
    case 3: *ip = tf->a3; break;
    case 4: *ip = tf->a4; break;
    case 5: *ip = tf->a5; break;
    default: return -1;
    }
    return 0;
}

// 系统调用分发入口 [cite: 1475-1486]
void syscall(void) {
    struct proc *p = current_proc;
    int num = p->trapframe->a7; // 系统调用号保存在 a7

    if (num > 0 && num < sizeof(syscalls) / sizeof(syscalls[0]) && syscalls[num]) {
        // 执行系统调用，返回值保存到 trapframe->a0
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("pid %d %s: unknown sys call %d\n",
               p->pid, "syscall", num);
        p->trapframe->a0 = -1;
    }
}

// 注意：确保 argaddr 函数也在这里或者被正确引用
int argaddr(int n, uint64_t *ip) {
    struct proc *p = current_proc;
    struct trapframe *tf = p->trapframe;
    switch (n) {
    case 0: *ip = tf->a0; break;
    case 1: *ip = tf->a1; break;
    case 2: *ip = tf->a2; break;
    // ... 其他参数 ...
    default: return -1;
    }
    return 0;
}