#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
#include "string.h" // 需要 strlen
//内核处理系统调用的核心，读取寄存器a7中的系统调用号，并将结果写入a0
#include "memory.h"
// 声明外部函数
extern int sys_write(void);
extern int sys_read(void);
extern int sys_getpid(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_wait(void);
extern int sys_sleep(void);
extern int sys_exec(void);
// --- 【新增：补充缺失的系统调用声明】 ---
extern int sys_open(void);
extern int sys_close(void);
extern int sys_mkdir(void);
extern int sys_chdir(void);
extern int sys_link(void);
extern int sys_unlink(void);

// 【新增】
extern int sys_dup(void);
extern int sys_fstat(void);

// 系统调用函数表
static int (*syscalls[])(void) = {
    [SYS_write]   sys_write,
    [SYS_read]    sys_read,
    [SYS_getpid]  sys_getpid,
    [SYS_exit]    sys_exit,
    [SYS_fork]    sys_fork,
    [SYS_wait]    sys_wait,
    [SYS_sleep]   sys_sleep,
    // --- 新增 ---
    [SYS_open]    sys_open,
    [SYS_close]   sys_close,
    [SYS_mkdir]   sys_mkdir,
    [SYS_chdir]   sys_chdir,
    [SYS_link]    sys_link,
    [SYS_unlink]  sys_unlink,

    // 【新增】对应 include/syscall.h 中的编号
    [SYS_dup]     sys_dup,    // SYS_dup = 10
    [SYS_fstat]   sys_fstat,  // SYS_fstat = 8
    [SYS_exec]    sys_exec, 
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

// 【新增 2】实现 fetchaddr 和 fetchstr
int fetchaddr(uint64 addr, uint64 *ip) {
  struct proc *p = current_proc;
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(uint64)) != 0)
    return -1;
  return 0;
}

int fetchstr(uint64 addr, char *buf, int max) {
  struct proc *p = current_proc;
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return -1;
  return strlen(buf);
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